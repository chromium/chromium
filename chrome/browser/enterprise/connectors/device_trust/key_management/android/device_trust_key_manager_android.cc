// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/android/device_trust_key_manager_android.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "crypto/hash.h"

namespace enterprise_connectors {

DeviceTrustKeyManagerAndroid::DeviceTrustKeyManagerAndroid(
    std::unique_ptr<AndroidAttestationTokenClient> client)
    : client_(std::move(client)) {
  CHECK(client_);
}

DeviceTrustKeyManagerAndroid::~DeviceTrustKeyManagerAndroid() = default;

void DeviceTrustKeyManagerAndroid::StartInitialization() {
  // No-op on Android as there is no signing key to load nor create.
}

// Key rotation is not supported on Android. Post a task to fulfill the
// interface contract and ensure the callback is never invoked synchronously.
void DeviceTrustKeyManagerAndroid::RotateKey(
    const std::string& /*nonce*/,
    base::OnceCallback<void(KeyRotationResult)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), KeyRotationResult::FAILURE));
}

// Post a task to fulfill the interface contract and guarantee the callback is
// never called synchronously, preventing re-entrancy.
void DeviceTrustKeyManagerAndroid::ExportPublicKeyAsync(
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void DeviceTrustKeyManagerAndroid::SignStringAsync(
    const std::string& str,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback) {
  if (!IsDeviceTrustAndroidAttestationTokensEnabled()) {
    // Post a task with `nullopt` to produce an unsigned response
    // (`kSuccessNoSignature`) while fulfilling the asynchronous interface
    // contract and ensuring the callback is never invoked synchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Bind the attestation token to the content of the serialized challenge
  // response.
  // TODO(crbug.com/463390232): Add latency logging for this operation.
  client_->GenerateToken(crypto::hash::Sha256(str), std::move(callback));
}

std::optional<DeviceTrustKeyManager::KeyMetadata>
DeviceTrustKeyManagerAndroid::GetLoadedKeyMetadata() const {
  return std::nullopt;
}

bool DeviceTrustKeyManagerAndroid::HasPermanentFailure() const {
  return false;
}

}  // namespace enterprise_connectors
