// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <map>
#include <string>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/ignore_result.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
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

void RunThreadCachePeriodicPurge() {
  TRACE_EVENT0("memory", "PeriodicPurge");
  auto& instance = internal::ThreadCacheRegistry::Instance();
  instance.RunPeriodicPurge();
  TimeDelta delay =
      Microseconds(instance.GetPeriodicPurgeNextIntervalInMicroseconds());
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(RunThreadCachePeriodicPurge), delay);
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
  auto& instance = internal::ThreadCacheRegistry::Instance();
  TimeDelta delay =
      Microseconds(instance.GetPeriodicPurgeNextIntervalInMicroseconds());
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(RunThreadCachePeriodicPurge), delay);
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

std::map<std::string, std::string> ProposeSyntheticFinchTrials(
    bool is_enterprise) {
  std::map<std::string, std::string> trials;

  // Records whether or not PartitionAlloc is used as the default allocator.
  trials.emplace("PartitionAllocEverywhere",
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
                 "Enabled"
#else
                 "Disabled"
#endif
  );

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // BackupRefPtr_Effective and PCScan_Effective record whether or not
  // BackupRefPtr and/or PCScan are enabled. The experiments aren't independent,
  // so having a synthetic Finch will help look only at cases where one isn't
  // affected by the other.

  // Whether PartitionAllocBackupRefPtr is enabled (as determined by
  // FeatureList::IsEnabled).
  bool brp_finch_enabled = false;
  ALLOW_UNUSED_LOCAL(brp_finch_enabled);
  // Whether PartitionAllocBackupRefPtr is set up for the default behavior. The
  // default behavior is when either the Finch flag is disabled, or is enabled
  // in brp-mode=disabled (these two options are equivalent).
  bool brp_nondefault_behavior = false;
  ALLOW_UNUSED_LOCAL(brp_nondefault_behavior);
  // Whether PartitionAllocBackupRefPtr is set up to enable BRP protection. It
  // requires the Finch flag to be enabled and brp-mode!=disabled*. Some modes,
  // e.g. disabled-but-3-way-split, do something (hence can't be considered the
  // default behavior), but don't enable BRP protection.
  bool brp_truly_enabled = false;
  ALLOW_UNUSED_LOCAL(brp_truly_enabled);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (FeatureList::IsEnabled(features::kPartitionAllocBackupRefPtr))
    brp_finch_enabled = true;
  if (brp_finch_enabled && features::kBackupRefPtrModeParam.Get() !=
                               features::BackupRefPtrMode::kDisabled)
    brp_nondefault_behavior = true;
  if (brp_finch_enabled && features::kBackupRefPtrModeParam.Get() ==
                               features::BackupRefPtrMode::kEnabled)
    brp_truly_enabled = true;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  bool pcscan_enabled =
#if defined(PA_ALLOW_PCSCAN)
      FeatureList::IsEnabled(features::kPartitionAllocPCScanBrowserOnly);
#else
      false;
#endif
  ALLOW_UNUSED_LOCAL(pcscan_enabled);

  std::string brp_group_name = "Unavailable";
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (pcscan_enabled) {
    // If PCScan is enabled, just ignore the population.
    brp_group_name = "Ignore_PCScanIsOn";
  } else if (!brp_finch_enabled) {
    // The control group is actually disguised as "enabled", but in fact it's
    // disabled using a param. This is to differentiate the population that
    // participates in the control group, from the population that isn't in any
    // group.
    brp_group_name = "Ignore_NoGroup";
  } else {
    switch (features::kBackupRefPtrModeParam.Get()) {
      case features::BackupRefPtrMode::kDisabled:
        brp_group_name = "Disabled";
        break;
      case features::BackupRefPtrMode::kEnabled:
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        brp_group_name = "EnabledPrevSlot";
#else
        brp_group_name = "EnabledBeforeAlloc";
#endif
        break;
      case features::BackupRefPtrMode::kDisabledButSplitPartitions2Way:
        brp_group_name = "DisabledBut2WaySplit";
        break;
      case features::BackupRefPtrMode::kDisabledButSplitPartitions3Way:
        brp_group_name = "DisabledBut3WaySplit";
        break;
    }

    if (features::kBackupRefPtrModeParam.Get() !=
        features::BackupRefPtrMode::kDisabled) {
      std::string process_selector;
      switch (features::kBackupRefPtrEnabledProcessesParam.Get()) {
        case features::BackupRefPtrEnabledProcesses::kBrowserOnly:
          process_selector = "BrowserOnly";
          break;
        case features::BackupRefPtrEnabledProcesses::kBrowserAndRenderer:
          process_selector = "BrowserAndRenderer";
          break;
        case features::BackupRefPtrEnabledProcesses::kNonRenderer:
          process_selector = "NonRenderer";
          break;
        case features::BackupRefPtrEnabledProcesses::kAllProcesses:
          process_selector = "AllProcesses";
          break;
      }

      brp_group_name += ("_" + process_selector);
    }
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  trials.emplace("BackupRefPtr_Effective", brp_group_name);

  // On 32-bit architectures, PCScan is not supported and permanently disabled.
  // Don't lump it into "Disabled", so that belonging to "Enabled"/"Disabled" is
  // fully controlled by Finch and thus have identical population sizes.
  std::string pcscan_group_name = "Unavailable";
  std::string pcscan_group_name_fallback = "Unavailable";
#if defined(PA_ALLOW_PCSCAN)
  if (brp_truly_enabled) {
    // If BRP protection is enabled, just ignore the population. Check
    // brp_truly_enabled, not brp_finch_enabled, because there are certain modes
    // where BRP protection is actually disabled.
    pcscan_group_name = "Ignore_BRPIsOn";
  } else {
    pcscan_group_name = (pcscan_enabled ? "Enabled" : "Disabled");
  }
  // In case we are incorrect that PCScan is independent of partition-split
  // modes, create a fallback trial that only takes into account the BRP Finch
  // settings that preserve the default behavior.
  if (brp_nondefault_behavior) {
    pcscan_group_name_fallback = "Ignore_BRPIsOn";
  } else {
    pcscan_group_name_fallback = (pcscan_enabled ? "Enabled" : "Disabled");
  }
#endif  // defined(PA_ALLOW_PCSCAN)
  trials.emplace("PCScan_Effective", pcscan_group_name);
  trials.emplace("PCScan_Effective_Fallback", pcscan_group_name_fallback);

  // This synthetic Finch setting reflects the new USE_BACKUP_REF_PTR behavior,
  // which simply compiles in the BackupRefPtr support, but keeps it disabled at
  // run-time (which can be further enabled via Finch).
  trials.emplace("BackupRefPtrSupport",
#if BUILDFLAG(USE_BACKUP_REF_PTR)
                 "CompiledIn"
#else
                 "Disabled"
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  );
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // This synthetic field trial for the BackupRefPtr binary A/B experiment is
  // set up such that:
  // 1) Enterprises are excluded from experiment, to make sure we honor
  //    ChromeVariations policy.
  // 2) The experiment binary (USE_BACKUP_REF_PTR) is delivered via Google
  //    Update to fraction X of the non-enterprise population.
  // 3) The control group is established in fraction X of non-enterprise
  //    popluation via Finch (PartitionAllocBackupRefPtrControl). Since this
  //    Finch is applicable only to 1-X of the non-enterprise population, we
  //    need to set it to Y=X/(1-X). E.g. if X=.333, Y=.5; if X=.01, Y=.0101.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  constexpr bool kIsBrpOn = true;  // experiment binary only
#else
  constexpr bool kIsBrpOn = false;  // non-experiment binary
#endif
  const bool is_brp_control =
      FeatureList::IsEnabled(features::kPartitionAllocBackupRefPtrControl);
  const char* group_name;
  if (is_enterprise) {
    if (kIsBrpOn) {  // is_enterprise && kIsBrpOn
      group_name = "Excluded_Enterprise_BrpOn";
    } else {  // is_enterprise && !kIsBrpOn
      group_name = "Excluded_Enterprise_BrpOff";
    }
  } else {
    if (kIsBrpOn) {  // !is_enterprise && kIsBrpOn
      group_name = "Enabled";
    } else {  // !is_enterprise && !kIsBrpOn
      if (is_brp_control) {
        group_name = "Control";
      } else {
        group_name = "Excluded_NonEnterprise";
      }
    }
  }
  trials.emplace("BackupRefPtrNoEnterprise", group_name);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  trials.emplace("FakeBinaryExperiment",
#if BUILDFLAG(USE_FAKE_BINARY_EXPERIMENT)
                 "Enabled"
#else
                 "Disabled"
#endif
  );

  return trials;
}

}  // namespace allocator
}  // namespace base
