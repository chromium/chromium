// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "components/ttc/app/public/error_codes.h"
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
class AudioPushFifo;
class AudioSystem;
}  // namespace media

namespace ttc {

// Implements the AudioIO subsystem for TTC. Manages microphone
// capture, speaker playback, and audio hardware routing via the
// Chrome Audio Service.
class AudioControllerImpl : public AudioController,
                            public media::AudioCapturerSource::CaptureCallback,
                            public media::AudioRendererSink::RenderCallback {
 public:
  using AudioStreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;
  using AudioSystemFactory =
      base::RepeatingCallback<std::unique_ptr<media::AudioSystem>()>;

  explicit AudioControllerImpl(
      AudioStreamFactoryBinder factory_binder = AudioStreamFactoryBinder(),
      AudioSystemFactory audio_system_factory = AudioSystemFactory());
  ~AudioControllerImpl() override;

  AudioControllerImpl(const AudioControllerImpl&) = delete;
  AudioControllerImpl& operator=(const AudioControllerImpl&) = delete;

  // AudioController:
  void StartCapture(std::string_view device_id) override;
  void StopCapture() override;
  bool is_capturing() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void PlayAudio(base::span<const int16_t> pcm_data,
                 const media::AudioParameters& params,
                 int64_t sequence_number) override;
  void PlayAudio(base::span<const int16_t> pcm_data,
                 int64_t sequence_number) override;
  void ClearPlaybackQueue() override;
  void StopPlayback() override;
  bool is_playing() const override;

  // media::AudioCapturerSource::CaptureCallback:
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

  // media::AudioRendererSink::RenderCallback:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;
  void OnRenderError() override;

  static media::AudioParameters GetDefaultPlaybackAudioParameters();

  // Format captured audio is converted to before being delivered to observers:
  // 16kHz mono PCM16, in chunks of kCaptureChunkDuration.
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
  // Called on the realtime capture thread, from `capture_fifo_`.
  void DeliverCapturedAudio(const media::AudioBus& audio_bus, int frame_delay);
  // (Re)creates `capture_callback_runner_`, dropping the captured audio that
  // the previous one has posted but not yet delivered.
  void ResetCaptureCallbackRunner();
  void OnCapturedAudioOnMainThread(std::vector<int16_t> pcm_data,
                                   media::AudioParameters params,
                                   float energy);
  void OnAudioRenderedOnMainThread(int64_t completed_sequence);
  void OnAudioErrorOnMainThread(ErrorCode error_code);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  AudioStreamFactoryBinder factory_binder_;
  AudioSystemFactory audio_system_factory_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  base::ObserverList<Observer,
                     /*check_empty=*/false,
                     base::ObserverListReentrancyPolicy::kAllowReentrancy>
      observers_;

  base::RepeatingCallback<
      void(std::vector<int16_t>, media::AudioParameters, float)>
      capture_callback_runner_;
  base::RepeatingCallback<void(int64_t)> render_callback_runner_;
  base::RepeatingCallback<void(ErrorCode)> error_callback_runner_;

  // True from StartCapture() until StopCapture(), including while the device
  // parameters query is in flight.
  bool capture_requested_ = false;
  std::string capture_device_id_;
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  // Created before the capture stream is started and destroyed after it is
  // stopped; only used on the realtime capture thread in between.
  // `capture_converter_` is only needed when the audio service doesn't already
  // deliver audio in GetBackendInputAudioParameters() format.
  std::unique_ptr<CaptureConverter> capture_converter_;
  // Re-buffers converted audio into the chunk size delivered to observers.
  std::unique_ptr<media::AudioPushFifo> capture_fifo_;

  std::unique_ptr<audio::OutputDevice> output_device_;

  mutable base::Lock playback_lock_;
  base::circular_deque<QueuedAudioChunk> playback_queue_
      GUARDED_BY(playback_lock_);
  AudioEnvelopeExtractor audio_envelope_extractor_;

  // Only for `capture_callback_runner_`, so that StopCapture() can drop the
  // audio it has posted.
  base::WeakPtrFactory<AudioControllerImpl> capture_weak_factory_{this};
  base::WeakPtrFactory<AudioControllerImpl> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_AUDIO_CONTROLLER_IMPL_H_
