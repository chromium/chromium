// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {
namespace {

media::mojom::RecognizerClientType GetRecognizerClientType() {
  // TODO(crbug.com/416494658): Remove kLiveCaption fallback once kSchoolTools
  // is proven to be well configured.
  return features::IsBocaClientTypeForSpeechRecognitionEnabled()
             ? media::mojom::RecognizerClientType::kSchoolTools
             : media::mojom::RecognizerClientType::kLiveCaption;
}

std::unique_ptr<SpeechRecognizer> CreateSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    Profile* profile,
    const std::string& device_id,
    media::mojom::SpeechRecognitionOptionsPtr options) {
  return std::make_unique<SpeechRecognitionRecognizerClientImpl>(
      delegate, profile, device_id, std::move(options));
}

}  // namespace

BabelOrcaSpeechRecognizerClient::BabelOrcaSpeechRecognizerClient(
    Profile* profile,
    SodaInstaller* soda_installer,
    const std::string& application_locale,
    const std::string& caption_language)
    : soda_installer_(soda_installer),
      primary_profile_(profile),
      default_language_(caption_language),
      source_language_(caption_language),
      speech_recognizer_factory_(base::BindRepeating(CreateSpeechRecognizer)) {}

BabelOrcaSpeechRecognizerClient::~BabelOrcaSpeechRecognizerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BabelOrcaSpeechRecognizerClient::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (!result.has_value()) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnTranscriptionResult(result.value(), source_language_);
  }
}

void BabelOrcaSpeechRecognizerClient::OnSpeechSoundLevelChanged(int16_t level) {
}

void BabelOrcaSpeechRecognizerClient::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (speech_recognizer_status_ ==
          SpeechRecognizerStatus::SPEECH_RECOGNITION_STOPPING &&
      new_state == SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
    return;
  }
  speech_recognizer_status_ = new_state;
  if (new_state == SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
    speech_recognizer_->Start();
    return;
  }
  if (new_state != SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR) {
    return;
  }
  LOG(ERROR) << "[BabelOrca] Speech recognition state changed to error.";
  speech_recognizer_.reset();
}

void BabelOrcaSpeechRecognizerClient::OnSpeechRecognitionStopped() {
  speech_recognizer_.reset();
}

void BabelOrcaSpeechRecognizerClient::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  if (event->asr_switch_result !=
      media::mojom::AsrSwitchResult::kSwitchSucceeded) {
    return;
  }
  source_language_ = event->language;
  for (auto& observer : observers_) {
    observer.OnLanguageIdentificationEvent(event);
  }
}

void BabelOrcaSpeechRecognizerClient::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If already installed, will immediately begin recognizing.
  started_ = true;
  soda_installer_->InstallSoda(
      base::BindOnce(&BabelOrcaSpeechRecognizerClient::OnSodaInstallResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BabelOrcaSpeechRecognizerClient::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  started_ = false;
  speech_recognizer_.reset();
}

void BabelOrcaSpeechRecognizerClient::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void BabelOrcaSpeechRecognizerClient::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void BabelOrcaSpeechRecognizerClient::OnSodaInstallResult(
    SodaInstaller::InstallationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_ || status != SodaInstaller::InstallationStatus::kReady) {
    return;
  }
  speech_recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  speech_recognizer_.reset();
  speech_recognizer_ = speech_recognizer_factory_.Run(
      weak_ptr_factory_.GetWeakPtr(), primary_profile_,
      media::AudioDeviceDescription::kDefaultDeviceId,
      media::mojom::SpeechRecognitionOptions::New(
          media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/true, default_language_,
          /*is_server_based=*/false, GetRecognizerClientType(),
          /*skip_continuously_empty_audio=*/true));
}

void BabelOrcaSpeechRecognizerClient::SetSpeechRecognizerFactoryForTesting(
    SpeechRecognizerFactory speech_recognizer_factory) {
  speech_recognizer_factory_ = std::move(speech_recognizer_factory);
}

}  // namespace ash::babelorca
