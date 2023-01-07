// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"

#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerErrorMessageHelperBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

PasswordManagerErrorMessageHelperBridge::
    ~PasswordManagerErrorMessageHelperBridge() = default;

void PasswordManagerErrorMessageHelperBridgeImpl::
    StartUpdateAccountCredentialsFlow(content::WebContents* web_contents) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (window_android == nullptr)
    return;
  Java_PasswordManagerErrorMessageHelperBridge_startUpdateAccountCredentialsFlow(
      base::android::AttachCurrentThread(), window_android->GetJavaObject());
}

bool PasswordManagerErrorMessageHelperBridgeImpl::ShouldShowErrorUI() {
  return Java_PasswordManagerErrorMessageHelperBridge_shouldShowErrorUi(
      base::android::AttachCurrentThread());
}

void PasswordManagerErrorMessageHelperBridgeImpl::SaveErrorUIShownTimestamp() {
  Java_PasswordManagerErrorMessageHelperBridge_saveErrorUiShownTimestamp(
      base::android::AttachCurrentThread());
}
