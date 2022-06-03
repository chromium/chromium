// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/trace_time.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/base/time.h"

namespace base {
namespace tracing {

int64_t TraceBootTicksNow() {
  // On Windows and Mac, TRACE_TIME_TICKS_NOW() behaves like boottime already.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
    defined(OS_FUCHSIA)
  struct timespec ts;
  int res = clock_gettime(CLOCK_BOOTTIME, &ts);
  if (res != -1)
    return static_cast<int64_t>(perfetto::base::FromPosixTimespec(ts).count());
#endif
  return TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds();
}

}  // namespace tracing
}  // namespace base