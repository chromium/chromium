// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/enterprise/device_trust/core/device_trust_key_manager.h"

namespace enterprise_connectors {

// Android `DeviceTrustKeyManager` implementation for unsigned attestation.
// Android does not provision a browser signing key, so every operation is a
// no-op. Returning `std::nullopt` from the signing methods makes the shared
// attestation flow fall back to generating an unsigned challenge response.
class DeviceTrustKeyManagerAndroid : public DeviceTrustKeyManager {
 public:
  DeviceTrustKeyManagerAndroid();
  ~DeviceTrustKeyManagerAndroid() override;

  DeviceTrustKeyManagerAndroid(const DeviceTrustKeyManagerAndroid&) = delete;
  DeviceTrustKeyManagerAndroid& operator=(const DeviceTrustKeyManagerAndroid&) =
      delete;

  // DeviceTrustKeyManager:
  void StartInitialization() override;
  void RotateKey(const std::string& nonce,
                 base::OnceCallback<void(KeyRotationResult)> callback) override;
  void ExportPublicKeyAsync(
      base::OnceCallback<void(std::optional<std::string>)> callback) override;
  void SignStringAsync(
      const std::string& str,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
      override;
  std::optional<KeyMetadata> GetLoadedKeyMetadata() const override;
  bool HasPermanentFailure() const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_
