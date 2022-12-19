// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/system_live_caption_service.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace ash {

SystemLiveCaptionService::SystemLiveCaptionService(Profile* profile)
    : profile_(profile),
      controller_(
          ::captions::LiveCaptionControllerFactory::GetForProfile(profile)) {
  DCHECK_EQ(ProfileManager::GetPrimaryUserProfile(), profile);

  // The controller handles all SODA installation / languages etc. for us. We
  // just subscribe to the interface that informs us when we're ready to go.
  SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile_)
      ->BindSpeechRecognitionBrowserObserver(
          browser_observer_receiver_.BindNewPipeAndPassRemote());
}

SystemLiveCaptionService::~SystemLiveCaptionService() = default;

void SystemLiveCaptionService::Shutdown() {
  controller_ = nullptr;
  StopRecognizing();
}

void SystemLiveCaptionService::OnSpeechResult(
    const std::u16string& /*text*/,
    bool /*is_final*/,
    const absl::optional<media::SpeechRecognitionResult>& result) {
  DCHECK(result.has_value());

  if (!controller_ || !controller_->DispatchTranscription(&context_, *result))
    StopRecognizing();
}

void SystemLiveCaptionService::OnSpeechSoundLevelChanged(int16_t level) {}

void SystemLiveCaptionService::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (!controller_)
    return;

  DCHECK(client_);

  if (new_state == SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
    client_->Start();
    return;
  }

  // We only use other state transitions to detect errors.
  if (new_state != SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR)
    return;

  controller_->OnError(
      &context_, ::captions::CaptionBubbleErrorType::kGeneric,
      base::RepeatingClosure(),
      base::BindRepeating(
          [](::captions::CaptionBubbleErrorType error_type, bool checked) {}));

  StopRecognizing();
}

void SystemLiveCaptionService::OnSpeechRecognitionStopped() {
  if (controller_)
    controller_->OnAudioStreamEnd(&context_);
}

void SystemLiveCaptionService::SpeechRecognitionAvailabilityChanged(
    bool is_speech_recognition_available) {
  if (!controller_)
    return;

  // The controller handles UI creation / destruction, we just need to start /
  // stop providing captions.

  if (is_speech_recognition_available && !client_) {
    // Need to wait for the recognizer to be ready before starting.
    client_ = std::make_unique<SpeechRecognitionRecognizerClientImpl>(
        weak_ptr_factory_.GetWeakPtr(), profile_,
        media::mojom::SpeechRecognitionOptions::New(
            media::mojom::SpeechRecognitionMode::kCaption,
            /*enable_formatting=*/false,
            prefs::GetLiveCaptionLanguageCode(profile_->GetPrefs()),
            /*is_server_based=*/false,
            media::mojom::RecognizerClientType::kLiveCaption));

    // Inject a fake audio system in tests.
    if (!create_audio_system_for_testing_.is_null()) {
      client_->set_audio_system_for_testing(  // IN-TEST
          create_audio_system_for_testing_.Run());
    }

    return;
  }

  if (!is_speech_recognition_available)
    StopRecognizing();
}

void SystemLiveCaptionService::SpeechRecognitionLanguageChanged(
    const std::string& language) {
  // TODO(b:260372471): pipe through language info.
}

void SystemLiveCaptionService::StopRecognizing() {
  if (!client_)
    return;

  client_->Stop();
  client_.reset();
}

}  // namespace ash
