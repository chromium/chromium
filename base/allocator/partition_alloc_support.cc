// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace allocator {

namespace {

#if defined(PA_ALLOW_PCSCAN)

constexpr const char* ScannerIdToTracingString(
    internal::StatsCollector::ScannerId id) {
  switch (id) {
    case internal::StatsCollector::ScannerId::kClear:
      return "PCScan.Scanner.Clear";
    case internal::StatsCollector::ScannerId::kScan:
      return "PCScan.Scanner.Scan";
    case internal::StatsCollector::ScannerId::kSweep:
      return "PCScan.Scanner.Sweep";
    case internal::StatsCollector::ScannerId::kOverall:
      return "PCScan.Scanner";
    case internal::StatsCollector::ScannerId::kNumIds:
      __builtin_unreachable();
  }
}

constexpr const char* MutatorIdToTracingString(
    internal::StatsCollector::MutatorId id) {
  switch (id) {
    case internal::StatsCollector::MutatorId::kClear:
      return "PCScan.Mutator.Clear";
    case internal::StatsCollector::MutatorId::kScanStack:
      return "PCScan.Mutator.ScanStack";
    case internal::StatsCollector::MutatorId::kScan:
      return "PCScan.Mutator.Scan";
    case internal::StatsCollector::MutatorId::kOverall:
      return "PCScan.Mutator";
    case internal::StatsCollector::MutatorId::kNumIds:
      __builtin_unreachable();
  }
}

// Inject TRACE_EVENT_BEGIN/END, TRACE_COUNTER1, and UmaHistogramTimes.
class StatsReporterImpl final : public StatsReporter {
 public:
  constexpr StatsReporterImpl() = default;

  void ReportTraceEvent(internal::StatsCollector::ScannerId id,
                        const PlatformThreadId tid,
                        TimeTicks start_time,
                        TimeTicks end_time) override {
    // TRACE_EVENT_* macros below drop most parameters when tracing is
    // disabled at compile time.
    ignore_result(tid);
    const char* tracing_id = ScannerIdToTracingString(id);
    TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString(tracing_id),
                      perfetto::ThreadTrack::ForThread(tid), start_time);
    TRACE_EVENT_END(kTraceCategory, perfetto::ThreadTrack::ForThread(tid),
                    end_time);
  }

  void ReportTraceEvent(internal::StatsCollector::MutatorId id,
                        const PlatformThreadId tid,
                        TimeTicks start_time,
                        TimeTicks end_time) override {
    // TRACE_EVENT_* macros below drop most parameters when tracing is
    // disabled at compile time.
    ignore_result(tid);
    const char* tracing_id = MutatorIdToTracingString(id);
    TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString(tracing_id),
                      perfetto::ThreadTrack::ForThread(tid), start_time);
    TRACE_EVENT_END(kTraceCategory, perfetto::ThreadTrack::ForThread(tid),
                    end_time);
  }

  void ReportSurvivedQuarantineSize(size_t survived_size) override {
    TRACE_COUNTER1(kTraceCategory, "PCScan.SurvivedQuarantineSize",
                   survived_size);
  }

  void ReportSurvivedQuarantinePercent(double survived_rate) override {
    // Multiply by 1000 since TRACE_COUNTER1 expects integer. In catapult,
    // divide back.
    // TODO(bikineev): Remove after switching to perfetto.
    TRACE_COUNTER1(kTraceCategory, "PCScan.SurvivedQuarantinePercent",
                   1000 * survived_rate);
  }

  void ReportStats(const char* stats_name, TimeDelta sample) override {
    UmaHistogramTimes(stats_name, sample);
  }

 private:
  static constexpr char kTraceCategory[] = "partition_alloc";
};

#endif  // defined(PA_ALLOW_PCSCAN)

}  // namespace

#if defined(PA_ALLOW_PCSCAN)
void RegisterPCScanStatsReporter() {
  static StatsReporterImpl s_reporter;
  static bool registered = false;

  DCHECK(!registered);

  internal::PCScan::RegisterStatsReporter(&s_reporter);
  registered = true;
}
#endif  // defined(PA_ALLOW_PCSCAN)

}  // namespace allocator
}  // namespace base
