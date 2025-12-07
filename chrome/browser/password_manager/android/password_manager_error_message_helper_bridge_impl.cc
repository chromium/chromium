// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PasswordManagerErrorMessageHelperBridge_jni.h"

PasswordManagerErrorMessageHelperBridge::
    ~PasswordManagerErrorMessageHelperBridge() = default;

void PasswordManagerErrorMessageHelperBridgeImpl::
    StartUpdateAccountCredentialsFlow(content::WebContents* web_contents) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (window_android == nullptr) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  Java_PasswordManagerErrorMessageHelperBridge_startUpdateAccountCredentialsFlow(
      base::android::AttachCurrentThread(), window_android->GetJavaObject(),
      profile->GetJavaObject());
}

void PasswordManagerErrorMessageHelperBridgeImpl::
    StartTrustedVaultKeyRetrievalFlow(
        content::WebContents* web_contents,
        trusted_vault::TrustedVaultUserActionTriggerForUMA
            user_action_trigger) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (window_android == nullptr) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  Java_PasswordManagerErrorMessageHelperBridge_startTrustedVaultKeyRetrievalFlow(
      base::android::AttachCurrentThread(), window_android->GetJavaObject(),
      profile->GetJavaObject(), static_cast<jint>(user_action_trigger));
}

bool PasswordManagerErrorMessageHelperBridgeImpl::ShouldShowSignInErrorUI(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return Java_PasswordManagerErrorMessageHelperBridge_shouldShowSignInErrorUi(
      base::android::AttachCurrentThread(), profile->GetJavaObject());
}

void PasswordManagerErrorMessageHelperBridgeImpl::SaveErrorUIShownTimestamp(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Java_PasswordManagerErrorMessageHelperBridge_saveErrorUiShownTimestamp(
      base::android::AttachCurrentThread(), profile->GetJavaObject());
}

DEFINE_JNI(PasswordManagerErrorMessageHelperBridge)
