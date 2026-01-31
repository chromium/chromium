// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/preloading/preloading_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/preloading/android/jni_headers/PreloadingFeatureMap_jni.h"

namespace chrome::preloading::android {

namespace {

// Array of features exposed through the Java PreloadingFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kPrewarm,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_PreloadingFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace chrome::preloading::android

DEFINE_JNI(PreloadingFeatureMap)
