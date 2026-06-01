// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/signin_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/signin_deep_link_payload_conversions.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SigninBridge_jni.h"

using base::android::JavaRef;

void SigninBridge::StartAddAccountFlow(
    TabAndroid* tab,
    const std::string& prefilled_email,
    const GURL& continue_url,
    bool is_web_signin,
    signin_metrics::AccessPoint access_point) {
  if (!tab) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_startAddAccountFlow(
      env, tab->GetJavaObject(), prefilled_email, continue_url, is_web_signin,
      static_cast<int32_t>(access_point));
}

void SigninBridge::OpenAccountManagementScreen(
    ui::WindowAndroid* window,
    signin::GAIAServiceType service_type) {
  DCHECK(window);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_openAccountManagementScreen(env, window->GetJavaObject(),
                                                static_cast<int>(service_type));
}

void SigninBridge::OpenAccountPickerBottomSheet(
    content::WebContents* web_contents,
    const GURL& continue_url,
    const std::optional<CoreAccountId>& account_id,
    bool is_web_signin,
    signin_metrics::AccessPoint access_point) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_openAccountPickerBottomSheet(
      env, tab->GetJavaObject(), continue_url, account_id, is_web_signin,
      static_cast<int32_t>(access_point));
}

void SigninBridge::StartUpdateCredentialsFlow(TabAndroid* tab,
                                              const GURL& continue_url,
                                              const CoreAccountId& account_id) {
  if (!tab) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_startUpdateCredentialsFlow(env, tab->GetJavaObject(),
                                               continue_url, account_id);
}

void SigninBridge::WaitForCookiesAndRedirect(TabAndroid* tab,
                                             const GURL& continue_url,
                                             const CoreAccountId& account_id) {
  if (!tab) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_waitForCookiesAndRedirect(env, tab->GetJavaObject(),
                                              continue_url, account_id);
}

void SigninBridge::StartSigninDeepLinkFlow(
    ui::WindowAndroid* window,
    Profile* profile,
    const signin::SigninDeepLinkPayload& payload) {
  if (!window || !profile) {
    return;
  }
  CHECK(profile->IsRegularProfile());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_startSigninDeepLinkFlow(env, window->GetJavaObject(),
                                            profile->GetJavaObject(), payload);
}

DEFINE_JNI(SigninBridge)
