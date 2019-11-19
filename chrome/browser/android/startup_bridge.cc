// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/startup_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/android/chrome_jni_headers/NativeStartupBridge_jni.h"
#include "chrome/browser/browser_process.h"

namespace android_startup {

void LoadFullBrowser() {
  if (g_browser_process)
    return;
  UMA_HISTOGRAM_BOOLEAN("Android.NativeStartupBridge.LoadFullBrowser",
                        true /*requested*/);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NativeStartupBridge_loadFullBrowser(env);
}

}  // namespace android_startup
