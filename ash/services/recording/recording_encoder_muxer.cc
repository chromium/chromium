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

// The video encoder is initialized asynchronously, and until that happens, all
// received video frames are added to |pending_video_frames_|. However, in order
// to avoid an OOM situation if the encoder takes too long to initialize or it
// never does, we impose an upper-bound to the number of pending frames. The
// below value is equal to the maximum number of in-flight frames that the
// capturer uses (See |viz::FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames|)
// before it stops sending frames. Once we hit that limit in
// |pending_video_frames_|, we will start dropping frames to let the capturer
// proceed, with an upper limit of how many frames we can drop that is
// equivalent to 4 seconds, after which we'll declare an encoder initialization
// failure.
constexpr size_t kMaxPendingFrames = 10;
constexpr size_t kMaxDroppedFrames = 4 * kMaxFrameRate;

}  // namespace

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

  if (!did_failure_occur())
    audio_encoder_->EncodeAudio(*audio_bus, capture_time);
}

void RecordingEncoderMuxer::FlushAndFinalize(base::OnceClosure on_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note that flushing the audio encoder is synchronous, so calling Flush() on
  // it will result in OnAudioEncoded() being called directly (if any audio
  // frames were still buffered and not processed). The video encoder responds
  // asynchronously.
  if (audio_encoder_)
    audio_encoder_->Flush();
  video_encoder_.Flush(
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderFlushed,
                     base::Unretained(this), std::move(on_done)));
}

RecordingEncoderMuxer::RecordingEncoderMuxer(
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    media::WebmMuxer::WriteDataCB muxer_output_callback,
    FailureCallback on_failure_callback)
    : audio_encoder_(
          !audio_input_params
              ? nullptr
              : std::make_unique<media::AudioOpusEncoder>(
                    *audio_input_params,
                    base::BindRepeating(&RecordingEncoderMuxer::OnAudioEncoded,
                                        base::Unretained(this)),
                    base::BindRepeating(&RecordingEncoderMuxer::OnEncoderStatus,
                                        base::Unretained(this),
                                        /*for_video=*/false),
                    // 0 means the encoder picks bitrate automatically.
                    /*bits_per_second=*/0)),
      webm_muxer_(media::kCodecOpus,
                  /*has_video_=*/true,
                  /*has_audio_=*/!!audio_input_params,
                  muxer_output_callback),
      on_failure_callback_(std::move(on_failure_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  video_encoder_.Initialize(
      media::VP8PROFILE_ANY, video_encoder_options,
      base::BindRepeating(&RecordingEncoderMuxer::OnVideoEncoderOutput,
                          base::Unretained(this)),
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderInitialized,
                     base::Unretained(this)));
}

RecordingEncoderMuxer::~RecordingEncoderMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RecordingEncoderMuxer::OnVideoEncoderInitialized(media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

void RecordingEncoderMuxer::EncodeVideoImpl(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_video_encoder_initialized_);

  if (did_failure_occur())
    return;

  video_visible_rect_sizes_.push(frame->visible_rect().size());
  video_encoder_.Encode(
      frame, /*key_frame=*/false,
      base::BindOnce(&RecordingEncoderMuxer::OnEncoderStatus,
                     base::Unretained(this), /*for_video=*/true));
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
    media::EncodedAudioBuffer encoded_audio) {
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
