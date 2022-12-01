// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connector_impl.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace eche_app {

EcheConnectorImpl::EcheConnectorImpl(
    FeatureStatusProvider* eche_feature_status_provider,
    secure_channel::ConnectionManager* connection_manager,
    EcheConnectionScheduler* connection_scheduler)
    : eche_feature_status_provider_(eche_feature_status_provider),
      connection_manager_(connection_manager),
      connection_scheduler_(connection_scheduler) {
  eche_feature_status_provider_->AddObserver(this);
  connection_manager_->AddObserver(this);
}

EcheConnectorImpl::~EcheConnectorImpl() {
  eche_feature_status_provider_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
}

void EcheConnectorImpl::SendMessage(const proto::ExoMessage message) {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  switch (feature_status) {
    case FeatureStatus::kDependentFeature:
      [[fallthrough]];
    case FeatureStatus::kDependentFeaturePending:
      PA_LOG(WARNING) << "Attempting to send message with ineligible dep";
      break;
    case FeatureStatus::kIneligible:
      PA_LOG(WARNING) << "Attempting to send message for ineligible feature";
      break;
    case FeatureStatus::kDisabled:
      PA_LOG(WARNING) << "Attempting to send message for disabled feature";
      QueueMessageWhenDisabled(message);
      break;
    case FeatureStatus::kDisconnected:
      AttemptNearbyConnection();
      [[fallthrough]];
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
  connection_scheduler_->DisconnectAndClearBackoffAttempts();
}

void EcheConnectorImpl::SendAppsSetupRequest() {
  PA_LOG(INFO) << "Send SendAppsSetupRequest";
  proto::SendAppsSetupRequest request;
  proto::ExoMessage message;
  *message.mutable_apps_setup_request() = std::move(request);
  SendMessage(message);
}

void EcheConnectorImpl::GetAppsAccessStateRequest() {
  PA_LOG(INFO) << "Send GetAppsAccessStateRequest";
  proto::GetAppsAccessStateRequest request;
  proto::ExoMessage message;
  *message.mutable_apps_access_state_request() = std::move(request);
  SendMessage(message);
}

void EcheConnectorImpl::AttemptNearbyConnection() {
  connection_scheduler_->ScheduleConnectionNow();
}

void EcheConnectorImpl::OnFeatureStatusChanged() {
  MaybeFlushQueue();
}

void EcheConnectorImpl::OnConnectionStatusChanged() {
  MaybeFlushQueue();
}

void EcheConnectorImpl::QueueMessageWhenDisabled(
    const proto::ExoMessage message) {
  if (!IsMessageAllowedWhenDisabled(message))
    return;
  switch (connection_manager_->GetStatus()) {
    case secure_channel::ConnectionManager::Status::kDisconnected:
      AttemptNearbyConnection();
      [[fallthrough]];
    case secure_channel::ConnectionManager::Status::kConnecting:
      PA_LOG(INFO) << "Connecting; queuing message";
      message_queue_.push(message);
      break;
    case secure_channel::ConnectionManager::Status::kConnected:
      message_queue_.push(message);
      FlushQueueWhenDisabled();
      break;
  }
}

bool EcheConnectorImpl::IsMessageAllowedWhenDisabled(
    const proto::ExoMessage message) {
  // We only allow ExoMessages related to the onboarding process when the Eche
  // feature is disabled. Other ExoMessages can only be delivered if the Eche
  // feature is enabled, and this approach avoids using the apps streaming
  // feature in unexpected states.
  return message.has_apps_access_state_request() ||
         message.has_apps_setup_request();
}

void EcheConnectorImpl::MaybeFlushQueue() {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  const bool isConnected =
      connection_manager_->GetStatus() ==
      secure_channel::ConnectionManager::Status::kConnected;
  if (message_queue_.empty() || !isConnected)
    return;
  if (feature_status == FeatureStatus::kConnected)
    FlushQueue();
  else if (feature_status == FeatureStatus::kDisabled)
    FlushQueueWhenDisabled();
}

void EcheConnectorImpl::FlushQueue() {
  const int size = GetMessageCount();
  for (int i = 0; i < size; i++) {
    connection_manager_->SendMessage(
        message_queue_.front().SerializeAsString());
    message_queue_.pop();
  }
}

void EcheConnectorImpl::FlushQueueWhenDisabled() {
  const int size = GetMessageCount();
  PA_LOG(INFO) << "Flushing message queue";
  for (int i = 0; i < size; i++) {
    proto::ExoMessage message = message_queue_.front();
    if (IsMessageAllowedWhenDisabled(message)) {
      connection_manager_->SendMessage(message.SerializeAsString());
    }
    message_queue_.pop();
  }
}

int EcheConnectorImpl::GetMessageCount() {
  return message_queue_.size();
}

}  // namespace eche_app
}  // namespace ash
