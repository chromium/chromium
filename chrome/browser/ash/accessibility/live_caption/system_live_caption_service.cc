// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/soda/constants.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace {
static constexpr base::TimeDelta kStopDelay = base::Seconds(5);
}  // namespace

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
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

SystemLiveCaptionService::~SystemLiveCaptionService() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void SystemLiveCaptionService::Shutdown() {
  controller_ = nullptr;
  StopRecognizing();
}

void SystemLiveCaptionService::OnSpeechResult(
    const std::u16string& /*text*/,
    bool /*is_final*/,
    const absl::optional<media::SpeechRecognitionResult>& result) {
  DCHECK(result.has_value());
  if (!controller_ || !controller_->DispatchTranscription(&context_, *result)) {
    StopRecognizing();
    // Hard and fast stop.
    client_.reset();
  }
}

void SystemLiveCaptionService::OnSpeechSoundLevelChanged(int16_t level) {}

void SystemLiveCaptionService::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (current_recognizer_status_ ==
          SpeechRecognizerStatus::SPEECH_RECOGNITION_STOPPING &&
      new_state == SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
    // Client finished stopping, so let's just update state to Ready and
    // return. The very next step is OnSpeechRecognitionStopped, which will
    // reset the client.
    new_state = SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY;
    return;
  }

  current_recognizer_status_ = new_state;
  if (!controller_)
    return;

  if (!client_) {
    CreateClient();
  }

  if (new_state == SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
    if (output_running_) {
      client_->Start();
    }
    return;
  }

  // We only use other state transitions to detect errors.
  if (new_state != SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR)
    return;

  LOG(ERROR) << "state changed to error, setting controller to error. further "
                "behavior not predictable.";
  controller_->OnError(
      &context_, ::captions::CaptionBubbleErrorType::kGeneric,
      base::RepeatingClosure(),
      base::BindRepeating(
          [](::captions::CaptionBubbleErrorType error_type, bool checked) {}));

  StopRecognizing();
  client_.reset();
}

void SystemLiveCaptionService::OnSpeechRecognitionStopped() {
  if (controller_) {
    controller_->OnAudioStreamEnd(&context_);
  }
  client_.reset();
}

void SystemLiveCaptionService::SpeechRecognitionAvailabilityChanged(
    bool is_speech_recognition_available) {
  if (!controller_)
    return;

  // The controller handles UI creation / destruction, we just need to start /
  // stop providing captions.

  if (is_speech_recognition_available && !client_) {
    // Need to wait for the recognizer to be ready before starting.
    CreateClient();
    // Inject a fake audio system in tests.
    if (!create_audio_system_for_testing_.is_null()) {
      client_->set_audio_system_for_testing(  // IN-TEST
          create_audio_system_for_testing_.Run());
    }

    return;
  }

  if (!is_speech_recognition_available) {
    StopRecognizing();
  }
}

void SystemLiveCaptionService::SpeechRecognitionLanguageChanged(
    const std::string& language) {
  // TODO(b:260372471): pipe through language info.
}

void SystemLiveCaptionService::StopRecognizing() {
  if (!client_)
    return;
  client_->Stop();
}

void SystemLiveCaptionService::OnNonChromeOutputStarted() {
  if (!output_running_) {
    stop_countdown_timer_.reset();  // delete a death timeout.
    if (current_recognizer_status_ ==
        SpeechRecognizerStatus::SPEECH_RECOGNITION_STOPPING) {
      // The audio restarted during stop, so that means we need to restart a new
      // recognizer, etc.
      CreateClient();
      current_recognizer_status_ =
          SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY;
    }
    if (current_recognizer_status_ ==
        SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY) {
      if (!client_) {
        CreateClient();
      }
      client_->Start();
    }
  }

  output_running_ = true;
}

void SystemLiveCaptionService::OnNonChromeOutputStopped() {
  if (output_running_) {
    stop_countdown_timer_ = std::make_unique<base::OneShotTimer>();
    stop_countdown_timer_->Start(
        FROM_HERE, kStopDelay, this,
        &SystemLiveCaptionService::StopTimeoutFinished);
  }
  output_running_ = false;
}

void SystemLiveCaptionService::StopTimeoutFinished() {
  StopRecognizing();
}

void SystemLiveCaptionService::CreateClient() {
  // We must reset first to detach everything first, and then reattach.
  client_.reset();
  client_ = std::make_unique<SpeechRecognitionRecognizerClientImpl>(
      weak_ptr_factory_.GetWeakPtr(), profile_,
      media::AudioDeviceDescription::kLoopbackWithoutChromeId,
      media::mojom::SpeechRecognitionOptions::New(
          media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/true,
          prefs::GetLiveCaptionLanguageCode(profile_->GetPrefs()),
          /*is_server_based=*/false,
          media::mojom::RecognizerClientType::kLiveCaption));
}

}  // namespace ash
