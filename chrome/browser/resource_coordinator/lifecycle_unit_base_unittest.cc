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

  MOCK_METHOD2(OnLifecycleUnitStateChanged,
               void(LifecycleUnit*, LifecycleUnitState));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));
};

class LifecycleUnitBaseTest : public testing::Test {
 public:
  LifecycleUnitBaseTest(const LifecycleUnitBaseTest&) = delete;
  LifecycleUnitBaseTest& operator=(const LifecycleUnitBaseTest&) = delete;

 protected:
  LifecycleUnitBaseTest() {
    metrics::DesktopSessionDurationTracker::Initialize();
  }

  ~LifecycleUnitBaseTest() override {
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  ScopedSetClocksForTesting scoped_set_clocks_for_testing_{&test_clock_,
                                                           &test_tick_clock_};
  testing::StrictMock<MockLifecycleUnitObserver> observer_;
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
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED);
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
  EXPECT_CALL(observer_, OnLifecycleUnitStateChanged(
                             &lifecycle_unit, lifecycle_unit.GetState()));
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED);
  testing::Mock::VerifyAndClear(&observer_);

  // Observer isn't notified when the state stays the same.
  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED);

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
