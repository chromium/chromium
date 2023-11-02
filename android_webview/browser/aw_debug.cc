// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process.h"
#include "android_webview/browser_jni_headers/AwDebug_jni.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "components/crash/core/common/crash_key.h"

using content::RenderProcessHost;

namespace android_webview {

static void JNI_AwDebug_SetSupportLibraryWebkitVersionCrashKey(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& version) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      crash_keys::kSupportLibraryWebkitVersion);
  crash_key.Set(ConvertJavaStringToUTF8(env, version));
}

}  // namespace android_webview
