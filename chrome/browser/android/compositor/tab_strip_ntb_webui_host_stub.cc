// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NtbWebUiCoordinator_jni.h"

namespace android {

static int64_t JNI_NtbWebUiCoordinator_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& caller,
    int32_t session_id,
    int64_t activity_creation_uptime_ms) {
  return 0;
}

static content::WebContents* JNI_NtbWebUiCoordinator_GetWebContents(
    JNIEnv* env,
    int64_t host_ptr) {
  NOTREACHED();
}

static void JNI_NtbWebUiCoordinator_Resize(JNIEnv* env,
                                           int64_t host_ptr,
                                           int32_t width_px,
                                           int32_t height_px) {
  NOTREACHED();
}

static void JNI_NtbWebUiCoordinator_Destroy(JNIEnv* env, int64_t host_ptr) {
  NOTREACHED();
}

}  // namespace android

DEFINE_JNI(NtbWebUiCoordinator)
