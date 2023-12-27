// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

namespace ash {

class AdaptiveChargingControllerTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    // The InitializeFake only wants to be called once.
    if (!chromeos::PowerManagerClient::Get())
      chromeos::PowerManagerClient::InitializeFake();
    power_manager_client_ = chromeos::FakePowerManagerClient::Get();

    adaptive_charging_controller_ =
        std::make_unique<AdaptiveChargingController>();

    NoSessionAshTestBase::SetUp();
  }

  void TearDown() override {
    adaptive_charging_controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  raw_ptr<chromeos::FakePowerManagerClient, DanglingUntriaged>
      power_manager_client_ = nullptr;
  std::unique_ptr<AdaptiveChargingController> adaptive_charging_controller_;
};

TEST_F(AdaptiveChargingControllerTest, IsAdaptiveChargingSupported) {
  // Case (1) default.
  EXPECT_FALSE(adaptive_charging_controller_->IsAdaptiveChargingSupported());

  // Case (2) update adaptive_charging_supported to true.
  power_manager::PowerSupplyProperties power_props;
  power_props.set_adaptive_charging_supported(true);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_TRUE(adaptive_charging_controller_->IsAdaptiveChargingSupported());

  // Case (3) update adaptive_charging_supported to false.
  // Because it was already set true in Case 2, it keeps true.
  power_props.set_adaptive_charging_supported(false);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_TRUE(adaptive_charging_controller_->IsAdaptiveChargingSupported());
}

TEST_F(AdaptiveChargingControllerTest, IsAdaptiveDelayingCharge) {
  // Case (1) default.
  EXPECT_FALSE(adaptive_charging_controller_->is_adaptive_delaying_charge());

  // Case (2) set_adaptive_delaying_charge to true without
  // adaptive_charging_heuristic_enabled.
  power_manager::PowerSupplyProperties power_props;
  power_props.set_adaptive_delaying_charge(true);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_TRUE(adaptive_charging_controller_->is_adaptive_delaying_charge());

  // Case (3) set_adaptive_delaying_charge to true with
  // |adaptive_charging_heuristic_enabled| == true.
  power_props.set_adaptive_delaying_charge(true);
  power_props.set_adaptive_charging_heuristic_enabled(true);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_TRUE(adaptive_charging_controller_->is_adaptive_delaying_charge());

  // Case (4) set_adaptive_delaying_charge to false.
  power_props.set_adaptive_delaying_charge(false);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_FALSE(adaptive_charging_controller_->is_adaptive_delaying_charge());
}

}  // namespace ash
