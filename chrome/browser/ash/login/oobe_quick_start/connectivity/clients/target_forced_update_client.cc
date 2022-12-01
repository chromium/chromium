// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/clients/target_forced_update_client.h"

namespace ash::quick_start {

const char kNotifySourceOfUpdateMessageKey[] = "isForcedUpdateRequired";

TargetForcedUpdateClient::TargetForcedUpdateClient(
    NearbyConnection* nearby_connection,
    QuickStartDecoder* quick_start_decoder)
    : TargetDeviceClientBase(nearby_connection, quick_start_decoder) {}

TargetForcedUpdateClient::~TargetForcedUpdateClient() = default;

void TargetForcedUpdateClient::NotifySourceOfUpdate() {
  base::Value::Dict message_payload;
  message_payload.Set(kNotifySourceOfUpdateMessageKey, true);
  SendPayload(message_payload);
}

void TargetForcedUpdateClient::AuthenticateConnection(ResultCallback callback) {
  NOTIMPLEMENTED();
}

void TargetForcedUpdateClient::OnDataRead(
    absl::optional<std::vector<uint8_t>> data) {
  NOTIMPLEMENTED();
}

}  // namespace ash::quick_start
