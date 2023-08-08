// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <map>
#include <string>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/partition_alloc_base/threading/platform_thread.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/at_exit.h"
#include "base/check.h"
#include "base/cpu.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/immediate_crash.h"
#include "base/location.h"
#include "base/memory/raw_ptr_asan_service.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/pending_task.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/shim/nonscannable_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"
#endif  // BUILDFLAG(USE_STARSCAN)

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#endif

namespace base::allocator {

namespace {

// When under this experiment avoid running periodic purging or reclaim for the
// first minute after the first attempt. This is based on the insight that
// processes often don't live paste this minute.
static BASE_FEATURE(kDelayFirstPeriodicPAPurgeOrReclaim,
                    "DelayFirstPeriodicPAPurgeOrReclaim",
                    base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::TimeDelta kFirstPAPurgeOrReclaimDelay = base::Minutes(1);

// This is defined in content/public/common/content_switches.h, which is not
// accessible in ::base. They must be kept in sync.
namespace switches {
[[maybe_unused]] constexpr char kRendererProcess[] = "renderer";
constexpr char kZygoteProcess[] = "zygote";
#if BUILDFLAG(USE_STARSCAN)
constexpr char kGpuProcess[] = "gpu-process";
constexpr char kUtilityProcess[] = "utility";
#endif
}  // namespace switches

#if BUILDFLAG(USE_STARSCAN)

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

#endif  // BUILDFLAG(USE_STARSCAN)

}  // namespace

#if BUILDFLAG(USE_STARSCAN)
void RegisterPCScanStatsReporter() {
  static StatsReporterImpl s_reporter;
  static bool registered = false;

  DCHECK(!registered);

  partition_alloc::internal::PCScan::RegisterStatsReporter(&s_reporter);
  registered = true;
}
#endif  // BUILDFLAG(USE_STARSCAN)

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

  TimeDelta delay = features::kPartitionAllocMemoryReclaimerInterval.Get();
  if (!delay.is_positive()) {
    delay =
        Microseconds(instance->GetRecommendedReclaimIntervalInMicroseconds());
  }

  task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(RunMemoryReclaimer, task_runner), delay);
}

}  // namespace

void StartThreadCachePeriodicPurge() {
  auto& instance = ::partition_alloc::ThreadCacheRegistry::Instance();
  TimeDelta delay =
      Microseconds(instance.GetPeriodicPurgeNextIntervalInMicroseconds());

  if (base::FeatureList::IsEnabled(kDelayFirstPeriodicPAPurgeOrReclaim)) {
    delay = std::max(delay, kFirstPAPurgeOrReclaimDelay);
  }

  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindOnce(RunThreadCachePeriodicPurge), delay);
}

void StartMemoryReclaimer(scoped_refptr<SequencedTaskRunner> task_runner) {
  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocMemoryReclaimer)) {
    return;
  }

  // Can be called several times.
  static bool is_memory_reclaimer_running = false;
  if (is_memory_reclaimer_running) {
    return;
  }
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
  TimeDelta delay = features::kPartitionAllocMemoryReclaimerInterval.Get();
  if (!delay.is_positive()) {
    delay = Microseconds(::partition_alloc::MemoryReclaimer::Instance()
                             ->GetRecommendedReclaimIntervalInMicroseconds());
  }

  if (base::FeatureList::IsEnabled(kDelayFirstPeriodicPAPurgeOrReclaim)) {
    delay = std::max(delay, kFirstPAPurgeOrReclaimDelay);
  }

  task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(RunMemoryReclaimer, task_runner), delay);
}

std::map<std::string, std::string> ProposeSyntheticFinchTrials() {
  std::map<std::string, std::string> trials;

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  trials.emplace("DanglingPointerDetector", "Enabled");
#else
  trials.emplace("DanglingPointerDetector", "Disabled");
#endif

  // This value is not surrounded by build flags as it is meant to be updated
  // manually in binary experiment patches.
  trials.emplace("VectorRawPtrExperiment", "Disabled");

#if BUILDFLAG(FORCIBLY_ENABLE_BACKUP_REF_PTR_IN_ALL_PROCESSES)
  trials.emplace(base::features::kRendererLiveBRPSyntheticTrialName, "Enabled");
#else
  trials.emplace(base::features::kRendererLiveBRPSyntheticTrialName, "Control");
#endif

#if PA_CONFIG(HAS_MEMORY_TAGGING)
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocMemoryTagging)) {
    if (base::CPU::GetInstanceNoAllocation().has_mte()) {
      trials.emplace("MemoryTaggingDogfood", "Enabled");
    } else {
      trials.emplace("MemoryTaggingDogfood", "Disabled");
    }
  }
#endif

  return trials;
}

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

namespace {

internal::PartitionLock g_stack_trace_buffer_lock;

struct DanglingPointerFreeInfo {
  debug::StackTrace stack_trace;
  debug::TaskTrace task_trace;
  uintptr_t id = 0;
};
using DanglingRawPtrBuffer =
    std::array<absl::optional<DanglingPointerFreeInfo>, 32>;
DanglingRawPtrBuffer g_stack_trace_buffer GUARDED_BY(g_stack_trace_buffer_lock);

void DanglingRawPtrDetected(uintptr_t id) {
  // This is called from inside the allocator. No allocation is allowed.

  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);

#if DCHECK_IS_ON()
  for (absl::optional<DanglingPointerFreeInfo>& entry : g_stack_trace_buffer) {
    PA_DCHECK(!entry || entry->id != id);
  }
#endif  // DCHECK_IS_ON()

  for (absl::optional<DanglingPointerFreeInfo>& entry : g_stack_trace_buffer) {
    if (!entry) {
      entry = {debug::StackTrace(), debug::TaskTrace(), id};
      return;
    }
  }

  // The StackTrace hasn't been recorded, because the buffer isn't large
  // enough.
}

// From the traces recorded in |DanglingRawPtrDetected|, extract the one
// whose id match |id|. Return nullopt if not found.
absl::optional<DanglingPointerFreeInfo> TakeDanglingPointerFreeInfo(
    uintptr_t id) {
  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);
  for (absl::optional<DanglingPointerFreeInfo>& entry : g_stack_trace_buffer) {
    if (entry && entry->id == id) {
      absl::optional<DanglingPointerFreeInfo> result(entry);
      entry = absl::nullopt;
      return result;
    }
  }
  return absl::nullopt;
}

// Extract from the StackTrace output, the signature of the pertinent caller.
// This function is meant to be used only by Chromium developers, to list what
// are all the dangling raw_ptr occurrences in a table.
std::string ExtractDanglingPtrSignature(std::string stacktrace) {
  std::vector<StringPiece> lines = SplitStringPiece(
      stacktrace, "\r\n", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);

  // We are looking for the callers of the function releasing the raw_ptr and
  // freeing memory:
  const StringPiece callees[] = {
      // Common signatures
      "internal::PartitionFree",
      "base::(anonymous namespace)::FreeFn",

      // Linux signatures
      "internal::RawPtrBackupRefImpl<>::ReleaseInternal()",
      "base::RefCountedThreadSafe<>::Release()",

      // Windows signatures
      "internal::RawPtrBackupRefImpl<0,0>::ReleaseInternal",
      "internal::RawPtrBackupRefImpl<0,1>::ReleaseInternal",
      "_free_base",

      // Mac signatures
      "internal::RawPtrBackupRefImpl<false, false>::ReleaseInternal",
      "internal::RawPtrBackupRefImpl<false, true>::ReleaseInternal",

      // Task traces are prefixed with "Task trace:" in
      // |TaskTrace::OutputToStream|
      "Task trace:",
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
    return "no_callee_match";
  }
  StringPiece caller = lines[caller_index];

  if (caller.empty()) {
    return "invalid_format";
  }

  // On Posix platforms |callers| follows the following format:
  //
  // #<index> <address> <symbol>
  //
  // See https://crsrc.org/c/base/debug/stack_trace_posix.cc
  if (caller[0] == '#') {
    const size_t address_start = caller.find(' ');
    const size_t function_start = caller.find(' ', address_start + 1);

    if (address_start == caller.npos || function_start == caller.npos) {
      return "invalid_format";
    }

    return std::string(caller.substr(function_start + 1));
  }

  // On Windows platforms |callers| follows the following format:
  //
  // \t<symbol> [0x<address>]+<displacement>(<filename>:<line>)
  //
  // See https://crsrc.org/c/base/debug/stack_trace_win.cc
  if (caller[0] == '\t') {
    const size_t symbol_start = 1;
    const size_t symbol_end = caller.find(' ');
    if (symbol_end == caller.npos) {
      return "invalid_format";
    }
    return std::string(caller.substr(symbol_start, symbol_end - symbol_start));
  }

  // On Mac platforms |callers| follows the following format:
  //
  // <index> <library> 0x<address> <symbol> + <line>
  //
  // See https://crsrc.org/c/base/debug/stack_trace_posix.cc
  if (caller[0] >= '0' && caller[0] <= '9') {
    const size_t address_start = caller.find("0x");
    const size_t symbol_start = caller.find(' ', address_start + 1) + 1;
    const size_t symbol_end = caller.find(' ', symbol_start);
    if (symbol_start == caller.npos || symbol_end == caller.npos) {
      return "invalid_format";
    }
    return std::string(caller.substr(symbol_start, symbol_end - symbol_start));
  }

  return "invalid_format";
}

std::string ExtractDanglingPtrSignature(debug::TaskTrace task_trace) {
  if (task_trace.empty()) {
    return "No active task";
  }
  return ExtractDanglingPtrSignature(task_trace.ToString());
}

std::string ExtractDanglingPtrSignature(
    absl::optional<DanglingPointerFreeInfo> free_info,
    debug::StackTrace release_stack_trace,
    debug::TaskTrace release_task_trace) {
  if (free_info) {
    return StringPrintf(
        "[DanglingSignature]\t%s\t%s\t%s\t%s",
        ExtractDanglingPtrSignature(free_info->stack_trace.ToString()).c_str(),
        ExtractDanglingPtrSignature(free_info->task_trace).c_str(),
        ExtractDanglingPtrSignature(release_stack_trace.ToString()).c_str(),
        ExtractDanglingPtrSignature(release_task_trace).c_str());
  }
  return StringPrintf(
      "[DanglingSignature]\t%s\t%s\t%s\t%s", "missing", "missing",
      ExtractDanglingPtrSignature(release_stack_trace.ToString()).c_str(),
      ExtractDanglingPtrSignature(release_task_trace).c_str());
}

bool operator==(const debug::TaskTrace& lhs, const debug::TaskTrace& rhs) {
  // Compare the addresses contained in the task traces.
  // The task traces are at most |PendingTask::kTaskBacktraceLength| long.
  std::array<const void*, PendingTask::kTaskBacktraceLength> addresses_lhs = {};
  std::array<const void*, PendingTask::kTaskBacktraceLength> addresses_rhs = {};
  lhs.GetAddresses(addresses_lhs);
  rhs.GetAddresses(addresses_rhs);
  return addresses_lhs == addresses_rhs;
}

template <features::DanglingPtrMode dangling_pointer_mode,
          features::DanglingPtrType dangling_pointer_type>
void DanglingRawPtrReleased(uintptr_t id) {
  // This is called from raw_ptr<>'s release operation. Making allocations is
  // allowed. In particular, symbolizing and printing the StackTraces may
  // allocate memory.
  debug::StackTrace stack_trace_release;
  debug::TaskTrace task_trace_release;
  absl::optional<DanglingPointerFreeInfo> free_info =
      TakeDanglingPointerFreeInfo(id);

  if constexpr (dangling_pointer_type ==
                features::DanglingPtrType::kCrossTask) {
    if (!free_info) {
      return;
    }
    if (task_trace_release == free_info->task_trace) {
      return;
    }
  }

  std::string dangling_signature = ExtractDanglingPtrSignature(
      free_info, stack_trace_release, task_trace_release);
  static const char dangling_ptr_footer[] =
      "\n"
      "\n"
      "Please check for more information on:\n"
      "https://chromium.googlesource.com/chromium/src/+/main/docs/"
      "dangling_ptr_guide.md\n"
      "\n"
      "Googlers: Please give us your feedback about the dangling pointer\n"
      "          detector at:\n"
      "          http://go/dangling-ptr-cq-survey\n";
  if (free_info) {
    LOG(ERROR) << "Detected dangling raw_ptr with id="
               << StringPrintf("0x%016" PRIxPTR, id) << ":\n"
               << dangling_signature << "\n\n"
               << "The memory was freed at:\n"
               << free_info->stack_trace << "\n"
               << free_info->task_trace << "\n"
               << "The dangling raw_ptr was released at:\n"
               << stack_trace_release << "\n"
               << task_trace_release << dangling_ptr_footer;
  } else {
    LOG(ERROR) << "Detected dangling raw_ptr with id="
               << StringPrintf("0x%016" PRIxPTR, id) << ":\n\n"
               << dangling_signature << "\n\n"
               << "It was not recorded where the memory was freed.\n\n"
               << "The dangling raw_ptr was released at:\n"
               << stack_trace_release << "\n"
               << task_trace_release << dangling_ptr_footer;
  }

  if constexpr (dangling_pointer_mode == features::DanglingPtrMode::kCrash) {
    ImmediateCrash();
  }
}

void CheckDanglingRawPtrBufferEmpty() {
  internal::PartitionAutoLock guard(g_stack_trace_buffer_lock);

  // TODO(https://crbug.com/1425095): Check for leaked refcount on Android.
#if BUILDFLAG(IS_ANDROID)
  g_stack_trace_buffer = DanglingRawPtrBuffer();
#else
  bool errors = false;
  for (auto entry : g_stack_trace_buffer) {
    if (!entry) {
      continue;
    }
    errors = true;
    LOG(ERROR) << "A freed allocation is still referenced by a dangling "
                  "pointer at exit, or at test end. Leaked raw_ptr/raw_ref "
                  "could cause PartitionAlloc's quarantine memory bloat."
                  "\n\n"
                  "Memory was released on:\n"
               << entry->task_trace << "\n"
               << entry->stack_trace << "\n";
  }
  CHECK(!errors);
#endif
}

}  // namespace

void InstallDanglingRawPtrChecks() {
  // Multiple tests can run within the same executable's execution. This line
  // ensures problems detected from the previous test are causing error before
  // entering the next one...
  CheckDanglingRawPtrBufferEmpty();

  // ... similarly, some allocation may stay forever in the quarantine and we
  // might ignore them if the executable exists. This line makes sure dangling
  // pointers errors are never ignored, by crashing at exit, as a last resort.
  // This makes quarantine memory bloat more likely to be detected.
  static bool first_run_in_process = true;
  if (first_run_in_process) {
    first_run_in_process = false;
    AtExitManager::RegisterTask(base::BindOnce(CheckDanglingRawPtrBufferEmpty));
  }

  if (!FeatureList::IsEnabled(features::kPartitionAllocDanglingPtr)) {
    partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t) {});
    partition_alloc::SetDanglingRawPtrReleasedFn([](uintptr_t) {});
    return;
  }

  partition_alloc::SetDanglingRawPtrDetectedFn(&DanglingRawPtrDetected);
  switch (features::kDanglingPtrModeParam.Get()) {
    case features::DanglingPtrMode::kCrash:
      switch (features::kDanglingPtrTypeParam.Get()) {
        case features::DanglingPtrType::kAll:
          partition_alloc::SetDanglingRawPtrReleasedFn(
              &DanglingRawPtrReleased<features::DanglingPtrMode::kCrash,
                                      features::DanglingPtrType::kAll>);
          break;
        case features::DanglingPtrType::kCrossTask:
          partition_alloc::SetDanglingRawPtrReleasedFn(
              &DanglingRawPtrReleased<features::DanglingPtrMode::kCrash,
                                      features::DanglingPtrType::kCrossTask>);
          break;
      }
      break;
    case features::DanglingPtrMode::kLogOnly:
      switch (features::kDanglingPtrTypeParam.Get()) {
        case features::DanglingPtrType::kAll:
          partition_alloc::SetDanglingRawPtrReleasedFn(
              &DanglingRawPtrReleased<features::DanglingPtrMode::kLogOnly,
                                      features::DanglingPtrType::kAll>);
          break;
        case features::DanglingPtrType::kCrossTask:
          partition_alloc::SetDanglingRawPtrReleasedFn(
              &DanglingRawPtrReleased<features::DanglingPtrMode::kLogOnly,
                                      features::DanglingPtrType::kCrossTask>);
          break;
      }
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

namespace {

#if BUILDFLAG(USE_STARSCAN)
void SetProcessNameForPCScan(const std::string& process_type) {
  const char* name = [&process_type] {
    if (process_type.empty()) {
      // Empty means browser process.
      return "Browser";
    }
    if (process_type == switches::kRendererProcess) {
      return "Renderer";
    }
    if (process_type == switches::kGpuProcess) {
      return "Gpu";
    }
    if (process_type == switches::kUtilityProcess) {
      return "Utility";
    }
    return static_cast<const char*>(nullptr);
  }();

  if (name) {
    partition_alloc::internal::PCScan::SetProcessName(name);
  }
}

bool EnablePCScanForMallocPartitionsIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partition_alloc::internal::base::PlatformThread::SetThreadNameHook(
      &base::PlatformThread::SetName);

  using Config = partition_alloc::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    allocator_shim::EnablePCScan({Config::WantedWriteProtectionMode::kEnabled,
                                  Config::SafepointMode::kEnabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return false;
}

bool EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  using Config = partition_alloc::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    allocator_shim::EnablePCScan({wp_mode, Config::SafepointMode::kEnabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return false;
}

bool EnablePCScanForMallocPartitionsInRendererProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  using Config = partition_alloc::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanRendererOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    allocator_shim::EnablePCScan({wp_mode, Config::SafepointMode::kDisabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return false;
}
#endif  // BUILDFLAG(USE_STARSCAN)

}  // namespace

void ReconfigurePartitionForKnownProcess(const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(keishi): Move the code to enable BRP back here after Finch
  // experiments.
}

PartitionAllocSupport* PartitionAllocSupport::Get() {
  static auto* singleton = new PartitionAllocSupport();
  return singleton;
}

PartitionAllocSupport::PartitionAllocSupport() = default;

void PartitionAllocSupport::ReconfigureForTests() {
  ReconfigureEarlyish("");
  base::AutoLock scoped_lock(lock_);
  called_for_tests_ = true;
}

// static
bool PartitionAllocSupport::ShouldEnableMemoryTagging(
    const std::string& process_type) {
  // Check kPartitionAllocMemoryTagging first so the Feature is activated even
  // when mte bootloader flag is disabled.
  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocMemoryTagging)) {
    return false;
  }
  if (!base::CPU::GetInstanceNoAllocation().has_mte()) {
    return false;
  }

  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kKillPartitionAllocMemoryTagging)) {
    return false;
  }
  switch (base::features::kMemoryTaggingEnabledProcessesParam.Get()) {
    case base::features::MemoryTaggingEnabledProcesses::kBrowserOnly:
      return process_type.empty();
    case base::features::MemoryTaggingEnabledProcesses::kNonRenderer:
      return process_type != switches::kRendererProcess;
    case base::features::MemoryTaggingEnabledProcesses::kAllProcesses:
      return true;
  }
}

// static
bool PartitionAllocSupport::ShouldEnableMemoryTaggingInRendererProcess() {
  return ShouldEnableMemoryTagging(switches::kRendererProcess);
}

// static
PartitionAllocSupport::BrpConfiguration
PartitionAllocSupport::GetBrpConfiguration(const std::string& process_type) {
  // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
  CHECK(base::FeatureList::GetInstance());

  bool enable_brp = false;
  bool enable_brp_for_ash = false;
  bool split_main_partition = false;
  bool use_dedicated_aligned_partition = false;
  bool process_affected_by_brp_flag = false;
  size_t ref_count_size = 0;

#if (BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&  \
     BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)) || \
    BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#if BUILDFLAG(FORCIBLY_ENABLE_BACKUP_REF_PTR_IN_ALL_PROCESSES)
  process_affected_by_brp_flag = true;
#else
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocBackupRefPtr)) {
    // No specified process type means this is the Browser process.
    switch (base::features::kBackupRefPtrEnabledProcessesParam.Get()) {
      case base::features::BackupRefPtrEnabledProcesses::kBrowserOnly:
        process_affected_by_brp_flag = process_type.empty();
        break;
      case base::features::BackupRefPtrEnabledProcesses::kBrowserAndRenderer:
        process_affected_by_brp_flag =
            process_type.empty() ||
            (process_type == switches::kRendererProcess);
        break;
      case base::features::BackupRefPtrEnabledProcesses::kNonRenderer:
        process_affected_by_brp_flag =
            (process_type != switches::kRendererProcess);
        break;
      case base::features::BackupRefPtrEnabledProcesses::kAllProcesses:
        process_affected_by_brp_flag = true;
        break;
    }
  }
#endif  // BUILDFLAG(FORCIBLY_ENABLE_BACKUP_REF_PTR_IN_ALL_PROCESSES)
#endif  // (BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)) ||
        // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (process_affected_by_brp_flag) {
    switch (base::features::kBackupRefPtrModeParam.Get()) {
      case base::features::BackupRefPtrMode::kDisabled:
        // Do nothing. Equivalent to !IsEnabled(kPartitionAllocBackupRefPtr).
        break;

      case base::features::BackupRefPtrMode::kEnabled:
        enable_brp = true;
        split_main_partition = true;
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        // AlignedAlloc relies on natural alignment offered by the allocator
        // (see the comment inside PartitionRoot::AlignedAllocFlags). Any extras
        // in front of the allocation will mess up that alignment. Such extras
        // are used when BackupRefPtr is on, in which case, we need a separate
        // partition, dedicated to handle only aligned allocations, where those
        // extras are disabled. However, if the "previous slot" variant is used,
        // no dedicated partition is needed, as the extras won't interfere with
        // the alignment requirements.
        use_dedicated_aligned_partition = true;
#endif
        break;

      case base::features::BackupRefPtrMode::kDisabledButSplitPartitions2Way:
        split_main_partition = true;
        break;

      case base::features::BackupRefPtrMode::kDisabledButSplitPartitions3Way:
        split_main_partition = true;
        use_dedicated_aligned_partition = true;
        break;
    }

    if (enable_brp) {
      switch (base::features::kBackupRefPtrRefCountSizeParam.Get()) {
        case base::features::BackupRefPtrRefCountSize::kNatural:
          ref_count_size = 0;
          break;
        case base::features::BackupRefPtrRefCountSize::k4B:
          ref_count_size = 4;
          break;
        case base::features::BackupRefPtrRefCountSize::k8B:
          ref_count_size = 8;
          break;
        case base::features::BackupRefPtrRefCountSize::k16B:
          ref_count_size = 16;
          break;
      }
    }
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  // Enabling BRP for Ash makes sense only when BRP is enabled. If it wasn't,
  // there would be no BRP pool, thus BRP would be equally inactive for Ash
  // pointers.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  enable_brp_for_ash =
      enable_brp && base::FeatureList::IsEnabled(
                        base::features::kPartitionAllocBackupRefPtrForAsh);
#endif

  return {
      enable_brp,
      enable_brp_for_ash,
      split_main_partition,
      use_dedicated_aligned_partition,
      process_affected_by_brp_flag,
      ref_count_size,
  };
}

void PartitionAllocSupport::ReconfigureEarlyish(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);

    // In tests, ReconfigureEarlyish() is called by ReconfigureForTest(), which
    // is earlier than ContentMain().
    if (called_for_tests_) {
      DCHECK(called_earlyish_);
      return;
    }

    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_earlyish_)
        << "ReconfigureEarlyish was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_earlyish_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }

  // These initializations are only relevant for PartitionAlloc-Everywhere
  // builds.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  allocator_shim::EnablePartitionAllocMemoryReclaimer();
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::ReconfigureAfterZygoteFork(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_after_zygote_fork_)
        << "ReconfigureAfterZygoteFork was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterZygoteFork without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterZygoteFork while "
           "ReconfigureEarlyish was called on non-zygote process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_zygote_fork_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }
}

void PartitionAllocSupport::ReconfigureAfterFeatureListInit(
    const std::string& process_type,
    bool configure_dangling_pointer_detector) {
  if (configure_dangling_pointer_detector) {
    base::allocator::InstallDanglingRawPtrChecks();
  }
  base::allocator::InstallUnretainedDanglingRawPtrChecks();
  {
    base::AutoLock scoped_lock(lock_);
    // Avoid initializing more than once.
    // TODO(bartekn): See if can be converted to (D)CHECK.
    if (called_after_feature_list_init_) {
      DCHECK_EQ(established_process_type_, process_type)
          << "ReconfigureAfterFeatureListInit was already called for process '"
          << established_process_type_ << "'; current process: '"
          << process_type << "'";
      return;
    }
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_NE(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureAfterZygoteFork; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, process_type)
        << "ReconfigureAfterFeatureListInit wasn't called for an already "
           "established process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_feature_list_init_ = true;
  }

  DCHECK_NE(process_type, switches::kZygoteProcess);
  [[maybe_unused]] BrpConfiguration brp_config =
      GetBrpConfiguration(process_type);

  if (brp_config.enable_brp_for_ash) {
    // This must be enabled before the BRP partition is created. See
    // RawPtrBackupRefImpl::UseBrp().
    base::RawPtrGlobalSettings::EnableExperimentalAsh();
  }

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
  if (brp_config.process_affected_by_brp_flag) {
    base::RawPtrAsanService::GetInstance().Configure(
        base::EnableDereferenceCheck(
            base::features::kBackupRefPtrAsanEnableDereferenceCheckParam.Get()),
        base::EnableExtractionCheck(
            base::features::kBackupRefPtrAsanEnableExtractionCheckParam.Get()),
        base::EnableInstantiationCheck(
            base::features::kBackupRefPtrAsanEnableInstantiationCheckParam
                .Get()));
  } else {
    base::RawPtrAsanService::GetInstance().Configure(
        base::EnableDereferenceCheck(false), base::EnableExtractionCheck(false),
        base::EnableInstantiationCheck(false));
  }
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto bucket_distribution = allocator_shim::BucketDistribution::kNeutral;
  // No specified type means we are in the browser.
  switch (process_type == ""
              ? base::features::kPartitionAllocBucketDistributionParam.Get()
              : base::features::BucketDistributionMode::kDefault) {
    case base::features::BucketDistributionMode::kDefault:
      break;
    case base::features::BucketDistributionMode::kDenser:
      bucket_distribution = allocator_shim::BucketDistribution::kDenser;
      break;
  }

  bool enable_memory_tagging = false;
  partition_alloc::TagViolationReportingMode memory_tagging_reporting_mode =
      partition_alloc::TagViolationReportingMode::kUndefined;

#if PA_CONFIG(HAS_MEMORY_TAGGING)
  // ShouldEnableMemoryTagging() checks kKillPartitionAllocMemoryTagging but
  // check here too to wrap the GetMemoryTaggingModeForCurrentThread() call.
  if (!base::FeatureList::IsEnabled(
          base::features::kKillPartitionAllocMemoryTagging)) {
    // If synchronous mode is enabled from startup it means this is a test and
    // memory tagging should be enabled.
    if (partition_alloc::internal::GetMemoryTaggingModeForCurrentThread() ==
        partition_alloc::TagViolationReportingMode::kSynchronous) {
      enable_memory_tagging = true;
      memory_tagging_reporting_mode =
          partition_alloc::TagViolationReportingMode::kSynchronous;
    } else {
      enable_memory_tagging = ShouldEnableMemoryTagging(process_type);
#if BUILDFLAG(IS_ANDROID)
      if (enable_memory_tagging) {
        switch (base::features::kMemtagModeParam.Get()) {
          case base::features::MemtagMode::kSync:
            memory_tagging_reporting_mode =
                partition_alloc::TagViolationReportingMode::kSynchronous;
            break;
          case base::features::MemtagMode::kAsync:
            memory_tagging_reporting_mode =
                partition_alloc::TagViolationReportingMode::kAsynchronous;
            break;
        }
        partition_alloc::internal::
            ChangeMemoryTaggingModeForAllThreadsPerProcess(
                memory_tagging_reporting_mode);
        CHECK_EQ(
            partition_alloc::internal::GetMemoryTaggingModeForCurrentThread(),
            memory_tagging_reporting_mode);
      } else if (base::CPU::GetInstanceNoAllocation().has_mte()) {
        memory_tagging_reporting_mode =
            partition_alloc::TagViolationReportingMode::kDisabled;
        partition_alloc::internal::
            ChangeMemoryTaggingModeForAllThreadsPerProcess(
                memory_tagging_reporting_mode);
        CHECK_EQ(
            partition_alloc::internal::GetMemoryTaggingModeForCurrentThread(),
            memory_tagging_reporting_mode);
      }
#endif  // BUILDFLAG(IS_ANDROID)
    }
  }
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

  if (enable_memory_tagging) {
    CHECK((memory_tagging_reporting_mode ==
           partition_alloc::TagViolationReportingMode::kSynchronous) ||
          (memory_tagging_reporting_mode ==
           partition_alloc::TagViolationReportingMode::kAsynchronous));
  } else {
    CHECK((memory_tagging_reporting_mode ==
           partition_alloc::TagViolationReportingMode::kUndefined) ||
          (memory_tagging_reporting_mode ==
           partition_alloc::TagViolationReportingMode::kDisabled));
  }

  allocator_shim::ConfigurePartitions(
      allocator_shim::EnableBrp(brp_config.enable_brp),
      allocator_shim::EnableMemoryTagging(enable_memory_tagging),
      memory_tagging_reporting_mode,
      allocator_shim::SplitMainPartition(brp_config.split_main_partition ||
                                         enable_memory_tagging),
      allocator_shim::UseDedicatedAlignedPartition(
          brp_config.use_dedicated_aligned_partition),
      brp_config.ref_count_size, bucket_distribution);

  const uint32_t extras_size = allocator_shim::GetMainPartitionRootExtrasSize();
  // As per description, extras are optional and are expected not to
  // exceed (cookie + max(BRP ref-count)) == 16 + 16 == 32 bytes.
  // 100 is a reasonable cap for this value.
  UmaHistogramCounts100("Memory.PartitionAlloc.PartitionRoot.ExtrasSize",
                        int(extras_size));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // If BRP is not enabled, check if any of PCScan flags is enabled.
  [[maybe_unused]] bool scan_enabled = false;
#if BUILDFLAG(USE_STARSCAN)
  if (!brp_config.enable_brp) {
    scan_enabled = EnablePCScanForMallocPartitionsIfNeeded();
    // No specified process type means this is the Browser process.
    if (process_type.empty()) {
      scan_enabled = scan_enabled ||
                     EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
    }
    if (process_type == switches::kRendererProcess) {
      scan_enabled = scan_enabled ||
                     EnablePCScanForMallocPartitionsInRendererProcessIfNeeded();
    }
    if (scan_enabled) {
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanStackScanning)) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        partition_alloc::internal::PCScan::EnableStackScanning();
        // Notify PCScan about the main thread.
        partition_alloc::internal::PCScan::NotifyThreadCreated(
            partition_alloc::internal::GetStackTop());
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
      }
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanImmediateFreeing)) {
        partition_alloc::internal::PCScan::EnableImmediateFreeing();
      }
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanEagerClearing)) {
        partition_alloc::internal::PCScan::SetClearType(
            partition_alloc::internal::PCScan::ClearType::kEager);
      }
      SetProcessNameForPCScan(process_type);
    }
  }
#endif  // BUILDFLAG(USE_STARSCAN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if BUILDFLAG(USE_STARSCAN)
  // Non-quarantinable partition is dealing with hot V8's zone allocations.
  // In case PCScan is enabled in Renderer, enable thread cache on this
  // partition. At the same time, thread cache on the main(malloc) partition
  // must be disabled, because only one partition can have it on.
  if (scan_enabled && process_type == switches::kRendererProcess) {
    allocator_shim::NonQuarantinableAllocator::Instance()
        .root()
        ->EnableThreadCacheIfSupported();
  } else
#endif  // BUILDFLAG(USE_STARSCAN)
  {
    allocator_shim::internal::PartitionAllocMalloc::Allocator()
        ->EnableThreadCacheIfSupported();
  }

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocLargeEmptySlotSpanRing)) {
    allocator_shim::internal::PartitionAllocMalloc::Allocator()
        ->EnableLargeEmptySlotSpanRing();
    allocator_shim::internal::PartitionAllocMalloc::AlignedAllocator()
        ->EnableLargeEmptySlotSpanRing();
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(IS_WIN)
  // Browser process only, since this is the one we want to prevent from
  // crashing the most (as it takes down all the tabs).
  if (base::FeatureList::IsEnabled(
          base::features::kPageAllocatorRetryOnCommitFailure) &&
      process_type.empty()) {
    partition_alloc::SetRetryOnCommitFailure(true);
  }
#endif
}

void PartitionAllocSupport::ReconfigureAfterTaskRunnerInit(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);

    // Init only once.
    if (called_after_thread_pool_init_) {
      return;
    }

    DCHECK_EQ(established_process_type_, process_type);
    // Enforce ordering.
    DCHECK(called_earlyish_);
    DCHECK(called_after_feature_list_init_);

    called_after_thread_pool_init_ = true;
  }

#if PA_CONFIG(THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // This should be called in specific processes, as the main thread is
  // initialized later.
  DCHECK(process_type != switches::kZygoteProcess);

  base::allocator::StartThreadCachePeriodicPurge();

#if BUILDFLAG(IS_ANDROID)
  // Lower thread cache limits to avoid stranding too much memory in the caches.
  if (SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled(
          features::kPartialLowEndModeExcludePartitionAllocSupport)) {
    ::partition_alloc::ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
        ::partition_alloc::ThreadCache::kDefaultMultiplier / 2.);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Renderer processes are more performance-sensitive, increase thread cache
  // limits.
  if (process_type == switches::kRendererProcess &&
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocLargeThreadCacheSize)) {
    largest_cached_size_ =
        ::partition_alloc::ThreadCacheLimits::kLargeSizeThreshold;

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
    // Devices almost always report less physical memory than what they actually
    // have, so anything above 3GiB will catch 4GiB and above.
    if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 3500) {
      largest_cached_size_ =
          ::partition_alloc::ThreadCacheLimits::kDefaultSizeThreshold;
    }
#endif  // BUILDFLAG(IS_ANDROID) && !defined(ARCH_CPU_64_BITS)

    ::partition_alloc::ThreadCache::SetLargestCachedSize(largest_cached_size_);
  }
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(USE_STARSCAN)
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanMUAwareScheduler)) {
    // Assign PCScan a task-based scheduling backend.
    static base::NoDestructor<
        partition_alloc::internal::MUAwareTaskBasedBackend>
        mu_aware_task_based_backend{
            partition_alloc::internal::PCScan::scheduler(),
            &partition_alloc::internal::PCScan::PerformDelayedScan};
    partition_alloc::internal::PCScan::scheduler().SetNewSchedulingBackend(
        *mu_aware_task_based_backend.get());
  }
#endif  // BUILDFLAG(USE_STARSCAN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::StartMemoryReclaimer(
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocSortActiveSlotSpans)) {
    partition_alloc::PartitionRoot::EnableSortActiveSlotSpans();
  }
}

void PartitionAllocSupport::OnForegrounded(bool has_main_frame) {
#if PA_CONFIG(THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess) {
      return;
    }
  }

  if (!base::FeatureList::IsEnabled(
          features::kLowerPAMemoryLimitForNonMainRenderers) ||
      has_main_frame) {
    ::partition_alloc::ThreadCache::SetLargestCachedSize(largest_cached_size_);
  }
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::OnBackgrounded() {
#if PA_CONFIG(THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess) {
      return;
    }
  }

  // Performance matters less for background renderers, don't pay the memory
  // cost.
  ::partition_alloc::ThreadCache::SetLargestCachedSize(
      ::partition_alloc::ThreadCacheLimits::kDefaultSizeThreshold);

  // In renderers, memory reclaim uses the "idle time" task runner to run
  // periodic reclaim. This does not always run when the renderer is idle, and
  // in particular after the renderer gets backgrounded. As a result, empty slot
  // spans are potentially never decommitted. To mitigate that, run a one-off
  // reclaim a few seconds later. Even if the renderer comes back to foreground
  // in the meantime, the worst case is a few more system calls.
  //
  // TODO(lizeb): Remove once/if the behavior of idle tasks changes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();
      }),
      base::Seconds(10));

#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
std::string PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
    std::string stacktrace) {
  return ExtractDanglingPtrSignature(stacktrace);
}
#endif

}  // namespace base::allocator
