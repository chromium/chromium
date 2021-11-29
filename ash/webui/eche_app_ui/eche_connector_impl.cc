// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connector_impl.h"

#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace eche_app {

EcheConnectorImpl::EcheConnectorImpl(
    EcheFeatureStatusProvider* eche_feature_status_provider,
    secure_channel::ConnectionManager* connection_manager)
    : eche_feature_status_provider_(eche_feature_status_provider),
      connection_manager_(connection_manager) {
  eche_feature_status_provider_->AddObserver(this);
}

EcheConnectorImpl::~EcheConnectorImpl() {
  eche_feature_status_provider_->RemoveObserver(this);
}

void EcheConnectorImpl::SendMessage(const std::string& message) {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  switch (feature_status) {
    case FeatureStatus::kDependentFeature:
      FALLTHROUGH;
    case FeatureStatus::kDependentFeaturePending:
      PA_LOG(WARNING) << "Attempting to send message with ineligible dep";
      break;
    case FeatureStatus::kNotEnabledByPhone:
      FALLTHROUGH;
    case FeatureStatus::kIneligible:
      PA_LOG(WARNING) << "Attempting to send message for ineligible feature";
      break;
    case FeatureStatus::kDisabled:
      PA_LOG(WARNING) << "Attempting to send message for disabled feature";
      break;
    case FeatureStatus::kDisconnected:
      connection_manager_->AttemptNearbyConnection();
      FALLTHROUGH;
    case FeatureStatus::kConnecting:
      PA_LOG(INFO) << "Connecting; queuing message";
      message_queue_.push(message);
      break;
    case FeatureStatus::kConnected:
      message_queue_.push(message);
      FlushQueue();
      break;
  }
}

void EcheConnectorImpl::Disconnect() {
  // Drain queue
  if (!message_queue_.empty())
    PA_LOG(INFO) << "Draining nonempty queue after manual disconnect";
  while (!message_queue_.empty())
    message_queue_.pop();
  connection_manager_->Disconnect();
}

void EcheConnectorImpl::SendAppsSetupRequest() {
  proto::SendAppsSetupRequest request;
  proto::ExoMessage message;
  *message.mutable_apps_setup_request() = std::move(request);
  SendMessage(message.SerializeAsString());
}

void EcheConnectorImpl::GetAppsAccessStateRequest() {
  proto::GetAppsAccessStateRequest request;
  proto::ExoMessage message;
  *message.mutable_apps_access_state_request() = std::move(request);
  SendMessage(message.SerializeAsString());
}

void EcheConnectorImpl::AttemptNearbyConnection() {
  connection_manager_->AttemptNearbyConnection();
}

void EcheConnectorImpl::OnFeatureStatusChanged() {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  if (feature_status == FeatureStatus::kConnected && !message_queue_.empty()) {
    PA_LOG(INFO) << "Flushing message queue";
    FlushQueue();
  }
}

void EcheConnectorImpl::FlushQueue() {
  const int size = message_queue_.size();
  for (int i = 0; i < size; i++) {
    connection_manager_->SendMessage(message_queue_.front());
    message_queue_.pop();
  }
}

}  // namespace eche_app
}  // namespace ash
