// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(42);

constexpr char kDiscardCountHistogram[] = "TabManager.Discarding.DiscardCount";
constexpr char kReloadCountHistogram[] = "TabManager.Discarding.ReloadCount";
constexpr char kDiscardToReloadTimeHistogram[] =
    "TabManager.Discarding.DiscardToReloadTime";
constexpr char kInactiveToReloadTimeHistogram[] =
    "TabManager.Discarding.InactiveToReloadTime";
constexpr char kReloadToCloseTimeHistogram[] =
    "TabManager.Discarding.ReloadToCloseTime";

class DiscardMetricsLifecycleUnitObserverTest : public testing::Test {
 protected:
  DiscardMetricsLifecycleUnitObserverTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    test_clock_.SetNowTicks(base::TimeTicks::Now());
    lifecycle_unit_->AddObserver(observer_);
  }

  // Owned by |lifecycle_unit|.
  DiscardMetricsLifecycleUnitObserver* observer_ =
      new DiscardMetricsLifecycleUnitObserver();

  std::unique_ptr<TestLifecycleUnit> lifecycle_unit_ =
      std::make_unique<TestLifecycleUnit>();

  base::HistogramTester histograms_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiscardMetricsLifecycleUnitObserverTest);
};

}  // namespace

TEST_F(DiscardMetricsLifecycleUnitObserverTest, DiscardReloadCount) {
  histograms_.ExpectTotalCount(kDiscardCountHistogram, 0);
  histograms_.ExpectTotalCount(kReloadCountHistogram, 0);

  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::SYSTEM_MEMORY_PRESSURE);
  histograms_.ExpectTotalCount(kDiscardCountHistogram, 1);
  histograms_.ExpectTotalCount(kReloadCountHistogram, 0);

  lifecycle_unit_->SetState(LifecycleUnitState::ACTIVE,
                            LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  histograms_.ExpectTotalCount(kDiscardCountHistogram, 1);
  histograms_.ExpectTotalCount(kReloadCountHistogram, 1);

  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
  histograms_.ExpectTotalCount(kDiscardCountHistogram, 2);
  histograms_.ExpectTotalCount(kReloadCountHistogram, 1);

  lifecycle_unit_->SetState(LifecycleUnitState::ACTIVE,
                            LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  histograms_.ExpectTotalCount(kDiscardCountHistogram, 2);
  histograms_.ExpectTotalCount(kReloadCountHistogram, 2);
}

TEST_F(DiscardMetricsLifecycleUnitObserverTest, DiscardToReloadTime) {
  histograms_.ExpectTotalCount(kDiscardToReloadTimeHistogram, 0);

  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
  test_clock_.Advance(kShortDelay);
  histograms_.ExpectTotalCount(kDiscardToReloadTimeHistogram, 0);

  lifecycle_unit_->SetState(LifecycleUnitState::ACTIVE,
                            LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  histograms_.ExpectTimeBucketCount(kDiscardToReloadTimeHistogram, kShortDelay,
                                    1);
}

TEST_F(DiscardMetricsLifecycleUnitObserverTest, InactiveToReloadTime) {
  histograms_.ExpectTotalCount(kInactiveToReloadTimeHistogram, 0);

  const base::TimeTicks last_focused_time = NowTicks();
  lifecycle_unit_->SetLastFocusedTime(last_focused_time);
  test_clock_.Advance(kShortDelay);
  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
  test_clock_.Advance(kShortDelay);
  histograms_.ExpectTotalCount(kInactiveToReloadTimeHistogram, 0);

  lifecycle_unit_->SetState(LifecycleUnitState::ACTIVE,
                            LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  histograms_.ExpectTimeBucketCount(kInactiveToReloadTimeHistogram,
                                    2 * kShortDelay, 1);
}

TEST_F(DiscardMetricsLifecycleUnitObserverTest,
       ReloadToCloseTimeNeverDiscarded) {
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);
  lifecycle_unit_.reset();
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);
}

TEST_F(DiscardMetricsLifecycleUnitObserverTest,
       ReloadToCloseTimeDiscardedButNotReloaded) {
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);

  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);

  lifecycle_unit_.reset();
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);
}

TEST_F(DiscardMetricsLifecycleUnitObserverTest, ReloadToCloseTime) {
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);

  test_clock_.Advance(kShortDelay * 1);
  lifecycle_unit_->SetState(
      LifecycleUnitState::DISCARDED,
      LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);

  test_clock_.Advance(kShortDelay * 2);
  lifecycle_unit_->SetState(LifecycleUnitState::ACTIVE,
                            LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  histograms_.ExpectTotalCount(kReloadToCloseTimeHistogram, 0);

  test_clock_.Advance(kShortDelay * 4);
  lifecycle_unit_.reset();
  histograms_.ExpectTimeBucketCount(kReloadToCloseTimeHistogram,
                                    4 * kShortDelay, 1);
}

}  // namespace resource_coordinator
