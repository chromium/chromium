// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_

#include <memory>

#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"

namespace enterprise_connectors {

class KeyRotationLauncher;

class DeviceTrustKeyManagerImpl : public DeviceTrustKeyManager {
 public:
  explicit DeviceTrustKeyManagerImpl(
      std::unique_ptr<KeyRotationLauncher> key_rotation_launcher);
  ~DeviceTrustKeyManagerImpl() override;

  // DeviceTrustKeyManager:
  void StartInitialization() override;
  void StartKeyRotation(const std::string& nonce) override;
  void ExportPublicKeyAsync(
      base::OnceCallback<void(absl::optional<std::string>)> callback) override;
  void SignStringAsync(
      const std::string& str,
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback)
      override;

 private:
  std::unique_ptr<KeyRotationLauncher> key_rotation_launcher_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_DEVICE_TRUST_KEY_MANAGER_IMPL_H_
