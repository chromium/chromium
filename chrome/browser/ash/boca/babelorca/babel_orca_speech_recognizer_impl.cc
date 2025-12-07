// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"
#include "components/live_caption/pref_names.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

BabelOrcaSpeechRecognizerImpl::BabelOrcaSpeechRecognizerImpl(
    Profile* profile,
    SodaInstaller* soda_installer,
    const std::string& application_locale,
    const std::string& caption_language)
    : ash::SystemLiveCaptionService(
          profile,
          ash::SystemLiveCaptionService::AudioSource::kUserMicrophone),
      soda_installer_(soda_installer),
      speech_recognition_event_handler_(application_locale),
      primary_profile_(profile),
      caption_language_(caption_language) {}
BabelOrcaSpeechRecognizerImpl::~BabelOrcaSpeechRecognizerImpl() = default;

void BabelOrcaSpeechRecognizerImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& result) {
  speech_recognition_event_handler_.OnSpeechResult(result);
}

void BabelOrcaSpeechRecognizerImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  speech_recognition_event_handler_.OnLanguageIdentificationEvent(event);
}

void BabelOrcaSpeechRecognizerImpl::Start() {
  // If already installed, will immediately begin recognizing.
  started_ = true;
  soda_installer_->InstallSoda(base::BindOnce(
      &BabelOrcaSpeechRecognizerImpl::OnSpeechRecognitionAvailabilityChanged,
      weak_ptr_factory_.GetWeakPtr()));
}

void BabelOrcaSpeechRecognizerImpl::Stop() {
  started_ = false;
  SpeechRecognitionAvailabilityChanged(false);
}

void BabelOrcaSpeechRecognizerImpl::AddObserver(Observer* obs) {
  speech_recognition_event_handler_.AddObserver(obs);
}

void BabelOrcaSpeechRecognizerImpl::RemoveObserver(Observer* obs) {
  speech_recognition_event_handler_.RemoveObserver(obs);
}

media::mojom::RecognizerClientType
BabelOrcaSpeechRecognizerImpl::GetRecognizerClientType() {
  return features::IsBocaClientTypeForSpeechRecognitionEnabled()
             ? media::mojom::RecognizerClientType::kSchoolTools
             : SystemLiveCaptionService::GetRecognizerClientType();
}

std::string BabelOrcaSpeechRecognizerImpl::GetPrimaryLanguageCode() const {
  return caption_language_;
}

void BabelOrcaSpeechRecognizerImpl::OnSpeechRecognitionAvailabilityChanged(
    SodaInstaller::InstallationStatus status) {
  if (!started_) {
    return;
  }
  SpeechRecognitionAvailabilityChanged(
      status == SodaInstaller::InstallationStatus::kReady);
}

}  // namespace ash::babelorca
