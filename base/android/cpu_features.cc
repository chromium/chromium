// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cpu-features.h>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/CpuFeatures_jni.h"

namespace base {
namespace android {

jint JNI_CpuFeatures_GetCoreCount(JNIEnv*) {
  return android_getCpuCount();
}

jlong JNI_CpuFeatures_GetCpuFeatures(JNIEnv*) {
  return static_cast<jlong>(android_getCpuFeatures());
}

}  // namespace android
}  // namespace base
