// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/ThreadUtils_jni.h"

namespace base {

namespace internal {

// - kRealtimeAudio corresponds to Android's PRIORITY_AUDIO = -16 value.
// - kDisplay corresponds to Android's PRIORITY_DISPLAY = -4 value.
// - kBackground corresponds to Android's PRIORITY_BACKGROUND = 10 value and can
// result in heavy throttling and force the thread onto a little core on
// big.LITTLE devices.
const ThreadPriorityToNiceValuePairForTest
    kThreadPriorityToNiceValueMapForTest[7] = {
        {ThreadPriorityForTest::kRealtimeAudio, -16},
        {ThreadPriorityForTest::kDisplay, -4},
        {ThreadPriorityForTest::kNormal, 0},
        {ThreadPriorityForTest::kResourceEfficient, 0},
        {ThreadPriorityForTest::kUtility, 1},
        {ThreadPriorityForTest::kBackground, 10},
};

// - kBackground corresponds to Android's PRIORITY_BACKGROUND = 10 value and can
// result in heavy throttling and force the thread onto a little core on
// big.LITTLE devices.
// - kUtility corresponds to Android's THREAD_PRIORITY_LESS_FAVORABLE = 1 value.
// - kDisplayCritical corresponds to Android's PRIORITY_DISPLAY = -4 value.
// - kRealtimeAudio corresponds to Android's PRIORITY_AUDIO = -16 value.
const ThreadTypeToNiceValuePair kThreadTypeToNiceValueMap[7] = {
    {ThreadType::kBackground, 10},       {ThreadType::kUtility, 1},
    {ThreadType::kResourceEfficient, 0}, {ThreadType::kDefault, 0},
    {ThreadType::kDisplayCritical, -4},  {ThreadType::kRealtimeAudio, -16},
};

bool CanSetThreadTypeToRealtimeAudio() {
  return true;
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                     MessagePumpType pump_type_hint) {
  // On Android, we set the Audio priority through JNI as Audio priority
  // will also allow the process to run while it is backgrounded.
  if (thread_type == ThreadType::kRealtimeAudio) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ThreadUtils_setThreadPriorityAudio(env, PlatformThread::CurrentId());
    return true;
  }
  // Recent versions of Android (O+) up the priority of the UI thread
  // automatically.
  if (thread_type == ThreadType::kDisplayCritical &&
      pump_type_hint == MessagePumpType::UI &&
      GetCurrentThreadNiceValue() <=
          ThreadTypeToNiceValue(ThreadType::kDisplayCritical)) {
    return true;
  }
  return false;
}

std::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_ThreadUtils_isThreadPriorityAudio(
      env, PlatformThread::CurrentId())) {
    return std::make_optional(ThreadPriorityForTest::kRealtimeAudio);
  }
  return std::nullopt;
}

}  // namespace internal

void PlatformThread::SetName(const std::string& name) {
  SetNameCommon(name);

  // Like linux, on android we can get the thread names to show up in the
  // debugger by setting the process name for the LWP.
  // We don't want to do this for the main thread because that would rename
  // the process, causing tools like killall to stop working.
  if (PlatformThread::CurrentId() == getpid())
    return;

  // Set the name for the LWP (which gets truncated to 15 characters).
  int err = prctl(PR_SET_NAME, name.c_str());
  if (err < 0 && errno != EPERM)
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
}


void InitThreading() {
}

void TerminateOnThread() {
  base::android::DetachFromVM();
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(ADDRESS_SANITIZER)
  return 0;
#else
  // AddressSanitizer bloats the stack approximately 2x. Default stack size of
  // 1Mb is not enough for some tests (see http://crbug.com/263749 for example).
  return 2 * (1 << 20);  // 2Mb
#endif
}

}  // namespace base
