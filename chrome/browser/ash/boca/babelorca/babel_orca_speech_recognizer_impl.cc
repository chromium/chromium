// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"

#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"

namespace ash::babelorca {

BabelOrcaSpeechRecognizerImpl::BabelOrcaSpeechRecognizerImpl(Profile* profile)
    : ash::SystemLiveCaptionService(
          profile,
          ash::SystemLiveCaptionService::AudioSource::kUserMicrophone),
      speech_recognition_event_handler_(
          prefs::GetUserMicrophoneCaptionLanguage(profile->GetPrefs())),
      primary_profile_(profile) {}
BabelOrcaSpeechRecognizerImpl::~BabelOrcaSpeechRecognizerImpl() = default;

void BabelOrcaSpeechRecognizerImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& result) {
  speech_recognition_event_handler_.OnSpeechResult(result);
}

void BabelOrcaSpeechRecognizerImpl::Start() {
  SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
      primary_profile_)
      ->ChangeBabelOrcaSpeechRecognitionAvailability(true);
  ::captions::LiveCaptionControllerFactory::GetForProfile(primary_profile_)
      ->ToggleLiveCaptionForBabelOrca(true);
}

void BabelOrcaSpeechRecognizerImpl::Stop() {
  SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
      primary_profile_)
      ->ChangeBabelOrcaSpeechRecognitionAvailability(false);
  ::captions::LiveCaptionControllerFactory::GetForProfile(primary_profile_)
      ->ToggleLiveCaptionForBabelOrca(false);
}

void BabelOrcaSpeechRecognizerImpl::ObserveTranscriptionResult(
    BabelOrcaSpeechRecognizer::TranscriptionResultCallback
        transcrcription_result_callback) {
  speech_recognition_event_handler_.SetTranscriptionResultCallback(
      std::move(transcrcription_result_callback));
}

void BabelOrcaSpeechRecognizerImpl::RemoveTranscriptionResultObservation() {
  speech_recognition_event_handler_.RemoveTranscriptionResultObservation();
}

}  // namespace ash::babelorca
