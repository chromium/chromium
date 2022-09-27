// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_

#include "ash/public/cpp/login_types.h"
#include "base/values.h"
#include "ui/base/ime/ash/input_method_manager.h"

class AccountId;

namespace ash::lock_screen_utils {

// Update current input method in the given IME state to last input method used
// by this user.
void SetUserInputMethod(const AccountId& account_id,
                        input_method::InputMethodManager::State* ime_state,
                        bool honor_device_policy);

// Get user's last input method ID.
std::string GetUserLastInputMethodId(const AccountId& account_id);

// Sets the currently allowed input method ID, including those that are enforced
// by device policy.
void EnforceDevicePolicyInputMethods(std::string user_input_method_id);

// Remove any policy limitations on allowed IMEs.
void StopEnforcingPolicyInputMethods();

// Update the keyboard settings for `account_id`.
void SetKeyboardSettings(const AccountId& account_id);

// Covert a base::Value::List of locale info to a list of ash struct LocaleItem.
std::vector<LocaleItem> FromListValueToLocaleItem(base::Value::List locales);

}  // namespace ash::lock_screen_utils

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_
