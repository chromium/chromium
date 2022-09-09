// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_sign_in_helper_bridge_impl.h"

#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerSignInHelperBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

PasswordManagerSignInHelperBridge::~PasswordManagerSignInHelperBridge() =
    default;

void PasswordManagerSignInHelperBridgeImpl::startUpdateAccountCredentialsFlow(
    JNIEnv* env,
    content::WebContents* web_contents) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (window_android == nullptr)
    return;
  Java_PasswordManagerSignInHelperBridge_startUpdateAccountCredentialsFlow(
      env, window_android->GetJavaObject());
}
