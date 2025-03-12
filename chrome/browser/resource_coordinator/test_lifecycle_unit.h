// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

namespace resource_coordinator {

class TestLifecycleUnit : public LifecycleUnitBase {
 public:
  using LifecycleUnitBase::SetState;

  explicit TestLifecycleUnit(
      base::TimeTicks last_focused_time = base::TimeTicks(),
      bool can_discard = true);
  explicit TestLifecycleUnit(LifecycleUnitSourceBase* source);

  TestLifecycleUnit(const TestLifecycleUnit&) = delete;
  TestLifecycleUnit& operator=(const TestLifecycleUnit&) = delete;

  ~TestLifecycleUnit() override;

  void SetLastFocusedTimeTicks(base::TimeTicks last_focused_time) {
    last_focused_time_ticks_ = last_focused_time;
  }

  void SetSortKey(LifecycleUnit::SortKey sort_key) { sort_key_ = sort_key; }

  void SetDiscardFailureReason(DecisionFailureReason failure_reason) {
    failure_reason_ = failure_reason;
  }

  void SetCanDiscard(bool can_discard) { can_discard_ = can_discard; }

  void SetDiscardReason(LifecycleUnitDiscardReason discard_reason) {
    discard_reason_ = discard_reason;
  }

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::TimeTicks GetLastFocusedTimeTicks() const override;
  base::Time GetLastFocusedTime() const override;
  SortKey GetSortKey() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  bool CanDiscard(LifecycleUnitDiscardReason reason,
                  DecisionDetails* decision_details) const override;
  bool Discard(LifecycleUnitDiscardReason discard_reason,
               uint64_t resident_set_size_estimate) override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;

 private:
  base::TimeTicks last_focused_time_ticks_;
  base::Time last_focused_time_;
  LifecycleUnit::SortKey sort_key_;
  bool can_discard_ = true;
  LifecycleUnitDiscardReason discard_reason_ =
      ::mojom::LifecycleUnitDiscardReason::EXTERNAL;
  std::optional<DecisionFailureReason> failure_reason_;
};

// Helper funtions for testing CanDiscard policy.
void ExpectCanDiscardTrue(const LifecycleUnit* lifecycle_unit,
                          LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardTrueAllReasons(const LifecycleUnit* lifecycle_unit);
void ExpectCanDiscardFalse(const LifecycleUnit* lifecycle_unit,
                           DecisionFailureReason failure_reason,
                           LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseAllReasons(const LifecycleUnit* lifecycle_unit,
                                     DecisionFailureReason failure_reason);
void ExpectCanDiscardFalseTrivial(const LifecycleUnit* lifecycle_unit,
                                  LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseTrivialAllReasons(const LifecycleUnit* lu);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
