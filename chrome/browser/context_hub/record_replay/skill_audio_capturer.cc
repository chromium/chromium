// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_audio_capturer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "components/capture_mode/audio_capturer.h"
#include "content/public/browser/audio_service.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "components/soda/soda_installer.h"
#else
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#endif

namespace context_hub {
namespace {

media::AudioParameters GetDefaultAudioParameters() {
  static constexpr int kDefaultAudioSampleRate = 16000;
  static constexpr int kPollingTimesPerSecond = 10;
  return media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), kDefaultAudioSampleRate,
      kDefaultAudioSampleRate / kPollingTimesPerSecond);
}

base::TimeDelta ResolveTranscriptTimestamp(
    const media::SpeechRecognitionResult& result,
    base::TimeTicks session_start_time) {
  if (result.timing_information.has_value()) {
    const media::TimingInformation& timing_info =
        result.timing_information.value();
    if (timing_info.originating_media_timestamps.has_value() &&
        !timing_info.originating_media_timestamps.value().empty()) {
      return timing_info.originating_media_timestamps.value().front().start;
    }
    return timing_info.audio_start_time;
  }
  return std::max(base::TimeDelta(),
                  base::TimeTicks::Now() - session_start_time);
}

}  // namespace

SkillAudioCapturer::SkillAudioCapturer(
    Profile* profile,
    OnTranscriptUpdateCallback on_transcript_update_callback)
    : profile_(CHECK_DEREF(profile)),
      on_transcript_update_callback_(std::move(on_transcript_update_callback)),
      audio_params_(GetDefaultAudioParameters()) {
  CHECK(on_transcript_update_callback_);
}

SkillAudioCapturer::~SkillAudioCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void SkillAudioCapturer::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_recording_) {
    return;
  }
  if (!speech_recognizer_.is_bound()) {
    InitializeSpeechRecognition();
  }
  if (!speech_recognizer_.is_bound()) {
    return;
  }

  is_recording_ = true;
  session_start_time_ = base::TimeTicks::Now();

  if (profile_->AsTestingProfile() && !audio_system_) {
    CHECK_IS_TEST();
    return;
  }

  if (!audio_system_) {
    audio_system_ = content::CreateAudioSystemForAudioService();
  }
  audio_system_->GetInputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::BindOnce(&SkillAudioCapturer::OnInputStreamParametersReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SkillAudioCapturer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_recording_) {
    return;
  }
  if (audio_capturer_) {
    audio_capturer_->Stop();
    audio_capturer_.reset();
  }
  is_recording_ = false;
  is_multichannel_supported_ = false;
  audio_params_ = GetDefaultAudioParameters();

  if (speech_recognizer_.is_bound()) {
    speech_recognizer_.reset();
  }
  speech_recognizer_client_.reset();
  speech_recognition_context_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SkillAudioCapturer::SetAudioSystemForTesting(
    std::unique_ptr<media::AudioSystem> audio_system) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  audio_system_ = std::move(audio_system);
}

void SkillAudioCapturer::OnAudioCapturedForTesting(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks audio_capture_time) {
  OnAudioCaptured(std::move(audio_bus), audio_capture_time);
  if (speech_recognizer_.is_bound()) {
    speech_recognizer_.FlushForTesting();
  }
}

void SkillAudioCapturer::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.transcription.empty()) {
    const base::TimeDelta timestamp =
        ResolveTranscriptTimestamp(result, session_start_time_);
    on_transcript_update_callback_.Run(result.transcription, timestamp,
                                       result.is_final);
  }
  std::move(reply).Run(true);
}

void SkillAudioCapturer::OnSpeechRecognitionStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void SkillAudioCapturer::OnSpeechRecognitionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void SkillAudioCapturer::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {}

void SkillAudioCapturer::InitializeSpeechRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition) ||
      !speech::SodaInstaller::GetInstance()) {
    return;
  }
  speech::SpeechRecognitionService* service =
      CrosSpeechRecognitionServiceFactory::GetForProfile(&profile_.get());
#else
  speech::SpeechRecognitionService* service =
      SpeechRecognitionServiceFactory::GetForProfile(&profile_.get());
#endif
  if (!service) {
    return;
  }

  service->BindSpeechRecognitionContext(
      speech_recognition_context_.BindNewPipeAndPassReceiver());
  speech_recognition_context_.set_disconnect_handler(
      base::BindOnce(&SkillAudioCapturer::OnSpeechRecognitionError,
                     weak_ptr_factory_.GetWeakPtr()));

  media::mojom::SpeechRecognitionOptionsPtr options =
      media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->enable_formatting = true;
  options->language = "en-US";
  options->is_server_based = false;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;

  speech_recognition_context_->BindRecognizer(
      speech_recognizer_.BindNewPipeAndPassReceiver(),
      speech_recognizer_client_.BindNewPipeAndPassRemote(), std::move(options),
      base::BindOnce(&SkillAudioCapturer::OnRecognizerBound,
                     weak_ptr_factory_.GetWeakPtr()));
  speech_recognizer_.set_disconnect_handler(
      base::BindOnce(&SkillAudioCapturer::OnSpeechRecognitionError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SkillAudioCapturer::OnRecognizerBound(bool is_multichannel_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_multichannel_supported_ = is_multichannel_supported;
}

void SkillAudioCapturer::OnInputStreamParametersReceived(
    const std::optional<media::AudioParameters>& device_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_recording_) {
    return;
  }
  if (!device_params.has_value() || !device_params.value().IsValid()) {
    Stop();
    return;
  }

  audio_params_ = device_params.value();
  if (profile_->AsTestingProfile()) {
    CHECK_IS_TEST();
    return;
  }

  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  content::GetAudioService().BindStreamFactory(
      stream_factory.InitWithNewPipeAndPassReceiver());
  audio_capturer_ = std::make_unique<capture_mode::AudioCapturer>(
      media::AudioDeviceDescription::kDefaultDeviceId,
      std::move(stream_factory), audio_params_,
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&SkillAudioCapturer::OnAudioCaptured,
                              weak_ptr_factory_.GetWeakPtr())));
  audio_capturer_->Start();
}

void SkillAudioCapturer::OnAudioCaptured(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks audio_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_recording_ || !speech_recognizer_ || !audio_bus ||
      audio_bus->frames() <= 0 || audio_bus->channels() <= 0) {
    return;
  }
  const base::TimeDelta media_start_pts =
      std::max(base::TimeDelta(), audio_capture_time - session_start_time_);
  const media::ChannelLayout channel_layout =
      audio_bus->channels() == audio_params_.channels()
          ? audio_params_.channel_layout()
          : media::GuessChannelLayout(audio_bus->channels());
  speech_recognizer_->SendAudioToSpeechRecognitionService(
      audio_converter_.ConvertToAudioDataS16(
          *audio_bus, audio_params_.sample_rate(), channel_layout,
          is_multichannel_supported_),
      media_start_pts);
}

}  // namespace context_hub
