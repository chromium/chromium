// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"

namespace ash::quick_start {

FakeConnection::Factory::Factory() = default;
FakeConnection::Factory::~Factory() = default;

std::unique_ptr<Connection> FakeConnection::Factory::Create(
    NearbyConnection* nearby_connection,
    Connection::SessionContext session_context,
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
    Connection::SessionContext session_context,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder,
    ConnectionClosedCallback on_connection_closed,
    ConnectionAuthenticatedCallback on_connection_authenticated)
    : Connection(nearby_connection,
                 session_context,
                 std::move(quick_start_decoder),
                 std::make_unique<Connection::NonceGenerator>(),
                 std::move(on_connection_closed),
                 std::move(on_connection_authenticated)) {}

FakeConnection::~FakeConnection() = default;

void FakeConnection::InitiateHandshake(const std::string& authentication_token,
                                       HandshakeSuccessCallback callback) {
  handshake_initiated_ = true;
  handshake_success_callback_ = std::move(callback);
}

bool FakeConnection::WasHandshakeInitiated() {
  return handshake_initiated_;
}

}  // namespace ash::quick_start
