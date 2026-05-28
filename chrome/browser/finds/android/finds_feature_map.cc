// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/finds/android/jni_headers/FindsFeatureMap_jni.h"
#include "chrome/browser/finds/core/finds_features.h"

namespace finds::android {

static int64_t JNI_FindsFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &features::kChromeFinds};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<int64_t>(kFeatureMap.get());
}

}  // namespace finds::android

DEFINE_JNI(FindsFeatureMap)
