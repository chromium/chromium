// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/flags/chrome_cached_flags.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeCachedFlags_jni.h"

namespace chrome::android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_ChromeCachedFlags_isEnabled(env, feature.name);
}

ScopedJavaDrivenFeatureForTesting::ScopedJavaDrivenFeatureForTesting(
    const base::Feature& feature,
    bool enabled)
    : feature_(feature) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (Java_ChromeCachedFlags_hasOverrideForTesting(  // IN-TEST
          env, feature.name)) {
    previous_override_ =
        Java_ChromeCachedFlags_getOverrideForTesting(  // IN-TEST
            env, feature.name);
  }
  Java_ChromeCachedFlags_setOverrideForTesting(  // IN-TEST
      env, feature.name, enabled);
}

ScopedJavaDrivenFeatureForTesting::~ScopedJavaDrivenFeatureForTesting() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (previous_override_.has_value()) {
    Java_ChromeCachedFlags_setOverrideForTesting(  // IN-TEST
        env, feature_->name, *previous_override_);
  } else {
    Java_ChromeCachedFlags_removeOverrideForTesting(  // IN-TEST
        env, feature_->name);
  }
}

}  // namespace chrome::android

DEFINE_JNI(ChromeCachedFlags)
