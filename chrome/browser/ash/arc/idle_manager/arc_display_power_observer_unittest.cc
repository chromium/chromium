// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcDisplayPowerObserverTest : public ash::AshTestBase {
 public:
  ArcDisplayPowerObserverTest() = default;

  ArcDisplayPowerObserverTest(const ArcDisplayPowerObserverTest&) = delete;
  ArcDisplayPowerObserverTest& operator=(const ArcDisplayPowerObserverTest&) =
      delete;

  ~ArcDisplayPowerObserverTest() override = default;

  void SetUp() override { ash::AshTestBase::SetUp(); }

  void TearDown() override { ash::AshTestBase::TearDown(); }

 protected:
  ArcDisplayPowerObserver* observer() { return &display_observer_; }
  display::DisplayConfigurator* display_configurator() {
    return ash::Shell::Get()->display_configurator();
  }
  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  ArcDisplayPowerObserver display_observer_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(ArcDisplayPowerObserverTest, TestConstructDestruct) {}

TEST_F(ArcDisplayPowerObserverTest, TestStatusChanges) {
  unittest::ThrottleTestObserver test_observer;
  EXPECT_EQ(0, test_observer.count());
  EXPECT_FALSE(display_configurator()->HasObserverForTesting(observer()));
  // base::Unretained below: safe because all involved objects share scope.
  observer()->StartObserving(
      profile(), base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                                     base::Unretained(&test_observer)));
  EXPECT_TRUE(display_configurator()->HasObserverForTesting(observer()));

  EXPECT_EQ(0, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());
  // Expected to never change
  EXPECT_FALSE(observer()->active());

  // Expected initial state
  EXPECT_FALSE(observer()->enforced());

  observer()->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ(1, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(1, test_observer.enforced_count());
  EXPECT_FALSE(observer()->active());
  EXPECT_TRUE(observer()->enforced());

  observer()->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_ON);
  EXPECT_EQ(2, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(1, test_observer.enforced_count());
  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  EXPECT_TRUE(display_configurator()->HasObserverForTesting(observer()));
  observer()->StopObserving();
  EXPECT_FALSE(display_configurator()->HasObserverForTesting(observer()));
}

}  // namespace arc
