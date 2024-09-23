// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/usage_clock.h"
#include "content/public/browser/visibility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MockLifecycleUnitObserver(const MockLifecycleUnitObserver&) = delete;
  MockLifecycleUnitObserver& operator=(const MockLifecycleUnitObserver&) =
      delete;

  MOCK_METHOD3(OnLifecycleUnitStateChanged,
               void(LifecycleUnit*,
                    LifecycleUnitState,
                    LifecycleUnitStateChangeReason));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit*, content::Visibility));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));
};

class LifecycleUnitBaseTest : public testing::Test {
 public:
  LifecycleUnitBaseTest(const LifecycleUnitBaseTest&) = delete;
  LifecycleUnitBaseTest& operator=(const LifecycleUnitBaseTest&) = delete;

 protected:
  LifecycleUnitBaseTest() {
    metrics::DesktopSessionDurationTracker::Initialize();
    usage_clock_ = std::make_unique<UsageClock>();
  }

  ~LifecycleUnitBaseTest() override {
    usage_clock_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  ScopedSetClocksForTesting scoped_set_clocks_for_testing_{&test_clock_,
                                                           &test_tick_clock_};
  testing::StrictMock<MockLifecycleUnitObserver> observer_;
  std::unique_ptr<UsageClock> usage_clock_;
};

}  // namespace

// Verify that GetID() returns different ids for different LifecycleUnits, but
// always the same id for the same LifecycleUnit.
TEST_F(LifecycleUnitBaseTest, GetID) {
  TestLifecycleUnit a;
  TestLifecycleUnit b;
  TestLifecycleUnit c;

  EXPECT_NE(a.GetID(), b.GetID());
  EXPECT_NE(a.GetID(), c.GetID());
  EXPECT_NE(b.GetID(), c.GetID());

  EXPECT_EQ(a.GetID(), a.GetID());
  EXPECT_EQ(b.GetID(), b.GetID());
  EXPECT_EQ(c.GetID(), c.GetID());
}

// Verify that the state change time is updated when the state changes.
TEST_F(LifecycleUnitBaseTest, SetStateUpdatesTime) {
  TestLifecycleUnit lifecycle_unit;
  EXPECT_EQ(NowTicks(), lifecycle_unit.GetStateChangeTime());

  test_tick_clock_.Advance(base::Seconds(1));
  base::TimeTicks first_state_change_time = NowTicks();
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED,
                          LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  EXPECT_EQ(first_state_change_time, lifecycle_unit.GetStateChangeTime());
  test_tick_clock_.Advance(base::Seconds(1));
  EXPECT_EQ(first_state_change_time, lifecycle_unit.GetStateChangeTime());
}

// Verify that observers are notified when the state changes and when the
// LifecycleUnit is destroyed.
TEST_F(LifecycleUnitBaseTest, SetStateNotifiesObservers) {
  TestLifecycleUnit lifecycle_unit;
  lifecycle_unit.AddObserver(&observer_);

  // Observer is notified when the state changes.
  EXPECT_CALL(observer_,
              OnLifecycleUnitStateChanged(
                  &lifecycle_unit, lifecycle_unit.GetState(),
                  LifecycleUnitStateChangeReason::BROWSER_INITIATED));
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED,
                          LifecycleUnitStateChangeReason::BROWSER_INITIATED);
  testing::Mock::VerifyAndClear(&observer_);

  // Observer isn't notified when the state stays the same.
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED,
                          LifecycleUnitStateChangeReason::BROWSER_INITIATED);

  lifecycle_unit.RemoveObserver(&observer_);
}

// Verify that observers are notified when the LifecycleUnit is destroyed.
TEST_F(LifecycleUnitBaseTest, DestroyNotifiesObservers) {
  {
    TestLifecycleUnit lifecycle_unit;
    lifecycle_unit.AddObserver(&observer_);
    EXPECT_CALL(observer_, OnLifecycleUnitDestroyed(&lifecycle_unit));
  }
  testing::Mock::VerifyAndClear(&observer_);
}

// Verify the initial GetWallTimeWhenHidden()/GetChromeUsageTimeWhenHidden() of
// a visible LifecycleUnit.
TEST_F(LifecycleUnitBaseTest, InitialLastActiveTimeForVisibleLifecycleUnit) {
  TestLifecycleUnit lifecycle_unit(content::Visibility::VISIBLE,
                                   usage_clock_.get());
  EXPECT_EQ(base::TimeTicks::Max(), lifecycle_unit.GetWallTimeWhenHidden());
  EXPECT_EQ(base::TimeDelta::Max(),
            lifecycle_unit.GetChromeUsageTimeWhenHidden());
}

// Verify the initial GetWallTimeWhenHidden()/GetChromeUsageTimeWhenHidden() of
// a hidden LifecycleUnit.
TEST_F(LifecycleUnitBaseTest, InitialLastActiveTimeForHiddenLifecycleUnit) {
  TestLifecycleUnit lifecycle_unit(content::Visibility::HIDDEN,
                                   usage_clock_.get());
  EXPECT_EQ(NowTicks(), lifecycle_unit.GetWallTimeWhenHidden());
  EXPECT_EQ(usage_clock_->GetTotalUsageTime(),
            lifecycle_unit.GetChromeUsageTimeWhenHidden());
}

// Verify that observers are notified when the visibility of the LifecyleUnit
// changes. Verify that GetWallTimeWhenHidden()/GetChromeUsageTimeWhenHidden()
// are updated properly.
TEST_F(LifecycleUnitBaseTest, VisibilityChangeNotifiesObserversAndUpdatesTime) {
  TestLifecycleUnit lifecycle_unit(content::Visibility::VISIBLE,
                                   usage_clock_.get());
  lifecycle_unit.AddObserver(&observer_);

  // Observer is notified when the visibility changes.
  test_tick_clock_.Advance(base::Minutes(1));
  base::TimeTicks wall_time_when_hidden = NowTicks();
  base::TimeDelta usage_time_when_hidden = usage_clock_->GetTotalUsageTime();
  EXPECT_CALL(observer_, OnLifecycleUnitVisibilityChanged(
                             &lifecycle_unit, content::Visibility::HIDDEN))
      .WillOnce(testing::Invoke(
          [&](LifecycleUnit* lifecycle_unit, content::Visibility visibility) {
            EXPECT_EQ(wall_time_when_hidden,
                      lifecycle_unit->GetWallTimeWhenHidden());
            EXPECT_EQ(usage_time_when_hidden,
                      lifecycle_unit->GetChromeUsageTimeWhenHidden());
          }));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::HIDDEN);
  testing::Mock::VerifyAndClear(&observer_);

  test_tick_clock_.Advance(base::Minutes(1));
  EXPECT_CALL(observer_, OnLifecycleUnitVisibilityChanged(
                             &lifecycle_unit, content::Visibility::OCCLUDED))
      .WillOnce(testing::Invoke(
          [&](LifecycleUnit* lifecycle_unit, content::Visibility visibility) {
            EXPECT_EQ(wall_time_when_hidden,
                      lifecycle_unit->GetWallTimeWhenHidden());
            EXPECT_EQ(usage_time_when_hidden,
                      lifecycle_unit->GetChromeUsageTimeWhenHidden());
          }));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(
      content::Visibility::OCCLUDED);
  testing::Mock::VerifyAndClear(&observer_);

  test_tick_clock_.Advance(base::Minutes(1));
  EXPECT_CALL(observer_, OnLifecycleUnitVisibilityChanged(
                             &lifecycle_unit, content::Visibility::VISIBLE))
      .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit,
                                    content::Visibility visibility) {
        EXPECT_TRUE(lifecycle_unit->GetWallTimeWhenHidden().is_max());
        EXPECT_TRUE(lifecycle_unit->GetChromeUsageTimeWhenHidden().is_max());
      }));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::VISIBLE);
  testing::Mock::VerifyAndClear(&observer_);

  lifecycle_unit.RemoveObserver(&observer_);
}

namespace {

class MockLifecycleUnitSource : public LifecycleUnitSourceBase {
 public:
  MockLifecycleUnitSource() = default;
  ~MockLifecycleUnitSource() override = default;

  MOCK_METHOD0(OnFirstLifecycleUnitCreated, void());
  MOCK_METHOD0(OnAllLifecycleUnitsDestroyed, void());
};

}  // namespace

TEST_F(LifecycleUnitBaseTest, SourceIsNotifiedOfUnitDeath) {
  MockLifecycleUnitSource source;
  EXPECT_EQ(0u, source.lifecycle_unit_count());

  EXPECT_CALL(source, OnFirstLifecycleUnitCreated());
  std::unique_ptr<TestLifecycleUnit> unit1 =
      std::make_unique<TestLifecycleUnit>(&source);
  testing::Mock::VerifyAndClear(&source);
  EXPECT_EQ(1u, source.lifecycle_unit_count());

  std::unique_ptr<TestLifecycleUnit> unit2 =
      std::make_unique<TestLifecycleUnit>(&source);
  testing::Mock::VerifyAndClear(&source);
  EXPECT_EQ(2u, source.lifecycle_unit_count());

  unit1.reset();
  testing::Mock::VerifyAndClear(&source);
  EXPECT_EQ(1u, source.lifecycle_unit_count());

  EXPECT_CALL(source, OnAllLifecycleUnitsDestroyed());
  unit2.reset();
  testing::Mock::VerifyAndClear(&source);
  EXPECT_EQ(0u, source.lifecycle_unit_count());
}

}  // namespace resource_coordinator
