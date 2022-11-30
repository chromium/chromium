// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/sensor/arc_iio_sensor_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_iio_sensor_instance.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcIioSensorBridgeTest : public testing::Test {
 public:
  ArcIioSensorBridgeTest() = default;
  ~ArcIioSensorBridgeTest() override = default;
  ArcIioSensorBridgeTest(const ArcIioSensorBridgeTest&) = delete;
  ArcIioSensorBridgeTest& operator=(const ArcIioSensorBridgeTest&) = delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    bridge_ = std::make_unique<ArcIioSensorBridge>(nullptr /* context */,
                                                   &bridge_service_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    bridge_ = nullptr;
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void InitializeInstance() {
    bridge_service_.iio_sensor()->SetInstance(&fake_instance_);
    WaitForInstanceReady(bridge_service_.iio_sensor());
  }

  base::test::TaskEnvironment task_environment_;

  ArcBridgeService bridge_service_;
  FakeIioSensorInstance fake_instance_;
  std::unique_ptr<ArcIioSensorBridge> bridge_;
};

TEST_F(ArcIioSensorBridgeTest, TabletMode) {
  // Verify the fake's initial state.
  EXPECT_FALSE(fake_instance_.is_tablet_mode_on());

  // ARC receives the tablet mode update just after initialization.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  InitializeInstance();
  EXPECT_TRUE(fake_instance_.is_tablet_mode_on());

  // ARC continues receiving updates after initialization.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  EXPECT_FALSE(fake_instance_.is_tablet_mode_on());

  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_TRUE(fake_instance_.is_tablet_mode_on());
}

}  // namespace arc
