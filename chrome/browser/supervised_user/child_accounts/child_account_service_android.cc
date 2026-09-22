// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/android/chrome_jni_headers/ChildAccountService_jni.h"

using base::android::AttachCurrentThread;

void ReauthenticateChildAccount(
    content::WebContents* web_contents,
    const CoreAccountInfo& accountInfo,
    const base::RepeatingCallback<void()>& on_failure_callback) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  CHECK(window_android)
      << "See SupervisedUserGoogleAuthNavigationThrottle to confirm that this "
         "call is never called with empty `window_android`";

  // Make a copy of the callback which can be passed as a pointer through
  // to Java.
  auto callback_copy =
      std::make_unique<base::RepeatingCallback<void()>>(on_failure_callback);

  JNIEnv* env = AttachCurrentThread();
  Java_ChildAccountService_reauthenticateChildAccount(
      env, window_android, accountInfo,
      reinterpret_cast<int64_t>(callback_copy.release()));
}

static void JNI_ChildAccountService_OnReauthenticationFailed(
    int64_t jcallbackPtr) {
  // Cast the pointer value back to a Callback and take ownership of it.
  std::unique_ptr<base::RepeatingCallback<void()>> callback(
      reinterpret_cast<base::RepeatingCallback<void()>*>(jcallbackPtr));

  callback->Run();
}

DEFINE_JNI(ChildAccountService)
