// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_

#include "components/password_manager/core/browser/manage_passwords_referrer.h"

namespace content {
class WebContents;
}

class Profile;

namespace password_manager_launcher {

// Opens the password settings page.
void ShowPasswordSettings(content::WebContents* web_contents,
                          password_manager::ManagePasswordsReferrer referrer,
                          bool manage_passkeys);

// Determines whether password management will be available if
// ShowPasswordSettings() is called with |manage_passkeys| set to true.
// This returns false if UPM isn't available for the current user, but the
// Play Services password manager will be shown anyway when passkeys are
// present.
bool CanManagePasswordsWhenPasskeysPresent(Profile* profile);

// Test override to prevent CanManagePasswordsWhenPasskeysPresent from invoking
// JNI.
void OverrideManagePasswordWhenPasskeysPresentForTesting(bool can_manage);

}  // namespace password_manager_launcher

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_
