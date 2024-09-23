// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"

#include "base/containers/contains.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/quick_start/logging.h"

namespace ash::quick_start {

TargetDeviceConnectionBroker::TargetDeviceConnectionBroker() = default;
TargetDeviceConnectionBroker::~TargetDeviceConnectionBroker() = default;

void TargetDeviceConnectionBroker::GetFeatureSupportStatusAsync(
    FeatureSupportStatusCallback callback) {
  feature_status_callbacks_.push_back(std::move(callback));
  MaybeNotifyFeatureStatus();
}

void TargetDeviceConnectionBroker::MaybeNotifyFeatureStatus() {
  constexpr FeatureSupportStatus kShouldNotNotifyStatus[] = {
      FeatureSupportStatus::kUndetermined,
      FeatureSupportStatus::kWaitingForAdapterToBecomePresent,
      FeatureSupportStatus::kWaitingForAdapterToBecomePowered};
  FeatureSupportStatus status = GetFeatureSupportStatus();

  if (base::Contains(kShouldNotNotifyStatus, status)) {
    return;
  }

  for (auto& callback : feature_status_callbacks_) {
    callback.Run(status);
  }
}

void TargetDeviceConnectionBroker::OnConnectionAuthenticated(
    base::WeakPtr<AuthenticatedConnection> authenticated_connection) {
  CHECK(connection_lifecycle_listener_)
      << "Missing connection_lifecycle_listener_";
  connection_lifecycle_listener_->OnConnectionAuthenticated(
      authenticated_connection);
}

void TargetDeviceConnectionBroker::OnConnectionClosed(
    ConnectionClosedReason reason) {
  CHECK(connection_lifecycle_listener_)
      << "Missing connection_lifecycle_listener_";
  QS_LOG(INFO) << "Connection closed: " << reason;
  connection_lifecycle_listener_->OnConnectionClosed(reason);
}

std::string TargetDeviceConnectionBroker::DerivePin(
    const std::string& authentication_token) const {
  std::string hash_str = base::SHA1HashString(authentication_token);
  std::vector<int8_t> hash_ints =
      std::vector<int8_t>(hash_str.begin(), hash_str.end());
  return base::NumberToString(
             std::abs((hash_ints[0] << 8 | hash_ints[1]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[2] << 8 | hash_ints[3]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[4] << 8 | hash_ints[5]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[6] << 8 | hash_ints[7]) % 10));
}

std::ostream& operator<<(
    std::ostream& stream,
    const TargetDeviceConnectionBroker::ConnectionClosedReason&
        connection_closed_reason) {
  switch (connection_closed_reason) {
    case TargetDeviceConnectionBroker::ConnectionClosedReason::kUserAborted:
      stream << "[user aborted]";
      break;
    case TargetDeviceConnectionBroker::ConnectionClosedReason::
        kAuthenticationFailed:
      stream << "[authentication failed]";
      break;
    case TargetDeviceConnectionBroker::ConnectionClosedReason::
        kTargetDeviceUpdate:
      stream << "[target device update]";
      break;
    case TargetDeviceConnectionBroker::ConnectionClosedReason::kResponseTimeout:
      stream << "[response timeout]";
      break;
    case TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError:
      stream << "[unknown error]";
      break;
    case TargetDeviceConnectionBroker::ConnectionClosedReason::
        kConnectionLifecycleListenerDestroyed:
      stream << "[ConnectionLifecycleListener destroyed]";
      break;
  }

  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const TargetDeviceConnectionBroker::FeatureSupportStatus&
        feature_support_status) {
  switch (feature_support_status) {
    case TargetDeviceConnectionBroker::FeatureSupportStatus::kUndetermined:
      stream << "[undetermined]";
      break;
    case TargetDeviceConnectionBroker::FeatureSupportStatus::kNotSupported:
      stream << "[not supported]";
      break;
    case TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported:
      stream << "[supported]";
      break;
    case TargetDeviceConnectionBroker::FeatureSupportStatus::
        kWaitingForAdapterToBecomePresent:
      stream << "[waiting for adapter to become present]";
      break;
    case TargetDeviceConnectionBroker::FeatureSupportStatus::
        kWaitingForAdapterToBecomePowered:
      stream << "[waiting for adapter to become powered]";
      break;
  }
  return stream;
}

}  // namespace ash::quick_start
