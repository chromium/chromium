// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/memory_pressure_listener_android.h"

#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory_coordinator/memory_pressure_source.h"
#include "base/task/single_thread_task_runner.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_jni/MemoryPressureListener_jni.h"

using base::android::JavaRef;

// Defined and called by JNI.
static void JNI_MemoryPressureListener_OnMemoryPressure(
    JNIEnv* env,
    int32_t memory_pressure_level) {
  // Sometimes, early in the process's lifetime, the main thread task runner is
  // not set yet.
  if (!base::SingleThreadTaskRunner::HasMainThreadDefault()) {
    return;
  }

  // Forward the notification to the MemoryPressureSource.
  base::SingleThreadTaskRunner::GetMainThreadDefault()->PostTask(
      FROM_HERE, base::BindOnce(&base::SetMemoryPressureLevel,
                                static_cast<base::MemoryPressureLevel>(
                                    memory_pressure_level)));
}

static void JNI_MemoryPressureListener_OnPreFreeze(JNIEnv* env) {
  base::android::PreFreezeBackgroundMemoryTrimmer::OnPreFreeze();
}

static bool JNI_MemoryPressureListener_IsTrimMemoryBackgroundCritical(
    JNIEnv* env) {
  return base::android::PreFreezeBackgroundMemoryTrimmer::SupportsModernTrim();
}

namespace base::android {

void MemoryPressureListenerAndroid::Initialize(JNIEnv* env) {
  Java_MemoryPressureListener_addNativeCallback(env);
}

}  // namespace base::android

DEFINE_JNI(MemoryPressureListener)
