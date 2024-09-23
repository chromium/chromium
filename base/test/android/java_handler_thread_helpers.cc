// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/java_handler_thread_helpers.h"

#include "base/android/java_handler_thread.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/test/base_unittests_jni_headers/JavaHandlerThreadHelpers_jni.h"

namespace base {
namespace android {

// static
std::unique_ptr<JavaHandlerThread> JavaHandlerThreadHelpers::CreateJavaFirst() {
  return std::make_unique<JavaHandlerThread>(
      nullptr, Java_JavaHandlerThreadHelpers_testAndGetJavaHandlerThread(
                   jni_zero::AttachCurrentThread()));
}

// static
void JavaHandlerThreadHelpers::ThrowExceptionAndAbort(WaitableEvent* event) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_JavaHandlerThreadHelpers_throwException(env);
  DCHECK(jni_zero::HasException(env));
  base::CurrentUIThread::Get()->Abort();
  event->Signal();
}

// static
bool JavaHandlerThreadHelpers::IsExceptionTestException(
    ScopedJavaLocalRef<jthrowable> exception) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_JavaHandlerThreadHelpers_isExceptionTestException(env, exception);
}

}  // namespace android
}  // namespace base
