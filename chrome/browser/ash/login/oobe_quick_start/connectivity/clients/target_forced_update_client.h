// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CLIENTS_TARGET_FORCED_UPDATE_CLIENT_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CLIENTS_TARGET_FORCED_UPDATE_CLIENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/quick_start_decoder.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_client_base.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

// TargetForcedUpdateClient is the client that will prepare requests and
// parse responses for all round trips between the Chromebook and Android phone
// related to preparing for a forced update and resuming the connection
// afterwards. Before the update occurs, the Chromebook must notify the phone.
// After the update occurs and the Nearby Connection is resumed, this client
// will handle the handshake required to authenticate the resumed connection.
class TargetForcedUpdateClient : public TargetDeviceClientBase {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  TargetForcedUpdateClient(NearbyConnection* nearby_connection,
                           QuickStartDecoder* quick_start_decoder);
  TargetForcedUpdateClient(const TargetForcedUpdateClient&) = delete;
  TargetForcedUpdateClient& operator=(const TargetForcedUpdateClient&) = delete;
  ~TargetForcedUpdateClient() override;

  // Inform source device when the Chromebook must update its OS.
  void NotifySourceOfUpdate();

  // Attempt to authenticate the Nearby Connection via an HMAC handshake.
  void AuthenticateConnection(ResultCallback callback);

 private:
  // TargetDeviceClientBase:
  void OnDataRead(absl::optional<std::vector<uint8_t>> data) override;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CLIENTS_TARGET_FORCED_UPDATE_CLIENT_H_
