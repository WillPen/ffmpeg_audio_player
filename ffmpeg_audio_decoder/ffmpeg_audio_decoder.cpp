// ffmpeg_audio_decoder.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample\swresample.h"
#include "libavutil/imgutils.h"
};

//链接库
#pragma comment(lib , "avformat.lib")
#pragma comment(lib , "avcodec.lib")
#pragma comment(lib , "avdevice.lib")
#pragma comment(lib , "avfilter.lib")
#pragma comment(lib , "avutil.lib")
#pragma comment(lib , "postproc.lib")
#pragma comment(lib , "swresample.lib")

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio : 48k*32/8=192000

int main(int argc, char* argv[])
{

	AVFormatContext	*pFormatCtx = NULL;
	int				videoindex = -1;
	AVCodecContext	*pCodecCtx = NULL;
	AVCodec			*pCodec = NULL;
	AVFrame	*pFrame = NULL;
	uint8_t *out_buffer = NULL;
	AVPacket *packet = NULL;
	struct SwrContext *au_convert_ctx;

	int ret = -1;

	char filepath[] = "leapfrog.mp3";

	FILE *fp_yuv = fopen("leapfrog.pcm", "wb+");


	//avcodec_register_all();//复用器等并没有使用到，不需要初始化，直接调用av_register_all就行
	av_register_all();
	//avformat_network_init();
	if (!(pFormatCtx = avformat_alloc_context()))
	{
		printf("avformat_alloc_context error!!,ret=%d\n", AVERROR(ENOMEM));
		return -1;
	}

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}

	av_dump_format(pFormatCtx, 0, filepath, 0);

	/*
	//another way to get the stream id
	for (i = 0; i < pFormatCtx->nb_streams; i++) {
	if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	videoindex = i;
	break;
	}
	}
	*/

	/* select the video stream */
	ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &pCodec, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return ret;
	}
	videoindex = ret; //video stream id

	pCodec = avcodec_find_decoder(pFormatCtx->streams[videoindex]->codecpar->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}

	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (pCodecCtx == NULL)
	{
		printf("Could not allocate AVCodecContext\n");
		return -1;
	}
	if ((ret = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoindex]->codecpar)) < 0)
	{
		printf("Failed to copy codec parameters to decoder context\n");
		return ret;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		printf("Could not open codec.\n");
		return -1;
	}

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	if (!packet) {
		fprintf(stderr, "Can not alloc packet\n");
		return -1;
	}
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	//FIX:Some Codec's Context Information is missing
	int64_t in_channel_layout;
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	//Swr
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoindex) {
			ret = avcodec_send_packet(pCodecCtx, packet);
			if (ret != 0)
			{
				printf("send pkt error.\n");
				return ret;
			}

			if (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE,
					(const uint8_t **)pFrame->data, pFrame->nb_samples);

				fwrite(out_buffer, 1, out_buffer_size, fp_yuv);
				printf("Succeed to decode 1 frame!\n");

			}
		}
		av_packet_unref(packet);
	}

	swr_free(&au_convert_ctx);
	fclose(fp_yuv);

	av_frame_free(&pFrame);
	avcodec_free_context(&pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}

