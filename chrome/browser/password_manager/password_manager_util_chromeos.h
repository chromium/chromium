// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_CHROMEOS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_CHROMEOS_H_

#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "components/password_manager/core/browser/reauth_purpose.h"

namespace password_manager_util_chromeos {

// Calls `InSessionAuthDialogController::ShowAuthDialog` to authenticate the
// currently active user using configured auth factors.
// On Lacros, makes a crosapi call to the `mojom::InSessionAuth` interface
// implemented by ash in crosapi::InSessionAuthAsh. This in turn calls
// `InSessionAuthDialogController::ShowAuthDialog` to authenticate the currently
// active user using configured auth factors.
void AuthenticateUser(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback callback);

}  // namespace password_manager_util_chromeos

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_CHROMEOS_H_
