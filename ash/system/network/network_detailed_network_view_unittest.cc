// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class NetworkDetailedNetworkViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    detailed_view_delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);
    network_detailed_network_view_ =
        std::make_unique<NetworkDetailedNetworkViewImpl>(
            detailed_view_delegate_.get(),
            /*delegate=*/nullptr);
  }

  void TearDown() override { AshTestBase::TearDown(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  std::unique_ptr<NetworkDetailedNetworkView> network_detailed_network_view_;
};

TEST_F(NetworkDetailedNetworkViewTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace ash