// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"

class GURL;

namespace enterprise_management {
class DeviceManagementRequest;
}

namespace enterprise_connectors {

class KeyPersistenceDelegate;
class KeyNetworkDelegate;

// Interface for the object in charge of handling key rotation logic inside the
// installer.
class KeyRotationManager {
 public:
  static constexpr int kMaxDMTokenLength = 4096;

  virtual ~KeyRotationManager() = default;

  static std::unique_ptr<KeyRotationManager> Create(
      std::unique_ptr<KeyNetworkDelegate> network_delegate);

  static std::unique_ptr<KeyRotationManager> Create();

  static std::unique_ptr<KeyRotationManager> CreateForTesting(
      std::unique_ptr<KeyNetworkDelegate> network_delegate,
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);

  static std::unique_ptr<KeyRotationManager> CreateForTesting(
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);

  static void SetForTesting(
      std::unique_ptr<KeyRotationManager> key_rotation_manager);

  // Rotates the key pair and returns the result of the key rotation to the
  // callback. If no key pair already exists, simply creates a new one.
  // `dm_token` is the DM token to use when sending the new public key to the
  // DM server at `dm_server_url`. The `nonce` is an opaque binary blob and is
  // used when building the upload request result of the rotation is
  // returned via the `result_callback`. This function will fail on linux
  // and windows if not called with admin rights.
  virtual void Rotate(
      const GURL& dm_server_url,
      const std::string& dm_token,
      const std::string& nonce,
      base::OnceCallback<void(KeyRotationResult)> result_callback) = 0;

  // Creates and returns the public key upload request. In case of error,
  // returns KeyRotationResult instead.
  // Currently, this function is only called and therefore tailored for Mac.
  virtual base::expected<const enterprise_management::DeviceManagementRequest,
                         KeyRotationResult>
  CreateUploadKeyRequest() = 0;

  // Gets the `response_code` from the upload key request and continues
  // the key rotation process. `result_callback` returns the rotation
  // result. The `old_key_pair` is only required in key rotation flows and
  // will be used to restore local storage if upload failed.
  virtual void OnDmServerResponse(
      scoped_refptr<SigningKeyPair> old_key_pair,
      base::OnceCallback<void(KeyRotationResult)> result_callback,
      KeyNetworkDelegate::HttpResponseCode response_code) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
