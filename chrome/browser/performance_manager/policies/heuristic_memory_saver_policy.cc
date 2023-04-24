// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"

#include "base/process/process_metrics.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::policies {

namespace {
HeuristicMemorySaverPolicy* g_heuristic_memory_saver_policy = nullptr;

const uint64_t kBytesPerMb = 1024 * 1024;

}  // namespace

HeuristicMemorySaverPolicy::HeuristicMemorySaverPolicy(
    uint64_t pmf_threshold_percent,
    uint64_t pmf_threshold_mb,
    base::TimeDelta threshold_reached_heartbeat_interval,
    base::TimeDelta threshold_not_reached_heartbeat_interval,
    base::TimeDelta minimum_time_in_background,
    AvailableMemoryCallback available_memory_cb,
    TotalMemoryCallback total_memory_cb)
    : pmf_threshold_percent_(pmf_threshold_percent),
      pmf_threshold_bytes_(pmf_threshold_mb * kBytesPerMb),
      threshold_reached_heartbeat_interval_(
          threshold_reached_heartbeat_interval),
      threshold_not_reached_heartbeat_interval_(
          threshold_not_reached_heartbeat_interval),
      minimum_time_in_background_(minimum_time_in_background),
      available_memory_cb_(available_memory_cb),
      total_memory_cb_(total_memory_cb) {
  CHECK(!g_heuristic_memory_saver_policy);
  CHECK_LE(pmf_threshold_percent_, 100UL);
  g_heuristic_memory_saver_policy = this;
}

HeuristicMemorySaverPolicy::~HeuristicMemorySaverPolicy() {
  CHECK_EQ(this, g_heuristic_memory_saver_policy);
  g_heuristic_memory_saver_policy = nullptr;
}

// static
HeuristicMemorySaverPolicy* HeuristicMemorySaverPolicy::GetInstance() {
  return g_heuristic_memory_saver_policy;
}

// GraphOwned:
void HeuristicMemorySaverPolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
}

void HeuristicMemorySaverPolicy::OnTakenFromGraph(Graph* graph) {
  SetActive(false);
  graph_ = nullptr;
}

void HeuristicMemorySaverPolicy::SetActive(bool active) {
  is_active_ = active;

  if (is_active_) {
    // Start the first timer as if the threshold was reached, memory will be
    // sampled in the callback and the next timer will be scheduled with the
    // appropriate interval.
    ScheduleNextHeartbeat(threshold_reached_heartbeat_interval_);
  } else {
    heartbeat_timer_.Stop();
  }
}

bool HeuristicMemorySaverPolicy::IsActive() const {
  return is_active_;
}

void HeuristicMemorySaverPolicy::OnHeartbeatCallback() {
  uint64_t available_memory = available_memory_cb_.Run();
  uint64_t total_physical_memory = total_memory_cb_.Run();

  base::TimeDelta next_interval = threshold_not_reached_heartbeat_interval_;

  if (available_memory < pmf_threshold_bytes_ &&
      static_cast<float>(available_memory) /
              static_cast<float>(total_physical_memory) * 100.f <
          static_cast<float>(pmf_threshold_percent_)) {
    PageDiscardingHelper::GetFromGraph(graph_)->DiscardAPage(
        /*post_discard_cb=*/base::DoNothing(),
        PageDiscardingHelper::DiscardReason::PROACTIVE,
        /*minimum_time_in_background=*/minimum_time_in_background_);
    next_interval = threshold_reached_heartbeat_interval_;
  }

  ScheduleNextHeartbeat(next_interval);
}

void HeuristicMemorySaverPolicy::ScheduleNextHeartbeat(
    base::TimeDelta interval) {
  heartbeat_timer_.Start(
      FROM_HERE, interval,
      base::BindOnce(&HeuristicMemorySaverPolicy::OnHeartbeatCallback,
                     base::Unretained(this)));
}

// static
uint64_t
HeuristicMemorySaverPolicy::DefaultGetAmountOfAvailablePhysicalMemory() {
  base::CheckedNumeric<uint64_t> available_memory =
      base::SysInfo::AmountOfAvailablePhysicalMemory();

#if BUILDFLAG(IS_MAC)
  // On macOS, we have access to the "free" memory figure, which only reports
  // memory that is completely unused. This is misleading because the OS will
  // try to keep pages in memory if there is space available, even though they
  // are inactive. This is so that subsequently accessing them is faster.
  //
  // Because of this, the reported amount of "free" memory is always very low on
  // macOS. Moreover, it's relatively cheap to dispose of pages in the pagecache
  // in most cases. On the other hand, we don't want to consider the page cache
  // as fully "free" memory since it does serve a purpose, and allocating so
  // much that there's no more room for it means the system will likely start
  // swapping.
  //
  // To address this, we'll treat a portion of the file-backed pagecache as
  // available for the purposes of memory saver. The factor used for this is
  // determined by the `kHeuristicMemorySaverPageCacheDiscountMac` feature
  // param.
  //
  // This treatment of the pagecache is very platform specific. On Linux for
  // instance, the computation is performed by the kernel (and is more
  // sophisticated). See the comment in sys_info_linux.cc's
  // `SysInfo::AmountOfAvailablePhysicalMemory`.
  constexpr uint64_t kBytesPerKb = 1024;
  base::SystemMemoryInfoKB info;
  if (base::GetSystemMemoryInfo(&info)) {
    int available_page_cache_percent =
        features::kHeuristicMemorySaverPageCacheDiscountMac.Get();
    CHECK_GE(available_page_cache_percent, 0);
    CHECK_LE(available_page_cache_percent, 100);
    available_memory += (info.file_backed * kBytesPerKb *
                         static_cast<uint64_t>(available_page_cache_percent)) /
                        100;
  }
#endif

  return available_memory.ValueOrDie();
}

// static
uint64_t HeuristicMemorySaverPolicy::DefaultGetAmountOfPhysicalMemory() {
  return base::SysInfo::AmountOfPhysicalMemory();
}

}  // namespace performance_manager::policies
