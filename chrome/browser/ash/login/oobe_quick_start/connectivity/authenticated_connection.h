// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

// Represents a connection that's been authenticated by the shapes verification
// or QR code flow.
class AuthenticatedConnection : public Connection {
 public:
  // TODO(b/234655072): Remove once we have implemented a way to set
  // NearbyConnection at the Connection level.
  explicit AuthenticatedConnection(NearbyConnection* nearby_connection);
  AuthenticatedConnection(AuthenticatedConnection&) = delete;
  AuthenticatedConnection& operator=(AuthenticatedConnection&) = delete;
  ~AuthenticatedConnection() override;

  void RequestAccountTransferAssertion();

 private:
  // Packages a BootstrapOptions request and sends it to the Android device.
  void SendBootstrapOptions();

  // Reusable method to serialize a payload into JSON bytes and send via Nearby
  // Connections.
  void SendPayload(const base::Value::Dict& message_payload);

  // Handle response received from SendBootstrapOptions. This is passed in as a
  // callback to NearbyConnection::Read().
  void OnBootstrapOptionsResponse(absl::optional<std::vector<uint8_t>> data);

  // TODO(b/234655072): Remove once we have  implemented a way to set
  // NearbyConnection at the Connection level.
  NearbyConnection* nearby_connection_;
  base::WeakPtrFactory<AuthenticatedConnection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
