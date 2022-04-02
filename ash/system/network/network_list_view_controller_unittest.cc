// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/network/fake_network_detailed_network_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class NetworkListViewControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    fake_network_detailed_network_view_ =
        std::make_unique<FakeNetworkDetailedNetworkView>(
            /*delegate=*/nullptr);

    network_list_view_controller_impl_ =
        std::make_unique<NetworkListViewControllerImpl>(
            fake_network_detailed_network_view_.get());
  }

  void TearDown() override {
    network_list_view_controller_impl_.reset();
    fake_network_detailed_network_view_.reset();

    AshTestBase::TearDown();
  }

  FakeNetworkDetailedNetworkView* fake_network_detailed_network_view() {
    return fake_network_detailed_network_view_.get();
  }

  NetworkListViewController* network_list_view_controller_impl() {
    return network_list_view_controller_impl_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeNetworkDetailedNetworkView>
      fake_network_detailed_network_view_;
  std::unique_ptr<NetworkListViewControllerImpl>
      network_list_view_controller_impl_;
};

TEST_F(NetworkListViewControllerTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace ash
