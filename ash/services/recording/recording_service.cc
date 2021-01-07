// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/recording/recording_service.h"

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
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "services/audio/public/cpp/device_factory.h"

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

// Calculates the bitrate used to initialize the video encoder based on the
// given |capture_size|.
uint64_t CalculateVpxEncoderBitrate(const gfx::Size& capture_size) {
  return std::max(kMinBitrateInBitsPerSecond,
                  static_cast<uint64_t>(capture_size.GetArea() *
                                        kBitsPerSecondPerSquarePixel));
}

media::AudioParameters GetAudioParameters() {
  static_assert(kAudioSampleRate % 100 == 0,
                "Audio sample rate is not divisible by 100");
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, kAudioSampleRate,
                                kAudioSampleRate / 100);
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
  DETACH_FROM_SEQUENCE(encoding_sequence_checker_);
}

RecordingService::~RecordingService() = default;

void RecordingService::RecordFullscreen(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& video_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(audio_stream_factory),
                    VideoCaptureParams::CreateForFullscreenCapture(
                        frame_sink_id, video_size));
}

void RecordingService::RecordWindow(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const viz::SubtreeCaptureId& subtree_capture_id,
    const gfx::Size& initial_video_size,
    const gfx::Size& max_video_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // TODO(crbug.com/1143930): Window recording doesn't produce any frames at the
  // moment.
  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(audio_stream_factory),
                    VideoCaptureParams::CreateForWindowCapture(
                        frame_sink_id, subtree_capture_id, initial_video_size,
                        max_video_size));
}

void RecordingService::RecordRegion(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& full_capture_size,
    const gfx::Rect& crop_region) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(audio_stream_factory),
                    VideoCaptureParams::CreateForRegionCapture(
                        frame_sink_id, full_capture_size, crop_region));
}

void RecordingService::StopRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  video_capturer_remote_->Stop();
  if (audio_capturer_)
    audio_capturer_->Stop();
  audio_capturer_.reset();
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
  // safer that we own our AudioBuses that are keep alive until encoded and
  // flushed.
  auto audio_data =
      media::AudioBus::Create(audio_source->channels(), audio_source->frames());
  audio_source->CopyTo(audio_data.get());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordingService::OnAudioCaptured, base::Unretained(this),
                     std::move(audio_data), audio_capture_time));
}

void RecordingService::OnCaptureError(const std::string& message) {
  LOG(ERROR) << message;
}

void RecordingService::OnCaptureMuted(bool is_muted) {}

void RecordingService::StartNewRecording(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
    std::unique_ptr<VideoCaptureParams> capture_params) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (current_video_capture_params_) {
    LOG(ERROR) << "Cannot start a new recording while another is in progress.";
    return;
  }

  client_remote_.reset();
  client_remote_.Bind(std::move(client));

  current_video_capture_params_ = std::move(capture_params);
  const auto capture_size = current_video_capture_params_->GetCaptureSize();
  media::VideoEncoder::Options video_encoder_options;
  video_encoder_options.bitrate = CalculateVpxEncoderBitrate(capture_size);
  video_encoder_options.framerate = kMaxFrameRate;
  video_encoder_options.frame_size = capture_size;
  // This value, expressed as a number of frames, forces the encoder to code
  // a keyframe if one has not been coded in the last keyframe_interval frames.
  video_encoder_options.keyframe_interval = 100;

  const bool should_record_audio = audio_stream_factory.is_valid();

  encoder_muxer_ = RecordingEncoderMuxer::Create(
      encoding_task_runner_, video_encoder_options,
      should_record_audio ? &audio_parameters_ : nullptr,
      base::BindRepeating(&RecordingService::OnMuxerWrite,
                          base::Unretained(this)),
      base::BindOnce(&RecordingService::OnEncodingFailure,
                     base::Unretained(this)));

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

void RecordingService::TerminateRecording(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  current_video_capture_params_.reset();
  video_capturer_remote_.reset();
  consumer_receiver_.reset();

  encoder_muxer_.AsyncCall(&RecordingEncoderMuxer::FlushAndFinalize)
      .WithArgs(base::BindOnce(&RecordingService::OnEncoderMuxerFlushed,
                               weak_ptr_factory_.GetWeakPtr(), success));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RecordingService::OnRecordingFailure,
                                base::Unretained(this)));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);
  DCHECK(encoder_muxer_);

  // If flushing the encoders and muxers resulted in some chunks being cached
  // here, we flush them to the client now.
  if (number_of_buffered_chunks_)
    FlushBufferedChunks();

  encoder_muxer_.Reset();
  main_task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&RecordingService::SignalRecordingEndedToClient,
                                base::Unretained(this), success));
}

void RecordingService::SignalMuxerOutputToClient(std::string muxer_output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  client_remote_->OnMuxerOutput(std::move(muxer_output));
}

void RecordingService::SignalRecordingEndedToClient(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  client_remote_->OnRecordingEnded(success);
}

void RecordingService::OnMuxerWrite(base::StringPiece data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);

  ++number_of_buffered_chunks_;
  muxed_chunks_buffer_.append(data.begin(), data.end());

  if (number_of_buffered_chunks_ >= kMaxBufferedChunks)
    FlushBufferedChunks();
}

void RecordingService::FlushBufferedChunks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);
  DCHECK(number_of_buffered_chunks_);

  main_task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&RecordingService::SignalMuxerOutputToClient,
                     base::Unretained(this), std::move(muxed_chunks_buffer_)));
  number_of_buffered_chunks_ = 0;
}

}  // namespace recording
