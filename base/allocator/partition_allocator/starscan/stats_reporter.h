// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_

#include "base/allocator/partition_allocator/starscan/stats_collector.h"

namespace partition_alloc {

static_assert(sizeof(uint32_t) >= sizeof(internal::base::PlatformThreadId),
              "sizeof(tid) must be larger than sizeof(PlatformThreadId)");

// StatsReporter is a wrapper to invoke TRACE_EVENT_BEGIN/END, TRACE_COUNTER1,
// and UmaHistogramTimes. It is used to just remove trace_log and uma
// dependencies from partition allocator.
class StatsReporter {
 public:
  virtual void ReportTraceEvent(internal::StatsCollector::ScannerId id,
                                uint32_t tid,
                                int64_t start_time_ticks_internal_value,
                                int64_t end_time_ticks_internal_value) {}
  virtual void ReportTraceEvent(internal::StatsCollector::MutatorId id,
                                uint32_t tid,
                                int64_t start_time_ticks_internal_value,
                                int64_t end_time_ticks_internal_value) {}

  virtual void ReportSurvivedQuarantineSize(size_t survived_size) {}

  virtual void ReportSurvivedQuarantinePercent(double survivied_rate) {}

  virtual void ReportStats(const char* stats_name, int64_t sample_in_usec) {}
};

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_REPORTER_H_
