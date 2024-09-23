// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/java_runtime.h"

#include "base/numerics/safe_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android_runtime_jni_headers/Runtime_jni.h"

namespace base {
namespace android {

void JavaRuntime::GetMemoryUsage(uint64_t* total_memory,
                                 uint64_t* free_memory) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> runtime =
      JNI_Runtime::Java_Runtime_getRuntime(env);
  *total_memory = checked_cast<uint64_t>(
      JNI_Runtime::Java_Runtime_totalMemory(env, runtime));
  *free_memory = checked_cast<uint64_t>(
      JNI_Runtime::Java_Runtime_freeMemory(env, runtime));
}

}  // namespace android
}  // namespace base
