// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class NetworkDetailedViewControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    network_detailed_view_controller_ =
        std::make_unique<NetworkDetailedViewController>(
            /*tray_controller=*/nullptr);
  }

  void TearDown() override {
    network_detailed_view_controller_.reset();

    AshTestBase::TearDown();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NetworkDetailedViewController>
      network_detailed_view_controller_;
};

TEST_F(NetworkDetailedViewControllerTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace ash
