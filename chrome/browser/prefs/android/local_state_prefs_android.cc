// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/prefs/android/jni_headers/LocalStatePrefs_jni.h"

namespace chrome_browser_prefs {

void OnLocalStatePrefsLoaded() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_LocalStatePrefs_setNativePrefsLoaded(env);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_LocalStatePrefs_GetPrefService(JNIEnv* env) {
  return g_browser_process->local_state()->GetJavaObject();
}

}  // namespace chrome_browser_prefs

DEFINE_JNI(LocalStatePrefs)
