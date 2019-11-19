// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/ChildAccountService_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::RunBooleanCallbackAndroid;
using base::android::ScopedJavaGlobalRef;

void JNI_ChildAccountService_ListenForChildStatusReceived(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ChildAccountService* service = ChildAccountServiceFactory::GetForProfile(
      profile_manager->GetLastUsedProfile());
  service->AddChildStatusReceivedCallback(
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(callback), true));
}

void ReauthenticateChildAccount(content::WebContents* web_contents,
                                const std::string& email,
                                const base::Callback<void(bool)>& callback) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();

  // Make a copy of the callback which can be passed as a pointer through
  // to Java.
  auto callback_copy = std::make_unique<base::Callback<void(bool)>>(callback);

  JNIEnv* env = AttachCurrentThread();
  Java_ChildAccountService_reauthenticateChildAccount(
      env, window_android->GetJavaObject(), ConvertUTF8ToJavaString(env, email),
      reinterpret_cast<jlong>(callback_copy.release()));
}

void JNI_ChildAccountService_OnReauthenticationResult(
    JNIEnv* env,
    jlong jcallbackPtr,
    jboolean result) {
  // Cast the pointer value back to a Callback and take ownership of it.
  std::unique_ptr<base::Callback<void(bool)>> callback(
      reinterpret_cast<base::Callback<void(bool)>*>(jcallbackPtr));

  callback->Run(result);
}
