// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/test_support_jni_headers/ThreadPoolTestHelpers_jni.h"

namespace base {

// ThreadPoolTestHelpers is a friend of ThreadPoolInstance which grants access
// to SetCanRun().
class ThreadPoolTestHelpers {
 public:
  // Enables/disables an execution fence that prevents tasks from running.
  static void BeginFenceForTesting();
  static void EndFenceForTesting();
};

// static
void ThreadPoolTestHelpers::BeginFenceForTesting() {
  ThreadPoolInstance::Get()->BeginFence();
}

// static
void ThreadPoolTestHelpers::EndFenceForTesting() {
  ThreadPoolInstance::Get()->EndFence();
}

}  // namespace base

void JNI_ThreadPoolTestHelpers_EnableThreadPoolExecutionForTesting(
    JNIEnv* env) {
  base::ThreadPoolTestHelpers::EndFenceForTesting();
}

void JNI_ThreadPoolTestHelpers_DisableThreadPoolExecutionForTesting(
    JNIEnv* env) {
  base::ThreadPoolTestHelpers::BeginFenceForTesting();
}