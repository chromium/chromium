// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/recording/recording_encoder_muxer.h"

#include "ash/services/recording/recording_service_constants.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"

namespace recording {

namespace {

// The audio and video encoders are initialized asynchronously, and until that
// happens, all received audio and video frames are added to
// |pending_video_frames_| and |pending_audio_frames_|. However, in order
// to avoid an OOM situation if the encoder takes too long to initialize or it
// never does, we impose an upper-bound to the number of pending frames. The
// below value is equal to the maximum number of in-flight frames that the
// capturer uses (See |viz::FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames|)
// before it stops sending frames. Once we hit that limit in
// |pending_video_frames_|, we will start dropping frames to let the capturer
// proceed, with an upper limit of how many frames we can drop that is
// equivalent to 4 seconds, after which we'll declare an encoder initialization
// failure. For convenience the same limit is used for as a cap on number of
// audio frames stored in |pending_audio_frames_|.
constexpr size_t kMaxPendingFrames = 10;
constexpr size_t kMaxDroppedFrames = 4 * kMaxFrameRate;

}  // namespace

RecordingEncoderMuxer::AudioFrame::AudioFrame(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks time)
    : bus(std::move(audio_bus)), capture_time(time) {}
RecordingEncoderMuxer::AudioFrame::AudioFrame(AudioFrame&&) = default;
RecordingEncoderMuxer::AudioFrame::~AudioFrame() = default;

// static
base::SequenceBound<RecordingEncoderMuxer> RecordingEncoderMuxer::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    media::WebmMuxer::WriteDataCB muxer_output_callback,
    FailureCallback on_failure_callback) {
  return base::SequenceBound<RecordingEncoderMuxer>(
      std::move(blocking_task_runner), video_encoder_options,
      audio_input_params, std::move(muxer_output_callback),
      std::move(on_failure_callback));
}

void RecordingEncoderMuxer::InitializeVideoEncoder(
    const media::VideoEncoder::Options& video_encoder_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: The VpxVideoEncoder supports changing the encoding options
  // dynamically, but it won't work for all frame size changes and may cause
  // encoding failures. Therefore, it's better to recreate and reinitialize a
  // new encoder. See media::VpxVideoEncoder::ChangeOptions() for more details.

  if (video_encoder_ && is_video_encoder_initialized_) {
    auto* encoder_ptr = video_encoder_.get();
    encoder_ptr->Flush(base::BindOnce(
        // Holds on to the old encoder until it flushes its buffers, then
        // destroys it.
        [](std::unique_ptr<media::VpxVideoEncoder> old_encoder,
           media::Status status) {},
        std::move(video_encoder_)));
  }

  is_video_encoder_initialized_ = false;
  video_encoder_ = std::make_unique<media::VpxVideoEncoder>();
  video_encoder_->Initialize(
      media::VP8PROFILE_ANY, video_encoder_options,
      base::BindRepeating(&RecordingEncoderMuxer::OnVideoEncoderOutput,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr(), video_encoder_.get()));
}

void RecordingEncoderMuxer::EncodeVideo(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_video_encoder_initialized_) {
    EncodeVideoImpl(std::move(frame));
    return;
  }

  pending_video_frames_.push_back(std::move(frame));
  if (pending_video_frames_.size() == kMaxPendingFrames) {
    pending_video_frames_.pop_front();
    DCHECK_LT(pending_video_frames_.size(), kMaxPendingFrames);

    if (++num_dropped_frames_ >= kMaxDroppedFrames) {
      LOG(ERROR) << "Video encoder took too long to initialize.";
      NotifyFailure(FailureType::kEncoderInitialization, /*for_video=*/true);
    }
  }
}

void RecordingEncoderMuxer::EncodeAudio(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  AudioFrame frame(std::move(audio_bus), capture_time);
  if (is_audio_encoder_initialized_) {
    EncodeAudioImpl(std::move(frame));
    return;
  }

  pending_audio_frames_.push_back(std::move(frame));
  if (pending_audio_frames_.size() == kMaxPendingFrames) {
    pending_audio_frames_.pop_front();
    DCHECK_LT(pending_audio_frames_.size(), kMaxPendingFrames);
  }
}

void RecordingEncoderMuxer::FlushAndFinalize(base::OnceClosure on_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_encoder_) {
    audio_encoder_->Flush(
        base::BindOnce(&RecordingEncoderMuxer::OnAudioEncoderFlushed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_done)));
  } else {
    OnAudioEncoderFlushed(std::move(on_done), media::OkStatus());
  }
}

RecordingEncoderMuxer::RecordingEncoderMuxer(
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    media::WebmMuxer::WriteDataCB muxer_output_callback,
    FailureCallback on_failure_callback)
    : webm_muxer_(media::kCodecOpus,
                  /*has_video_=*/true,
                  /*has_audio_=*/!!audio_input_params,
                  muxer_output_callback),
      on_failure_callback_(std::move(on_failure_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_input_params) {
    media::AudioEncoder::Options audio_encoder_options;
    audio_encoder_options.channels = audio_input_params->channels();
    audio_encoder_options.sample_rate = audio_input_params->sample_rate();
    InitializeAudioEncoder(audio_encoder_options);
  }

  InitializeVideoEncoder(video_encoder_options);
}

RecordingEncoderMuxer::~RecordingEncoderMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RecordingEncoderMuxer::InitializeAudioEncoder(
    const media::AudioEncoder::Options& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_audio_encoder_initialized_ = false;
  audio_encoder_ = std::make_unique<media::AudioOpusEncoder>();
  audio_encoder_->Initialize(
      options,
      base::BindRepeating(&RecordingEncoderMuxer::OnAudioEncoded,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RecordingEncoderMuxer::OnAudioEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecordingEncoderMuxer::OnAudioEncoderInitialized(media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the audio encoder: "
               << status.message();
    NotifyFailure(FailureType::kEncoderInitialization,
                  /*for_video=*/false);
    return;
  }

  is_audio_encoder_initialized_ = true;
  for (auto& frame : pending_audio_frames_)
    EncodeAudioImpl(std::move(frame));
  pending_audio_frames_.clear();
}

void RecordingEncoderMuxer::OnVideoEncoderInitialized(
    media::VpxVideoEncoder* encoder,
    media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore initialization of encoders that were removed as part of
  // reinitialization.
  if (video_encoder_.get() != encoder)
    return;

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the video encoder: "
               << status.message();
    NotifyFailure(FailureType::kEncoderInitialization,
                  /*for_video=*/true);
    return;
  }

  is_video_encoder_initialized_ = true;
  for (auto& frame : pending_video_frames_)
    EncodeVideoImpl(frame);
  pending_video_frames_.clear();
}

void RecordingEncoderMuxer::EncodeAudioImpl(AudioFrame frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_audio_encoder_initialized_);

  if (did_failure_occur())
    return;

  audio_encoder_->Encode(
      std::move(frame.bus), frame.capture_time,
      base::BindOnce(&RecordingEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/false));
}

void RecordingEncoderMuxer::EncodeVideoImpl(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_video_encoder_initialized_);

  if (did_failure_occur())
    return;

  video_visible_rect_sizes_.push(frame->visible_rect().size());
  video_encoder_->Encode(
      frame, /*key_frame=*/false,
      base::BindOnce(&RecordingEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/true));
}

void RecordingEncoderMuxer::OnVideoEncoderOutput(
    media::VideoEncoderOutput output,
    base::Optional<media::VideoEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media::WebmMuxer::VideoParameters params(video_visible_rect_sizes_.front(),
                                           kMaxFrameRate, media::kCodecVP8,
                                           kColorSpace);
  video_visible_rect_sizes_.pop();

  // TODO(crbug.com/1143798): Explore changing the WebmMuxer so it doesn't work
  // with strings, to avoid copying the encoded data.
  std::string data{reinterpret_cast<const char*>(output.data.get()),
                   output.size};
  webm_muxer_.OnEncodedVideo(params, std::move(data), std::string(),
                             base::TimeTicks() + output.timestamp,
                             output.key_frame);
}

void RecordingEncoderMuxer::OnAudioEncoded(
    media::EncodedAudioBuffer encoded_audio,
    base::Optional<media::AudioEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  // TODO(crbug.com/1143798): Explore changing the WebmMuxer so it doesn't work
  // with strings, to avoid copying the encoded data.
  std::string encoded_data{
      reinterpret_cast<const char*>(encoded_audio.encoded_data.get()),
      encoded_audio.encoded_data_size};
  webm_muxer_.OnEncodedAudio(encoded_audio.params, std::move(encoded_data),
                             encoded_audio.timestamp);
}

void RecordingEncoderMuxer::OnAudioEncoderFlushed(base::OnceClosure on_done,
                                                  media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok())
    LOG(ERROR) << "Could not flush audio encoder: " << status.message();

  DCHECK(video_encoder_);
  video_encoder_->Flush(
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderFlushed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_done)));
}

void RecordingEncoderMuxer::OnVideoEncoderFlushed(base::OnceClosure on_done,
                                                  media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not flush remaining video frames: "
               << status.message();
  }

  webm_muxer_.Flush();
  std::move(on_done).Run();
}

void RecordingEncoderMuxer::OnEncoderStatus(bool for_video,
                                            media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.is_ok())
    return;

  LOG(ERROR) << "Failed to encode " << (for_video ? "video" : "audio")
             << " frame: " << status.message();
  NotifyFailure(FailureType::kEncoding, for_video);
}

void RecordingEncoderMuxer::NotifyFailure(FailureType type, bool for_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_failure_callback_)
    std::move(on_failure_callback_).Run(type, for_video);
}

}  // namespace recording
