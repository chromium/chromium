// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <optional>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/ThreadUtils_jni.h"

namespace base {

BASE_FEATURE(kIncreaseDisplayCriticalThreadPriority,
             "RaiseDisplayCriticalThreadPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace internal {

// Returns true if the kDisplayCriticalThreadPriority should be boosted.
static bool ShouldBoostDisplayCriticalThreadPriority() {
  // ADPF-equipped Google Pixels are excluded from the study because of
  // potential input jank. Because Finch doesn't support per-device targeting,
  // switch this off even if the flag's on. TODO (ritownsend): make it possible
  // to switch this back on for Pixel.
  static bool is_google_soc = SysInfo::SocManufacturer() == "Google";
  return !is_google_soc &&
         base::FeatureList::IsEnabled(kIncreaseDisplayCriticalThreadPriority);
}

// - kRealtimeAudio corresponds to Android's PRIORITY_AUDIO = -16 value.
// - kDisplay corresponds to Android's PRIORITY_DISPLAY = -4 value.
// - kUtility corresponds to Android's THREAD_PRIORITY_LESS_FAVORABLE = 1 value.
// - kBackground corresponds to Android's PRIORITY_BACKGROUND = 10
//   value. Contrary to the matching Java APi in Android <13, this does not
//   restrict the thread to (subset of) little cores.
const ThreadTypeToNiceValuePairForTest kThreadTypeToNiceValueMapForTest[7] = {
    {ThreadType::kRealtimeAudio, -16}, {ThreadType::kDisplayCritical, -4},
    {ThreadType::kDefault, 0},         {ThreadType::kUtility, 1},
    {ThreadType::kBackground, 10},
};

// - kBackground corresponds to Android's PRIORITY_BACKGROUND = 10 value and can
// result in heavy throttling and force the thread onto a little core on
// big.LITTLE devices.
// - kUtility corresponds to Android's THREAD_PRIORITY_LESS_FAVORABLE = 1 value.
// - kDisplayCritical and kInteractive correspond to Android's PRIORITY_DISPLAY
// = -4 value.
// - kRealtimeAudio corresponds to Android's PRIORITY_AUDIO = -16 value.

int ThreadTypeToNiceValue(const ThreadType thread_type) {
  switch (thread_type) {
    case ThreadType::kBackground:
      return 10;
    case ThreadType::kUtility:
      return 1;
    case ThreadType::kDefault:
      return 0;
    case ThreadType::kDisplayCritical:
    case ThreadType::kInteractive:
      if (ShouldBoostDisplayCriticalThreadPriority()) {
        return -12;
      }
      return -4;
    case ThreadType::kRealtimeAudio:
      return -16;
  }
}

bool CanSetThreadTypeToRealtimeAudio() {
  return true;
}

void SetCurrentThreadTypeImpl(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
  // We set the Audio priority through JNI as the Java setThreadPriority will
  // put it into a preferable cgroup, whereas the "normal" C++ call wouldn't.
  // However, with
  // https://android-review.googlesource.com/c/platform/system/core/+/1975808
  // this becomes obsolete and we can avoid this starting in API level 33.
  if (thread_type == ThreadType::kRealtimeAudio &&
      base::android::android_info::sdk_int() <
          base::android::android_info::SDK_VERSION_T) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ThreadUtils_setThreadPriorityAudio(env,
                                            PlatformThread::CurrentId().raw());
    return;
  }
  // Recent versions of Android (O+) up the priority of the UI thread
  // automatically.
  if (thread_type == ThreadType::kDisplayCritical &&
      pump_type_hint == MessagePumpType::UI &&
      GetCurrentThreadNiceValue() <=
          ThreadTypeToNiceValue(ThreadType::kDisplayCritical)) {
    return;
  }
  SetThreadNiceFromType(PlatformThread::CurrentId(), thread_type);
}

std::optional<ThreadType> GetCurrentEffectiveThreadTypeForPlatformForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_ThreadUtils_isThreadPriorityAudio(
          env, PlatformThread::CurrentId().raw())) {
    return std::make_optional(ThreadType::kRealtimeAudio);
  }
  return std::nullopt;
}

PlatformPriorityOverride SetThreadTypeOverride(
    PlatformThreadHandle thread_handle,
    ThreadType thread_type) {
  PlatformThreadId thread_id(
      pthread_gettid_np(thread_handle.platform_handle()));
  if (GetThreadNiceValue(thread_id) <= ThreadTypeToNiceValue(thread_type)) {
    return false;
  }
  return SetThreadNiceFromType(thread_id, thread_type);
}

void RemoveThreadTypeOverrideImpl(
    const PlatformPriorityOverride& priority_override_handle,
    ThreadType thread_type) {
  if (!priority_override_handle) {
    return;
  }
  SetCurrentThreadTypeImpl(thread_type, MessagePumpType::DEFAULT);
}

}  // namespace internal

void PlatformThread::SetName(const std::string& name) {
  SetNameCommon(name);

  // Like linux, on android we can get the thread names to show up in the
  // debugger by setting the process name for the LWP.
  // We don't want to do this for the main thread because that would rename
  // the process, causing tools like killall to stop working.
  if (PlatformThread::CurrentId().raw() == getpid()) {
    return;
  }

  // Set the name for the LWP (which gets truncated to 15 characters).
  int err = prctl(PR_SET_NAME, name.c_str());
  if (err < 0 && errno != EPERM) {
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
  }
}

void InitThreading() {}

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

DEFINE_JNI(ThreadUtils)
