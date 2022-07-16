// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_message_receiver.h"

#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"

namespace ash {
namespace eche_app {

EcheMessageReceiver::EcheMessageReceiver() = default;
EcheMessageReceiver::~EcheMessageReceiver() = default;

void EcheMessageReceiver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheMessageReceiver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheMessageReceiver::NotifyGetAppsAccessStateResponse(
    proto::GetAppsAccessStateResponse apps_access_state_response) {
  for (auto& observer : observer_list_)
    observer.OnGetAppsAccessStateResponseReceived(apps_access_state_response);
}

void EcheMessageReceiver::NotifySendAppsSetupResponse(
    proto::SendAppsSetupResponse apps_setup_response) {
  for (auto& observer : observer_list_)
    observer.OnSendAppsSetupResponseReceived(apps_setup_response);
}

void EcheMessageReceiver::NotifyStatusChange(
    proto::StatusChange status_change) {
  for (auto& observer : observer_list_)
    observer.OnStatusChange(status_change.type());
}

}  // namespace eche_app
}  // namespace ash
