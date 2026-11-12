// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/android/local_state_prefs_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/prefs/android/jni_headers/LocalStatePrefs_jni.h"

namespace {

PrefService* GetLocalState() {
  return g_browser_process
             ? g_browser_process->local_state()
             : ChromeFeatureListCreator::GetInstance()->local_state();
}

}  // namespace

namespace chrome_browser_prefs {

void OnLocalStatePrefsLoaded() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_LocalStatePrefs_setNativePrefsLoaded(env);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_LocalStatePrefs_GetPrefService(JNIEnv* env) {
  return GetLocalState()->GetJavaObject();
}

}  // namespace chrome_browser_prefs

DEFINE_JNI(LocalStatePrefs)
