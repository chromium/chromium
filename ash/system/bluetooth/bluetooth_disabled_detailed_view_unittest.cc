// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class BluetoothDisabledDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);
  }

  void TearDown() override { AshTestBase::TearDown(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BluetoothDisabledDetailedViewTest, CanConstruct) {
  BluetoothDisabledDetailedView bluetooth_disabled_detailed_view;
  bluetooth_disabled_detailed_view.SetVisible(true);
}

}  // namespace ash
