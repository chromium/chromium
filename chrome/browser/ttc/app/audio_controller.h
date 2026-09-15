// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace audio {
class OutputDevice;
}

namespace ttc {

// Implements the AudioIO subsystem for TTC. Manages microphone
// capture, speaker playback, and audio hardware routing via the
// Chrome Audio Service.
class AudioController : public media::AudioCapturerSource::CaptureCallback,
                        public media::AudioRendererSink::RenderCallback {
 public:
  using AudioCaptureCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& pcm_data,
                                   const media::AudioParameters& params)>;
  using AudioEnergyCallback = base::RepeatingCallback<void(float energy)>;
  using PlaybackCompletionCallback =
      base::RepeatingCallback<void(int64_t sequence_number)>;
  using AudioStreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;

  explicit AudioController(
      AudioStreamFactoryBinder factory_binder = AudioStreamFactoryBinder());
  ~AudioController() override;

  AudioController(const AudioController&) = delete;
  AudioController& operator=(const AudioController&) = delete;

  // --- Audio Capture (Microphone Input) ---
  // Starts capturing microphone audio from `device_id` (or default if empty).
  void StartCapture(std::string_view device_id = "");
  void StopCapture();
  bool is_capturing() const { return audio_capturer_source_ != nullptr; }

  // Subscribes to incoming captured audio frames (16kHz mono PCM16 by default).
  base::CallbackListSubscription AddAudioCaptureListener(
      AudioCaptureCallback callback);

  // Subscribes to calculated RMS audio energy (0.0 to 1.0) of captured or
  // rendered audio for visualizer UI.
  base::CallbackListSubscription AddAudioEnergyListener(
      AudioEnergyCallback callback);

  // Subscribes to notifications when an audio chunk sequence finishes playback.
  base::CallbackListSubscription AddPlaybackCompletionListener(
      PlaybackCompletionCallback callback);

  // --- Audio Playback (Speaker Output) ---
  // Enqueues audio data to be rendered natively with specific parameters.
  void PlayAudio(base::span<const uint8_t> pcm_data,
                 const media::AudioParameters& params,
                 int64_t sequence_number = 0);

  // Enqueues audio data using default 24kHz mono PCM16 parameters.
  void PlayAudio(base::span<const uint8_t> pcm_data,
                 int64_t sequence_number = 0);

  // Instantly flushes all queued playback audio (used on barge-in /
  // interruption).
  void ClearPlaybackQueue();
  void StopPlayback();
  bool is_playing() const;

  // --- media::AudioCapturerSource::CaptureCallback ---
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

  // --- media::AudioRendererSink::RenderCallback ---
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;
  void OnRenderError() override;

  static media::AudioParameters GetDefaultPlaybackAudioParameters();
  static media::AudioParameters GetDefaultCaptureAudioParameters();
  static AudioStreamFactoryBinder GetDefaultAudioStreamFactoryBinder();
  [[nodiscard]] static base::AutoReset<AudioStreamFactoryBinder>
  SetDefaultAudioStreamFactoryBinderForTesting(AudioStreamFactoryBinder binder);

 private:
  struct QueuedAudioChunk {
    std::vector<uint8_t> pcm_data;
    size_t read_offset = 0;
    int64_t sequence_number = 0;
  };

  void CreateAudioOutputDevice(const media::AudioParameters& params);
  void OnCapturedAudioOnMainThread(std::vector<uint8_t> pcm_data,
                                   media::AudioParameters params,
                                   float energy);
  void OnAudioRenderedOnMainThread(int64_t completed_sequence, float energy);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  AudioStreamFactoryBinder factory_binder_;

  base::RepeatingCallbackList<void(const std::vector<uint8_t>&,
                                   const media::AudioParameters&)>
      capture_callbacks_;
  base::RepeatingCallbackList<void(float)> energy_callbacks_;
  base::RepeatingCallbackList<void(int64_t)> completion_callbacks_;

  base::RepeatingCallback<
      void(std::vector<uint8_t>, media::AudioParameters, float)>
      capture_callback_runner_;
  base::RepeatingCallback<void(int64_t, float)> render_callback_runner_;

  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  std::unique_ptr<audio::OutputDevice> output_device_;

  mutable base::Lock playback_lock_;
  base::circular_deque<QueuedAudioChunk> playback_queue_
      GUARDED_BY(playback_lock_);

  base::WeakPtrFactory<AudioController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_
