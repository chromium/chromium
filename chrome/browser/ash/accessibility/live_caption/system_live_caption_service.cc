// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_util.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/soda/constants.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom-forward.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "system_live_caption_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
static constexpr base::TimeDelta kStopDelay = base::Seconds(5);
}  // namespace

namespace ash {

SystemLiveCaptionService::SystemLiveCaptionService(Profile* profile)
    : profile_(profile),
      controller_(
          ::captions::LiveCaptionControllerFactory::GetForProfile(profile)),
      // Unretained is safe because the live caption service outlives the
      // caption bubble that uses this callback.
      context_(
          base::BindRepeating(&SystemLiveCaptionService::OpenCaptionSettings,
                              base::Unretained(this))) {
  DCHECK_EQ(ProfileManager::GetPrimaryUserProfile(), profile);
  // The controller handles all SODA installation / languages etc. for us. We
  // just subscribe to the interface that informs us when we're ready to go.
  SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile_)
      ->BindSpeechRecognitionBrowserObserver(
          browser_observer_receiver_.BindNewPipeAndPassRemote());
  source_language_ =
      profile_->GetPrefs()->GetString(prefs::kLiveCaptionLanguageCode);
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
    const std::optional<media::SpeechRecognitionResult>& result) {
  // TODO(robsc): add in the other sides stability functionality.
  DCHECK(result.has_value());
  if (!controller_) {
    StopRecognizing();
    // Hard and fast stop.
    client_.reset();
    return;
  }

  bool exec_result = true;
  auto* prefs = profile_->GetPrefs();
  std::string target_language =
      prefs->GetString(prefs::kLiveTranslateTargetLanguageCode);
  if (media::IsLiveTranslateEnabled() &&
      prefs->GetBoolean(prefs::kLiveTranslateEnabled) &&
      l10n_util::GetLanguage(target_language) !=
          l10n_util::GetLanguage(source_language_)) {
    auto cache_result = translation_cache_.FindCachedTranslationOrRemaining(
        result->transcription, source_language_, target_language);
    std::string cached_translation = cache_result.second;
    std::string string_to_translate = cache_result.first;

    if (!string_to_translate.empty()) {
      characters_translated_ += string_to_translate.size();
      ::captions::LiveTranslateControllerFactory::GetForProfile(profile_)
          ->GetTranslation(
              string_to_translate, source_language_, target_language,
              base::BindOnce(&SystemLiveCaptionService::OnTranslationCallback,
                             weak_ptr_factory_.GetWeakPtr(), cached_translation,
                             string_to_translate, source_language_,
                             target_language, result->is_final));
    } else {
      exec_result = controller_->DispatchTranscription(
          &context_,
          media::SpeechRecognitionResult(cached_translation, result->is_final));
    }
  } else {
    exec_result = controller_->DispatchTranscription(&context_, *result);
  }

  if (!exec_result) {
    StopRecognizing();
    // Hard and fast stop.
    client_.reset();
  }
}

void SystemLiveCaptionService::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  if (!controller_) {
    return;
  }

  if (event->asr_switch_result ==
      media::mojom::AsrSwitchResult::kSwitchSucceeded) {
    source_language_ = event->language;
  }
  controller_->OnLanguageIdentificationEvent(&context_, std::move(event));
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
    current_recognizer_status_ =
        SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY;
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
  if (!controller_) {
    return;
  }
  // The controller handles UI creation / destruction, we just need to start /
  // stop providing captions.

  if (is_speech_recognition_available) {
    if (!client_) {
      // Need to wait for the recognizer to be ready before starting.
      CreateClient();
      // Inject a fake audio system in tests.
      if (!create_audio_system_for_testing_.is_null()) {
        client_->set_audio_system_for_testing(  // IN-TEST
            create_audio_system_for_testing_.Run());
      }
    }
    // At startup, when asr becomes available, we need to know whether we are in
    // speech or not right now, and pretend that speech started at that
    // moment. This is common when live captions is switched on during audio
    // playback.
    int32_t current_streams = GetNumberOfNonChromeOutputStreams();
    if (current_streams > 0) {
      OnNonChromeOutputStarted();
    } else {
      OnNonChromeOutputStopped();
    }
  } else {
    // Speech recognition no longer available, super hard stop. This can happen
    // when a user disables live captions, for instance.
    StopRecognizing();
    // Hard reset by deleting the client and resetting, hard, output_running_ to
    // false.
    client_.reset();
    output_running_ = false;
  }
}

void SystemLiveCaptionService::SpeechRecognitionLanguageChanged(
    const std::string& language) {
  // Set the new language, if we have a client stop recognizing.  SODA will
  // notify us when the new language pack is ready to use in
  // OnSpeechRecognitionAvailability changed.
  source_language_ = language;
  output_running_ = false;
  StopRecognizing();
}

void SystemLiveCaptionService::SpeechRecognitionMaskOffensiveWordsChanged(
    bool mask_offensive_words) {
  // TODO(crbug.com/40924425): pipe through offensive word mask.
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
  // At this point, we can count the number of chars translated for this
  // session.
  if (media::IsLiveTranslateEnabled() && characters_translated_ > 0) {
    base::UmaHistogramCounts10M(
        "Accessibility.LiveTranslate.CharactersTranslatedChromeOS",
        characters_translated_);
    characters_translated_ = 0;
  }
}

void SystemLiveCaptionService::CreateClient() {
  // We must reset to detach everything first, and then reattach.
  client_.reset();
  client_ = std::make_unique<SpeechRecognitionRecognizerClientImpl>(
      weak_ptr_factory_.GetWeakPtr(), profile_,
      media::AudioDeviceDescription::kLoopbackWithoutChromeId,
      media::mojom::SpeechRecognitionOptions::New(
          media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/true,
          prefs::GetLiveCaptionLanguageCode(profile_->GetPrefs()),
          /*is_server_based=*/false,
          media::mojom::RecognizerClientType::kLiveCaption,
          /*skip_continuously_empty_audio=*/true));
}

void SystemLiveCaptionService::OnTranslationCallback(
    const std::string& cached_translation,
    const std::string& original_transcription,
    const std::string& source_language,
    const std::string& target_language,
    bool is_final,
    const std::string& result) {
  std::string formatted_result = result;
  // Don't cache the translation if the source language is an ideographic
  // language but the target language is not. This avoids translate
  // sentence by sentence because the Cloud Translation API does not properly
  // translate ideographic punctuation marks.
  if (!::captions::IsIdeographicLocale(source_language) ||
      ::captions::IsIdeographicLocale(target_language)) {
    if (is_final) {
      translation_cache_.Clear();
    } else {
      translation_cache_.InsertIntoCache(original_transcription, result,
                                         source_language, target_language);
    }
  } else {
    // Append a space after final results when translating from an ideographic
    // to non-ideographic locale. The Speech On-Device API (SODA) automatically
    // prepends a space to recognition events after a final event, but only for
    // non-ideographic locales.
    // TODO(crbug.com/40261536): Consider moving this to the
    // LiveTranslateController.
    if (is_final) {
      formatted_result += " ";
    }
  }

  auto text = base::StrCat({cached_translation, formatted_result});

  if (!controller_->DispatchTranscription(
          &context_, media::SpeechRecognitionResult(text, is_final))) {
    StopRecognizing();
  }
}

void SystemLiveCaptionService::OpenCaptionSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kAudioAndCaptionsSubpagePath);
}

uint32_t SystemLiveCaptionService::GetNumberOfNonChromeOutputStreams() {
  if (num_output_streams_for_testing_.has_value()) {
    return num_output_streams_for_testing_.value();
  }

  return CrasAudioHandler::Get()->NumberOfNonChromeOutputStreams();
}

}  // namespace ash
