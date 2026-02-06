// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"

#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksBridge_jni.h"

namespace contextual_tasks {

static int64_t JNI_ContextualTasksBridge_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new ContextualTasksBridge(env, caller));
}

ContextualTasksBridge::ContextualTasksBridge(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : java_obj_(obj) {
  // TODO(shaktisahu): Instantiate ActiveTaskContextProviderImpl once desktop
  // dependencies are removed.
}

ContextualTasksBridge::~ContextualTasksBridge() = default;

void ContextualTasksBridge::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksBridge)
