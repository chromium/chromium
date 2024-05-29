// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChildAccountService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::RunBooleanCallbackAndroid;
using base::android::ScopedJavaGlobalRef;

void ReauthenticateChildAccount(
    content::WebContents* web_contents,
    const std::string& email,
    const base::RepeatingCallback<void()>& on_failure_callback) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android) {
    // The native view may not be available on shutdown (crbug.com/1468955).
    on_failure_callback.Run();
    return;
  }

  // Make a copy of the callback which can be passed as a pointer through
  // to Java.
  auto callback_copy =
      std::make_unique<base::RepeatingCallback<void()>>(on_failure_callback);

  JNIEnv* env = AttachCurrentThread();
  Java_ChildAccountService_reauthenticateChildAccount(
      env, window_android->GetJavaObject(), email,
      reinterpret_cast<jlong>(callback_copy.release()));
}

void JNI_ChildAccountService_OnReauthenticationFailed(JNIEnv* env,
                                                      jlong jcallbackPtr) {
  // Cast the pointer value back to a Callback and take ownership of it.
  std::unique_ptr<base::RepeatingCallback<void()>> callback(
      reinterpret_cast<base::RepeatingCallback<void()>*>(jcallbackPtr));

  callback->Run();
}
