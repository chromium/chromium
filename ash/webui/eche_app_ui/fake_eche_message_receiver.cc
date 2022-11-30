// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_eche_message_receiver.h"

namespace ash {
namespace eche_app {

FakeEcheMessageReceiver::FakeEcheMessageReceiver() = default;
FakeEcheMessageReceiver::~FakeEcheMessageReceiver() = default;

void FakeEcheMessageReceiver::FakeGetAppsAccessStateResponse(
    eche_app::proto::Result result,
    eche_app::proto::AppsAccessState status) {
  proto::GetAppsAccessStateResponse response;
  response.set_result(result);
  response.set_apps_access_state(status);
  NotifyGetAppsAccessStateResponse(response);
}

void FakeEcheMessageReceiver::FakeSendAppsSetupResponse(
    eche_app::proto::Result result,
    eche_app::proto::AppsAccessState status) {
  proto::SendAppsSetupResponse response;
  response.set_result(result);
  response.set_apps_access_state(status);
  NotifySendAppsSetupResponse(response);
}

void FakeEcheMessageReceiver::FakeStatusChange(
    proto::StatusChangeType status_change_type) {
  proto::StatusChange status_change;
  status_change.set_type(status_change_type);
  NotifyStatusChange(status_change);
}

void FakeEcheMessageReceiver::FakeAppPolicyStateChange(
    proto::AppStreamingPolicy app_policy_state) {
  proto::PolicyStateChange policy_state_change;
  policy_state_change.set_app_policy_state(app_policy_state);
  NotifyAppPolicyStateChange(policy_state_change);
}

}  // namespace eche_app
}  // namespace ash
