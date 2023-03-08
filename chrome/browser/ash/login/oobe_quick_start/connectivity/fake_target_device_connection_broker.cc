// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_quick_start_decoder.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

namespace ash::quick_start {

namespace {

// Arbitrary string to use as the connection's authentication token.
constexpr char kAuthenticationToken[] = "auth_token";

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

}  // namespace

FakeTargetDeviceConnectionBroker::Factory::Factory() = default;

FakeTargetDeviceConnectionBroker::Factory::~Factory() = default;

std::unique_ptr<TargetDeviceConnectionBroker>
FakeTargetDeviceConnectionBroker::Factory::CreateInstance(
    RandomSessionId session_id) {
  auto connection_broker = std::make_unique<FakeTargetDeviceConnectionBroker>();
  connection_broker->set_feature_support_status(
      initial_feature_support_status_);
  instances_.push_back(connection_broker.get());
  return std::move(connection_broker);
}

FakeTargetDeviceConnectionBroker::FakeTargetDeviceConnectionBroker() = default;

FakeTargetDeviceConnectionBroker::~FakeTargetDeviceConnectionBroker() = default;

TargetDeviceConnectionBroker::FeatureSupportStatus
FakeTargetDeviceConnectionBroker::GetFeatureSupportStatus() const {
  return feature_support_status_;
}

void FakeTargetDeviceConnectionBroker::StartAdvertising(
    ConnectionLifecycleListener* listener,
    ResultCallback on_start_advertising_callback) {
  ++num_start_advertising_calls_;
  connection_lifecycle_listener_ = listener;
  on_start_advertising_callback_ = std::move(on_start_advertising_callback);
}

void FakeTargetDeviceConnectionBroker::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  ++num_stop_advertising_calls_;
  on_stop_advertising_callback_ = std::move(on_stop_advertising_callback);
}

void FakeTargetDeviceConnectionBroker::InitiateConnection(
    const std::string& source_device_id) {
  auto random_session_id = RandomSessionId();
  fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
  NearbyConnection* nearby_connection = fake_nearby_connection_.get();
  fake_quick_start_decoder_ = std::make_unique<FakeQuickStartDecoder>();
  auto fake_incomming_connection = std::make_unique<FakeIncommingConnection>(
      nearby_connection, random_session_id, kAuthenticationToken);
  connection_lifecycle_listener_->OnIncomingConnectionInitiated(
      source_device_id, fake_incomming_connection->AsWeakPtr());
  fake_connection_ = std::move(fake_incomming_connection);
}

void FakeTargetDeviceConnectionBroker::AuthenticateConnection(
    const std::string& source_device_id) {
  fake_connection_.reset();
  fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
  NearbyConnection* nearby_connection = fake_nearby_connection_.get();
  mojo::PendingRemote<mojom::QuickStartDecoder> remote;
  fake_quick_start_decoder_ = std::make_unique<FakeQuickStartDecoder>();
  auto random_session_id = RandomSessionId();
  auto fake_authenticated_connection =
      std::make_unique<FakeAuthenticatedConnection>(
          nearby_connection,
          mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
              fake_quick_start_decoder_->GetRemote()),
          random_session_id, kSharedSecret);
  connection_lifecycle_listener_->OnConnectionAuthenticated(
      source_device_id, fake_authenticated_connection->AsWeakPtr());

  fake_connection_ = std::move(fake_authenticated_connection);
}

void FakeTargetDeviceConnectionBroker::RejectConnection(
    const std::string& source_device_id) {
  connection_lifecycle_listener_->OnConnectionRejected(source_device_id);
}

void FakeTargetDeviceConnectionBroker::CloseConnection(
    const std::string& source_device_id) {
  connection_lifecycle_listener_->OnConnectionClosed(source_device_id);
}

}  // namespace ash::quick_start
