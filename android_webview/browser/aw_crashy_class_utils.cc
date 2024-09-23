// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/compiler_specific.h"
#include "base/logging.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwCrashyClassUtils_jni.h"

namespace android_webview {

// Do not inline this function. This is meant to simulate a crash like we would
// see in a real incident. Real crashes will generally have at least 1
// non-inlined stack frame, so we force this to happen in this case using both
// the NOINLINE and NOOPT annotations (for some reason, NOINLINE is not enough).
NOINLINE NOOPT void ThisFunctionWillCrash() {
  LOG(FATAL) << "WebView Forced Native Crash for WebView Browser Process";
}

static void JNI_AwCrashyClassUtils_CrashInNative(JNIEnv* env) {
  ThisFunctionWillCrash();
}

}  // namespace android_webview
