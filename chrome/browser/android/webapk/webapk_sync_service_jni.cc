// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkSyncService_jni.h"

using base::android::JavaRef;

namespace webapk {

// TODO(crbug.com/400662034): Clean up these JNI calls and the corresponding
// Java code.

static void JNI_WebApkSyncService_OnWebApkUsed(
    JNIEnv* env,
    const JavaRef<jbyteArray>& java_webapk_specifics,
    bool is_install) {}

static void JNI_WebApkSyncService_OnWebApkUninstalled(
    JNIEnv* env,
    const std::string& java_manifest_id) {}

static void JNI_WebApkSyncService_RemoveOldWebAPKsFromSync(
    JNIEnv* env,
    int64_t java_current_time_ms_since_unix_epoch) {}

static void JNI_WebApkSyncService_FetchRestorableApps(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& java_callback) {}

}  // namespace webapk

DEFINE_JNI(WebApkSyncService)
