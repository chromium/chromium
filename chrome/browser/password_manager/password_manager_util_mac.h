// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_

#include <string>

#include "components/password_manager/core/browser/reauth_purpose.h"

namespace password_manager_util_mac {

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated.
bool AuthenticateUser(std::u16string prompt_string);

// Returns message that will appear in the login prompt for old authentication
// API that predates the biometric authentication API.
std::u16string GetMessageForNonBiometricLoginPrompt(
    password_manager::ReauthPurpose purpose);

// Returns message that will appear in the login prompt when biometric
// authentication is used.
std::u16string GetMessageForBiometricLoginPrompt(
    password_manager::ReauthPurpose purpose);

}  // namespace password_manager_util_mac

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_
