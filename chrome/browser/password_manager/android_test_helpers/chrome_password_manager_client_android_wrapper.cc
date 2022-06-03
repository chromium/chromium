// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android_test_helpers/jni_headers/PasswordManagerClientBridgeForTesting_jni.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {

// static
void JNI_PasswordManagerClientBridgeForTesting_SetLeakDialogWasShownForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean j_value) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  return ChromePasswordManagerClient::FromWebContents(web_contents)
      ->SetCredentialLeakDialogWasShownForTesting(j_value);
}

}  // namespace password_manager
