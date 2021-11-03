// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_

#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

// StatsReporter is a wrapper to invoke TRACE_EVENT_BEGIN/END, TRACE_COUNTER1,
// and UmaHistogramTimes. It is used to just remove trace_log and uma
// dependencies from partition allocator.
class StatsReporter {
 public:
  virtual void ReportTraceEvent(internal::StatsCollector::ScannerId id,
                                const PlatformThreadId tid,
                                TimeTicks start_time,
                                TimeTicks end_time) {}
  virtual void ReportTraceEvent(internal::StatsCollector::MutatorId id,
                                const PlatformThreadId tid,
                                TimeTicks start_time,
                                TimeTicks end_time) {}

  virtual void ReportSurvivedQuarantineSize(size_t survived_size) {}

  virtual void ReportSurvivedQuarantinePercent(double survivied_rate) {}

  virtual void ReportStats(const char* stats_name, TimeDelta sample) {}
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_
