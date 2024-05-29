// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preferences/android/chrome_shared_preferences.h"

#include "base/android/jni_android.h"

#include <memory>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/preferences/jni_headers/ChromeSharedPreferences_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::SharedPreferencesManager;

namespace android::shared_preferences {

const SharedPreferencesManager GetChromeSharedPreferences() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_ChromeSharedPreferences_getInstance(env);
  return SharedPreferencesManager(jshared_prefs_manager, env);
}

}  // namespace android::shared_preferences
