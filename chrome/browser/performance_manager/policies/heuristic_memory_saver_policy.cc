// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::policies {

namespace {

HeuristicMemorySaverPolicy* g_heuristic_memory_saver_policy = nullptr;

constexpr uint64_t kBytesPerMb = 1024 * 1024;

base::TimeDelta GetThresholdReachedHeartbeatInterval() {
  constexpr base::TimeDelta kDefault = base::Seconds(10);
  base::TimeDelta interval =
      features::kHeuristicMemorySaverThresholdReachedHeartbeatInterval.Get();
  return interval.is_zero() ? kDefault : interval;
}

base::TimeDelta GetThresholdNotReachedHeartbeatInterval() {
  constexpr base::TimeDelta kDefault = base::Seconds(60);
  base::TimeDelta interval =
      features::kHeuristicMemorySaverThresholdNotReachedHeartbeatInterval.Get();
  return interval.is_zero() ? kDefault : interval;
}

base::TimeDelta GetMinimumTimeInBackground() {
  constexpr base::TimeDelta kDefault = base::Hours(2);
  base::TimeDelta delta =
      features::kHeuristicMemorySaverMinimumTimeInBackground.Get();
  return delta.is_zero() ? kDefault : delta;
}

int GetAvailableMemoryThresholdPercent() {
  constexpr int kDefault = 5;
  int threshold =
      features::kHeuristicMemorySaverAvailableMemoryThresholdPercent.Get();
  return threshold < 0 ? kDefault : threshold;
}

int GetAvailableMemoryThresholdMb() {
  constexpr int kDefault = 4096;
  int threshold =
      features::kHeuristicMemorySaverAvailableMemoryThresholdMb.Get();
  return threshold < 0 ? kDefault : threshold;
}

#if BUILDFLAG(IS_MAC)
int GetPageCacheDiscountMac() {
  constexpr int kDefault = 50;
  int discount = features::kHeuristicMemorySaverPageCacheDiscountMac.Get();
  return discount < 0 ? kDefault : discount;
}
#endif

}  // namespace

HeuristicMemorySaverPolicy::HeuristicMemorySaverPolicy(
    AvailableMemoryCallback available_memory_cb,
    TotalMemoryCallback total_memory_cb)
    : available_memory_cb_(std::move(available_memory_cb)),
      total_memory_cb_(std::move(total_memory_cb)) {
  CHECK(!g_heuristic_memory_saver_policy);
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
    ScheduleNextHeartbeat(GetThresholdReachedHeartbeatInterval());
  } else {
    heartbeat_timer_.Stop();
  }
}

bool HeuristicMemorySaverPolicy::IsActive() const {
  return is_active_;
}

base::TimeDelta
HeuristicMemorySaverPolicy::GetThresholdReachedHeartbeatIntervalForTesting()
    const {
  return GetThresholdReachedHeartbeatInterval();
}

base::TimeDelta
HeuristicMemorySaverPolicy::GetThresholdNotReachedHeartbeatIntervalForTesting()
    const {
  return GetThresholdNotReachedHeartbeatInterval();
}

base::TimeDelta
HeuristicMemorySaverPolicy::GetMinimumTimeInBackgroundForTesting() const {
  return GetMinimumTimeInBackground();
}

void HeuristicMemorySaverPolicy::OnHeartbeatCallback() {
  const uint64_t available_memory = available_memory_cb_.Run();
  const uint64_t total_physical_memory = total_memory_cb_.Run();

  base::TimeDelta next_interval = GetThresholdNotReachedHeartbeatInterval();

  const int pmf_threshold_percent = GetAvailableMemoryThresholdPercent();
  CHECK_LE(0, pmf_threshold_percent);
  CHECK_LE(pmf_threshold_percent, 100);
  const uint64_t pmf_threshold_bytes =
      GetAvailableMemoryThresholdMb() * kBytesPerMb;
  if (available_memory < pmf_threshold_bytes &&
      static_cast<float>(available_memory) /
              static_cast<float>(total_physical_memory) * 100.f <
          static_cast<float>(pmf_threshold_percent)) {
    PageDiscardingHelper::GetFromGraph(graph_)->DiscardAPage(
        /*post_discard_cb=*/base::DoNothing(),
        PageDiscardingHelper::DiscardReason::PROACTIVE,
        GetMinimumTimeInBackground());
    next_interval = GetThresholdReachedHeartbeatInterval();
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
    const int available_page_cache_percent = GetPageCacheDiscountMac();
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
