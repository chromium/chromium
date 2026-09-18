// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
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
#include "base/time/time.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace audio {
class OutputDevice;
}

namespace media {
class AudioBus;
class AudioSystem;
}  // namespace media

namespace ttc {

// Implements the AudioIO subsystem for TTC. Manages microphone
// capture, speaker playback, and audio hardware routing via the
// Chrome Audio Service.
class AudioController : public media::AudioCapturerSource::CaptureCallback,
                        public media::AudioRendererSink::RenderCallback {
 public:
  // `pcm_data` holds signed 16-bit interleaved PCM samples.
  using AudioCaptureCallback =
      base::RepeatingCallback<void(base::span<const int16_t> pcm_data,
                                   const media::AudioParameters& params)>;
  using AudioEnergyCallback = base::RepeatingCallback<void(float energy)>;
  using PlaybackCompletionCallback =
      base::RepeatingCallback<void(int64_t sequence_number)>;
  using AudioStreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;
  using AudioSystemFactory =
      base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>;

  explicit AudioController(
      AudioStreamFactoryBinder factory_binder = AudioStreamFactoryBinder(),
      AudioSystemFactory audio_system_factory = AudioSystemFactory());
  ~AudioController() override;

  AudioController(const AudioController&) = delete;
  AudioController& operator=(const AudioController&) = delete;

  // --- Audio Capture (Microphone Input) ---
  // Starts capturing microphone audio from `device_id` (or default if empty).
  // The device's parameters are queried asynchronously, so the capture stream
  // is only opened once they have been received.
  void StartCapture(std::string_view device_id = "");
  void StopCapture();
  bool is_capturing() const { return capture_requested_; }

  // Subscribes to incoming captured audio frames. Audio is always delivered in
  // GetBackendInputAudioParameters() format, regardless of the format the
  // capture device runs at.
  base::CallbackListSubscription AddAudioCaptureListener(
      AudioCaptureCallback callback);

  // Subscribes to calculated RMS audio energy (0.0 to 1.0) of captured
  // microphone audio for visualizer UI.
  base::CallbackListSubscription AddAudioEnergyListener(
      AudioEnergyCallback callback);

  // Subscribes to notifications when an audio chunk sequence finishes playback.
  base::CallbackListSubscription AddPlaybackCompletionListener(
      PlaybackCompletionCallback callback);

  // --- Audio Playback (Speaker Output) ---
  // Enqueues signed PCM16 audio data to be rendered natively with specific
  // parameters.
  void PlayAudio(base::span<const int16_t> pcm_data,
                 const media::AudioParameters& params,
                 int64_t sequence_number = 0);

  // Enqueues audio data using default 24kHz mono PCM16 parameters.
  void PlayAudio(base::span<const int16_t> pcm_data,
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

  // Format captured audio is converted to before being delivered to capture
  // listeners: 16kHz mono PCM16, in chunks of kCaptureChunkDuration.
  static media::AudioParameters GetBackendInputAudioParameters();

  static AudioStreamFactoryBinder GetDefaultAudioStreamFactoryBinder();
  [[nodiscard]] static base::AutoReset<AudioStreamFactoryBinder>
  SetDefaultAudioStreamFactoryBinderForTesting(AudioStreamFactoryBinder binder);

 private:
  // Converts device-native capture audio into
  // GetBackendInputAudioParameters() format.
  class CaptureConverter;

  struct QueuedAudioChunk {
    std::vector<int16_t> pcm_data;
    size_t read_offset = 0;
    int64_t sequence_number = 0;
  };

  void CreateAudioOutputDevice(const media::AudioParameters& params);
  media::AudioSystem* GetAudioSystem();
  void OnInputDeviceParametersReceived(
      const std::string& device_id,
      const std::optional<media::AudioParameters>& device_params);
  // Called on the realtime capture thread.
  void DeliverCapturedAudio(const media::AudioBus& audio_bus);
  void OnCapturedAudioOnMainThread(std::vector<int16_t> pcm_data,
                                   media::AudioParameters params,
                                   float energy);
  void OnAudioRenderedOnMainThread(int64_t completed_sequence);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  AudioStreamFactoryBinder factory_binder_;
  AudioSystemFactory audio_system_factory_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  base::RepeatingCallbackList<void(base::span<const int16_t>,
                                   const media::AudioParameters&)>
      capture_callbacks_;
  base::RepeatingCallbackList<void(float)> energy_callbacks_;
  base::RepeatingCallbackList<void(int64_t)> completion_callbacks_;

  base::RepeatingCallback<
      void(std::vector<int16_t>, media::AudioParameters, float)>
      capture_callback_runner_;
  base::RepeatingCallback<void(int64_t)> render_callback_runner_;

  // True from StartCapture() until StopCapture(), including while the device
  // parameters query is in flight.
  bool capture_requested_ = false;
  std::string capture_device_id_;
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  // Created before the capture stream is started and destroyed after it is
  // stopped; only used on the realtime capture thread in between.
  std::unique_ptr<CaptureConverter> capture_converter_;

  std::unique_ptr<audio::OutputDevice> output_device_;

  mutable base::Lock playback_lock_;
  base::circular_deque<QueuedAudioChunk> playback_queue_
      GUARDED_BY(playback_lock_);

  base::WeakPtrFactory<AudioController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_H_
