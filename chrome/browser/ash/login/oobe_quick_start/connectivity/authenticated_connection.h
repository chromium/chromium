// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "components/cbor/values.h"
#include "url/origin.h"

namespace ash::quick_start {

class AuthenticatedConnectionTest;

// Represents a connection that's been authenticated by the shapes verification
// or QR code flow.
class AuthenticatedConnection : public Connection {
 public:
  explicit AuthenticatedConnection(NearbyConnection* nearby_connection);
  AuthenticatedConnection(AuthenticatedConnection&) = delete;
  AuthenticatedConnection& operator=(AuthenticatedConnection&) = delete;
  ~AuthenticatedConnection() override;

  void RequestAccountTransferAssertion(const std::string& challenge_b64url);

 private:
  friend class AuthenticatedConnectionTest;

  // Packages a BootstrapOptions request and sends it to the Android device.
  void SendBootstrapOptions();

  // Packages a FIDO GetInfo request and sends it to the Android device.
  void GetInfo();

  // Packages a SecondDeviceAuthPayload request with FIDO GetAssertion and sends
  // it to the Android device.
  void RequestAssertion();

  // Handle response received from SendBootstrapOptions. This is passed in as a
  // callback to NearbyConnection::Read().
  void OnBootstrapOptionsResponse(absl::optional<std::vector<uint8_t>> data);

  // Handle response received from GetInfo. This is passed in as a callback to
  // NearbyConnection::Read().
  void OnFidoGetInfoResponse(absl::optional<std::vector<uint8_t>> data);

  // Handle response received from RequestAssertion. This is passed in as a
  // callback to NearbyConnection::Read().
  void OnFidoGetAssertionResponse(absl::optional<std::vector<uint8_t>> data);

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
  base::WeakPtrFactory<AuthenticatedConnection> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
