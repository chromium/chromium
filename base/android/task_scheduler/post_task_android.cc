// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/post_task_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/PostTask_jni.h"

namespace base {

// static
void PostTaskAndroid::SignalNativeSchedulerReady() {
  Java_PostTask_onNativeSchedulerReady(jni_zero::AttachCurrentThread());
}

}  // namespace base
