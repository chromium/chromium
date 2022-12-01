// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"

#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcOnBatteryObserverTest : public testing::Test {
 public:
  ArcOnBatteryObserverTest() = default;

  ArcOnBatteryObserverTest(const ArcOnBatteryObserverTest&) = delete;
  ArcOnBatteryObserverTest& operator=(const ArcOnBatteryObserverTest&) = delete;

  ~ArcOnBatteryObserverTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    testing_profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    testing_profile_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  ArcOnBatteryObserver* observer() { return &display_observer_; }
  chromeos::PowerManagerClient* power_manager() {
    return chromeos::PowerManagerClient::Get();
  }
  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  ArcOnBatteryObserver display_observer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(ArcOnBatteryObserverTest, TestConstructDestruct) {}

TEST_F(ArcOnBatteryObserverTest, TestStatusChanges) {
  unittest::ThrottleTestObserver test_observer;
  EXPECT_EQ(0, test_observer.count());

  EXPECT_FALSE(power_manager()->HasObserver(observer()));
  // base::Unretained below: safe because all involved objects share scope.
  observer()->StartObserving(
      profile(), base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                                     base::Unretained(&test_observer)));
  EXPECT_TRUE(power_manager()->HasObserver(observer()));

  EXPECT_EQ(0, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());

  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  power_manager::PowerSupplyProperties no_battery_power;
  no_battery_power.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);

  observer()->PowerChanged(no_battery_power);
  EXPECT_EQ(1, test_observer.count());
  EXPECT_EQ(1, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());
  EXPECT_TRUE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  power_manager::PowerSupplyProperties yes_battery_power;
  yes_battery_power.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);

  observer()->PowerChanged(yes_battery_power);
  EXPECT_EQ(2, test_observer.count());
  EXPECT_EQ(1, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());
  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  EXPECT_TRUE(power_manager()->HasObserver(observer()));
  observer()->StopObserving();
  EXPECT_FALSE(power_manager()->HasObserver(observer()));
}

}  // namespace arc
