// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SIGN_IN_HELPER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SIGN_IN_HELPER_BRIDGE_IMPL_H_

#include <jni.h>

#include "chrome/browser/password_manager/android/password_manager_sign_in_helper_bridge.h"
#include "content/public/browser/web_contents.h"

class PasswordManagerSignInHelperBridgeImpl
    : public PasswordManagerSignInHelperBridge {
 public:
  void startUpdateAccountCredentialsFlow(
      JNIEnv* env,
      content::WebContents* web_contents) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SIGN_IN_HELPER_BRIDGE_IMPL_H_
