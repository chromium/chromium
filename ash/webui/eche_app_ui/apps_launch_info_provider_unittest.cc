// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include <memory>
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {

class AppsLaunchInfoProviderTest : public testing::Test {
 public:
  AppsLaunchInfoProviderTest() = default;
  AppsLaunchInfoProviderTest(const AppsLaunchInfoProviderTest&) = delete;
  AppsLaunchInfoProviderTest& operator=(const AppsLaunchInfoProviderTest&) =
      delete;
  ~AppsLaunchInfoProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    handler_ = std::make_unique<EcheConnectionStatusHandler>();
    provider_ = std::make_unique<AppsLaunchInfoProvider>(handler_.get());
  }

  void TearDown() override {
    provider_.reset();
    handler_.reset();
  }

  void NotifyConnectionStatusForUiChanged(mojom::ConnectionStatus status) {
    handler_->SetConnectionStatusForUi(status);
  }

  mojom::ConnectionStatus GetLastConnectionStatus() {
    return provider_->GetConnectionStatusForUi();
  }

  void SetEntryPoint(mojom::AppStreamLaunchEntryPoint entry_point) {
    provider_->SetEntryPoint(entry_point);
  }

  mojom::AppStreamLaunchEntryPoint GetEntryPoint() {
    return provider_->entry_point();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<EcheConnectionStatusHandler> handler_;
  std::unique_ptr<AppsLaunchInfoProvider> provider_;
};

TEST_F(AppsLaunchInfoProviderTest, OnConnectionStatusForUiChanged) {
  EXPECT_EQ(GetLastConnectionStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusForUiChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetLastConnectionStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);

  NotifyConnectionStatusForUiChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);

  NotifyConnectionStatusForUiChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetLastConnectionStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);

  NotifyConnectionStatusForUiChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetLastConnectionStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
}

TEST_F(AppsLaunchInfoProviderTest, SetEntryPoint) {
  EXPECT_EQ(GetEntryPoint(), mojom::AppStreamLaunchEntryPoint::UNKNOWN);

  SetEntryPoint(mojom::AppStreamLaunchEntryPoint::NOTIFICATION);
  EXPECT_EQ(GetEntryPoint(), mojom::AppStreamLaunchEntryPoint::NOTIFICATION);

  SetEntryPoint(mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  EXPECT_EQ(GetEntryPoint(), mojom::AppStreamLaunchEntryPoint::APPS_LIST);

  SetEntryPoint(mojom::AppStreamLaunchEntryPoint::RECENT_APPS);
  EXPECT_EQ(GetEntryPoint(), mojom::AppStreamLaunchEntryPoint::RECENT_APPS);
}

}  // namespace ash::eche_app