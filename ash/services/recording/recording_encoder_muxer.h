// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_RECORDING_RECORDING_ENCODER_MUXER_H_
#define ASH_SERVICES_RECORDING_RECORDING_ENCODER_MUXER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/muxers/webm_muxer.h"
#include "media/video/vpx_video_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace recording {

// The type of the failures that can happen while encoding.
enum class FailureType {
  kEncoderInitialization,
  kEncoding,
};

// Defines a callback type to notify the user of RecordingEncoderMuxer of a
// failure while encoding audio or video frames.
// TODO(afakhry): It's possible we don't need |type| or |for_video|.
using OnFailureCallback =
    base::OnceCallback<void(FailureType type, bool for_video)>;

// Defines a callback type to push the output of the muxer to the user of
// RecordingEncoderMuxer. Note that this differs from the type of the
// |WebmMuxer|'s write callback (|media::WebmMuxer::WriteDataCB|) in that the
// below callback uses |std::string| to provide the output rather than
// |base::StringPiece|. This enables the user to bind the callback to a
// different sequence than the one used by the muxer, since a |StringPiece| does
// not own its buffer and by the time the callback is invoked on another
// sequence, the muxer might have already clobbered the underlying buffer.
using OnMuxerOutputCallback = base::RepeatingCallback<void(std::string)>;

// Encapsulates encoding and muxing audio and video frame. An instance of this
// object can only be interacted with via a |base::SequenceBound| wrapper, which
// guarantees all encoding and muxing operations as well as destruction of the
// instance are done on the sequenced |blocking_task_runner| given to Create().
// This prevents expensive encoding and muxing operations from blocking the main
// thread of the recording service, on which the video frames are delivered.
//
// This object performs VP8 video encoding and Opus audio encoding, and mux the
// audio and video encoded frames into a Webm container.
class RecordingEncoderMuxer {
 public:
  RecordingEncoderMuxer(const RecordingEncoderMuxer&) = delete;
  RecordingEncoderMuxer& operator=(const RecordingEncoderMuxer&) = delete;

  // Creates an instance of this class that is bound to the given sequenced
  // |blocking_task_runner| on which all operations as well as destruction will
  // happen. |video_encoder_options| and |audio_input_params| will be used to
  // initialize the video and audio encoders respectively.
  // If |audio_input_params| is nullptr, then the service is not recording
  // audio, and the muxer will be initialized accordingly.
  // |muxer_output_callback| will be called to provide the muxer output chunks
  // ready to be sent to the recording service client.
  // |on_failure_callback| will be called to inform the owner of this object of
  // a failure, after which all subsequent calls to EncodeVideo() and
  // EncodeAudio() will be ignored.
  //
  // By default, both |muxer_output_callback| and |on_failure_callback| will be
  // called on the same sequence of |blocking_task_runner| (unless the caller
  // binds the given callbacks to a different sequence by means of
  // base::BindPostTask()).
  static base::SequenceBound<RecordingEncoderMuxer> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const media::VideoEncoder::Options& video_encoder_options,
      const media::AudioParameters* audio_input_params,
      OnMuxerOutputCallback muxer_output_callback,
      OnFailureCallback on_failure_callback);

  bool did_failure_occur() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !on_failure_callback_;
  }

  // Creates and initializes the video encoder if none exists, or recreates and
  // reinitializes it otherwise. This is useful when the video frame dimensions
  // may need to change dynamically (such as when a recorded window gets moved
  // to a display with different bounds).
  void InitializeVideoEncoder(
      const media::VideoEncoder::Options& video_encoder_options);

  // Encodes and muxes the given video |frame|.
  void EncodeVideo(scoped_refptr<media::VideoFrame> frame);

  // Encodes and muxes the given audio frame in |audio_bus| captured at
  // |capture_time|.
  void EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                   base::TimeTicks capture_time);

  // Audio and video encoders as well as the WebmMuxer may buffer several frames
  // before they're processed. It is important to flush all those buffers before
  // releasing this object so as not to drop the final portion of the recording.
  // |on_done| will be called when all remaining buffered frames have been
  // processed and sent to |muxer_output_callback|.
  // By default, |on_done| will be called on the same sequence of
  // |blocking_task_runner| unless the caller binds it to another sequence by
  // means of base::BindPostTask().
  void FlushAndFinalize(base::OnceClosure on_done);

 private:
  struct AudioFrame {
    AudioFrame(std::unique_ptr<media::AudioBus>, base::TimeTicks);
    AudioFrame(AudioFrame&&);
    ~AudioFrame();

    std::unique_ptr<media::AudioBus> bus;
    base::TimeTicks capture_time;
  };

  friend class base::SequenceBound<RecordingEncoderMuxer>;

  RecordingEncoderMuxer(
      const media::VideoEncoder::Options& video_encoder_options,
      const media::AudioParameters* audio_input_params,
      OnMuxerOutputCallback muxer_output_callback,
      OnFailureCallback on_failure_callback);
  ~RecordingEncoderMuxer();

  // Creates and initializes the audio encoder.
  void InitializeAudioEncoder(const media::AudioEncoder::Options& options);

  // Called when the audio encoder is initialized to provide the |status| of
  // the initialization.
  void OnAudioEncoderInitialized(media::Status status);

  // Called when the video |encoder| is initialized to provide the |status| of
  // the initialization. If initialization failed, |on_failure_callback_| will
  // be triggered.
  void OnVideoEncoderInitialized(media::VpxVideoEncoder* encoder,
                                 media::Status status);

  // Performs the actual encoding of the given audio |frame|. It should never be
  // called before the audio encoder is initialized. Audio frames received
  // before initialization should be added to |pending_audio_frames_| and
  // handled once initialization is complete.
  void EncodeAudioImpl(AudioFrame frame);

  // Performs the actual encoding of the given video |frame|. It should never be
  // called before the video encoder is initialized. Video frames received
  // before initialization should be added to |pending_video_frames_| and
  // handled once initialization is complete.
  void EncodeVideoImpl(scoped_refptr<media::VideoFrame> frame);

  // Called by the video encoder to provided the encoded video frame |output|,
  // which will then by sent to muxer.
  void OnVideoEncoderOutput(
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription> codec_description);

  // Called by the audio encoder to provide the |encoded_audio|.
  void OnAudioEncoded(
      media::EncodedAudioBuffer encoded_audio,
      absl::optional<media::AudioEncoder::CodecDescription> codec_description);

  // Called by the |webm_muxer_| to deliver a muxer output chunk |data|.
  void OnMuxerWrite(base::StringPiece data);

  // Called when the audio encoder flushes all its buffered frames, at which
  // point we can flush the video encoder. |on_done| will be passed to
  // OnVideoEncoderFlushed()
  void OnAudioEncoderFlushed(base::OnceClosure on_done, media::Status status);

  // Called when the video encoder flushes all its buffered frames, at which
  // point we can flush the muxer. |on_done| will be called to signal that
  // flushing is complete.
  void OnVideoEncoderFlushed(base::OnceClosure on_done, media::Status status);

  // Called by both the audio and video encoders to provide the |status| of
  // encoding tasks.
  void OnEncoderStatus(bool for_video, media::Status status);

  // Notifies the owner of this object (via |on_failure_callback_|) that a
  // failure of |type| has occurred during audio or video encoding depending on
  // the value of |for_video|.
  void NotifyFailure(FailureType type, bool for_video);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<media::VpxVideoEncoder> video_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<media::AudioOpusEncoder> audio_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A callback to deliver the muxer output to the client of this class. By
  // default, it is invoked on the same sequence of this class, unless the
  // client binds it to a different sequence (by means of |base::BindPostTask|)
  // before giving it to us.
  // Note that it has to be declared before |webm_muxer_| because the latter
  // depends on it, so |webm_muxer_| should be destroyed first.
  OnMuxerOutputCallback on_muxer_output_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  media::WebmMuxer webm_muxer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds video frames that were received before the video encoder is
  // initialized, so that they can be processed once initialization is complete.
  base::circular_deque<scoped_refptr<media::VideoFrame>> pending_video_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds audio frames that were received before the audio encoder is
  // initialized, so that they can be processed once initialization is complete.
  base::circular_deque<AudioFrame> pending_audio_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The total number of frames that we dropped to keep the size of
  // |pending_video_frames_| limited to |kMaxPendingFrames| to avoid consuming
  // too much memory, or stalling the capturer since it has a maximum number of
  // in-flight frames at a time.
  size_t num_dropped_frames_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // A queue containing the sizes of the visible region of the received video
  // frame in the same order of their encoding. Note that the visible rect sizes
  // may change from frame to frame (e.g. when recording a window, and the
  // window gets resized).
  base::queue<gfx::Size> video_visible_rect_sizes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A callback triggered when a failure happens during encoding. Once
  // triggered, this callback is null, and therefore indicates that a failure
  // occurred (See did_failure_occur() above).
  OnFailureCallback on_failure_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // True once video encoder is initialized successfully.
  bool is_video_encoder_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // True once audio encoder is initialized successfully.
  bool is_audio_encoder_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  base::WeakPtrFactory<RecordingEncoderMuxer> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace recording

#endif  // ASH_SERVICES_RECORDING_RECORDING_ENCODER_MUXER_H_
