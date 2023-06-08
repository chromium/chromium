// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_saver_controller.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

class BatterySaverControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>(
        features::kBatterySaver);
    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();

    // Let initial PowerStatus call to
    // PowerManagerClient::GetBatterySaverModeState to complete.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    chromeos::PowerManagerClient::Shutdown();
    scoped_feature_list_.reset();
  }

 protected:
  BatterySaverController* battery_saver_controller() {
    return Shell::Get()->battery_saver_controller();
  }

  void UpdatePowerStatus(double battery_percent, bool charging) {
    power_manager::PowerSupplyProperties props;
    props.set_battery_percent(battery_percent);
    auto external_power =
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
    if (charging) {
      external_power = power_manager::PowerSupplyProperties_ExternalPower_AC;
    }
    props.set_external_power(external_power);

    base::RunLoop run_loop;
    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
    run_loop.RunUntilIdle();
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_ = nullptr;
};

// Test that the automatic logic to turn battery saver on and off without
// direct user action works.
TEST_F(BatterySaverControllerTest, AutoEnableDisable) {
  // Battery near full and charging, no battery saver.
  UpdatePowerStatus(80.0, true);
  EXPECT_FALSE(PowerStatus::Get()->IsBatterySaverActive());

  // Battery near full and discharging, still no battery saver.
  UpdatePowerStatus(80.0, false);
  EXPECT_FALSE(PowerStatus::Get()->IsBatterySaverActive());

  // Battery discharging but just above the activation %, still no battery
  // saver.
  UpdatePowerStatus(BatterySaverController::kActivationChargePercent + 0.1,
                    false);
  EXPECT_FALSE(PowerStatus::Get()->IsBatterySaverActive());

  // Battery dicharging and at activation %, battery saver turns on.
  UpdatePowerStatus(BatterySaverController::kActivationChargePercent, false);
  EXPECT_TRUE(PowerStatus::Get()->IsBatterySaverActive());

  // Discharge more, battery saver remains on.
  UpdatePowerStatus(5.0, false);
  EXPECT_TRUE(PowerStatus::Get()->IsBatterySaverActive());

  // Start charging, even with a low battery %, battery saver disables.
  UpdatePowerStatus(5.0, true);
  EXPECT_FALSE(PowerStatus::Get()->IsBatterySaverActive());
}

// TODO(mwoj): Test that notifications are sent and dismissed.
// And that opting out via the notification turns off battery saver.
// And that notifications don't appear after they have been declined until
// charge goes back above activation %.

}  // namespace ash
