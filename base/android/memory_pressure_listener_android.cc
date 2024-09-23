// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/memory_pressure_listener_android.h"

#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/memory/memory_pressure_listener.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_jni/MemoryPressureListener_jni.h"

using base::android::JavaParamRef;

// Defined and called by JNI.
static void JNI_MemoryPressureListener_OnMemoryPressure(
    JNIEnv* env,
    jint memory_pressure_level) {
  base::MemoryPressureListener::NotifyMemoryPressure(
      static_cast<base::MemoryPressureListener::MemoryPressureLevel>(
          memory_pressure_level));
}

static void JNI_MemoryPressureListener_OnPreFreeze(JNIEnv* env) {
  base::android::PreFreezeBackgroundMemoryTrimmer::OnPreFreeze();
}

static jboolean JNI_MemoryPressureListener_IsTrimMemoryBackgroundCritical(
    JNIEnv* env) {
  return base::android::PreFreezeBackgroundMemoryTrimmer::
      IsTrimMemoryBackgroundCritical();
}

namespace base::android {

void MemoryPressureListenerAndroid::Initialize(JNIEnv* env) {
  Java_MemoryPressureListener_addNativeCallback(env);
}

}  // namespace base::android
