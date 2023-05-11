// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph.h"

namespace base {
class TimeDelta;
}

namespace performance_manager::policies {

// A memory saver policy that discards a tab that has been in the background for
// at least X amount of time, as long as the percentage of available system
// memory is smaller than Y, checking at a frequency of Z. X, Y, and Z being
// parameters to the policy.
//
// The parameters are set through base::FeatureParam:
//
// * kHeuristicMemorySaverAvailableMemoryThresholdPercent and
//   kHeuristicMemorySaverAvailableMemoryThresholdMb: the amount of free memory
//   this policy tries to maintain, i.e. it will start discarding when the
//   percentage available memory < percent AND available memory < mb.
//
// * kHeuristicMemorySaverThresholdReachedHeartbeatInterval: the time interval
//   at which this policy will check whether a tab should be discarded, when the
//   last check found that the threshold was reached.
//
// * kHeuristicMemorySaverThresholdNotReachedHeartbeatInterval: the time
//   interval at which this policy will check whether a tab should be discarded,
//   when the last check found that the threshold was not reached.
//
// * kHeuristicMemorySaverMinimumTimeInBackground: the minimum amount of time a
//   page must spend in the background before being considered eligible for
//   discarding.
class HeuristicMemorySaverPolicy : public GraphOwned {
 public:
  using AvailableMemoryCallback = base::RepeatingCallback<uint64_t()>;
  using TotalMemoryCallback = base::RepeatingCallback<uint64_t()>;

  // `available_memory_cb` and `total_memory_cb` allow mocking memory
  // measurements for testing.
  HeuristicMemorySaverPolicy(
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

  base::TimeDelta GetThresholdReachedHeartbeatIntervalForTesting() const;
  base::TimeDelta GetThresholdNotReachedHeartbeatIntervalForTesting() const;
  base::TimeDelta GetMinimumTimeInBackgroundForTesting() const;

 private:
  void OnHeartbeatCallback();
  void ScheduleNextHeartbeat(base::TimeDelta interval);

  static uint64_t DefaultGetAmountOfAvailablePhysicalMemory();
  static uint64_t DefaultGetAmountOfPhysicalMemory();

  bool is_active_ = false;
  base::OneShotTimer heartbeat_timer_;

  AvailableMemoryCallback available_memory_cb_;
  TotalMemoryCallback total_memory_cb_;

  raw_ptr<Graph> graph_ = nullptr;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HEURISTIC_MEMORY_SAVER_POLICY_H_
