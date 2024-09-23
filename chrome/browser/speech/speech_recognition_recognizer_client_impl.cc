// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "components/language/core/common/locale_util.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"

namespace {

// Sample rate used by content::SpeechRecognizerImpl, which is used
// by NetworkSpeechRecognizer.
static constexpr int kAudioSampleRate = 16000;

// This is about how many times we want the audio callback to happen per second.
// Web speech recognition happens about 10 time per second, so we take that
// convervative number here. We can increase if it seems laggy.
static constexpr int kPollingTimesPerSecond = 10;

media::AudioParameters GetAudioParameters(
    const std::optional<media::AudioParameters>& params,
    bool is_multichannel_supported) {
  if (params) {
    media::AudioParameters result = params.value();
    int sample_rate = params->sample_rate();
    int frames_per_buffer = std::max(params->frames_per_buffer(),
                                     sample_rate / kPollingTimesPerSecond);
    media::ChannelLayoutConfig channel_layout_config =
        is_multichannel_supported ? params->channel_layout_config()
                                  : media::ChannelLayoutConfig::Mono();
    result.Reset(params->format(), channel_layout_config, sample_rate,
                 frames_per_buffer);
    return result;
  }

  static_assert(kAudioSampleRate % 100 == 0,
                "Audio sample rate is not divisible by 100");
  return media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      is_multichannel_supported ? media::ChannelLayoutConfig::Stereo()
                                : media::ChannelLayoutConfig::Mono(),
      kAudioSampleRate, kAudioSampleRate / kPollingTimesPerSecond);
}

inline bool IsLanguageSupported(const speech::SodaInstaller* soda_installer,
                                const speech::LanguageCode language_code) {
  for (auto const& language : soda_installer->GetAvailableLanguages()) {
    if (speech::GetLanguageCode(language) == language_code)
      return true;
  }
  return false;
}

inline ash::OnDeviceRecognitionAvailability InstallationErrorToAvailability(
    speech::SodaInstaller::ErrorCode error_code) {
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError:
      return ash::OnDeviceRecognitionAvailability::
          kSodaInstallationErrorUnspecified;
    case speech::SodaInstaller::ErrorCode::kNeedsReboot:
      return ash::OnDeviceRecognitionAvailability::
          kSodaInstallationErrorNeedsReboot;
  }
}

}  // namespace

ash::OnDeviceRecognitionAvailability
SpeechRecognitionRecognizerClientImpl::GetOnDeviceSpeechRecognitionAvailability(
    const std::string& language) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return ash::OnDeviceRecognitionAvailability::kSodaNotAvailable;
  }

  const auto language_code = speech::GetLanguageCode(language);
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();

  if (soda_installer->IsSodaInstalled(language_code))
    return ash::OnDeviceRecognitionAvailability::kAvailable;

  if (!IsLanguageSupported(soda_installer, language_code))
    return ash::OnDeviceRecognitionAvailability::kUserLanguageNotAvailable;

  // Maybe SODA is currently installing.
  if (soda_installer->IsSodaDownloading(language_code) ||
      soda_installer->IsSodaDownloading(speech::LanguageCode::kNone)) {
    return ash::OnDeviceRecognitionAvailability::kSodaInstalling;
  }

  // It is possible that there was some installation issues for SODA which we
  // can surface to the user.
  const auto binary_error_code =
      soda_installer->GetSodaInstallErrorCode(speech::LanguageCode::kNone);
  if (binary_error_code)
    return InstallationErrorToAvailability(binary_error_code.value());

  const auto language_error_code =
      soda_installer->GetSodaInstallErrorCode(language_code);
  if (language_error_code)
    return InstallationErrorToAvailability(language_error_code.value());

  return ash::OnDeviceRecognitionAvailability::kSodaNotInstalled;
}

ash::ServerBasedRecognitionAvailability
SpeechRecognitionRecognizerClientImpl::GetServerBasedRecognitionAvailability(
    const std::string& language) {
  if (!(ash::features::IsInternalServerSideSpeechRecognitionEnabled() ||
        ash::features::IsInternalServerSideSpeechRecognitionEnabledByFinch())) {
    return ash::ServerBasedRecognitionAvailability::
        kServerBasedRecognitionNotAvailable;
  }

  static constexpr auto kSupportedLanguagesAndLocales =
      base::MakeFixedFlatSet<std::string_view>({
          "de",              // German
          "de-AT",           // German (Austria)
          "de-CH",           // German (Switzerland)
          "de-DE",           // German (Germany)
          "de-LI",           // German (Italy)
          "en",              // English
          "en-AU",           // English (Australia)
          "en-CA",           // English (Canada)
          "en-GB",           // English (UK)
          "en-GB-oxendict",  // English (UK, OED spelling)
          "en-IE",           // English (Ireland)
          "en-NZ",           // English (New Zealand)
          "en-US",           // English (US)
          "en-XA",           // Long strings Pseudolocale
          "en-ZA",           // English (South Africa)
          "es",              // Spanish
          "es-419",          // Spanish (Latin America)
          "es-AR",           // Spanish (Argentina)
          "es-CL",           // Spanish (Chile)
          "es-CO",           // Spanish (Colombia)
          "es-CR",           // Spanish (Costa Rica)
          "es-ES",           // Spanish (Spain)
          "es-HN",           // Spanish (Honduras)
          "es-MX",           // Spanish (Mexico)
          "es-PE",           // Spanish (Peru)
          "es-US",           // Spanish (US)
          "es-UY",           // Spanish (Uruguay)
          "es-VE",           // Spanish (Venezuela)
          "fr",              // French
          "fr-CA",           // French (Canada)
          "fr-CH",           // French (Switzerland)
          "fr-FR",           // French (France)
          "id",              // Indonesian
          "it",              // Italian
          "it-CH",           // Italian (Switzerland)
          "it-IT",           // Italian (Italy)
          "ja",              // Japanese
          "ko",              // Korean
          "pt",              // Portuguese
          "pt-BR",           // Portuguese (Brazil)
          "pt-PT",           // Portuguese (Portugal)
          "ru",              // Russian
          "sv",              // Swedish
          "tr",              // Turkish
      });

  bool is_supported =
      ash::features::IsInternalServerSideSpeechRecognitionEnabled() &&
      kSupportedLanguagesAndLocales.contains(language);

  if (is_supported ||
      ash::features::IsInternalServerSideSpeechRecognitionEnabledByFinch()) {
    return ash::ServerBasedRecognitionAvailability::kAvailable;
  }

  return ash::ServerBasedRecognitionAvailability::kUserLanguageNotAvailable;
}

SpeechRecognitionRecognizerClientImpl::SpeechRecognitionRecognizerClientImpl(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    Profile* profile,
    const std::string& device_id,
    media::mojom::SpeechRecognitionOptionsPtr options)
    : SpeechRecognizer(delegate), device_id_(device_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(options->language.has_value());
  language_ = options->language.value();

  // Connect the AudioSourceSpeechRecognitionContext & bind an
  // AudioSourceFetcher recognizer.
  CrosSpeechRecognitionServiceFactory::GetForProfile(profile)
      ->BindAudioSourceSpeechRecognitionContext(
          audio_source_speech_recognition_context_
              .BindNewPipeAndPassReceiver());
  audio_source_speech_recognition_context_->BindAudioSourceFetcher(
      audio_source_fetcher_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      std::move(options),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &SpeechRecognitionRecognizerClientImpl::OnRecognizerBound,
          weak_factory_.GetWeakPtr())));

  audio_source_speech_recognition_context_.set_disconnect_handler(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &SpeechRecognitionRecognizerClientImpl::OnRecognizerDisconnected,
          weak_factory_.GetWeakPtr())));
}

SpeechRecognitionRecognizerClientImpl::
    ~SpeechRecognitionRecognizerClientImpl() {
  audio_source_fetcher_->Stop();
  audio_source_fetcher_.reset();
  speech_recognition_client_receiver_.reset();
  audio_source_speech_recognition_context_.reset();
}

void SpeechRecognitionRecognizerClientImpl::Start() {
  // Get audio parameters from the AudioSystem, and use these to start
  // recognition from the callback.
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  waiting_for_params_ = true;
  audio_system_->GetInputStreamParameters(
      device_id_, base::BindOnce(&SpeechRecognitionRecognizerClientImpl::
                                     StartFetchingOnInputDeviceInfo,
                                 weak_factory_.GetWeakPtr()));
}

void SpeechRecognitionRecognizerClientImpl::Stop() {
  audio_source_fetcher_->Stop();
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNITION_STOPPING);
}

void SpeechRecognitionRecognizerClientImpl::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Returning true ensures the speech recognition continues.
  std::move(reply).Run(true);

  if (!result.transcription.size())
    return;
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_IN_SPEECH);
  delegate()->OnSpeechResult(base::UTF8ToUTF16(result.transcription),
                             result.is_final, result);
}

void SpeechRecognitionRecognizerClientImpl::OnSpeechRecognitionError() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
}

void SpeechRecognitionRecognizerClientImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  delegate()->OnLanguageIdentificationEvent(std::move(event));
}

void SpeechRecognitionRecognizerClientImpl::OnSpeechRecognitionStopped() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
  delegate()->OnSpeechRecognitionStopped();
}

void SpeechRecognitionRecognizerClientImpl::OnRecognizerBound(
    bool is_multichannel_supported) {
  is_multichannel_supported_ = is_multichannel_supported;
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

void SpeechRecognitionRecognizerClientImpl::OnRecognizerDisconnected() {
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
}

void SpeechRecognitionRecognizerClientImpl::StartFetchingOnInputDeviceInfo(
    const std::optional<media::AudioParameters>& params) {
  // waiting_for_params_ was set before requesting audio params from the
  // AudioSystem, which returns here asynchronously. If this has changed, then
  // we shouldn't start up any more.
  if (!waiting_for_params_)
    return;
  waiting_for_params_ = false;

  // Bind to an AudioSourceFetcher in the Speech Recognition service,
  // passing the stream factory so it can listen to mic audio.
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  content::GetAudioServiceStreamFactoryBinder().Run(
      stream_factory.InitWithNewPipeAndPassReceiver());
  audio_source_fetcher_->Start(
      std::move(stream_factory), device_id_,
      GetAudioParameters(params, is_multichannel_supported_));
  UpdateStatus(SpeechRecognizerStatus::SPEECH_RECOGNIZER_RECOGNIZING);
}

void SpeechRecognitionRecognizerClientImpl::UpdateStatus(
    SpeechRecognizerStatus state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  waiting_for_params_ = false;
  if (state_ == state)
    return;

  state_ = state;
  // Since the |OnSpeechRecognitionStateChanged| call below can destroy |this|
  // it should be the last thing done in here.
  delegate()->OnSpeechRecognitionStateChanged(state);
}
