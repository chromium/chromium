// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace tray {

class BluetoothDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);

    detailed_view_delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);
    bluetooth_detailed_view_ = std::make_unique<BluetoothDetailedViewImpl>(
        detailed_view_delegate_.get(),
        /*delegate=*/nullptr);
  }

  void TearDown() override { AshTestBase::TearDown(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  std::unique_ptr<BluetoothDetailedView> bluetooth_detailed_view_;
};

TEST_F(BluetoothDetailedViewTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace tray
}  // namespace ash
