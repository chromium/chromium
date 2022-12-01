// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connector_impl.h"
#include "ash/webui/eche_app_ui/fake_eche_connection_scheduler.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"

#include <memory>
#include <vector>

#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
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
        &fake_feature_status_provider_, &fake_connection_manager_,
        &fake_connection_scheduler_);
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  void SetFeatureStatus(FeatureStatus feature_status) {
    fake_feature_status_provider_.SetStatus(feature_status);
  }

  void SendNotAllowedMessage() {
    proto::ExoMessage message;
    connector_->SendMessage(message);
  }

  FakeFeatureStatusProvider fake_feature_status_provider_;
  secure_channel::FakeConnectionManager fake_connection_manager_;
  FakeEcheConnectionScheduler fake_connection_scheduler_;
  std::unique_ptr<EcheConnectorImpl> connector_;
};

// Tests SendAppsSetupRequest in different feature status.
TEST_F(EcheConnectorImplTest, SendAppsSetupRequest) {
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetFeatureStatus(FeatureStatus::kIneligible);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisabled);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);

  connector_->SendAppsSetupRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 2u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kConnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 2u);
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

  SetFeatureStatus(FeatureStatus::kIneligible);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisabled);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);

  connector_->GetAppsAccessStateRequest();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 2u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 1u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kConnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 2u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);
}

// Tests Disconnect will disconnect ConnectionManager and clear message queue.
TEST_F(EcheConnectorImplTest, Disconnect) {
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  connector_->SendAppsSetupRequest();

  connector_->Disconnect();

  EXPECT_EQ(connector_->GetMessageCount(), 0);
  EXPECT_EQ(fake_connection_scheduler_.num_disconnect_calls(), 1u);
}

// Tests Send not allowed message with disabled feature status.
TEST_F(EcheConnectorImplTest, SendMessage) {
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  SendNotAllowedMessage();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetFeatureStatus(FeatureStatus::kDisabled);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  SendNotAllowedMessage();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);

  SendNotAllowedMessage();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(fake_connection_scheduler_.num_schedule_connection_now_calls(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 1);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  SendNotAllowedMessage();

  EXPECT_EQ(fake_connection_manager_.sent_messages().size(), 0u);
  EXPECT_EQ(connector_->GetMessageCount(), 0);
}

}  // namespace eche_app
}  // namespace ash
