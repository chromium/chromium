// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/wifi_credentials.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
namespace ash::quick_start {

// Represents a connection that's been authenticated by the shapes verification
// or QR code flow.
class AuthenticatedConnection : public Connection {
 public:
  using RequestAccountTransferAssertionCallback =
      base::OnceCallback<void(absl::optional<FidoAssertionInfo>)>;

  using RequestWifiCredentialsCallback =
      base::OnceCallback<void(absl::optional<WifiCredentials>)>;

  AuthenticatedConnection(NearbyConnection* nearby_connection,
                          mojo::SharedRemote<mojom::QuickStartDecoder> remote,
                          RandomSessionId session_id,
                          SharedSecret shared_secret);

  AuthenticatedConnection(AuthenticatedConnection&) = delete;
  AuthenticatedConnection& operator=(AuthenticatedConnection&) = delete;
  ~AuthenticatedConnection() override;

  void RequestAccountTransferAssertion(
      const std::string& challenge_b64url,
      RequestAccountTransferAssertionCallback callback);

  void RequestWifiCredentials(int32_t session_id,
                              RequestWifiCredentialsCallback callback);

  void NotifySourceOfUpdate();

 private:
  using ConnectionResponseCallback =
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)>;

  // Parses a raw response and converts it to a WifiCredentialsResponse
  void OnRequestWifiCredentialsResponse(
      RequestWifiCredentialsCallback callback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  // Parses a raw AssertionResponse and converts it into a FidoAssertionInfo
  void OnRequestAccountTransferAssertionResponse(
      RequestAccountTransferAssertionCallback callback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  void ParseWifiCredentialsResponse(
      RequestWifiCredentialsCallback callback,
      ::ash::quick_start::mojom::GetWifiCredentialsResponsePtr response);

  void GenerateFidoAssertionInfo(
      RequestAccountTransferAssertionCallback callback,
      ::ash::quick_start::mojom::GetAssertionResponsePtr response);

  void SendMessage(std::unique_ptr<QuickStartMessage> message,
                   ConnectionResponseCallback callback);
  mojo::SharedRemote<mojom::QuickStartDecoder> decoder_;

  base::WeakPtrFactory<AuthenticatedConnection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
