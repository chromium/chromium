// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_

#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::policies {

// A memory saver policy that discards a tab that has been in the background for
// at least X amount of time, as long as the percentage of available system
// memory is smaller than Y, checking at a frequency of Z. X, Y, and Z being
// parameters to the policy.
class HeuristicMemorySaverPolicy : public GraphOwned {
 public:
  using AvailableMemoryCallback = base::RepeatingCallback<uint64_t()>;
  using TotalMemoryCallback = base::RepeatingCallback<uint64_t()>;
  // `pmf_threshold_percent`: the amount of free memory this policy tries to
  // maintain, i.e. it will start discarding when the percentage available
  // memory < pmf_threshold_percent
  // `threshold_reached_heartbeat_interval`: the time interval at which this
  // policy will check whether a tab should be discarded, when the last check
  // found that the threshold was reached.
  // `threshold_not_reached_heartbeat_interval`: the time interval at which this
  // policy will check whether a tab should be discarded, when the last check
  // found that the threshold was not reached. `minimum_time_in_background`: the
  // minimum amount of time a page must spend in the background before being
  // considered eligible for discarding. `available_memory_cb` and
  // `total_memory_cb` allow mocking memory measurements for testing.
  HeuristicMemorySaverPolicy(
      uint64_t pmf_threshold_percent,
      base::TimeDelta threshold_reached_heartbeat_interval,
      base::TimeDelta threshold_not_reached_heartbeat_interval,
      base::TimeDelta minimum_time_in_background,
      AvailableMemoryCallback available_memory_cb =
          base::BindRepeating(&HeuristicMemorySaverPolicy::
                                  DefaultGetAmountOfAvailablePhysicalMemory),
      TotalMemoryCallback total_memory_cb = base::BindRepeating(
          &HeuristicMemorySaverPolicy::DefaultGetAmountOfPhysicalMemory));
  ~HeuristicMemorySaverPolicy() override;

  static HeuristicMemorySaverPolicy* GetInstance();

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void SetActive(bool enabled);
  bool IsActive() const;

 private:
  void OnHeartbeatCallback();
  void ScheduleNextHeartbeat(base::TimeDelta interval);

  static uint64_t DefaultGetAmountOfAvailablePhysicalMemory();
  static uint64_t DefaultGetAmountOfPhysicalMemory();

  uint64_t pmf_threshold_percent_;
  base::TimeDelta threshold_reached_heartbeat_interval_;
  base::TimeDelta threshold_not_reached_heartbeat_interval_;
  base::TimeDelta minimum_time_in_background_;

  bool is_active_ = false;
  base::OneShotTimer heartbeat_timer_;

  AvailableMemoryCallback available_memory_cb_;
  TotalMemoryCallback total_memory_cb_;

  raw_ptr<Graph> graph_ = nullptr;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_
