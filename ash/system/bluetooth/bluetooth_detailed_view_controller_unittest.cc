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
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

namespace ash {
namespace tray {
namespace {

class FakeBluetoothDetailedViewFactory : BluetoothDetailedView::Factory {
 public:
  FakeBluetoothDetailedViewFactory() = default;
  FakeBluetoothDetailedViewFactory(const FakeBluetoothDetailedViewFactory&) =
      delete;
  const FakeBluetoothDetailedViewFactory& operator=(
      const FakeBluetoothDetailedViewFactory&) = delete;
  ~FakeBluetoothDetailedViewFactory() override = default;

 private:
  std::unique_ptr<BluetoothDetailedView> CreateForTesting(
      BluetoothDetailedView::Delegate* delegate) override {
    return std::make_unique<FakeBluetoothDetailedView>(delegate);
  }
};

class FakeBluetoothDeviceListControllerFactory
    : BluetoothDeviceListController::Factory {
 public:
  FakeBluetoothDeviceListControllerFactory() = default;
  FakeBluetoothDeviceListControllerFactory(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  const FakeBluetoothDeviceListControllerFactory& operator=(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  ~FakeBluetoothDeviceListControllerFactory() override = default;

 private:
  std::unique_ptr<BluetoothDeviceListController> CreateForTesting() override {
    return std::make_unique<FakeBluetoothDeviceListController>();
  }
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
  }

  void TearDown() override {
    bluetooth_detailed_view_controller_.reset();

    AshTestBase::TearDown();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<BluetoothDetailedViewController>
      bluetooth_detailed_view_controller_;
  chromeos::bluetooth_config::ScopedBluetoothConfigTestHelper
      scoped_bluetooth_config_test_helper_;
};

TEST_F(BluetoothDetailedViewControllerTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace tray
}  // namespace ash
