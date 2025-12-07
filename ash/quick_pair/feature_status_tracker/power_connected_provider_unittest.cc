// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/power_connected_provider.h"

#include "ash/display/refresh_rate_controller.h"
#include "ash/shell.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace quick_pair {

class PowerConnectedProviderTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // This test may delete/recreate PowerStatus, which can cause dangling
    // pointer in RefreshRateController which observes the power status. Let it
    // forget the power status so that it doesn't reported as a dangling
    // pointer.
    ash::Shell::Get()
        ->refresh_rate_controller()
        ->StopObservingPowerStatusForTest();
    // And BatterySaverController.
    ash::Shell::Get()
        ->battery_saver_controller()
        ->StopObservingPowerStatusForTest();
  }

  // PowerStatus must be initialized before AshTestBase::TearDown() because the
  // latter calls PowerStatus::Shutdown, which crashes if the PowerStatus is not
  // initialized.
  void TearDown() override {
    power_connected_provider_.reset();
    if (!PowerStatus::IsInitialized()) {
      PowerStatus::Initialize();
    }
    AshTestBase::TearDown();
  }

  void SetPowerPropertiesAndNotifyProvider(
      power_manager::PowerSupplyProperties_ExternalPower power_status) {
    if (!power_connected_provider_) {
      return;
    }

    if (!PowerStatus::IsInitialized()) {
      return;
    }

    properties_.set_external_power(power_status);
    PowerStatus::Get()->SetProtoForTesting(properties_);
    power_connected_provider_->OnPowerStatusChanged();
  }

  void InitializeProvider() {
    power_connected_provider_ = std::make_unique<PowerConnectedProvider>();
  }

  void InitializePowerStatus(bool should_initialize) {
    if (should_initialize && !PowerStatus::IsInitialized()) {
      PowerStatus::Initialize();
    } else if (!should_initialize && PowerStatus::IsInitialized()) {
      PowerStatus::Shutdown();
    }
    CHECK(should_initialize == PowerStatus::IsInitialized());
  }

  bool IsPowerConnected() {
    return power_connected_provider_ && power_connected_provider_->is_enabled();
  }

 private:
  std::unique_ptr<PowerConnectedProvider> power_connected_provider_;
  power_manager::PowerSupplyProperties properties_;
};

TEST_F(PowerConnectedProviderTest, DisabledIfPowerStatusNotInitialized) {
  InitializePowerStatus(/*should_initialize=*/false);
  InitializeProvider();
  EXPECT_FALSE(IsPowerConnected());
}

TEST_F(PowerConnectedProviderTest, EnabledWhenACConnection) {
  InitializePowerStatus(/*should_initialize=*/true);
  InitializeProvider();
  SetPowerPropertiesAndNotifyProvider(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  EXPECT_TRUE(IsPowerConnected());
}

TEST_F(PowerConnectedProviderTest, EnabledWhenUSBConnection) {
  InitializePowerStatus(/*should_initialize=*/true);
  InitializeProvider();
  SetPowerPropertiesAndNotifyProvider(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  EXPECT_TRUE(IsPowerConnected());
}

TEST_F(PowerConnectedProviderTest, DisabledWhenDisconnected) {
  InitializePowerStatus(/*should_initialize=*/true);
  InitializeProvider();
  SetPowerPropertiesAndNotifyProvider(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  EXPECT_FALSE(IsPowerConnected());
}

}  // namespace quick_pair
}  // namespace ash
