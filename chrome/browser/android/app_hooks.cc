// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/app_hooks.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/AppHooks_jni.h"

using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

AppHooks::AppHooks() = default;

AppHooks::~AppHooks() = default;

ScopedJavaLocalRef<jobject> AppHooks::GetOfflinePagesCCTRequestDoneCallback() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> app_hooks_obj = Java_AppHooks_get(env);

  return Java_AppHooks_getOfflinePagesCCTRequestDoneCallback(env,
                                                             app_hooks_obj);
}

}  // namespace android
}  // namespace chrome
