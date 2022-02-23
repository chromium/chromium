// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connector_impl.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"

#include <memory>
#include <vector>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

class EcheConnectorImplTest : public testing::Test {
 protected:
  EcheConnectorImplTest() = default;
  EcheConnectorImplTest(const EcheConnectorImplTest&) = delete;
  EcheConnectorImplTest& operator=(const EcheConnectorImplTest&) = delete;
  ~EcheConnectorImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    connector_ = std::make_unique<EcheConnectorImpl>(
        &fake_feature_status_provider_, &fake_connection_manager_);
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  void SetFeatureStatus(FeatureStatus feature_status) {
    fake_feature_status_provider_.SetStatus(feature_status);
  }

  FakeFeatureStatusProvider fake_feature_status_provider_;
  secure_channel::FakeConnectionManager fake_connection_manager_;
  std::unique_ptr<EcheConnectorImpl> connector_;
};

// Tests SendAppsSetupRequest in different feature status.
TEST_F(EcheConnectorImplTest, SendAppsSetupRequest) {
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kNotEnabledByPhone);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kIneligible);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kDisabled);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kDisconnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 2);

  SetFeatureStatus(FeatureStatus::kConnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 3u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);
}

// Tests GetAppsAccessStateRequest in different feature status.
TEST_F(EcheConnectorImplTest, GetAppsAccessStateRequest) {
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kNotEnabledByPhone);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kIneligible);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kDisabled);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kDisconnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_manager_.num_attempt_connection_calls(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 2);

  SetFeatureStatus(FeatureStatus::kConnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 3u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);
}

// Tests Disconnect will disconnect ConnectionManager and clear message queue.
TEST_F(EcheConnectorImplTest, Disconnect) {
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  connector_->SendAppsSetupRequest();

  connector_->Disconnect();

  EXPECT_EQ(connector_->GetMessageCount(), 0);
  EXPECT_EQ(fake_connection_manager_.num_disconnect_calls(), 1u);
}

}  // namespace eche_app
}  // namespace ash
