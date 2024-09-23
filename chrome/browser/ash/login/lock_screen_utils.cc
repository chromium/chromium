// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock_screen_utils.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/input_method/ime_controller_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "ui/base/ime/ash/ime_keyboard.h"

namespace ash::lock_screen_utils {
namespace {

bool SetUserInputMethodImpl(
    const std::string& user_input_method_id,
    input_method::InputMethodManager::State* ime_state) {
  if (!input_method::InputMethodManager::Get()->IsLoginKeyboard(
          user_input_method_id)) {
    LOG(WARNING) << "SetUserInputMethod: stored user last input method '"
                 << user_input_method_id
                 << "' is no longer Full Latin Keyboard Language"
                 << " (entry dropped). Use hardware default instead.";
    return false;
  }
  if (!base::Contains(ime_state->GetEnabledInputMethodIds(),
                      user_input_method_id)) {
    if (!ime_state->EnableInputMethod(user_input_method_id)) {
      DLOG(ERROR) << "SetUserInputMethod: user input method '"
                  << user_input_method_id
                  << "' is not enabled and enabling failed (ignored!).";
      return false;
    }
  }
  ime_state->ChangeInputMethod(user_input_method_id, false /* show_message */);

  return true;
}

}  // namespace

void SetUserInputMethod(const AccountId& account_id,
                        input_method::InputMethodManager::State* ime_state,
                        bool honor_device_policy) {
  bool succeed = false;

  const std::string input_method_id = GetUserLastInputMethodId(account_id);

  if (honor_device_policy)
    EnforceDevicePolicyInputMethods(input_method_id);

  if (!input_method_id.empty())
    succeed = SetUserInputMethodImpl(input_method_id, ime_state);

  // This is also a case when last layout is set only for a few local users,
  // thus others need to be switched to default locale.
  // Otherwise they will end up using another user's locale to log in.
  if (!succeed) {
    DVLOG(0) << "SetUserInputMethod: failed to set user layout. Switching to "
                "default.";

    ime_state->SetInputMethodLoginDefault();
  }
}

std::string GetUserLastInputMethodId(const AccountId& account_id) {
  if (!account_id.is_valid())
    return std::string();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (const std::string* input_method_id =
          known_user.GetUserLastInputMethodId(account_id)) {
    return *input_method_id;
  }

  // Try profile prefs. For the ephemeral case known_user does not persist the
  // data.
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile && profile->GetPrefs()) {
    std::string input_method_id =
        profile->GetPrefs()->GetString(prefs::kLastLoginInputMethod);
    if (!input_method_id.empty())
      return input_method_id;
  }

  return std::string();
}

void EnforceDevicePolicyInputMethods(std::string user_input_method_id) {
  auto* cros_settings = CrosSettings::Get();
  const base::Value::List* login_screen_input_methods = nullptr;
  if (!cros_settings->GetList(kDeviceLoginScreenInputMethods,
                              &login_screen_input_methods) ||
      login_screen_input_methods->empty()) {
    StopEnforcingPolicyInputMethods();
    return;
  }

  std::vector<std::string> allowed_input_method_ids;

  // Add user's input method first so it is pre-selected.
  if (!user_input_method_id.empty()) {
    allowed_input_method_ids.push_back(user_input_method_id);
  }

  for (const auto& input_method_entry : *login_screen_input_methods) {
    if (input_method_entry.is_string())
      allowed_input_method_ids.push_back(input_method_entry.GetString());
  }
  auto imm_state = input_method::InputMethodManager::Get()->GetActiveIMEState();
  bool managed_by_policy =
      imm_state->SetAllowedInputMethods(allowed_input_method_ids);
  if (managed_by_policy) {
    imm_state->ReplaceEnabledInputMethods(
        imm_state->GetAllowedInputMethodIds());
  }
  if (ImeControllerClientImpl::Get())  // Can be null in tests.
    ImeControllerClientImpl::Get()->SetImesManagedByPolicy(true);
}

void StopEnforcingPolicyInputMethods() {
  // Empty means all input methods are allowed
  auto imm_state = input_method::InputMethodManager::Get()->GetActiveIMEState();
  imm_state->SetAllowedInputMethods(std::vector<std::string>());
  if (ImeControllerClientImpl::Get())  // Can be null in tests.
    ImeControllerClientImpl::Get()->SetImesManagedByPolicy(false);
  imm_state->SetInputMethodLoginDefault();
}

void SetKeyboardSettings(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (std::optional<bool> auto_repeat_enabled =
          known_user.FindBoolPath(account_id, prefs::kXkbAutoRepeatEnabled);
      auto_repeat_enabled.has_value()) {
    if (!auto_repeat_enabled.value()) {
      input_method::InputMethodManager::Get()
          ->GetImeKeyboard()
          ->SetAutoRepeatEnabled(false);
      return;
    }
  }

  input_method::AutoRepeatRate rate{
      .initial_delay = kDefaultKeyAutoRepeatDelay,
      .repeat_interval = kDefaultKeyAutoRepeatInterval,
  };

  if (auto delay =
          known_user.FindIntPath(account_id, prefs::kXkbAutoRepeatDelay);
      delay) {
    rate.initial_delay = base::Milliseconds(delay.value());
  }

  if (auto interval =
          known_user.FindIntPath(account_id, prefs::kXkbAutoRepeatInterval);
      interval) {
    rate.repeat_interval = base::Milliseconds(interval.value());
  }

  input_method::InputMethodManager::Get()
      ->GetImeKeyboard()
      ->SetAutoRepeatEnabled(true);
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetAutoRepeatRate(
      rate);
}

std::vector<LocaleItem> FromListValueToLocaleItem(base::Value::List locales) {
  std::vector<LocaleItem> result;
  for (const auto& locale : locales) {
    if (!locale.is_dict())
      continue;
    const auto& dictionary = locale.GetDict();
    LocaleItem locale_item;
    if (const std::string* language_code = dictionary.FindString("value")) {
      locale_item.language_code = *language_code;
    }
    if (const std::string* title = dictionary.FindString("title")) {
      locale_item.title = *title;
    }
    if (const std::string* group_name =
            dictionary.FindString("optionGroupName")) {
      if (!group_name->empty())
        locale_item.group_name = *group_name;
    }
    result.push_back(std::move(locale_item));
  }
  return result;
}

}  // namespace ash::lock_screen_utils
