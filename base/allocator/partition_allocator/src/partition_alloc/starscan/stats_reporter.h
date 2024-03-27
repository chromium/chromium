// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STARSCAN_STATS_REPORTER_H_
#define PARTITION_ALLOC_STARSCAN_STATS_REPORTER_H_

#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "partition_alloc/starscan/stats_collector.h"

namespace partition_alloc {

// StatsReporter is a wrapper to invoke TRACE_EVENT_BEGIN/END, TRACE_COUNTER1,
// and UmaHistogramTimes. It is used to just remove trace_log and uma
// dependencies from PartitionAlloc.
class StatsReporter {
 public:
  virtual void ReportTraceEvent(internal::StatsCollector::ScannerId id,
                                internal::base::PlatformThreadId tid,
                                int64_t start_time_ticks_internal_value,
                                int64_t end_time_ticks_internal_value) {}
  virtual void ReportTraceEvent(internal::StatsCollector::MutatorId id,
                                internal::base::PlatformThreadId tid,
                                int64_t start_time_ticks_internal_value,
                                int64_t end_time_ticks_internal_value) {}

  virtual void ReportSurvivedQuarantineSize(size_t survived_size) {}

  virtual void ReportSurvivedQuarantinePercent(double survivied_rate) {}

  virtual void ReportStats(const char* stats_name, int64_t sample_in_usec) {}
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_STARSCAN_STATS_REPORTER_H_
