// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_TASK_SCHEDULER_POST_TASK_ANDROID_H_
#define BASE_ANDROID_TASK_SCHEDULER_POST_TASK_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/base_export.h"
#include "base/task/task_traits.h"

namespace base {

// C++ interface for PostTask.java
class BASE_EXPORT PostTaskAndroid {
 public:
  // Routes Java tasks posted via TaskScheduler APIs through the C++
  // TaskScheduler.
  static void SignalNativeSchedulerReady();

  // Signals that the C++ scheduler has shutdown. Needed to make unit tests that
  // repeatedly create and destroy the scheduler work.
  static void SignalNativeSchedulerShutdown();

  static TaskTraits CreateTaskTraits(
      JNIEnv* env,
      jboolean priority_set_explicitly,
      jint priority,
      jboolean may_block,
      jbyte extension_id,
      const base::android::JavaParamRef<jbyteArray>& extension_data);

  // We don't know ahead of time which thread this will run on so it looks up
  // the JNI environment and the bindings dynamically (albeit with caching).
  static void RunJavaTask(base::android::ScopedJavaGlobalRef<jobject> task);

  DISALLOW_COPY_AND_ASSIGN(PostTaskAndroid);
};

}  // namespace base

#endif  // BASE_ANDROID_TASK_SCHEDULER_POST_TASK_ANDROID_H_
