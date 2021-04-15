// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/recording/recording_service.h"

#include <cstdlib>

#include "ash/services/recording/recording_encoder_muxer.h"
#include "ash/services/recording/recording_service_constants.h"
#include "ash/services/recording/video_capture_params.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_codecs.h"
#include "media/base/status.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/audio/public/cpp/device_factory.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace recording {

namespace {

// For a capture size of 320 by 240, we use a bitrate of 256 kbit/s. Based on
// that, we calculate the bits per second per squared pixel.
constexpr uint64_t kMinBitrateInBitsPerSecond = 256 * 1000;
constexpr float kBitsPerSecondPerSquarePixel =
    static_cast<float>(kMinBitrateInBitsPerSecond) / (320.f * 240.f);

// The maximum number of muxed chunks to buffer before sending them over IPC to
// the client. This value has been chosen as half the average number of chunks
// needed to fill a buffer of size 512 KB while recording a screen size of
// 1366 x 768 for about a minute and a half. Note that bombarding the client
// (e.g. Ash) with a ton of IPCs will cause the captured video to sometimes be
// janky.
// TODO(afakhry): Choose a different value if needed, or make it a function of
// the capture size (like the bitrate), or a function of the time since the last
// IPC call to the client.
constexpr int kMaxBufferedChunks = 238;

// The size within which we will try to fit a thumbnail image extracted from the
// first valid video frame. The value was chosen to be suitable with the image
// container in the notification UI.
constexpr gfx::Size kThumbnailSize{328, 184};

// Calculates the bitrate used to initialize the video encoder based on the
// given |capture_size|.
uint64_t CalculateVpxEncoderBitrate(const gfx::Size& capture_size) {
  return std::max(kMinBitrateInBitsPerSecond,
                  static_cast<uint64_t>(capture_size.GetArea() *
                                        kBitsPerSecondPerSquarePixel));
}

// Given the desired |capture_size|, it creates and returns the options needed
// to configure the video encoder.
media::VideoEncoder::Options CreateVideoEncoderOptions(
    const gfx::Size& capture_size) {
  media::VideoEncoder::Options video_encoder_options;
  video_encoder_options.bitrate = CalculateVpxEncoderBitrate(capture_size);
  video_encoder_options.framerate = kMaxFrameRate;
  video_encoder_options.frame_size = capture_size;
  // This value, expressed as a number of frames, forces the encoder to code
  // a keyframe if one has not been coded in the last keyframe_interval frames.
  video_encoder_options.keyframe_interval = 100;
  return video_encoder_options;
}

media::AudioParameters GetAudioParameters() {
  static_assert(kAudioSampleRate % 100 == 0,
                "Audio sample rate is not divisible by 100");
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, kAudioSampleRate,
                                kAudioSampleRate / 100);
}

// Extracts a potentially scaled-down RGB image from the given video |frame|,
// which is suitable to use as a thumbnail for the video.
gfx::ImageSkia ExtractImageFromVideoFrame(const media::VideoFrame& frame) {
  const gfx::Size visible_size = frame.visible_rect().size();
  media::PaintCanvasVideoRenderer renderer;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(visible_size.width(), visible_size.height());
  renderer.ConvertVideoFrameToRGBPixels(&frame, bitmap.getPixels(),
                                        bitmap.rowBytes());

  // Since this image will be used as a thumbnail, we can scale it down to save
  // on memory if needed. For example, if recording a FHD display, that will be
  // (for 12 bits/pixel):
  // 1920 * 1080 * 12 / 8, which is approx. = 3 MB, which is a lot to keep
  // around for a thumbnail.
  const gfx::ImageSkia thumbnail = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  if (visible_size.width() <= kThumbnailSize.width() &&
      visible_size.height() <= kThumbnailSize.height()) {
    return thumbnail;
  }

  const gfx::Size scaled_size =
      media::ScaleSizeToFitWithinTarget(visible_size, kThumbnailSize);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      thumbnail, skia::ImageOperations::ResizeMethod::RESIZE_BETTER,
      scaled_size);
}

// Called when the channel to the client of the recording service gets
// disconnected. At that point, there's nothing useful to do here, and instead
// of wasting resources encoding/muxing remaining frames, and flushing the
// buffers, we terminate the recording service process immediately.
void TerminateServiceImmediately() {
  LOG(ERROR)
      << "The recording service client was disconnected. Exiting immediately.";
  std::exit(EXIT_FAILURE);
}

}  // namespace

RecordingService::RecordingService(
    mojo::PendingReceiver<mojom::RecordingService> receiver)
    : audio_parameters_(GetAudioParameters()),
      receiver_(this, std::move(receiver)),
      consumer_receiver_(this),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoding_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // We use |USER_VISIBLE| here as opposed to |BEST_EFFORT| since the
          // latter is extremely low priority and may stall encoding for random
          // reasons.
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
}

RecordingService::~RecordingService() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_)
    return;

  // If the service gets destructed while recording in progress, the client must
  // be still connected (since otherwise the service process would have been
  // immediately terminated). We attempt to flush whatever we have right now
  // before exiting.
  DCHECK(client_remote_.is_bound());
  DCHECK(client_remote_.is_connected());
  StopRecording();
  video_capturer_remote_.reset();
  consumer_receiver_.reset();
  if (number_of_buffered_chunks_)
    FlushBufferedChunks();
  SignalRecordingEndedToClient(/*success=*/false);

  // Note that we don't need to call FlushAndFinalize() on the |encoder_muxer_|,
  // since it will be done asynchronously on the |encoding_task_runner_|, and by
  // then this |RecordingService| instance will have been already gone.
}

void RecordingService::RecordFullscreen(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& frame_sink_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(audio_stream_factory),
                    VideoCaptureParams::CreateForFullscreenCapture(
                        frame_sink_id, frame_sink_size));
}

void RecordingService::RecordWindow(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& frame_sink_size,
    const viz::SubtreeCaptureId& subtree_capture_id,
    const gfx::Size& window_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(
      std::move(client), std::move(video_capturer),
      std::move(audio_stream_factory),
      VideoCaptureParams::CreateForWindowCapture(
          frame_sink_id, subtree_capture_id, window_size, frame_sink_size));
}

void RecordingService::RecordRegion(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& frame_sink_size,
    const gfx::Rect& crop_region) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(audio_stream_factory),
                    VideoCaptureParams::CreateForRegionCapture(
                        frame_sink_id, frame_sink_size, crop_region));
}

void RecordingService::StopRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  video_capturer_remote_->Stop();
  if (audio_capturer_)
    audio_capturer_->Stop();
  audio_capturer_.reset();
}

void RecordingService::OnRecordedWindowChangingRoot(
    const viz::FrameSinkId& new_frame_sink_id,
    const gfx::Size& new_frame_sink_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  // If there's a change in the new root's size, we must reconfigure the video
  // encoder so that output video has the correct dimensions.
  if (current_video_capture_params_->OnRecordedWindowChangingRoot(
          video_capturer_remote_, new_frame_sink_id, new_frame_sink_size)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnRecordedWindowSizeChanged(
    const gfx::Size& new_window_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  if (current_video_capture_params_->OnRecordedWindowSizeChanged(
          video_capturer_remote_, new_window_size)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnFrameSinkSizeChanged(
    const gfx::Size& new_frame_sink_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  // If there's a change in the new root's size, we must reconfigure the video
  // encoder so that output video has the correct dimensions.
  if (current_video_capture_params_->OnFrameSinkSizeChanged(
          video_capturer_remote_, new_frame_sink_size)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  // We ignore any subsequent frames after a failure.
  if (did_failure_occur_)
    return;

  if (!data.IsValid()) {
    DLOG(ERROR) << "Video frame shared memory is invalid.";
    return;
  }

  base::ReadOnlySharedMemoryMapping mapping = data.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Mapping of video frame shared memory failed.";
    return;
  }

  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  if (!info->color_space) {
    DLOG(ERROR) << "Missing mandatory color space info.";
    return;
  }

  DCHECK(current_video_capture_params_);
  const gfx::Rect& visible_rect =
      current_video_capture_params_->GetVideoFrameVisibleRect(
          info->visible_rect);
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      info->pixel_format, info->coded_size, visible_rect, visible_rect.size(),
      reinterpret_cast<uint8_t*>(const_cast<void*>(mapping.memory())),
      mapping.size(), info->timestamp);
  if (!frame) {
    DLOG(ERROR) << "Failed to create a VideoFrame.";
    return;
  }

  // Takes ownership of |mapping| and |callbacks| to keep them alive until
  // |frame| is released.
  frame->AddDestructionObserver(base::BindOnce(
      [](base::ReadOnlySharedMemoryMapping mapping,
         mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
             callbacks) {},
      std::move(mapping), std::move(callbacks)));
  frame->set_metadata(info->metadata);
  frame->set_color_space(info->color_space.value());

  if (video_thumbnail_.isNull())
    video_thumbnail_ = ExtractImageFromVideoFrame(*frame);

  encoder_muxer_.AsyncCall(&RecordingEncoderMuxer::EncodeVideo).WithArgs(frame);
}

void RecordingService::OnStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // If a failure occurred, we don't wait till the capturer sends us this
  // signal. The recording had already been terminated by now.
  if (!did_failure_occur_)
    TerminateRecording(/*success=*/true);
}

void RecordingService::OnLog(const std::string& message) {
  DLOG(WARNING) << message;
}

void RecordingService::OnCaptureStarted() {}

void RecordingService::Capture(const media::AudioBus* audio_source,
                               base::TimeTicks audio_capture_time,
                               double volume,
                               bool key_pressed) {
  // This is called on a worker thread created by the |audio_capturer_| (See
  // |media::AudioDeviceThread|. The given |audio_source| wraps audio data in a
  // shared memory with the audio service. Calling |audio_capturer_->Stop()|
  // will destroy that thread and the shared memory mapping before we get a
  // chance to encode and flush the remaining frames (See
  // media::AudioInputDevice::Stop(), and
  // media::AudioInputDevice::AudioThreadCallback::Process() for details). It is
  // safer that we own our AudioBuses that are kept alive until encoded and
  // flushed.
  auto audio_data =
      media::AudioBus::Create(audio_source->channels(), audio_source->frames());
  audio_source->CopyTo(audio_data.get());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RecordingService::OnAudioCaptured,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(audio_data), audio_capture_time));
}

void RecordingService::OnCaptureError(const std::string& message) {
  LOG(ERROR) << message;
}

void RecordingService::OnCaptureMuted(bool is_muted) {}

void RecordingService::StartNewRecording(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    std::unique_ptr<VideoCaptureParams> capture_params) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (current_video_capture_params_) {
    LOG(ERROR) << "Cannot start a new recording while another is in progress.";
    return;
  }

  client_remote_.reset();
  client_remote_.Bind(std::move(client));
  client_remote_.set_disconnect_handler(
      base::BindOnce(&TerminateServiceImmediately));

  current_video_capture_params_ = std::move(capture_params);
  const bool should_record_audio = audio_stream_factory.is_valid();

  encoder_muxer_ = RecordingEncoderMuxer::Create(
      encoding_task_runner_,
      CreateVideoEncoderOptions(current_video_capture_params_->GetVideoSize()),
      should_record_audio ? &audio_parameters_ : nullptr,
      BindRepeatingToMainThread(&RecordingService::OnMuxerOutput),
      BindOnceToMainThread(&RecordingService::OnEncodingFailure));

  ConnectAndStartVideoCapturer(std::move(video_capturer));

  if (!should_record_audio)
    return;

  audio_capturer_ = audio::CreateInputDevice(
      std::move(audio_stream_factory),
      std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      audio::DeadStreamDetection::kEnabled);
  DCHECK(audio_capturer_);
  audio_capturer_->Initialize(audio_parameters_, this);
  audio_capturer_->Start();
}

void RecordingService::ReconfigureVideoEncoder() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(current_video_capture_params_);

  encoder_muxer_.AsyncCall(&RecordingEncoderMuxer::InitializeVideoEncoder)
      .WithArgs(CreateVideoEncoderOptions(
          current_video_capture_params_->GetVideoSize()));
}

void RecordingService::TerminateRecording(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  current_video_capture_params_.reset();
  video_capturer_remote_.reset();
  consumer_receiver_.reset();

  encoder_muxer_.AsyncCall(&RecordingEncoderMuxer::FlushAndFinalize)
      .WithArgs(BindOnceToMainThread(&RecordingService::OnEncoderMuxerFlushed,
                                     success));
}

void RecordingService::ConnectAndStartVideoCapturer(
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(current_video_capture_params_);

  video_capturer_remote_.reset();
  video_capturer_remote_.Bind(std::move(video_capturer));
  // The GPU process could crash while recording is in progress, and the video
  // capturer will be disconnected. We need to handle this event gracefully.
  video_capturer_remote_.set_disconnect_handler(base::BindOnce(
      &RecordingService::OnVideoCapturerDisconnected, base::Unretained(this)));
  current_video_capture_params_->InitializeVideoCapturer(
      video_capturer_remote_);
  video_capturer_remote_->Start(consumer_receiver_.BindNewPipeAndPassRemote());
}

void RecordingService::OnVideoCapturerDisconnected() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // On a crash in the GPU, the video capturer gets disconnected, so we can't
  // communicate with it any longer, but we can still communicate with the audio
  // capturer. We will stop the recording and flush whatever video chunks we
  // currently have.
  did_failure_occur_ = true;
  if (audio_capturer_)
    audio_capturer_->Stop();
  audio_capturer_.reset();
  TerminateRecording(/*success=*/false);
}

void RecordingService::OnAudioCaptured(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks audio_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  // We ignore any subsequent frames after a failure.
  if (did_failure_occur_)
    return;

  encoder_muxer_.AsyncCall(&RecordingEncoderMuxer::EncodeAudio)
      .WithArgs(std::move(audio_bus), audio_capture_time);
}

void RecordingService::OnEncodingFailure(FailureType type, bool for_video) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  OnRecordingFailure();
}

void RecordingService::OnRecordingFailure() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  did_failure_occur_ = true;
  StopRecording();
  // We don't wait for the video capturer to send us the OnStopped() signal, we
  // terminate recording immediately. We still need to flush the encoders, and
  // muxer since they may contain valid frames from before the failure occurred,
  // that we can propagate to the client.
  TerminateRecording(/*success=*/false);
}

void RecordingService::OnEncoderMuxerFlushed(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // If flushing the encoders and muxers resulted in some chunks being cached
  // here, we flush them to the client now.
  if (number_of_buffered_chunks_)
    FlushBufferedChunks();

  SignalRecordingEndedToClient(success);
}

void RecordingService::SignalMuxerOutputToClient(std::string muxer_output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  client_remote_->OnMuxerOutput(std::move(muxer_output));
}

void RecordingService::SignalRecordingEndedToClient(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  encoder_muxer_.Reset();
  client_remote_->OnRecordingEnded(success, video_thumbnail_);
}

void RecordingService::OnMuxerOutput(std::string data) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  ++number_of_buffered_chunks_;
  muxed_chunks_buffer_.append(data);

  if (number_of_buffered_chunks_ >= kMaxBufferedChunks)
    FlushBufferedChunks();
}

void RecordingService::FlushBufferedChunks() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(number_of_buffered_chunks_);

  SignalMuxerOutputToClient(std::move(muxed_chunks_buffer_));
  number_of_buffered_chunks_ = 0;
}

}  // namespace recording
