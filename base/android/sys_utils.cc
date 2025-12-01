// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/process/process_metrics.h"
#include "base/sys_utils_jni/SysUtils_jni.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace android {

// Logs the number of minor / major page faults to tracing (and also the time to
// collect) the metrics. Does nothing if tracing is not enabled.
static void JNI_SysUtils_LogPageFaultCountToTracing(JNIEnv* env) {
  // This is racy, but we are OK losing data, and collecting it is potentially
  // expensive (reading and parsing a file).
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("startup", &enabled);
  if (!enabled) {
    return;
  }
  TRACE_EVENT_BEGIN2("memory", "CollectPageFaultCount", "minor", 0, "major", 0);
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));
  base::PageFaultCounts counts;
  process_metrics->GetPageFaultCounts(&counts);
  TRACE_EVENT_END2("memory", "CollectPageFaults", "minor", counts.minor,
                   "major", counts.major);
}

int GetCachedLowMemoryDeviceThresholdMb() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<int>(Java_SysUtils_getLowMemoryDeviceThresholdMb(env));
}

}  // namespace android

}  // namespace base

DEFINE_JNI(SysUtils)
