// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

namespace resource_coordinator {

class TestLifecycleUnit : public LifecycleUnitBase {
 public:
  using LifecycleUnitBase::SetState;

  explicit TestLifecycleUnit(
      base::TimeTicks last_focused_time = base::TimeTicks());
  explicit TestLifecycleUnit(LifecycleUnitSourceBase* source);

  TestLifecycleUnit(const TestLifecycleUnit&) = delete;
  TestLifecycleUnit& operator=(const TestLifecycleUnit&) = delete;

  ~TestLifecycleUnit() override;

  void SetLastFocusedTimeTicks(base::TimeTicks last_focused_time) {
    last_focused_time_ticks_ = last_focused_time;
  }

  void SetDiscardReason(LifecycleUnitDiscardReason discard_reason) {
    discard_reason_ = discard_reason;
  }

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::TimeTicks GetLastFocusedTimeTicks() const override;
  base::Time GetLastFocusedTime() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  bool Discard(LifecycleUnitDiscardReason discard_reason,
               uint64_t resident_set_size_estimate) override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;

 private:
  base::TimeTicks last_focused_time_ticks_;
  base::Time last_focused_time_;
  LifecycleUnitDiscardReason discard_reason_ =
      ::mojom::LifecycleUnitDiscardReason::EXTERNAL;
};

// Helper funtions for testing CanDiscard policy.
void ExpectCanDiscardTrue(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardTrueAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit);

// Checks the tab cannot be discarded and the cannot discard reason includes
// |failure_reason|.
void ExpectCanDiscardFalse(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    performance_manager::policies::CannotDiscardReason failure_reason,
    LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    performance_manager::policies::CannotDiscardReason failure_reason);

void ExpectCanDiscardFalseTrivial(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseTrivialAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
