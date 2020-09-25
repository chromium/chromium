// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock_screen_utils.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {
namespace lock_screen_utils {

namespace {

bool SetUserInputMethodImpl(
    const std::string& user_input_method,
    input_method::InputMethodManager::State* ime_state) {
  if (!chromeos::input_method::InputMethodManager::Get()->IsLoginKeyboard(
          user_input_method)) {
    LOG(WARNING) << "SetUserInputMethod: stored user last input method '"
                 << user_input_method
                 << "' is no longer Full Latin Keyboard Language"
                 << " (entry dropped). Use hardware default instead.";
    return false;
  }
  if (!base::Contains(ime_state->GetActiveInputMethodIds(),
                      user_input_method)) {
    if (!ime_state->EnableInputMethod(user_input_method)) {
      DLOG(ERROR) << "SetUserInputMethod: user input method '"
                  << user_input_method
                  << "' is not enabled and enabling failed (ignored!).";
      return false;
    }
  }
  ime_state->ChangeInputMethod(user_input_method, false /* show_message */);

  return true;
}

}  // namespace

void SetUserInputMethod(const AccountId& account_id,
                        input_method::InputMethodManager::State* ime_state,
                        bool honor_device_policy) {
  bool succeed = false;

  const std::string input_method = GetUserLastInputMethod(account_id);

  if (honor_device_policy)
    EnforceDevicePolicyInputMethods(input_method);

  if (!input_method.empty())
    succeed = SetUserInputMethodImpl(input_method, ime_state);

  // This is also a case when last layout is set only for a few local users,
  // thus others need to be switched to default locale.
  // Otherwise they will end up using another user's locale to log in.
  if (!succeed) {
    DVLOG(0) << "SetUserInputMethod: failed to set user layout. Switching to "
                "default.";

    ime_state->SetInputMethodLoginDefault();
  }
}

std::string GetUserLastInputMethod(const AccountId& account_id) {
  if (!account_id.is_valid())
    return std::string();
  std::string input_method;
  if (user_manager::known_user::GetUserLastInputMethod(account_id,
                                                       &input_method)) {
    return input_method;
  }

  // Try profile prefs. For the ephemeral case known_user does not persist the
  // data.
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile && profile->GetPrefs()) {
    input_method = profile->GetPrefs()->GetString(prefs::kLastLoginInputMethod);
    if (!input_method.empty())
      return input_method;
  }

  // Try to use old values.
  PrefService* const local_state = g_browser_process->local_state();
  const base::DictionaryValue* users_last_input_methods =
      local_state->GetDictionary(::prefs::kUsersLastInputMethod);

  if (!users_last_input_methods) {
    DLOG(WARNING) << "GetUserLastInputMethod: no kUsersLastInputMethod";
    return std::string();
  }

  if (!users_last_input_methods->GetStringWithoutPathExpansion(
          account_id.GetUserEmail(), &input_method)) {
    DVLOG(0) << "GetUserLastInputMethod: no input method for this user";
    return std::string();
  }

  return input_method;
}

void EnforceDevicePolicyInputMethods(std::string user_input_method) {
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
  imm->GetActiveIMEState()->SetInputMethodLoginDefault();
}

void SetKeyboardSettings(const AccountId& account_id) {
  bool auto_repeat_enabled = ash::kDefaultKeyAutoRepeatEnabled;
  if (user_manager::known_user::GetBooleanPref(
          account_id, ash::prefs::kXkbAutoRepeatEnabled,
          &auto_repeat_enabled) &&
      !auto_repeat_enabled) {
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetAutoRepeatEnabled(false);
    return;
  }

  int auto_repeat_delay = ash::kDefaultKeyAutoRepeatDelay.InMilliseconds();
  int auto_repeat_interval =
      ash::kDefaultKeyAutoRepeatInterval.InMilliseconds();
  user_manager::known_user::GetIntegerPref(
      account_id, ash::prefs::kXkbAutoRepeatDelay, &auto_repeat_delay);
  user_manager::known_user::GetIntegerPref(
      account_id, ash::prefs::kXkbAutoRepeatInterval, &auto_repeat_interval);
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
