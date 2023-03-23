// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "heuristic_memory_saver_policy.h"

namespace performance_manager::policies {

namespace {
HeuristicMemorySaverPolicy* g_heuristic_memory_saver_policy = nullptr;
}  // namespace

HeuristicMemorySaverPolicy::HeuristicMemorySaverPolicy(
    uint64_t pmf_threshold_percent,
    base::TimeDelta threshold_reached_heartbeat_interval,
    base::TimeDelta threshold_not_reached_heartbeat_interval,
    base::TimeDelta minimum_time_in_background,
    AvailableMemoryCallback available_memory_cb,
    TotalMemoryCallback total_memory_cb)
    : pmf_threshold_percent_(pmf_threshold_percent),
      threshold_reached_heartbeat_interval_(
          threshold_reached_heartbeat_interval),
      threshold_not_reached_heartbeat_interval_(
          threshold_not_reached_heartbeat_interval),
      minimum_time_in_background_(minimum_time_in_background),
      available_memory_cb_(available_memory_cb),
      total_memory_cb_(total_memory_cb) {
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

  if (static_cast<float>(available_memory) /
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
  return base::SysInfo::AmountOfAvailablePhysicalMemory();
}

// static
uint64_t HeuristicMemorySaverPolicy::DefaultGetAmountOfPhysicalMemory() {
  return base::SysInfo::AmountOfPhysicalMemory();
}

}  // namespace performance_manager::policies
