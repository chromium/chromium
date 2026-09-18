// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_controller.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/cpp/output_device.h"

namespace ttc {

namespace {

// Captured audio is always delivered to the backend as 16kHz mono PCM16. The
// capture device itself is opened with its native parameters and converted to
// this format.
constexpr int kBackendInputSampleRate = 16000;
constexpr int kBackendInputChunkDurationMs = 100;

// Hardware microphone input sample rate is natively 48kHz on macOS CoreAudio
// and Android (AAudio/OpenSLES), while ConversationImpl downsamples 48kHz to
// 16kHz for model input. On other platforms, 16kHz capture is requested
// directly.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
constexpr int kDefaultPlaybackFramesPerBuffer = 480;  // 20ms at 24kHz
#else
constexpr int kDefaultPlaybackFramesPerBuffer = 2400;  // 100ms chunks
#endif

// Model execution service audio output is 24kHz mono PCM16 across all
// platforms. Resampling from 24kHz to hardware device sample rate (e.g. 48kHz)
// is handled automatically by the Chrome Audio Service.
constexpr int kDefaultPlaybackSampleRate = 24000;

AudioController::AudioStreamFactoryBinder& GetDefaultBinderForTestingStorage() {
  static base::NoDestructor<AudioController::AudioStreamFactoryBinder> binder;
  return *binder;
}

}  // namespace

// Resamples and channel-mixes capture audio from the device's native format to
// the format delivered to listeners. Constructed and destroyed on the main
// sequence, but Convert() runs on the realtime capture thread.
// TODO(bokan): This comes from SpeechRecognizerImpl - consider creating a
// shareable implementation.
class AudioController::CaptureConverter
    : public media::AudioConverter::InputCallback {
 public:
  CaptureConverter(const media::AudioParameters& input_params,
                   const media::AudioParameters& output_params)
      : converter_(input_params, output_params, /*disable_fifo=*/false),
        input_bus_(media::AudioBus::Create(input_params)),
        output_bus_(media::AudioBus::Create(output_params)),
        input_params_(input_params) {
    converter_.AddInput(this);
    converter_.PrimeWithSilence();
  }

  CaptureConverter(const CaptureConverter&) = delete;
  CaptureConverter& operator=(const CaptureConverter&) = delete;

  ~CaptureConverter() override { converter_.RemoveInput(this); }

  // Converts one buffer of `source`. The returned bus is owned by this object
  // and remains valid until the next call to Convert().
  const media::AudioBus& Convert(const media::AudioBus& source) {
    CHECK_EQ(source.frames(), input_params_.frames_per_buffer());
    CHECK_EQ(source.channels(), input_params_.channels());
    data_was_converted_ = false;
    source.CopyTo(input_bus_.get());
    converter_.Convert(output_bus_.get());
    return *output_bus_;
  }

  // False if the last Convert() call was satisfied entirely from buffered data
  // without consuming the input, in which case Convert() must be called again
  // with the same input. See https://crbug.com/506051.
  bool data_was_converted() const { return data_was_converted_; }

 private:
  // media::AudioConverter::InputCallback:
  double ProvideInput(media::AudioBus* dest,
                      uint32_t frames_delayed,
                      const media::AudioGlitchInfo& glitch_info) override {
    input_bus_->CopyTo(dest);
    data_was_converted_ = true;
    return 1.0;
  }

  media::AudioConverter converter_;
  std::unique_ptr<media::AudioBus> input_bus_;
  std::unique_ptr<media::AudioBus> output_bus_;
  const media::AudioParameters input_params_;
  bool data_was_converted_ = false;
};

// static
media::AudioParameters AudioController::GetBackendInputAudioParameters() {
  return media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), kBackendInputSampleRate,
      kBackendInputSampleRate * kBackendInputChunkDurationMs / 1000);
}

// static
media::AudioParameters AudioController::GetDefaultPlaybackAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                kDefaultPlaybackSampleRate,
                                kDefaultPlaybackFramesPerBuffer);
}

// static
base::AutoReset<AudioController::AudioStreamFactoryBinder>
AudioController::SetDefaultAudioStreamFactoryBinderForTesting(
    AudioStreamFactoryBinder binder) {
  return base::AutoReset<AudioStreamFactoryBinder>(
      &GetDefaultBinderForTestingStorage(), std::move(binder));
}

// static
AudioController::AudioStreamFactoryBinder
AudioController::GetDefaultAudioStreamFactoryBinder() {
  if (GetDefaultBinderForTestingStorage()) {
    return GetDefaultBinderForTestingStorage();
  }
  return content::GetAudioServiceStreamFactoryBinder();
}

AudioController::AudioController(AudioStreamFactoryBinder factory_binder,
                                 AudioSystemFactory audio_system_factory)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      factory_binder_(std::move(factory_binder)),
      audio_system_factory_(std::move(audio_system_factory)) {
  capture_callback_runner_ = base::BindPostTask(
      main_task_runner_,
      base::BindRepeating(&AudioController::OnCapturedAudioOnMainThread,
                          weak_factory_.GetWeakPtr()));
  render_callback_runner_ = base::BindPostTask(
      main_task_runner_,
      base::BindRepeating(&AudioController::OnAudioRenderedOnMainThread,
                          weak_factory_.GetWeakPtr()));
}

AudioController::~AudioController() {
  StopCapture();
  StopPlayback();
}

media::AudioSystem* AudioController::GetAudioSystem() {
  if (!audio_system_) {
    audio_system_ = audio_system_factory_
                        ? audio_system_factory_.Run()
                        : content::CreateAudioSystemForAudioService();
  }
  return audio_system_.get();
}

void AudioController::StartCapture(std::string_view device_id) {
  if (capture_requested_ || !factory_binder_) {
    return;
  }
  capture_requested_ = true;
  capture_device_id_ = device_id.empty()
                           ? media::AudioDeviceDescription::kDefaultDeviceId
                           : std::string(device_id);

  GetAudioSystem()->GetInputStreamParameters(
      capture_device_id_,
      base::BindOnce(&AudioController::OnInputDeviceParametersReceived,
                     weak_factory_.GetWeakPtr(), capture_device_id_));
}

void AudioController::OnInputDeviceParametersReceived(
    const std::string& device_id,
    const std::optional<media::AudioParameters>& device_params) {
  if (!capture_requested_ || audio_capturer_source_ ||
      device_id != capture_device_id_) {
    return;
  }

  if (!device_params.has_value()) {
    VLOG(1) << "AudioController: no parameters for capture device "
            << device_id;
    capture_requested_ = false;
    return;
  }

  // Open the device with its native format, but with the same chunk duration
  // used for delivery so that each captured buffer produces exactly one
  // converted buffer.
  media::AudioParameters input_params = device_params.value();
  input_params.set_frames_per_buffer(input_params.sample_rate() *
                                     kBackendInputChunkDurationMs / 1000);

  capture_converter_ = std::make_unique<CaptureConverter>(
      input_params, GetBackendInputAudioParameters());

  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  factory_binder_.Run(stream_factory.InitWithNewPipeAndPassReceiver());

  audio_capturer_source_ =
      audio::CreateInputDevice(std::move(stream_factory), device_id,
                               audio::DeadStreamDetection::kEnabled);
  audio_capturer_source_->Initialize(input_params, this);
  audio_capturer_source_->Start();
}

void AudioController::StopCapture() {
  capture_requested_ = false;
  capture_device_id_.clear();
  if (audio_capturer_source_) {
    // Stop() joins the realtime capture thread, so no Capture() call can be in
    // flight once it returns and the converter can be safely destroyed.
    audio_capturer_source_->Stop();
    audio_capturer_source_ = nullptr;
  }
  capture_converter_.reset();
}

base::CallbackListSubscription AudioController::AddAudioCaptureListener(
    AudioCaptureCallback callback) {
  return capture_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription AudioController::AddAudioEnergyListener(
    AudioEnergyCallback callback) {
  return energy_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription AudioController::AddPlaybackCompletionListener(
    PlaybackCompletionCallback callback) {
  return completion_callbacks_.Add(std::move(callback));
}

void AudioController::PlayAudio(base::span<const int16_t> pcm_data,
                                const media::AudioParameters& params,
                                int64_t sequence_number) {
  // Ignore empty or placeholder/preamble chunks (< 2 samples) that carry no
  // usable audio and can cause buffer underruns.
  if (pcm_data.size() < 2u) {
    return;
  }
  {
    base::AutoLock auto_lock(playback_lock_);
    playback_queue_.push_back(
        {std::vector<int16_t>(pcm_data.begin(), pcm_data.end()), 0,
         sequence_number});
  }
  CreateAudioOutputDevice(params);
}

void AudioController::PlayAudio(base::span<const int16_t> pcm_data,
                                int64_t sequence_number) {
  PlayAudio(pcm_data, GetDefaultPlaybackAudioParameters(), sequence_number);
}

void AudioController::ClearPlaybackQueue() {
  base::AutoLock auto_lock(playback_lock_);
  playback_queue_.clear();
  energy_callbacks_.Notify(0.0f);
}

void AudioController::StopPlayback() {
  ClearPlaybackQueue();
  output_device_.reset();
}

bool AudioController::is_playing() const {
  base::AutoLock auto_lock(playback_lock_);
  return !playback_queue_.empty();
}

void AudioController::CreateAudioOutputDevice(
    const media::AudioParameters& params) {
  if (output_device_ || !factory_binder_) {
    return;
  }
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  factory_binder_.Run(stream_factory.InitWithNewPipeAndPassReceiver());

  output_device_ = std::make_unique<audio::OutputDevice>(
      std::move(stream_factory), params, this,
      media::AudioDeviceDescription::kDefaultDeviceId);
  output_device_->Play();
}

void AudioController::OnCaptureStarted() {}

void AudioController::Capture(const media::AudioBus* audio_source,
                              base::TimeTicks audio_capture_time,
                              const media::AudioGlitchInfo& glitch_info,
                              double volume) {
  // NOTE: This callback is executed on the real-time audio capture thread.
  // We convert and compute energy here, then post to main_task_runner_ via
  // capture_callback_runner_ for thread safety.
  if (!audio_source || audio_source->frames() == 0) {
    return;
  }

  if (!capture_converter_) {
    DeliverCapturedAudio(*audio_source);
    return;
  }

  DeliverCapturedAudio(capture_converter_->Convert(*audio_source));

  // The converter can occasionally satisfy a call entirely from its buffered
  // data without consuming `audio_source`; one extra call is then needed to
  // avoid dropping it. See https://crbug.com/506051.
  if (!capture_converter_->data_was_converted()) {
    DeliverCapturedAudio(capture_converter_->Convert(*audio_source));
  }
}

void AudioController::DeliverCapturedAudio(const media::AudioBus& audio_bus) {
  std::vector<int16_t> pcm_data(audio_bus.frames() * audio_bus.channels());
  audio_bus.ToInterleaved<media::SignedInt16SampleTypeTraits>(
      base::span(pcm_data));

  float sum_squares = 0.0f;
  base::span<const float> channel_data = audio_bus.channel(0);
  for (int i = 0; i < audio_bus.frames(); ++i) {
    sum_squares += channel_data[i] * channel_data[i];
  }
  float energy = std::sqrt(sum_squares / audio_bus.frames());

  media::AudioParameters params = GetBackendInputAudioParameters();
  params.set_frames_per_buffer(audio_bus.frames());

  capture_callback_runner_.Run(std::move(pcm_data), params, energy);
}

void AudioController::OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                                     const std::string& message) {
  VLOG(1) << "AudioController audio capture error: " << message;
}

void AudioController::OnCaptureMuted(bool is_muted) {}

void AudioController::OnCapturedAudioOnMainThread(std::vector<int16_t> pcm_data,
                                                  media::AudioParameters params,
                                                  float energy) {
  capture_callbacks_.Notify(pcm_data, params);
  energy_callbacks_.Notify(energy);
}

int AudioController::Render(base::TimeDelta delay,
                            base::TimeTicks delay_timestamp,
                            const media::AudioGlitchInfo& glitch_info,
                            media::AudioBus* dest) {
  if (!dest || dest->frames() == 0) {
    return 0;
  }

  int frames_rendered = 0;
  int64_t completed_sequence = -1;

  {
    base::AutoLock auto_lock(playback_lock_);
    while (frames_rendered < dest->frames() && !playback_queue_.empty()) {
      auto& chunk = playback_queue_.front();
      const size_t total_chunk_frames = chunk.pcm_data.size();
      const size_t frames_available_in_chunk =
          total_chunk_frames - chunk.read_offset;

      size_t frames_to_copy =
          std::min(static_cast<size_t>(dest->frames() - frames_rendered),
                   frames_available_in_chunk);

      base::span<const int16_t> mono_samples =
          base::span(chunk.pcm_data).subspan(chunk.read_offset, frames_to_copy);
      base::span<float> ch0 = dest->channel(0);
      for (size_t i = 0; i < frames_to_copy; ++i) {
        ch0[frames_rendered + i] =
            media::SignedInt16SampleTypeTraits::ToFloat(mono_samples[i]);
      }

      frames_rendered += frames_to_copy;
      chunk.read_offset += frames_to_copy;

      if (chunk.read_offset >= total_chunk_frames) {
        completed_sequence = chunk.sequence_number;
        playback_queue_.pop_front();
      }
    }
  }

  if (frames_rendered < dest->frames()) {
    dest->ZeroFramesPartial(frames_rendered, dest->frames() - frames_rendered);
  }

  if (dest->channels() > 1) {
    for (int ch = 1; ch < dest->channels(); ++ch) {
      dest->channel(ch).copy_from(dest->channel(0));
    }
  }

  if (completed_sequence >= 0) {
    render_callback_runner_.Run(completed_sequence);
  }

  return dest->frames();
}

void AudioController::OnRenderError() {
  VLOG(1) << "AudioController audio render error";
}

void AudioController::OnAudioRenderedOnMainThread(int64_t completed_sequence) {
  if (completed_sequence >= 0) {
    completion_callbacks_.Notify(completed_sequence);
  }
}

}  // namespace ttc
