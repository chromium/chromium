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

namespace password_manager_launcher {

void ShowPasswordSettings(content::WebContents* web_contents,
                          password_manager::ManagePasswordsReferrer referrer,
                          bool manage_passkeys) {
  Java_PasswordManagerLauncher_showPasswordSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents(),
      static_cast<int>(referrer), manage_passkeys);
}

}  // namespace password_manager_launcher

DEFINE_JNI(PasswordManagerLauncher)
