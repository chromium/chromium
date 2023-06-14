// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "url/url_util.h"

namespace ash::quick_start {

TargetDeviceConnectionBroker::TargetDeviceConnectionBroker() = default;
TargetDeviceConnectionBroker::~TargetDeviceConnectionBroker() = default;

void TargetDeviceConnectionBroker::GetFeatureSupportStatusAsync(
    FeatureSupportStatusCallback callback) {
  feature_status_callbacks_.push_back(std::move(callback));
  MaybeNotifyFeatureStatus();
}

void TargetDeviceConnectionBroker::MaybeNotifyFeatureStatus() {
  FeatureSupportStatus status = GetFeatureSupportStatus();
  if (status == FeatureSupportStatus::kUndetermined) {
    return;
  }

  auto callbacks = std::exchange(feature_status_callbacks_, {});

  for (auto& callback : callbacks) {
    std::move(callback).Run(status);
  }
}

void TargetDeviceConnectionBroker::OnConnectionAuthenticated(
    base::WeakPtr<AuthenticatedConnection> authenticated_connection) {
  CHECK(connection_lifecycle_listener_);
  connection_lifecycle_listener_->OnConnectionAuthenticated(
      authenticated_connection);
}

void TargetDeviceConnectionBroker::OnConnectionClosed(
    ConnectionClosedReason reason) {
  CHECK(connection_lifecycle_listener_);
  connection_lifecycle_listener_->OnConnectionClosed(reason);
}

std::vector<uint8_t> TargetDeviceConnectionBroker::GetQrCodeData(
    const RandomSessionId& random_session_id,
    const SharedSecret shared_secret) const {
  std::string shared_secret_str(shared_secret.begin(), shared_secret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  url::RawCanonOutputT<char> shared_secret_base64_uriencoded;
  url::EncodeURIComponent(shared_secret_base64.data(),
                          shared_secret_base64.size(),
                          &shared_secret_base64_uriencoded);

  std::string url = "https://signin.google/qs/" + random_session_id.ToString() +
                    "?key=" +
                    std::string(shared_secret_base64_uriencoded.data(),
                                shared_secret_base64_uriencoded.length());

  return std::vector<uint8_t>(url.begin(), url.end());
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

}  // namespace ash::quick_start
