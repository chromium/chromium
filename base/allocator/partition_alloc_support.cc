// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/ignore_result.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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

namespace {

bool g_memory_reclaimer_running = false;

void DelayedPurgeActionForThreadCache(OnceClosure task, base::TimeDelta delay) {
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](OnceClosure task) {
            TRACE_EVENT0("memory", "PeriodicPurge");
            std::move(task).Run();
          },
          std::move(task)),
      delay);
}

base::RepeatingTimer& GetTimer() {
  static base::NoDestructor<base::RepeatingTimer> timer;
  return *timer.get();
}

void ReclaimPeriodically() {
  TRACE_EVENT0("base", "PartitionAllocMemoryReclaimer::Reclaim()");
  PartitionAllocMemoryReclaimer::Instance()->ReclaimNormal();
}

}  // namespace

void StartThreadCachePeriodicPurge() {
  internal::ThreadCacheRegistry::Instance().StartPeriodicPurge(
      DelayedPurgeActionForThreadCache);
}

void StartMemoryReclaimer(scoped_refptr<SequencedTaskRunner> task_runner) {
  // Can be called several times.
  if (g_memory_reclaimer_running)
    return;

  g_memory_reclaimer_running = true;
  // The caller of the API fully controls where running the reclaim.
  // However there are a few reasons to recommend that the caller runs
  // it on the main thread:
  // - Most of PartitionAlloc's usage is on the main thread, hence PA's metadata
  //   is more likely in cache when executing on the main thread.
  // - Memory reclaim takes the partition lock for each partition. As a
  //   consequence, while reclaim is running, the main thread is unlikely to be
  //   able to make progress, as it would be waiting on the lock.
  // - Finally, this runs in idle time only, so there should be no visible
  //   impact.
  //
  // From local testing, time to reclaim is 100us-1ms, and reclaiming every few
  // seconds is useful. Since this is meant to run during idle time only, it is
  // a reasonable starting point balancing effectivenes vs cost. See
  // crbug.com/942512 for details and experimental results.
  GetTimer().SetTaskRunner(task_runner);
  GetTimer().Start(FROM_HERE,
                   PartitionAllocMemoryReclaimer::Instance()
                       ->GetRecommendedReclaimInterval(),
                   BindRepeating(&ReclaimPeriodically));
}

}  // namespace allocator
}  // namespace base
