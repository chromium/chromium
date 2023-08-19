// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/mac_startup_profiler.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

// static
MacStartupProfiler* MacStartupProfiler::GetInstance() {
  return base::Singleton<MacStartupProfiler>::get();
}

MacStartupProfiler::MacStartupProfiler() : recorded_metrics_(false) {
}

MacStartupProfiler::~MacStartupProfiler() {
}

void MacStartupProfiler::Profile(Location location) {
  profiled_ticks_[location] = base::TimeTicks::Now();
}

void MacStartupProfiler::RecordMetrics() {
  const base::TimeTicks main_entry_ticks =
      startup_metric_utils::GetCommon().MainEntryPointTicks();
  DCHECK(!main_entry_ticks.is_null());
  DCHECK(!recorded_metrics_);

  recorded_metrics_ = true;

  for (const std::pair<const Location, base::TimeTicks>& entry :
       profiled_ticks_)
    RecordHistogram(entry.first, entry.second - main_entry_ticks);
}

const std::string MacStartupProfiler::HistogramName(Location location) {
  std::string prefix("Startup.OSX.");
  switch (location) {
    case PRE_MAIN_MESSAGE_LOOP_START:
      return prefix + "PreMainMessageLoopStart";
    case AWAKE_FROM_NIB:
      return prefix + "AwakeFromNib";
    case POST_MAIN_MESSAGE_LOOP_START:
      return prefix + "PostMainMessageLoopStart";
    case PRE_PROFILE_INIT:
      return prefix + "PreProfileInit";
    case POST_PROFILE_INIT:
      return prefix + "PostProfileInit";
    case WILL_FINISH_LAUNCHING:
      return prefix + "WillFinishLaunching";
    case DID_FINISH_LAUNCHING:
      return prefix + "DockIconWillFinishBouncing";
  }
}

void MacStartupProfiler::RecordHistogram(Location location,
                                         const base::TimeDelta& delta) {
  const std::string name(HistogramName(location));
  base::TimeDelta min = base::Milliseconds(10);
  base::TimeDelta max = base::Minutes(1);
  int bucket_count = 100;

  // No need to cache the histogram pointers, since each invocation of this
  // method will be the first and only usage of a histogram with that given
  // name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      name,
      min,
      max,
      bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(delta);
}
