// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/java_handler_thread_helpers.h"

#include "base/android/java_handler_thread.h"
#include "base/message_loop/message_loop_current.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/base_unittests_jni_headers/JavaHandlerThreadHelpers_jni.h"

namespace base {
namespace android {

// static
std::unique_ptr<JavaHandlerThread> JavaHandlerThreadHelpers::CreateJavaFirst() {
  return std::make_unique<JavaHandlerThread>(
      nullptr, Java_JavaHandlerThreadHelpers_testAndGetJavaHandlerThread(
                   base::android::AttachCurrentThread()));
}

// static
void JavaHandlerThreadHelpers::ThrowExceptionAndAbort(WaitableEvent* event) {
  JNIEnv* env = AttachCurrentThread();
  Java_JavaHandlerThreadHelpers_throwException(env);
  DCHECK(HasException(env));
  base::MessageLoopCurrentForUI::Get()->Abort();
  event->Signal();
}

// static
bool JavaHandlerThreadHelpers::IsExceptionTestException(
    ScopedJavaLocalRef<jthrowable> exception) {
  JNIEnv* env = AttachCurrentThread();
  return Java_JavaHandlerThreadHelpers_isExceptionTestException(env, exception);
}

}  // namespace android
}  // namespace base
