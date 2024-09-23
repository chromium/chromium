// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/FeatureList_jni.h"

namespace base {
namespace android {

static jboolean JNI_FeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

}  // namespace android
}  // namespace base
