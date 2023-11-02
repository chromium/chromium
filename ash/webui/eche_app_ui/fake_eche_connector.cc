// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_eche_connector.h"

namespace ash {
namespace eche_app {

FakeEcheConnector::FakeEcheConnector() = default;
FakeEcheConnector::~FakeEcheConnector() = default;

void FakeEcheConnector::SendMessage(const proto::ExoMessage message) {}
void FakeEcheConnector::Disconnect() {}

void FakeEcheConnector::SendAppsSetupRequest() {
  ++send_apps_setup_request_count_;
}

void FakeEcheConnector::GetAppsAccessStateRequest() {
  ++get_apps_access_state_request_count_;
}

void FakeEcheConnector::AttemptNearbyConnection() {
  ++attempt_nearby_connection_count_;
}

}  // namespace eche_app
}  // namespace ash
