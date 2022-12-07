// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

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
  auto fake_incomming_connection = std::make_unique<FakeIncommingConnection>(
      nearby_connection, random_session_id);
  connection_lifecycle_listener_->OnIncomingConnectionInitiated(
      source_device_id, fake_incomming_connection->AsWeakPtr());
  fake_connection_ = std::move(fake_incomming_connection);
}

void FakeTargetDeviceConnectionBroker::AuthenticateConnection(
    const std::string& source_device_id) {
  fake_connection_.reset();
  fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
  NearbyConnection* nearby_connection = fake_nearby_connection_.get();
  auto fake_authenticated_connection =
      std::make_unique<FakeAuthenticatedConnection>(nearby_connection);
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
