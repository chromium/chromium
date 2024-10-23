// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"

#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"

namespace ash::babelorca {

BabelOrcaSpeechRecognizerImpl::BabelOrcaSpeechRecognizerImpl(Profile* profile)
    : ash::SystemLiveCaptionService(
          profile,
          ash::SystemLiveCaptionService::AudioSource::kUserMicrophone),
      primary_profile_(profile) {}
BabelOrcaSpeechRecognizerImpl::~BabelOrcaSpeechRecognizerImpl() = default;

void BabelOrcaSpeechRecognizerImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (transcription_result_callback_ && result.has_value()) {
    // TODO Change source language on language identification events.
    transcription_result_callback_.Run(
        result.value(),
        prefs::GetUserMicrophoneCaptionLanguage(primary_profile_->GetPrefs()));
  }
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
  transcription_result_callback_ = transcrcription_result_callback;
}

void BabelOrcaSpeechRecognizerImpl::RemoveTranscriptionResultObservation() {
  transcription_result_callback_.Reset();
}

}  // namespace ash::babelorca
