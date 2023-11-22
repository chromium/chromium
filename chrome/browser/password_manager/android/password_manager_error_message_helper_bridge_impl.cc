// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"

#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerErrorMessageHelperBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
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
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  Java_PasswordManagerErrorMessageHelperBridge_startUpdateAccountCredentialsFlow(
      base::android::AttachCurrentThread(), window_android->GetJavaObject(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

bool PasswordManagerErrorMessageHelperBridgeImpl::ShouldShowErrorUI(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return Java_PasswordManagerErrorMessageHelperBridge_shouldShowErrorUi(
      base::android::AttachCurrentThread(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

void PasswordManagerErrorMessageHelperBridgeImpl::SaveErrorUIShownTimestamp(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Java_PasswordManagerErrorMessageHelperBridge_saveErrorUiShownTimestamp(
      base::android::AttachCurrentThread(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}
