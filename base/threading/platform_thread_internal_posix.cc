// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_internal_posix.h"

#include <errno.h>
#include <sys/resource.h>

#include <ostream>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/notimplemented.h"

namespace base::internal {

ThreadType NiceValueToThreadTypeForTest(int nice_value) {
  // Try to find a priority that best describes |nice_value|. If there isn't
  // an exact match, this method returns the closest priority whose nice value
  // is higher (lower priority) than |nice_value|.
  for (const auto& pair : kThreadTypeToNiceValueMapForTest) {
    if (pair.nice_value >= nice_value) {
      return pair.priority;
    }
  }

  // Reaching here means |nice_value| is more than any of the defined
  // priorities. The lowest priority is suitable in this case.
  return ThreadType::kBackground;
}

int GetCurrentThreadNiceValue() {
  return GetThreadNiceValue(PlatformThreadId{0});
}

int GetThreadNiceValue(PlatformThreadId id) {
  // Need to clear errno before calling getpriority():
  // http://man7.org/linux/man-pages/man2/getpriority.2.html
  errno = 0;
  int nice_value = getpriority(PRIO_PROCESS, static_cast<id_t>(id.raw()));
  if (errno != 0) {
    DVPLOG(1) << "Failed to get nice value of thread ("
              << PlatformThread::CurrentId() << ")";
    return 0;
  }

  return nice_value;
}

bool SetThreadNiceFromType(PlatformThreadId thread_id, ThreadType thread_type) {
  // setpriority(2) should change the whole thread group's (i.e. process)
  // priority. However, as stated in the bugs section of
  // http://man7.org/linux/man-pages/man2/getpriority.2.html: "under the current
  // Linux/NPTL implementation of POSIX threads, the nice value is a per-thread
  // attribute". Also, 0 is preferred to the current thread id since it is
  // equivalent but makes sandboxing easier (https://crbug.com/399473).
  pid_t syscall_tid =
      thread_id == PlatformThread::CurrentId() ? 0 : thread_id.raw();
  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread " << thread_id << " to "
              << nice_setting;
    return false;
  }
  return true;
}

}  // namespace base::internal
