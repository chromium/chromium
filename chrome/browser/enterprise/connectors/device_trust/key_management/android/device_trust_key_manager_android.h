// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/enterprise/device_trust/core/device_trust_key_manager.h"

namespace enterprise_connectors {

class AndroidAttestationTokenClient;

// Android `DeviceTrustKeyManager` implementation. Android does not provision a
// browser signing key. Instead, when `kDeviceTrustAndroidAttestationTokens` is
// enabled, "signing" a payload produces an attestation token bound to the
// SHA-256 hash of that payload. When the feature is disabled, the signing
// methods return `std::nullopt`, which makes the shared attestation flow fall
// back to generating an unsigned challenge response.
class DeviceTrustKeyManagerAndroid : public DeviceTrustKeyManager {
 public:
  explicit DeviceTrustKeyManagerAndroid(
      std::unique_ptr<AndroidAttestationTokenClient> client);
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

 private:
  std::unique_ptr<AndroidAttestationTokenClient> client_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_ANDROID_DEVICE_TRUST_KEY_MANAGER_ANDROID_H_
