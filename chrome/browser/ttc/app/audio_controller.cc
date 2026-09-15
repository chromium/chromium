// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_controller.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/cpp/output_device.h"

namespace ttc {

namespace {

// Low-latency hardware sample rate is natively 48kHz on macOS CoreAudio and
// Android (AAudio/OpenSLES), as well as Linux and Windows WASAPI.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
constexpr int kDefaultCaptureSampleRate = 48000;
constexpr int kDefaultCaptureFramesPerBuffer = 480;  // 10ms at 48kHz

constexpr int kDefaultPlaybackSampleRate = 48000;
constexpr int kDefaultPlaybackFramesPerBuffer = 480;
#else
constexpr int kDefaultCaptureSampleRate = 16000;
constexpr int kDefaultCaptureFramesPerBuffer = 1600;  // 100ms chunks

constexpr int kDefaultPlaybackSampleRate = 24000;
constexpr int kDefaultPlaybackFramesPerBuffer = 2400;  // 100ms chunks
#endif

AudioController::AudioStreamFactoryBinder& GetDefaultBinderForTestingStorage() {
  static base::NoDestructor<AudioController::AudioStreamFactoryBinder> binder;
  return *binder;
}

}  // namespace

// static
media::AudioParameters AudioController::GetDefaultCaptureAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                kDefaultCaptureSampleRate,
                                kDefaultCaptureFramesPerBuffer);
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

AudioController::AudioController(AudioStreamFactoryBinder factory_binder)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      factory_binder_(std::move(factory_binder)) {
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

void AudioController::StartCapture(std::string_view device_id) {
  if (audio_capturer_source_ || !factory_binder_) {
    return;
  }
  const std::string effective_device_id =
      device_id.empty() ? media::AudioDeviceDescription::kDefaultDeviceId
                        : std::string(device_id);

  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  factory_binder_.Run(stream_factory.InitWithNewPipeAndPassReceiver());

  audio_capturer_source_ =
      audio::CreateInputDevice(std::move(stream_factory), effective_device_id,
                               audio::DeadStreamDetection::kEnabled);

  media::AudioParameters input_params = GetDefaultCaptureAudioParameters();
  audio_capturer_source_->Initialize(input_params, this);
  audio_capturer_source_->Start();
}

void AudioController::StopCapture() {
  if (audio_capturer_source_) {
    audio_capturer_source_->Stop();
    audio_capturer_source_ = nullptr;
  }
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

void AudioController::PlayAudio(base::span<const uint8_t> pcm_data,
                                const media::AudioParameters& params,
                                int64_t sequence_number) {
  if (pcm_data.empty()) {
    return;
  }
  {
    base::AutoLock auto_lock(playback_lock_);
    playback_queue_.push_back(
        {std::vector<uint8_t>(pcm_data.begin(), pcm_data.end()), 0,
         sequence_number});
  }
  CreateAudioOutputDevice(params);
}

void AudioController::PlayAudio(base::span<const uint8_t> pcm_data,
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
  // We compute energy and convert samples here, then post to main_task_runner_
  // via capture_callback_runner_ for thread safety.
  if (!audio_source || audio_source->frames() == 0) {
    return;
  }

  std::vector<uint8_t> pcm_data(audio_source->frames() * sizeof(int16_t));
  audio_source->ToInterleavedBytes<media::SignedInt16SampleTypeTraits>(
      base::span(pcm_data));

  float sum_squares = 0.0f;
  base::span<const float> channel_data = audio_source->channel(0);
  for (int i = 0; i < audio_source->frames(); ++i) {
    sum_squares += channel_data[i] * channel_data[i];
  }
  float energy = std::sqrt(sum_squares / audio_source->frames());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                kDefaultCaptureSampleRate,
                                audio_source->frames());

  capture_callback_runner_.Run(std::move(pcm_data), params, energy);
}

void AudioController::OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                                     const std::string& message) {
  VLOG(1) << "AudioController audio capture error: " << message;
}

void AudioController::OnCaptureMuted(bool is_muted) {}

void AudioController::OnCapturedAudioOnMainThread(std::vector<uint8_t> pcm_data,
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
      const size_t bytes_per_frame = sizeof(int16_t);
      const size_t total_chunk_frames = chunk.pcm_data.size() / bytes_per_frame;
      const size_t frames_available_in_chunk =
          total_chunk_frames - chunk.read_offset;

      size_t frames_to_copy =
          std::min(static_cast<size_t>(dest->frames() - frames_rendered),
                   frames_available_in_chunk);

      base::span<const uint8_t> chunk_bytes(chunk.pcm_data);
      base::span<const uint8_t> sub_span =
          chunk_bytes.subspan(chunk.read_offset * bytes_per_frame,
                              frames_to_copy * bytes_per_frame);

      auto mono_samples =
          base::subtle::reinterpret_span<const int16_t>(sub_span);
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

  float energy = 0.0f;
  if (frames_rendered > 0) {
    base::span<const float> channel = dest->channel(0);
    float sum_squares = 0.0f;
    for (int i = 0; i < frames_rendered; ++i) {
      sum_squares += channel[i] * channel[i];
    }
    energy = std::sqrt(sum_squares / frames_rendered);
  }

  if (completed_sequence >= 0 || frames_rendered > 0) {
    render_callback_runner_.Run(completed_sequence, energy);
  }

  return dest->frames();
}

void AudioController::OnRenderError() {
  VLOG(1) << "AudioController audio render error";
}

void AudioController::OnAudioRenderedOnMainThread(int64_t completed_sequence,
                                                  float energy) {
  if (completed_sequence >= 0) {
    completion_callbacks_.Notify(completed_sequence);
  }
  if (energy > 0.0f) {
    energy_callbacks_.Notify(energy);
  }
}

}  // namespace ttc
