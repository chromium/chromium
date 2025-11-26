// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"

#include "base/containers/contains.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TestLifecycleUnit::TestLifecycleUnit(base::TimeTicks last_focused_time)
    : LifecycleUnitBase(nullptr), last_focused_time_ticks_(last_focused_time) {}

TestLifecycleUnit::TestLifecycleUnit(LifecycleUnitSourceBase* source)
    : LifecycleUnitBase(source) {}

TestLifecycleUnit::~TestLifecycleUnit() {
  OnLifecycleUnitDestroyed();
}

TabLifecycleUnitExternal* TestLifecycleUnit::AsTabLifecycleUnitExternal() {
  return nullptr;
}

base::TimeTicks TestLifecycleUnit::GetLastFocusedTimeTicks() const {
  return last_focused_time_ticks_;
}

base::Time TestLifecycleUnit::GetLastFocusedTime() const {
  return last_focused_time_;
}

mojom::LifecycleUnitLoadingState TestLifecycleUnit::GetLoadingState() const {
  return mojom::LifecycleUnitLoadingState::LOADED;
}

bool TestLifecycleUnit::Load() {
  return false;
}

bool TestLifecycleUnit::Discard(LifecycleUnitDiscardReason discard_reason,
                                uint64_t resident_set_size_estimate) {
  return false;
}

LifecycleUnitDiscardReason TestLifecycleUnit::GetDiscardReason() const {
  return discard_reason_;
}

using performance_manager::policies::CanDiscardResult;
using performance_manager::policies::CannotDiscardReason;
using performance_manager::policies::CanDiscardResult::kDisallowed;
using performance_manager::policies::CanDiscardResult::kEligible;
using performance_manager::policies::CanDiscardResult::kProtected;

namespace {

CanDiscardResult CanDiscardHelper(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    LifecycleUnitDiscardReason discard_reason,
    std::vector<CannotDiscardReason>* cannot_discard_reasons) {
  base::WeakPtr<performance_manager::PageNode> page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          tab_lifecycle_unit->GetWebContents());
  CHECK(page_node);
  performance_manager::policies::DiscardEligibilityPolicy* eligiblity_policy =
      performance_manager::policies::DiscardEligibilityPolicy::GetFromGraph(
          page_node->GetGraph());
  CHECK(eligiblity_policy);
  eligiblity_policy->SetNoDiscardPatternsForProfile(
      page_node->GetBrowserContextID(), {});

  CanDiscardResult result = eligiblity_policy->CanDiscard(
      page_node.get(), discard_reason,
      /*minimum_time_in_background*/ base::TimeDelta(), cannot_discard_reasons);
  return result;
}

}  // namespace

void ExpectCanDiscardTrue(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    LifecycleUnitDiscardReason discard_reason) {
  CanDiscardResult result =
      CanDiscardHelper(tab_lifecycle_unit, discard_reason, nullptr);
  EXPECT_TRUE(result == kEligible);
}

void ExpectCanDiscardTrueAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit) {
  ExpectCanDiscardTrue(tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardTrue(tab_lifecycle_unit, LifecycleUnitDiscardReason::URGENT);
}

void ExpectCanDiscardFalse(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    CannotDiscardReason failure_reason,
    LifecycleUnitDiscardReason discard_reason) {
  std::vector<CannotDiscardReason> failure_reasons;
  CanDiscardResult result =
      CanDiscardHelper(tab_lifecycle_unit, discard_reason, &failure_reasons);
  EXPECT_TRUE(result == kDisallowed || result == kProtected);
  EXPECT_TRUE(base::Contains(failure_reasons, failure_reason));
}

void ExpectCanDiscardFalseAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    CannotDiscardReason failure_reason) {
  ExpectCanDiscardFalse(tab_lifecycle_unit, failure_reason,
                        LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalse(tab_lifecycle_unit, failure_reason,
                        LifecycleUnitDiscardReason::URGENT);
}

void ExpectCanDiscardFalseTrivial(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit,
    LifecycleUnitDiscardReason discard_reason) {
  CanDiscardResult result =
      CanDiscardHelper(tab_lifecycle_unit, discard_reason, nullptr);
  EXPECT_TRUE(result == kDisallowed || result == kProtected);
}

void ExpectCanDiscardFalseTrivialAllReasons(
    const TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit) {
  ExpectCanDiscardFalseTrivial(tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::EXTERNAL);
  ExpectCanDiscardFalseTrivial(tab_lifecycle_unit,
                               LifecycleUnitDiscardReason::URGENT);
}

}  // namespace resource_coordinator
