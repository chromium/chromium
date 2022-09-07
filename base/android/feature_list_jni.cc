// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_jni_headers/FeatureList_jni.h"
#include "base/feature_list.h"

namespace base {
namespace android {

static jboolean JNI_FeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

}  // namespace android
}  // namespace base
