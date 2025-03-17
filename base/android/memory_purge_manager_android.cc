// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/memory_purge_manager_android.h"

#include "base/android/build_info.h"
#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/functional/bind.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_jni/MemoryPurgeManager_jni.h"

static void JNI_MemoryPurgeManager_PostDelayedPurgeTaskOnUiThread(JNIEnv* env,
                                                                  jlong delay) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::android::PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      task_runner, FROM_HERE,
      base::BindOnce([](base::MemoryReductionTaskContext task_type) {
        const bool called_from_pre_freeze =
            task_type == base::MemoryReductionTaskContext::kProactive;
        Java_MemoryPurgeManager_doDelayedPurge(jni_zero::AttachCurrentThread(),
                                               called_from_pre_freeze);
      }),
      base::Milliseconds(static_cast<long>(delay)));
}

static jboolean JNI_MemoryPurgeManager_IsOnPreFreezeMemoryTrimEnabled(
    JNIEnv* env) {
  return base::android::PreFreezeBackgroundMemoryTrimmer::ShouldUseModernTrim();
}
