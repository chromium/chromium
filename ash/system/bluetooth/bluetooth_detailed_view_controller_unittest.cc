// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/system/bluetooth/fake_bluetooth_device_list_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

namespace ash {
namespace tray {
namespace {

using chromeos::bluetooth_config::AdapterStateController;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;

class FakeBluetoothDetailedViewFactory : public BluetoothDetailedView::Factory {
 public:
  FakeBluetoothDetailedViewFactory() = default;
  FakeBluetoothDetailedViewFactory(const FakeBluetoothDetailedViewFactory&) =
      delete;
  const FakeBluetoothDetailedViewFactory& operator=(
      const FakeBluetoothDetailedViewFactory&) = delete;
  ~FakeBluetoothDetailedViewFactory() override = default;

  FakeBluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_;
  }

 private:
  std::unique_ptr<BluetoothDetailedView> CreateForTesting(
      BluetoothDetailedView::Delegate* delegate) override {
    DCHECK(!bluetooth_detailed_view_);
    std::unique_ptr<FakeBluetoothDetailedView> bluetooth_detailed_view =
        std::make_unique<FakeBluetoothDetailedView>(delegate);
    bluetooth_detailed_view_ = bluetooth_detailed_view.get();
    return bluetooth_detailed_view;
  }

  FakeBluetoothDetailedView* bluetooth_detailed_view_ = nullptr;
};

class FakeBluetoothDeviceListControllerFactory
    : public BluetoothDeviceListController::Factory {
 public:
  FakeBluetoothDeviceListControllerFactory() = default;
  FakeBluetoothDeviceListControllerFactory(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  const FakeBluetoothDeviceListControllerFactory& operator=(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  ~FakeBluetoothDeviceListControllerFactory() override = default;

  FakeBluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_;
  }

 private:
  std::unique_ptr<BluetoothDeviceListController> CreateForTesting() override {
    DCHECK(!bluetooth_device_list_controller_);
    std::unique_ptr<FakeBluetoothDeviceListController>
        bluetooth_device_list_controller =
            std::make_unique<FakeBluetoothDeviceListController>();
    bluetooth_device_list_controller_ = bluetooth_device_list_controller.get();
    return bluetooth_device_list_controller;
  }

  FakeBluetoothDeviceListController* bluetooth_device_list_controller_ =
      nullptr;
};

}  // namespace

class BluetoothDetailedViewControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);

    bluetooth_detailed_view_controller_ =
        std::make_unique<BluetoothDetailedViewController>(
            /*tray_controller=*/nullptr);

    BluetoothDetailedView::Factory::SetFactoryForTesting(
        &bluetooth_detailed_view_factory_);
    BluetoothDeviceListController::Factory::SetFactoryForTesting(
        &bluetooth_device_list_controller_factory_);

    // We have access to the fakes through our factories so we don't bother
    // caching the view here.
    static_cast<DetailedViewController*>(
        bluetooth_detailed_view_controller_.get())
        ->CreateView();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    BluetoothDeviceListController::Factory::SetFactoryForTesting(nullptr);
    BluetoothDetailedView::Factory::SetFactoryForTesting(nullptr);
    bluetooth_detailed_view_controller_.reset();

    AshTestBase::TearDown();
  }

  BluetoothSystemState GetBluetoothAdapterState() {
    return scoped_bluetooth_config_test_helper_.fake_adapter_state_controller()
        ->GetAdapterState();
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    scoped_bluetooth_config_test_helper_.fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  BluetoothDetailedView::Delegate* bluetooth_detailed_view_delegate() {
    return bluetooth_detailed_view_controller_.get();
  }

  FakeBluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_factory_.bluetooth_detailed_view();
  }

  FakeBluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_factory_
        .bluetooth_device_list_controller();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  chromeos::bluetooth_config::ScopedBluetoothConfigTestHelper
      scoped_bluetooth_config_test_helper_;
  std::unique_ptr<BluetoothDetailedViewController>
      bluetooth_detailed_view_controller_;
  FakeBluetoothDetailedViewFactory bluetooth_detailed_view_factory_;
  FakeBluetoothDeviceListControllerFactory
      bluetooth_device_list_controller_factory_;
};

TEST_F(BluetoothDetailedViewControllerTest,
       NotifiesWhenBluetoothEnabledStateChanges) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().has_value());
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .has_value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .value());

  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_FALSE(bluetooth_device_list_controller()
                   ->last_bluetooth_enabled_state()
                   .value());

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .value());
}

TEST_F(BluetoothDetailedViewControllerTest,
       ChangesBluetoothEnabledStateWhenTogglePressed) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
}

}  // namespace tray
}  // namespace ash
