// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "android_webview/browser_jni_headers/AwCrashyClassUtils_jni.h"
#include "base/check.h"

namespace android_webview {

static void JNI_AwCrashyClassUtils_CrashInNative(JNIEnv* env) {
  CHECK(false) << "WebView Forced Native Crash for WebView Browser Process";
}

}  // namespace android_webview
