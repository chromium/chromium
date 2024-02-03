// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "fake_connection.h"

namespace ash::quick_start {

FakeConnection::Factory::Factory() = default;
FakeConnection::Factory::~Factory() = default;

std::unique_ptr<Connection> FakeConnection::Factory::Create(
    NearbyConnection* nearby_connection,
    SessionContext* session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated) {
  auto connection = std::make_unique<FakeConnection>(
      nearby_connection, session_context, std::move(quick_start_decoder),
      std::move(on_connection_closed), std::move(on_connection_authenticated));
  instance_ = connection->weak_ptr_factory_.GetWeakPtr();
  return std::move(connection);
}

FakeConnection::FakeConnection(
    NearbyConnection* nearby_connection,
    SessionContext* session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : Connection(nearby_connection,
                 session_context,
                 std::move(quick_start_decoder),
                 std::move(on_connection_closed),
                 std::move(on_connection_authenticated)) {}

FakeConnection::~FakeConnection() = default;

void FakeConnection::InitiateHandshake(const std::string& authentication_token,
                                       HandshakeSuccessCallback callback) {
  handshake_initiated_ = true;
  handshake_success_callback_ = std::move(callback);
}

void FakeConnection::RequestWifiCredentials(
    RequestWifiCredentialsCallback callback) {
  wifi_credentials_callback_ = std::move(callback);
}

void FakeConnection::WaitForUserVerification(
    AwaitUserVerificationCallback callback) {
  await_user_verification_callback_ = std::move(callback);
}

void FakeConnection::RequestAccountInfo(RequestAccountInfoCallback callback) {
  request_account_info_callback_ = std::move(callback);
}

void FakeConnection::RequestAccountTransferAssertion(
    const Base64UrlString& challenge,
    RequestAccountTransferAssertionCallback callback) {
  challenge_ = challenge;
  request_account_transfer_assertion_callback_ = std::move(callback);
}

void FakeConnection::SendWifiCredentials(
    std::optional<mojom::WifiCredentials> credentials) {
  CHECK(wifi_credentials_callback_);
  std::move(wifi_credentials_callback_).Run(credentials);
}

void FakeConnection::VerifyUser(
    std::optional<mojom::UserVerificationResponse> response) {
  CHECK(await_user_verification_callback_);
  std::move(await_user_verification_callback_).Run(response);
}

void FakeConnection::SendAccountInfo(std::string email) {
  std::move(request_account_info_callback_).Run(email);
}

void FakeConnection::SendAccountTransferAssertionInfo(
    std::optional<FidoAssertionInfo> assertion_info) {
  std::move(request_account_transfer_assertion_callback_).Run(assertion_info);
}

void FakeConnection::HandleHandshakeResult(bool success) {
  CHECK(handshake_success_callback_);
  std::move(handshake_success_callback_).Run(success);
}

bool FakeConnection::WasHandshakeInitiated() {
  return handshake_initiated_;
}

}  // namespace ash::quick_start
