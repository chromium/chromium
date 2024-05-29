// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PasswordManagerLauncher_jni.h"

namespace {

static bool g_override_for_testing_set = false;
static bool g_manage_password_when_passkeys_present_override = false;

}  // namespace

namespace password_manager_launcher {

void ShowPasswordSettings(content::WebContents* web_contents,
                          password_manager::ManagePasswordsReferrer referrer,
                          bool manage_passkeys) {
  Java_PasswordManagerLauncher_showPasswordSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents(),
      static_cast<int>(referrer), manage_passkeys);
}

bool CanManagePasswordsWhenPasskeysPresent(Profile* profile) {
  if (g_override_for_testing_set) {
    return g_manage_password_when_passkeys_present_override;
  }
  return Java_PasswordManagerLauncher_canManagePasswordsWhenPasskeysPresent(
      base::android::AttachCurrentThread(), profile->GetJavaObject());
}

void OverrideManagePasswordWhenPasskeysPresentForTesting(bool can_manage) {
  g_override_for_testing_set = true;
  g_manage_password_when_passkeys_present_override = can_manage;
}

}  // namespace password_manager_launcher
