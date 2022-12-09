// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <map>
#include <string>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/feature_list.h"
#include "base/immediate_crash.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(STARSCAN)
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"
#endif  // BUILDFLAG(STARSCAN)

namespace base {
namespace allocator {

namespace {

#if defined(PA_ALLOW_PCSCAN)

#if BUILDFLAG(ENABLE_BASE_TRACING)
constexpr const char* ScannerIdToTracingString(
    partition_alloc::internal::StatsCollector::ScannerId id) {
  switch (id) {
    case partition_alloc::internal::StatsCollector::ScannerId::kClear:
      return "PCScan.Scanner.Clear";
    case partition_alloc::internal::StatsCollector::ScannerId::kScan:
      return "PCScan.Scanner.Scan";
    case partition_alloc::internal::StatsCollector::ScannerId::kSweep:
      return "PCScan.Scanner.Sweep";
    case partition_alloc::internal::StatsCollector::ScannerId::kOverall:
      return "PCScan.Scanner";
    case partition_alloc::internal::StatsCollector::ScannerId::kNumIds:
      __builtin_unreachable();
  }
}

constexpr const char* MutatorIdToTracingString(
    partition_alloc::internal::StatsCollector::MutatorId id) {
  switch (id) {
    case partition_alloc::internal::StatsCollector::MutatorId::kClear:
      return "PCScan.Mutator.Clear";
    case partition_alloc::internal::StatsCollector::MutatorId::kScanStack:
      return "PCScan.Mutator.ScanStack";
    case partition_alloc::internal::StatsCollector::MutatorId::kScan:
      return "PCScan.Mutator.Scan";
    case partition_alloc::internal::StatsCollector::MutatorId::kOverall:
      return "PCScan.Mutator";
    case partition_alloc::internal::StatsCollector::MutatorId::kNumIds:
      __builtin_unreachable();
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

// Inject TRACE_EVENT_BEGIN/END, TRACE_COUNTER1, and UmaHistogramTimes.
class StatsReporterImpl final : public partition_alloc::StatsReporter {
 public:
  void ReportTraceEvent(
      partition_alloc::internal::StatsCollector::ScannerId id,
      [[maybe_unused]] partition_alloc::internal::base::PlatformThreadId tid,
      int64_t start_time_ticks_internal_value,
      int64_t end_time_ticks_internal_value) override {
#if BUILDFLAG(ENABLE_BASE_TRACING)
    // TRACE_EVENT_* macros below drop most parameters when tracing is
    // disabled at compile time.
    const char* tracing_id = ScannerIdToTracingString(id);
    const TimeTicks start_time =
        TimeTicks::FromInternalValue(start_time_ticks_internal_value);
    const TimeTicks end_time =
        TimeTicks::FromInternalValue(end_time_ticks_internal_value);
    TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString(tracing_id),
                      perfetto::ThreadTrack::ForThread(tid), start_time);
    TRACE_EVENT_END(kTraceCategory, perfetto::ThreadTrack::ForThread(tid),
                    end_time);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
  }

  void ReportTraceEvent(
      partition_alloc::internal::StatsCollector::MutatorId id,
      [[maybe_unused]] partition_alloc::internal::base::PlatformThreadId tid,
      int64_t start_time_ticks_internal_value,
      int64_t end_time_ticks_internal_value) override {
#if BUILDFLAG(ENABLE_BASE_TRACING)
    // TRACE_EVENT_* macros below drop most parameters when tracing is
    // disabled at compile time.
    const char* tracing_id = MutatorIdToTracingString(id);
    const TimeTicks start_time =
        TimeTicks::FromInternalValue(start_time_ticks_internal_value);
    const TimeTicks end_time =
        TimeTicks::FromInternalValue(end_time_ticks_internal_value);
    TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString(tracing_id),
                      perfetto::ThreadTrack::ForThread(tid), start_time);
    TRACE_EVENT_END(kTraceCategory, perfetto::ThreadTrack::ForThread(tid),
                    end_time);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
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

  void ReportStats(const char* stats_name, int64_t sample_in_usec) override {
    TimeDelta sample = Microseconds(sample_in_usec);
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

  partition_alloc::internal::PCScan::RegisterStatsReporter(&s_reporter);
  registered = true;
}
#endif  // defined(PA_ALLOW_PCSCAN)

namespace {

void RunThreadCachePeriodicPurge() {
  // Micros, since periodic purge should typically take at most a few ms.
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Memory.PartitionAlloc.PeriodicPurge");
  TRACE_EVENT0("memory", "PeriodicPurge");
  auto& instance = ::partition_alloc::ThreadCacheRegistry::Instance();
  instance.RunPeriodicPurge();
  TimeDelta delay =
      Microseconds(instance.GetPeriodicPurgeNextIntervalInMicroseconds());
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindOnce(RunThreadCachePeriodicPurge), delay);
}

void RunMemoryReclaimer(scoped_refptr<SequencedTaskRunner> task_runner) {
  TRACE_EVENT0("base", "partition_alloc::MemoryReclaimer::Reclaim()");
  auto* instance = ::partition_alloc::MemoryReclaimer::Instance();

  {
    // Micros, since memory reclaiming should typically take at most a few ms.
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Memory.PartitionAlloc.MemoryReclaim");
    instance->ReclaimNormal();
  }

  TimeDelta delay =
      Microseconds(instance->GetRecommendedReclaimIntervalInMicroseconds());
  task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(RunMemoryReclaimer, task_runner), delay);
}

}  // namespace

void StartThreadCachePeriodicPurge() {
  auto& instance = ::partition_alloc::ThreadCacheRegistry::Instance();
  TimeDelta delay =
      Microseconds(instance.GetPeriodicPurgeNextIntervalInMicroseconds());
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindOnce(RunThreadCachePeriodicPurge), delay);
}

void StartMemoryReclaimer(scoped_refptr<SequencedTaskRunner> task_runner) {
  // Can be called several times.
  static bool is_memory_reclaimer_running = false;
  if (is_memory_reclaimer_running)
    return;
  is_memory_reclaimer_running = true;

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
  auto* instance = ::partition_alloc::MemoryReclaimer::Instance();
  TimeDelta delay =
      Microseconds(instance->GetRecommendedReclaimIntervalInMicroseconds());
  task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(RunMemoryReclaimer, task_runner), delay);
}

std::map<std::string, std::string> ProposeSyntheticFinchTrials() {
  std::map<std::string, std::string> trials;

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // BackupRefPtr_Effective and PCScan_Effective record whether or not
  // BackupRefPtr and/or PCScan are enabled. The experiments aren't independent,
  // so having a synthetic Finch will help look only at cases where one isn't
  // affected by the other.

  // Whether PartitionAllocBackupRefPtr is enabled (as determined by
  // FeatureList::IsEnabled).
  [[maybe_unused]] bool brp_finch_enabled = false;
  // Whether PartitionAllocBackupRefPtr is set up for the default behavior. The
  // default behavior is when either the Finch flag is disabled, or is enabled
  // in brp-mode=disabled (these two options are equivalent).
  [[maybe_unused]] bool brp_nondefault_behavior = false;
  // Whether PartitionAllocBackupRefPtr is set up to enable BRP protection. It
  // requires the Finch flag to be enabled and brp-mode!=disabled*. Some modes,
  // e.g. disabled-but-3-way-split, do something (hence can't be considered the
  // default behavior), but don't enable BRP protection.
  [[maybe_unused]] bool brp_truly_enabled = false;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (FeatureList::IsEnabled(features::kPartitionAllocBackupRefPtr))
    brp_finch_enabled = true;
  if (brp_finch_enabled && features::kBackupRefPtrModeParam.Get() !=
                               features::BackupRefPtrMode::kDisabled)
    brp_nondefault_behavior = true;
  if (brp_finch_enabled && features::kBackupRefPtrModeParam.Get() ==
                               features::BackupRefPtrMode::kEnabled)
    brp_truly_enabled = true;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  [[maybe_unused]] bool pcscan_enabled =
#if defined(PA_ALLOW_PCSCAN)
      FeatureList::IsEnabled(features::kPartitionAllocPCScanBrowserOnly);
#else
      false;
#endif

  std::string brp_group_name = "Unavailable";
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
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
      case features::BackupRefPtrMode::kEnabledWithoutZapping:
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        brp_group_name = "EnabledPrevSlotWithoutZapping";
#else
        brp_group_name = "EnabledBeforeAllocWithoutZapping";
#endif
        break;
      case features::BackupRefPtrMode::kDisabledButSplitPartitions2Way:
        brp_group_name = "DisabledBut2WaySplit";
        break;
      case features::BackupRefPtrMode::kDisabledButSplitPartitions3Way:
        brp_group_name = "DisabledBut3WaySplit";
        break;
      case features::BackupRefPtrMode::kDisabledButAddDummyRefCount:
        brp_group_name = "DisabledButAddDummyRefCount";
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
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
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
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  trials.emplace("DanglingPointerDetector", "Enabled");
#else
  trials.emplace("DanglingPointerDetector", "Disabled");
#endif

  return trials;
}

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

namespace {

internal::PartitionLock g_stack_trace_buffer_lock;

struct StackTraceWithID {
  debug::StackTrace stack_trace;
  uintptr_t id = 0;
};
using DanglingRawPtrBuffer = std::array<absl::optional<StackTraceWithID>, 32>;
DanglingRawPtrBuffer g_stack_trace_buffer GUARDED_BY(g_stack_trace_buffer_lock);

void DanglingRawPtrDetected(uintptr_t id) {
  // This is called from inside the allocator. No allocation is allowed.

  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);

#if DCHECK_IS_ON()
  for (absl::optional<StackTraceWithID>& entry : g_stack_trace_buffer)
    PA_DCHECK(!entry || entry->id != id);
#endif  // DCHECK_IS_ON()

  for (absl::optional<StackTraceWithID>& entry : g_stack_trace_buffer) {
    if (!entry) {
      entry = {debug::StackTrace(), id};
      return;
    }
  }

  // The StackTrace hasn't been recorded, because the buffer isn't large
  // enough.
}

// From the StackTrace recorded in |DanglingRawPtrDetected|, extract the one
// whose id match |id|. Return nullopt if not found.
absl::optional<debug::StackTrace> TakeStackTrace(uintptr_t id) {
  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);
  for (absl::optional<StackTraceWithID>& entry : g_stack_trace_buffer) {
    if (entry && entry->id == id) {
      debug::StackTrace stack_trace = std::move(entry->stack_trace);
      entry = absl::nullopt;
      return stack_trace;
    }
  }
  return absl::nullopt;
}

// Extract from the StackTrace output, the signature of the pertinent caller.
// This function is meant to be used only by Chromium developers, to list what
// are all the dangling raw_ptr occurrences in a table.
std::string ExtractDanglingPtrSignature(std::string stacktrace) {
  std::vector<StringPiece> lines = SplitStringPiece(
      stacktrace, "\r\n", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);

  // We are looking for the callers of the function releasing the raw_ptr and
  // freeing memory:
  const StringPiece callees[] = {
      "internal::BackupRefPtrImpl<>::ReleaseInternal()",
      "internal::PartitionFree()",
      "base::(anonymous namespace)::FreeFn()",
  };
  size_t caller_index = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    for (const auto& callee : callees) {
      if (lines[i].find(callee) != StringPiece::npos) {
        caller_index = i + 1;
      }
    }
  }
  if (caller_index >= lines.size()) {
    return "undefined";
  }
  StringPiece caller = lines[caller_index];

  // |callers| follows the following format:
  //
  //    #4 0x56051fe3404b content::GeneratedCodeCache::DidCreateBackend()
  //    -- -------------- -----------------------------------------------
  // Depth Address        Function

  size_t address_start = caller.find(' ');
  size_t function_start = caller.find(' ', address_start + 1);

  if (address_start == caller.npos || function_start == caller.npos) {
    return "undefined";
  }

  return std::string(caller.substr(function_start + 1));
}

void DanglingRawPtrReleasedLogSignature(uintptr_t id) {
  // This is called from raw_ptr<>'s release operation. Making allocations is
  // allowed. In particular, symbolizing and printing the StackTraces may
  // allocate memory.

  debug::StackTrace stack_trace_release;
  absl::optional<debug::StackTrace> stack_trace_free = TakeStackTrace(id);

  if (stack_trace_free) {
    LOG(ERROR) << StringPrintf(
        "[DanglingSignature]\t%s\t%s",
        ExtractDanglingPtrSignature(stack_trace_release.ToString()).c_str(),
        ExtractDanglingPtrSignature(stack_trace_free->ToString()).c_str());
  } else {
    LOG(ERROR) << StringPrintf(
        "[DanglingSignature]\t%s\tmissing-stacktrace",
        ExtractDanglingPtrSignature(stack_trace_release.ToString()).c_str());
  }
}

void DanglingRawPtrReleasedCrash(uintptr_t id) {
  // This is called from raw_ptr<>'s release operation. Making allocations is
  // allowed. In particular, symbolizing and printing the StackTraces may
  // allocate memory.
  debug::StackTrace stack_trace_release;
  debug::TaskTrace task_trace_release;
  absl::optional<debug::StackTrace> stack_trace_free = TakeStackTrace(id);

  static const char dangling_ptr_footer[] =
      "\n"
      "\n"
      "Please check for more information on:\n"
      "https://chromium.googlesource.com/chromium/src/+/main/docs/"
      "dangling_ptr_guide.md\n";

  if (stack_trace_free) {
    LOG(ERROR) << "Detected dangling raw_ptr with id="
               << StringPrintf("0x%016" PRIxPTR, id) << ":\n\n"
               << "The memory was freed at:\n"
               << *stack_trace_free << "\n"
               << "The dangling raw_ptr was released at:\n"
               << stack_trace_release << task_trace_release
               << dangling_ptr_footer;
  } else {
    LOG(ERROR) << "Detected dangling raw_ptr with id="
               << StringPrintf("0x%016" PRIxPTR, id) << ":\n\n"
               << "It was not recorded where the memory was freed.\n\n"
               << "The dangling raw_ptr was released at:\n"
               << stack_trace_release << task_trace_release
               << dangling_ptr_footer;
  }
  ImmediateCrash();
}

void ClearDanglingRawPtrBuffer() {
  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);
  g_stack_trace_buffer = DanglingRawPtrBuffer();
}

}  // namespace

void InstallDanglingRawPtrChecks() {
  // Clearing storage is useful for running multiple unit tests without
  // restarting the test executable.
  ClearDanglingRawPtrBuffer();

  if (!FeatureList::IsEnabled(features::kPartitionAllocDanglingPtr)) {
    partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t) {});
    partition_alloc::SetDanglingRawPtrReleasedFn([](uintptr_t) {});
    return;
  }

  switch (features::kDanglingPtrModeParam.Get()) {
    case features::DanglingPtrMode::kCrash:
      partition_alloc::SetDanglingRawPtrDetectedFn(DanglingRawPtrDetected);
      partition_alloc::SetDanglingRawPtrReleasedFn(DanglingRawPtrReleasedCrash);
      break;

    case features::DanglingPtrMode::kLogSignature:
      partition_alloc::SetDanglingRawPtrDetectedFn(DanglingRawPtrDetected);
      partition_alloc::SetDanglingRawPtrReleasedFn(
          DanglingRawPtrReleasedLogSignature);
      break;
  }
}

// TODO(arthursonzogni): There might exist long lived dangling raw_ptr. If there
// is a dangling pointer, we should crash at some point. Consider providing an
// API to periodically check the buffer.

#else   // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
void InstallDanglingRawPtrChecks() {}
#endif  // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

void UnretainedDanglingRawPtrDetectedDumpWithoutCrashing(uintptr_t id) {
  PA_NO_CODE_FOLDING();
  debug::DumpWithoutCrashing();
}

void UnretainedDanglingRawPtrDetectedCrash(uintptr_t id) {
  debug::TaskTrace task_trace;
  debug::StackTrace stack_trace;
  LOG(ERROR) << "Detected dangling raw_ptr in unretained with id="
             << StringPrintf("0x%016" PRIxPTR, id) << ":\n\n"
             << task_trace << stack_trace;
  ImmediateCrash();
}

void InstallUnretainedDanglingRawPtrChecks() {
  if (!FeatureList::IsEnabled(features::kPartitionAllocUnretainedDanglingPtr)) {
    partition_alloc::SetUnretainedDanglingRawPtrDetectedFn([](uintptr_t) {});
    partition_alloc::SetUnretainedDanglingRawPtrCheckEnabled(/*enabled=*/false);
    return;
  }

  partition_alloc::SetUnretainedDanglingRawPtrCheckEnabled(/*enabled=*/true);
  switch (features::kUnretainedDanglingPtrModeParam.Get()) {
    case features::UnretainedDanglingPtrMode::kCrash:
      partition_alloc::SetUnretainedDanglingRawPtrDetectedFn(
          &UnretainedDanglingRawPtrDetectedCrash);
      break;

    case features::UnretainedDanglingPtrMode::kDumpWithoutCrashing:
      partition_alloc::SetUnretainedDanglingRawPtrDetectedFn(
          &UnretainedDanglingRawPtrDetectedDumpWithoutCrashing);
      break;
  }
}

}  // namespace allocator
}  // namespace base
