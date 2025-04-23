// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/sys_utils.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/trace_event/base_tracing.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/sys_utils_jni/SysUtils_jni.h"

namespace base {
namespace android {

enum class IsLowMemoryOptions {
  kAlwaysFalse,
  kAlwaysTrue,
  kJavalessApproximation
};

const base::FeatureParam<IsLowMemoryOptions>::Option kIsLowMemoryOptions[] = {
    {IsLowMemoryOptions::kAlwaysFalse, "AlwaysFalse"},
    {IsLowMemoryOptions::kAlwaysTrue, "AlwaysTrue"},
    {IsLowMemoryOptions::kJavalessApproximation, "JavalessApproximation"}};

BASE_FEATURE(kIsCurrentlyLowMemoryJavaless,
             "IsCurrentlyLowMemoryJavaless",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_ENUM_PARAM(IsLowMemoryOptions,
                        kIsCurrentlyLowMemoryOption,
                        &kIsCurrentlyLowMemoryJavaless,
                        "IsCurrentlyLowMemoryOption",
                        IsLowMemoryOptions::kJavalessApproximation,
                        &kIsLowMemoryOptions);

bool SysUtils::IsCurrentlyLowMemory() {
  if (!FeatureList::IsEnabled(kIsCurrentlyLowMemoryJavaless)) {
    return Java_SysUtils_isCurrentlyLowMemory(AttachCurrentThread());
  }
  switch (kIsCurrentlyLowMemoryOption.Get()) {
    case IsLowMemoryOptions::kAlwaysFalse:
      return false;
    case IsLowMemoryOptions::kAlwaysTrue:
      return true;
    case IsLowMemoryOptions::kJavalessApproximation:
      // Picked 138240 as it is the number we used to check against in Java's
      // MemoryInfo.lowMemory. There are a lot of exceptions and edge cases,
      // but capturing all them likely isn't worth it, so using the basic number
      // instead. To see where this number is calculated, look at
      // https://android.googlesource.com/platform/frameworks/base/+/a8e26bab3b8b3d939da5c646411578b565735473/services/core/java/com/android/server/am/ProcessList.java#1743
      return base::SysInfo::AmountOfAvailablePhysicalMemory() < 138240;
  }
}

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

}  // namespace android

}  // namespace base
