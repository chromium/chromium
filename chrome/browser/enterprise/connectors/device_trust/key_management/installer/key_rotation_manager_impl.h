// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace crypto {
class UnexportableSigningKey;
}  // namespace crypto

namespace enterprise_connectors {

class KeyNetworkDelegate;
class KeyPersistenceDelegate;

class KeyRotationManagerImpl : public KeyRotationManager {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;

  KeyRotationManagerImpl(
      std::unique_ptr<KeyNetworkDelegate> network_delegate,
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);
  ~KeyRotationManagerImpl() override;

  // KeyRotationManager:
  [[nodiscard]] bool RotateWithAdminRights(const GURL& dm_server_url,
                                           const std::string& dm_token,
                                           const std::string& nonce) override;

 private:
  // Builds the protobuf message needed to tell DM server about the new public
  // for this device.  `nonce` is an opaque binary blob and should not be
  // treated as an ASCII or UTF-8 string.
  bool BuildUploadPublicKeyRequest(
      KeyTrustLevel new_trust_level,
      const std::unique_ptr<crypto::UnexportableSigningKey>& new_key_pair,
      const std::string& nonce,
      enterprise_management::BrowserPublicKeyUploadRequest* request);

  std::unique_ptr<KeyNetworkDelegate> network_delegate_;
  std::unique_ptr<KeyPersistenceDelegate> persistence_delegate_;
  std::unique_ptr<SigningKeyPair> key_pair_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_
