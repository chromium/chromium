// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/signin_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SigninBridge_jni.h"

using base::android::JavaParamRef;

// static
void SigninBridge::LaunchSigninActivity(
    ui::WindowAndroid* window,
    signin_metrics::AccessPoint access_point) {
  if (window) {
    Java_SigninBridge_launchSigninActivity(base::android::AttachCurrentThread(),
                                           window->GetJavaObject(),
                                           static_cast<int>(access_point));
  }
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
    const std::string& continue_url) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninBridge_openAccountPickerBottomSheet(
      env, tab->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, continue_url));
}
