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
#include "base/notreached.h"

namespace base {

namespace internal {

BASE_EXPORT int ThreadTypeToNiceValue(ThreadType thread_type) {
  for (const auto& pair : kThreadTypeToNiceValueMap) {
    if (pair.thread_type == thread_type)
      return pair.nice_value;
  }
  NOTREACHED() << "Unknown ThreadType";
}

ThreadPriorityForTest NiceValueToThreadPriorityForTest(int nice_value) {
  // Try to find a priority that best describes |nice_value|. If there isn't
  // an exact match, this method returns the closest priority whose nice value
  // is higher (lower priority) than |nice_value|.
  for (const auto& pair : kThreadPriorityToNiceValueMapForTest) {
    if (pair.nice_value >= nice_value)
      return pair.priority;
  }

  // Reaching here means |nice_value| is more than any of the defined
  // priorities. The lowest priority is suitable in this case.
  return ThreadPriorityForTest::kBackground;
}

int GetCurrentThreadNiceValue() {
#if BUILDFLAG(IS_NACL)
  NOTIMPLEMENTED();
  return 0;
#else

  // Need to clear errno before calling getpriority():
  // http://man7.org/linux/man-pages/man2/getpriority.2.html
  errno = 0;
  int nice_value = getpriority(PRIO_PROCESS, 0);
  if (errno != 0) {
    DVPLOG(1) << "Failed to get nice value of thread ("
              << PlatformThread::CurrentId() << ")";
    return 0;
  }

  return nice_value;
#endif
}

}  // namespace internal

}  // namespace base
