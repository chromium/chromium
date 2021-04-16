// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_jni_headers/Features_jni.h"
#include "base/feature_list.h"

namespace base {
namespace android {

jboolean JNI_Features_IsEnabled(JNIEnv* env, jlong native_feature_pointer) {
  return base::FeatureList::IsEnabled(
      *reinterpret_cast<base::Feature*>(native_feature_pointer));
}

}  // namespace android
}  // namespace base
