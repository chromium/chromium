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
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "components/cbor/values.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "url/origin.h"

namespace ash::quick_start {

class AuthenticatedConnectionTest;

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

  friend class AuthenticatedConnectionTest;

  // Packages a BootstrapOptions request and sends it to the Android device.
  void SendBootstrapOptions(ConnectionResponseCallback callback);

  // Packages a FIDO GetInfo request and sends it to the Android device.
  void GetInfo(ConnectionResponseCallback callback);

  // Packages a SecondDeviceAuthPayload request with FIDO GetAssertion and sends
  // it to the Android device.
  void RequestAssertion(ConnectionResponseCallback callback);

  // Parses a raw response and converts it to a WifiCredentialsResponse
  void ParseWifiCredentialsResponse(
      RequestWifiCredentialsCallback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  // Parses a raw AssertionResponse and converts it into a FidoAssertionInfo
  void ParseAssertionResponse(
      RequestAccountTransferAssertionCallback callback,
      absl::optional<std::vector<uint8_t>> response_bytes);

  void GenerateFidoAssertionInfo(
      RequestAccountTransferAssertionCallback callback,
      ::ash::quick_start::mojom::GetAssertionResponsePtr response);

  // GenerateGetAssertionRequest will take challenge bytes and create an
  // instance of cbor::Value of the GetAssertionRequest which can then be CBOR
  // encoded.
  cbor::Value GenerateGetAssertionRequest();

  // CBOREncodeGetAssertionRequest will take a CtapGetAssertionRequest struct
  // and encode it into CBOR encoded bytes that can be understood by a FIDO
  // authenticator.
  std::vector<uint8_t> CBOREncodeGetAssertionRequest(
      const cbor::Value& request);

  // This JSON encoding does not follow the strict requirements of the spec[1],
  // but that's ok because the validator doesn't demand that.
  // [1] https://www.w3.org/TR/webauthn-2/#clientdatajson-serialization
  std::string CreateFidoClientDataJson(const url::Origin& orgin);

  std::string challenge_b64url_;
  mojo::SharedRemote<mojom::QuickStartDecoder> decoder_;

  base::WeakPtrFactory<AuthenticatedConnection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
