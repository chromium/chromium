// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_

#include "components/password_manager/core/browser/reauth_purpose.h"

namespace password_manager_util_mac {

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated.
bool AuthenticateUser(password_manager::ReauthPurpose purpose);

}  // namespace password_manager_util_mac

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_MAC_H_
