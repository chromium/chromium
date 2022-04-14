// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

class MockObserver : public AdaptiveChargingController::Observer {
 public:
  MOCK_METHOD(void, OnAdaptiveChargingStarted, ());
  MOCK_METHOD(void, OnAdaptiveChargingStopped, ());
};

}  // namespace

class AdaptiveChargingControllerTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    // The InitializeFake only wants to be called once.
    if (!chromeos::PowerManagerClient::Get())
      chromeos::PowerManagerClient::InitializeFake();
    power_manager_client_ = chromeos::FakePowerManagerClient::Get();

    adaptive_charging_controller_ =
        std::make_unique<AdaptiveChargingController>();

    adaptive_charging_controller_->AddObserver(&observer_);

    NoSessionAshTestBase::SetUp();
  }

  void TearDown() override {
    adaptive_charging_controller_->RemoveObserver(&observer_);
    adaptive_charging_controller_.reset();

    NoSessionAshTestBase::TearDown();
  }

 protected:
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;
  std::unique_ptr<AdaptiveChargingController> adaptive_charging_controller_;
  MockObserver observer_;
};

TEST_F(AdaptiveChargingControllerTest, IsAdaptiveChargingSupported) {
  // There should be no call for OnAdaptiveChargingStarted or
  // OnAdaptiveChargingStopped.
  EXPECT_CALL(observer_, OnAdaptiveChargingStarted).Times(0);
  EXPECT_CALL(observer_, OnAdaptiveChargingStopped).Times(0);

  // Case (1) default.
  EXPECT_FALSE(adaptive_charging_controller_->IsAdaptiveChargingSupported());

  // Case (2) update adaptive_charging_supported to true.
  power_manager::PowerSupplyProperties power_props;
  power_props.set_adaptive_charging_supported(true);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_TRUE(adaptive_charging_controller_->IsAdaptiveChargingSupported());

  // Case (3) update adaptive_charging_supported to false.
  power_props.set_adaptive_charging_supported(false);
  power_manager_client_->UpdatePowerProperties(power_props);
  EXPECT_FALSE(adaptive_charging_controller_->IsAdaptiveChargingSupported());
}

TEST_F(AdaptiveChargingControllerTest, OnAdaptiveChargingStartedAndStopped) {
  // Case (1) set_adaptive_delaying_charge to true should invoke
  // OnAdaptiveChargingStarted.
  EXPECT_CALL(observer_, OnAdaptiveChargingStarted).Times(1);
  power_manager::PowerSupplyProperties power_props;
  power_props.set_adaptive_delaying_charge(true);
  power_manager_client_->UpdatePowerProperties(power_props);

  // Case (2) set_adaptive_delaying_charge to false should invoke
  // OnAdaptiveChargingStopped.
  EXPECT_CALL(observer_, OnAdaptiveChargingStopped).Times(1);
  power_props.set_adaptive_delaying_charge(false);
  power_manager_client_->UpdatePowerProperties(power_props);

  // Case (3) set_adaptive_delaying_charge to false a second time should NOT
  // invoke OnAdaptiveChargingStopped.
  power_manager_client_->UpdatePowerProperties(power_props);
}

}  // namespace ash
