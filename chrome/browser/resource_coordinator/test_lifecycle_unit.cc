// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"

#include "chrome/browser/resource_coordinator/lifecycle_unit_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TestLifecycleUnit::TestLifecycleUnit(base::TimeTicks last_focused_time,
                                     base::ProcessHandle process_handle,
                                     bool can_discard)
    : LifecycleUnitBase(nullptr, content::Visibility::VISIBLE, nullptr),
      last_focused_time_ticks_(last_focused_time),
      process_handle_(process_handle),
      sort_key_(last_focused_time),
      can_discard_(can_discard) {}

TestLifecycleUnit::TestLifecycleUnit(content::Visibility visibility,
                                     UsageClock* usage_clock)
    : LifecycleUnitBase(nullptr, visibility, usage_clock) {}

TestLifecycleUnit::TestLifecycleUnit(LifecycleUnitSourceBase* source)
    : LifecycleUnitBase(source, content::Visibility::VISIBLE, nullptr) {}

TestLifecycleUnit::~TestLifecycleUnit() {
  OnLifecycleUnitDestroyed();
}

TabLifecycleUnitExternal* TestLifecycleUnit::AsTabLifecycleUnitExternal() {
  return nullptr;
}

std::u16string TestLifecycleUnit::GetTitle() const {
  return title_;
}

base::TimeTicks TestLifecycleUnit::GetLastFocusedTimeTicks() const {
  return last_focused_time_ticks_;
}

base::Time TestLifecycleUnit::GetLastFocusedTime() const {
  return last_focused_time_;
}

base::ProcessHandle TestLifecycleUnit::GetProcessHandle() const {
  return process_handle_;
}

LifecycleUnit::SortKey TestLifecycleUnit::GetSortKey() const {
  return sort_key_;
}

content::Visibility TestLifecycleUnit::GetVisibility() const {
  return content::Visibility::VISIBLE;
}

mojom::LifecycleUnitLoadingState TestLifecycleUnit::GetLoadingState() const {
  return mojom::LifecycleUnitLoadingState::LOADED;
}

bool TestLifecycleUnit::Load() {
  return false;
}

int TestLifecycleUnit::GetEstimatedMemoryFreedOnDiscardKB() const {
  return estimated_memory_freed_kb_;
}

bool TestLifecycleUnit::CanDiscard(mojom::LifecycleUnitDiscardReason reason,
                                   DecisionDetails* decision_details) const {
  if (failure_reason_) {
    decision_details->AddReason(*failure_reason_);
  }
  return can_discard_;
}

bool TestLifecycleUnit::Discard(LifecycleUnitDiscardReason discard_reason,
                                uint64_t resident_set_size_estimate) {
  return false;
}

LifecycleUnitDiscardReason TestLifecycleUnit::GetDiscardReason() const {
  return mojom::LifecycleUnitDiscardReason::EXTERNAL;
}

void ExpectCanDiscardTrue(const LifecycleUnit* lifecycle_unit,
                          LifecycleUnitDiscardReason discard_reason) {
  DecisionDetails decision_details;
  EXPECT_TRUE(lifecycle_unit->CanDiscard(discard_reason, &decision_details));
  EXPECT_TRUE(decision_details.IsPositive());
  EXPECT_EQ(1u, decision_details.reasons().size());
  EXPECT_EQ(DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE,
            decision_details.SuccessReason());
}

void ExpectCanDiscardTrueAllReasons(const LifecycleUnit* lifecycle_unit) {
  ExpectCanDiscardTrue(lifecycle_unit, LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardTrue(lifecycle_unit, LifecycleUnitDiscardReason::URGENT);
}

void ExpectCanDiscardFalse(const LifecycleUnit* lifecycle_unit,
                           DecisionFailureReason failure_reason,
                           LifecycleUnitDiscardReason discard_reason) {
  DecisionDetails decision_details;
  EXPECT_FALSE(lifecycle_unit->CanDiscard(discard_reason, &decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  EXPECT_EQ(1u, decision_details.reasons().size());
  EXPECT_EQ(failure_reason, decision_details.FailureReason());
}

void ExpectCanDiscardFalseAllReasons(const LifecycleUnit* lifecycle_unit,
                                     DecisionFailureReason failure_reason) {
  ExpectCanDiscardFalse(lifecycle_unit, failure_reason,
                        LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalse(lifecycle_unit, failure_reason,
                        LifecycleUnitDiscardReason::URGENT);
}

void ExpectCanDiscardFalseTrivial(const LifecycleUnit* lifecycle_unit,
                                  LifecycleUnitDiscardReason discard_reason) {
  DecisionDetails decision_details;
  EXPECT_FALSE(lifecycle_unit->CanDiscard(discard_reason, &decision_details));
  EXPECT_FALSE(decision_details.IsPositive());
  // |reasons()| will either contain the status for the 4 local site features
  // heuristics or be empty if the database doesn't track this lifecycle unit.
  EXPECT_TRUE(decision_details.reasons().empty() ||
              (decision_details.reasons().size() == 4));
}

void ExpectCanDiscardFalseTrivialAllReasons(
    const LifecycleUnit* lifecycle_unit) {
  ExpectCanDiscardFalseTrivial(lifecycle_unit,
                               LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalseTrivial(lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);
}

}  // namespace resource_coordinator
