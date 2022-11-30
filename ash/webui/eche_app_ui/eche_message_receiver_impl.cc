// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_message_receiver_impl.h"

namespace ash {
namespace eche_app {

EcheMessageReceiverImpl::EcheMessageReceiverImpl(
    secure_channel::ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
  connection_manager_->AddObserver(this);
}

EcheMessageReceiverImpl::~EcheMessageReceiverImpl() {
  connection_manager_->RemoveObserver(this);
}

void EcheMessageReceiverImpl::OnMessageReceived(const std::string& payload) {
  proto::ExoMessage message;
  message.ParseFromString(payload);
  if (message.has_apps_access_state_response()) {
    NotifyGetAppsAccessStateResponse(message.apps_access_state_response());
  } else if (message.has_apps_setup_response()) {
    NotifySendAppsSetupResponse(message.apps_setup_response());
  } else if (message.has_status_change()) {
    NotifyStatusChange(message.status_change());
  } else if (message.has_policy_state_change()) {
    NotifyAppPolicyStateChange(message.policy_state_change());
  }
}

}  // namespace eche_app
}  // namespace ash
