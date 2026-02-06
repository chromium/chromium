// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_

#include <memory>

#include "third_party/jni_zero/jni_zero.h"

namespace contextual_tasks {

class ActiveTaskContextProvider;

// Native counterpart of ContextualTasksBridge.java.
// Owned by the Java ContextualTasksBridge.
class ContextualTasksBridge {
 public:
  ContextualTasksBridge(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);
  ~ContextualTasksBridge();

  // Disallow copy/assign.
  ContextualTasksBridge(const ContextualTasksBridge&) = delete;
  ContextualTasksBridge& operator=(const ContextualTasksBridge&) = delete;

  void Destroy(JNIEnv* env);

 private:
  // The provider that tracks the task associated with the active tab.
  std::unique_ptr<ActiveTaskContextProvider> active_task_context_provider_;

  jni_zero::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
