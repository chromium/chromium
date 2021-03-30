// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_

#include "ash/public/cpp/login_types.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

class AccountId;

namespace base {
class ListValue;
}

namespace chromeos {
namespace lock_screen_utils {

// Update current input method (namely keyboard layout) in the given IME state
// to last input method used by this user.
void SetUserInputMethod(const AccountId& account_id,
                        input_method::InputMethodManager::State* ime_state,
                        bool honor_device_policy);

// Get user's last input method.
std::string GetUserLastInputMethod(const AccountId& account_id);

// Sets the currently allowed input method, including those that are enforced
// by device policy.
void EnforceDevicePolicyInputMethods(std::string user_input_method);

// Remove any policy limitations on allowed IMEs.
void StopEnforcingPolicyInputMethods();

// Update the keyboard settings for `account_id`.
void SetKeyboardSettings(const AccountId& account_id);

// Covert a ListValue of locale info to a list of ash struct LocaleItem.
std::vector<ash::LocaleItem> FromListValueToLocaleItem(
    std::unique_ptr<base::ListValue> locales);

}  // namespace lock_screen_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_UTILS_H_
