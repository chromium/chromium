// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include "ash/components/audio/sounds.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
    : composition_(std::make_unique<ui::CompositionText>()), profile_(profile) {
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

  speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
      GetUserLanguage(profile_));
  speech_recognizer_->Start();
  return true;
}

void Dictation::OnSpeechResult(
    const base::string16& query,
    bool is_final,
    base::Optional<std::vector<base::TimeDelta>> word_offsets) {
  composition_->text = query;

  if (!is_final) {
    // If ChromeVox is enabled, we don't want to show intermediate results
    if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled())
      return;

    ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
    if (input_context)
      input_context->UpdateCompositionText(*composition_, 0, true);
    return;
  }
  DictationOff();
}

void Dictation::OnSpeechSoundLevelChanged(int16_t level) {}

void Dictation::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_RECOGNIZING)
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationStart));
  else if (new_state == SPEECH_RECOGNIZER_READY)
    // This state is only reached when nothing has been said for a fixed time.
    // In this case, the expected behavior is for dictation to terminate.
    DictationOff();
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
  if (!speech_recognizer_)
    return;

  if (!composition_->text.empty()) {
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationEnd));

    ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
    if (input_context)
      input_context->CommitText(
          base::UTF16ToUTF8(composition_->text),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

    composition_->text = base::string16();
  } else {
    audio::SoundsManager::Get()->Play(
        static_cast<int>(Sound::kDictationCancel));
  }

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleDictation, false /* enabled */);
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
  speech_recognizer_.reset();
}

}  // namespace ash
