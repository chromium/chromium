// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_features.h"
#include "base/android/feature_map.h"
#include "base/base_jni/BaseFeatureMap_jni.h"
#include "base/no_destructor.h"

namespace base::android {

namespace {

// Array of features exposed through the Java BaseFeatureMap API. Entries in
// this array refer to features defined in base_features.h.
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kBrowserProcessMemoryPurge,
    &features::kCrashBrowserOnChildMismatchIfBrowserChanged,
    &features::kCrashBrowserOnAnyChildMismatch,
};  // namespace

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_BaseFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace base::android
