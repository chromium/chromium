// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/dictation_chromeos.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"

namespace chromeos {

namespace {

const char kDefaultProfileLanguage[] = "en-US";

std::string GetUserLanguage(Profile* profile) {
  // Convert from the ID used in the pref to a language identifier.
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(
      profile->GetPrefs()->GetString(prefs::kLanguageCurrentInputMethod));
  std::vector<std::string> languages;
  input_method::InputMethodManager::Get()
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

DictationChromeos::DictationChromeos(Profile* profile)
    : composition_(std::make_unique<ui::CompositionText>()), profile_(profile) {
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->AddObserver(this);
}

DictationChromeos::~DictationChromeos() {
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->RemoveObserver(this);
}

bool DictationChromeos::OnToggleDictation() {
  if (speech_recognizer_) {
    DictationOff();
    return false;
  }

  speech_recognizer_ = std::make_unique<SpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
      GetUserLanguage(profile_));
  speech_recognizer_->Start(nullptr /* preamble */);
  return true;
}

void DictationChromeos::OnSpeechResult(const base::string16& query,
                                       bool is_final) {
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

void DictationChromeos::OnSpeechSoundLevelChanged(int16_t level) {}

void DictationChromeos::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_RECOGNIZING)
    audio::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_START);
  else if (new_state == SPEECH_RECOGNIZER_READY)
    // This state is only reached when nothing has been said for a fixed time.
    // In this case, the expected behavior is for dictation to terminate.
    DictationOff();
}

void DictationChromeos::GetSpeechAuthParameters(std::string* auth_scope,
                                                std::string* auth_token) {}

void DictationChromeos::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!client)
    return;

  if (client->GetFocusReason() ==
      ui::TextInputClient::FocusReason::FOCUS_REASON_NONE)
    return;

  DictationOff();
}

void DictationChromeos::DictationOff() {
  if (!speech_recognizer_)
    return;

  if (!composition_->text.empty()) {
    audio::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_END);

    ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
    if (input_context)
      input_context->CommitText(base::UTF16ToUTF8(composition_->text));

    composition_->text = base::string16();
  } else {
    audio::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_CANCEL);
  }

  chromeos::AccessibilityStatusEventDetails details(
      chromeos::AccessibilityNotificationType::ACCESSIBILITY_TOGGLE_DICTATION,
      false /* enabled */);
  chromeos::AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(
      details);
  speech_recognizer_.reset();
}

}  // namespace chromeos
