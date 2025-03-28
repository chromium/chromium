// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"
#include "components/live_caption/pref_names.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

namespace {
void UnwrapSodaInstallationStatus(
    base::OnceCallback<void(bool)> availabililty_callback,
    SodaInstaller::InstallationStatus status) {
  std::move(availabililty_callback)
      .Run(status == SodaInstaller::InstallationStatus::kReady);
}
}  // namespace

BabelOrcaSpeechRecognizerImpl::BabelOrcaSpeechRecognizerImpl(
    Profile* profile,
    PrefService* global_prefs,
    const std::string& application_locale)
    : ash::SystemLiveCaptionService(
          profile,
          ash::SystemLiveCaptionService::AudioSource::kUserMicrophone),
      soda_installer_(global_prefs, profile->GetPrefs(), application_locale),
      speech_recognition_event_handler_(application_locale),
      primary_profile_(profile) {
}
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
  // TODO(384026579): Notify Producer of error, then retry or alert user.
  soda_installer_.InstallSoda(base::BindOnce(
      &UnwrapSodaInstallationStatus,
      base::BindOnce(
          &SystemLiveCaptionService::SpeechRecognitionAvailabilityChanged,
          service_ptr_factory_.GetWeakPtr())));
}

void BabelOrcaSpeechRecognizerImpl::Stop() {
  SpeechRecognitionAvailabilityChanged(false);
}

void BabelOrcaSpeechRecognizerImpl::ObserveSpeechRecognition(
    BabelOrcaSpeechRecognizer::TranscriptionResultCallback
        transcription_result_callback,
    BabelOrcaSpeechRecognizer::LanguageIdentificationEventCallback
        language_identification_callback) {
  speech_recognition_event_handler_.SetTranscriptionResultCallback(
      std::move(transcription_result_callback),
      std::move(language_identification_callback));
}

void BabelOrcaSpeechRecognizerImpl::RemoveSpeechRecognitionObservation() {
  speech_recognition_event_handler_.RemoveSpeechRecognitionObservation();
}

}  // namespace ash::babelorca
