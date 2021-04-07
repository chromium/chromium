// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include "ash/components/audio/sounds.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/composition_text.h"

namespace ash {
namespace {

const char kDefaultProfileLanguage[] = "en-US";

std::string GetUserLanguage(Profile* profile) {
  // Convert from the ID used in the pref to a language identifier.
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(
      profile->GetPrefs()->GetString(prefs::kLanguageCurrentInputMethod));
  std::vector<std::string> languages;
  chromeos::input_method::InputMethodManager::Get()
      ->GetInputMethodUtil()
      ->GetLanguageCodesFromInputMethodIds(input_method_ids, &languages);

  std::string user_language;
  if (!languages.empty())
    user_language = languages[0];

  // If we don't find a language, fall back to using the locale.
  if (user_language.empty())
    user_language =
        profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);

  return user_language.empty() ? kDefaultProfileLanguage : user_language;
}

// Returns the current input context. This may change during the session, even
// if the IME engine does not change, because remote mojo applications have
// their own instance of InputMethodChromeOS. See comment on InputMethodBridge.
ui::IMEInputContextHandlerInterface* GetInputContext() {
  return ui::IMEBridge::Get()->GetInputContextHandler();
}

}  // namespace

Dictation::Dictation(Profile* profile)
    : current_state_(SPEECH_RECOGNIZER_OFF),
      composition_(std::make_unique<ui::CompositionText>()),
      profile_(profile) {
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->AddObserver(this);
}

Dictation::~Dictation() {
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->RemoveObserver(this);
}

bool Dictation::OnToggleDictation() {
  if (speech_recognizer_) {
    DictationOff();
    return false;
  }
  has_committed_text_ = false;
  std::string language = GetUserLanguage(profile_);
  if (switches::IsExperimentalAccessibilityDictationOfflineEnabled() &&
      OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(language)) {
    // On-device recognition is behind a flag and then only available if
    // SODA is installed on-device.
    speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
        weak_ptr_factory_.GetWeakPtr(), profile_, language);
    base::UmaHistogramBoolean("Accessibility.CrosDictation.UsedOnDeviceSpeech",
                              true);
  } else {
    speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
        weak_ptr_factory_.GetWeakPtr(),
        content::BrowserContext::GetDefaultStoragePartition(profile_)
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        language);
    base::UmaHistogramBoolean("Accessibility.CrosDictation.UsedOnDeviceSpeech",
                              false);
  }
  return true;
}

void Dictation::OnSpeechResult(
    const std::u16string& transcription,
    bool is_final,
    const base::Optional<SpeechRecognizerDelegate::TranscriptTiming>&
        word_offsets) {
  // If the first character of text isn't a space, add a space before it.
  // NetworkSpeechRecognizer adds the preceding space but
  // OnDeviceSpeechRecognizer does not. This is also done in
  // CaptionBubbleModel::CommitPartialText.
  // TODO(crbug.com/1055150): This feature is launching for English first.
  // Make sure spacing is correct for all languages.
  if (has_committed_text_ && transcription.size() > 0 &&
      transcription.compare(0, 1, u" ") != 0) {
    composition_->text = u" " + transcription;
  } else {
    composition_->text = transcription;
  }

  if (!is_final) {
    // If ChromeVox is enabled, we don't want to show intermediate results
    if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled())
      return;

    ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
    if (input_context)
      input_context->UpdateCompositionText(*composition_, 0, true);
    return;
  }
  if (switches::IsExperimentalAccessibilityDictationListeningEnabled()) {
    CommitCurrentText();
  } else {
    // Turn off after finalized speech.
    DictationOff();
  }
}

void Dictation::OnSpeechSoundLevelChanged(int16_t level) {}

void Dictation::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  SpeechRecognizerStatus next_state = new_state;
  if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    // If we are starting to listen to audio, play a tone for the user.
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationStart));
  } else if (new_state == SPEECH_RECOGNIZER_ERROR) {
    DictationOff();
    next_state = SPEECH_RECOGNIZER_OFF;
  } else if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer was initialized after being created, and
      // is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // This state is only reached when nothing has been said for a fixed time.
      // In this case, the expected behavior is for dictation to terminate.
      DictationOff();
      next_state = SPEECH_RECOGNIZER_OFF;
    }
  }
  current_state_ = next_state;
}

void Dictation::OnTextInputStateChanged(const ui::TextInputClient* client) {
  if (!client)
    return;

  if (client->GetFocusReason() ==
      ui::TextInputClient::FocusReason::FOCUS_REASON_NONE)
    return;

  DictationOff();
}

void Dictation::DictationOff() {
  current_state_ = SPEECH_RECOGNIZER_OFF;
  if (!speech_recognizer_)
    return;

  CommitCurrentText();
  if (!composition_->text.empty()) {
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationEnd));
  } else {
    audio::SoundsManager::Get()->Play(
        static_cast<int>(Sound::kDictationCancel));
  }

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleDictation, false /* enabled */);
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
  speech_recognizer_.reset();
}

void Dictation::CommitCurrentText() {
  if (composition_->text.empty()) {
    return;
  }
  has_committed_text_ = true;
  ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
  if (input_context) {
    input_context->CommitText(
        composition_->text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  composition_->text = std::u16string();
}

}  // namespace ash
