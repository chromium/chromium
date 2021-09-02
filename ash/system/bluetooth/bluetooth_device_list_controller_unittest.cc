// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class BluetoothDeviceListControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);

    fake_bluetooth_detailed_view_ =
        std::make_unique<tray::FakeBluetoothDetailedView>(/*delegate=*/nullptr);
    bluetooth_device_list_controller_impl_ =
        std::make_unique<BluetoothDeviceListControllerImpl>(
            fake_bluetooth_detailed_view_.get());
  }

  void TearDown() override { AshTestBase::TearDown(); }

  tray::BluetoothDetailedView* bluetooth_detailed_view() {
    return fake_bluetooth_detailed_view_.get();
  }

  BluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_impl_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<tray::FakeBluetoothDetailedView>
      fake_bluetooth_detailed_view_;
  std::unique_ptr<BluetoothDeviceListControllerImpl>
      bluetooth_device_list_controller_impl_;
};

TEST_F(BluetoothDeviceListControllerTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace ash
