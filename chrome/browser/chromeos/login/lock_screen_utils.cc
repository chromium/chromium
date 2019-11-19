// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock_screen_utils.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {
namespace lock_screen_utils {

void SetUserInputMethod(const std::string& username,
                        input_method::InputMethodManager::State* ime_state) {
  bool succeed = false;

  const std::string input_method = GetUserLastInputMethod(username);

  EnforcePolicyInputMethods(input_method);

  if (!input_method.empty())
    succeed = SetUserInputMethodImpl(username, input_method, ime_state);

  // This is also a case when last layout is set only for a few local users,
  // thus others need to be switched to default locale.
  // Otherwise they will end up using another user's locale to log in.
  if (!succeed) {
    DVLOG(0) << "SetUserInputMethod: failed to set user layout. Switching to "
                "default.";

    ime_state->SetInputMethodLoginDefault();
  }
}

std::string GetUserLastInputMethod(const std::string& username) {
  PrefService* const local_state = g_browser_process->local_state();
  const base::DictionaryValue* users_last_input_methods =
      local_state->GetDictionary(prefs::kUsersLastInputMethod);

  if (!users_last_input_methods) {
    DLOG(WARNING) << "GetUserLastInputMethod: no kUsersLastInputMethod";
    return std::string();
  }

  std::string input_method;

  if (!users_last_input_methods->GetStringWithoutPathExpansion(username,
                                                               &input_method)) {
    DVLOG(0) << "GetUserLastInputMethod: no input method for this user";
    return std::string();
  }

  return input_method;
}

bool SetUserInputMethodImpl(
    const std::string& username,
    const std::string& user_input_method,
    input_method::InputMethodManager::State* ime_state) {
  if (!chromeos::input_method::InputMethodManager::Get()->IsLoginKeyboard(
          user_input_method)) {
    LOG(WARNING) << "SetUserInputMethod: stored user last input method '"
                 << user_input_method
                 << "' is no longer Full Latin Keyboard Language"
                 << " (entry dropped). Use hardware default instead.";

    PrefService* const local_state = g_browser_process->local_state();
    DictionaryPrefUpdate updater(local_state, prefs::kUsersLastInputMethod);

    base::DictionaryValue* const users_last_input_methods = updater.Get();
    if (users_last_input_methods)
      users_last_input_methods->SetKey(username, base::Value(""));
    return false;
  }
  if (!base::Contains(ime_state->GetActiveInputMethodIds(),
                      user_input_method)) {
    if (!ime_state->EnableInputMethod(user_input_method)) {
      DLOG(ERROR) << "SetUserInputMethod: user input method '"
                  << user_input_method
                  << "' is not enabled and enabling failed (ignored!).";
    }
  }
  ime_state->ChangeInputMethod(user_input_method, false /* show_message */);

  return true;
}

void EnforcePolicyInputMethods(std::string user_input_method) {
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  const base::ListValue* login_screen_input_methods = nullptr;
  if (!cros_settings->GetList(chromeos::kDeviceLoginScreenInputMethods,
                              &login_screen_input_methods) ||
      login_screen_input_methods->empty()) {
    StopEnforcingPolicyInputMethods();
    return;
  }

  std::vector<std::string> allowed_input_methods;

  // Add user's input method first so it is pre-selected.
  if (!user_input_method.empty()) {
    allowed_input_methods.push_back(user_input_method);
  }

  std::string input_method;
  for (const auto& input_method_entry : *login_screen_input_methods) {
    if (input_method_entry.GetAsString(&input_method))
      allowed_input_methods.push_back(input_method);
  }
  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();
  imm->GetActiveIMEState()->SetAllowedInputMethods(allowed_input_methods, true);
  if (ImeControllerClient::Get())  // Can be null in tests.
    ImeControllerClient::Get()->SetImesManagedByPolicy(true);
}

void StopEnforcingPolicyInputMethods() {
  // Empty means all input methods are allowed
  std::vector<std::string> allowed_input_methods;
  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();
  imm->GetActiveIMEState()->SetAllowedInputMethods(allowed_input_methods, true);
  if (ImeControllerClient::Get())  // Can be null in tests.
    ImeControllerClient::Get()->SetImesManagedByPolicy(false);
}

void SetKeyboardSettings(const AccountId& account_id) {
  bool auto_repeat_enabled = language_prefs::kXkbAutoRepeatEnabled;
  if (user_manager::known_user::GetBooleanPref(
          account_id, prefs::kLanguageXkbAutoRepeatEnabled,
          &auto_repeat_enabled) &&
      !auto_repeat_enabled) {
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetAutoRepeatEnabled(false);
    return;
  }

  int auto_repeat_delay = language_prefs::kXkbAutoRepeatDelayInMs;
  int auto_repeat_interval = language_prefs::kXkbAutoRepeatIntervalInMs;
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kLanguageXkbAutoRepeatDelay, &auto_repeat_delay);
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kLanguageXkbAutoRepeatInterval, &auto_repeat_interval);
  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = auto_repeat_delay;
  rate.repeat_interval_in_ms = auto_repeat_interval;
  input_method::InputMethodManager::Get()
      ->GetImeKeyboard()
      ->SetAutoRepeatEnabled(true);
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetAutoRepeatRate(
      rate);
}

std::vector<ash::LocaleItem> FromListValueToLocaleItem(
    std::unique_ptr<base::ListValue> locales) {
  std::vector<ash::LocaleItem> result;
  for (const auto& locale : *locales) {
    const base::DictionaryValue* dictionary;
    if (!locale.GetAsDictionary(&dictionary))
      continue;

    ash::LocaleItem locale_item;
    dictionary->GetString("value", &locale_item.language_code);
    dictionary->GetString("title", &locale_item.title);
    std::string group_name;
    dictionary->GetString("optionGroupName", &group_name);
    if (!group_name.empty())
      locale_item.group_name = group_name;
    result.push_back(std::move(locale_item));
  }
  return result;
}

}  // namespace lock_screen_utils
}  // namespace chromeos
