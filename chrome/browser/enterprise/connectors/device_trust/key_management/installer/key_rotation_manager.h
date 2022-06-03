// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"

class GURL;

namespace enterprise_connectors {

class KeyPersistenceDelegate;
class KeyNetworkDelegate;

// Interface for the object in charge of handling key rotation logic inside the
// installer.
class KeyRotationManager {
 public:
  // Status of rotation attempts made with RotateWithAdminRights().
  // Must be kept in sync with the DeviceTrustKeyRotationStatus UMA enum.
  // Making this public here to access from tests.
  enum class RotationStatus {
    SUCCESS,
    FAILURE_CANNOT_GENERATE_NEW_KEY,
    FAILURE_CANNOT_STORE_KEY,
    FAILURE_CANNOT_BUILD_REQUEST,
    FAILURE_CANNOT_UPLOAD_KEY,
    FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED,
    FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED,
    FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED,
    kMaxValue = FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED,
  };

  virtual ~KeyRotationManager() = default;

  static std::unique_ptr<KeyRotationManager> Create();

  static std::unique_ptr<KeyRotationManager> CreateForTesting(
      std::unique_ptr<KeyNetworkDelegate> network_delegate,
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);

  // Rotates the key pair.  If no key pair already exists, simply creates a
  // new one.  `dm_token` the DM token to use when sending the new public key to
  // the DM server.  This function will fail if not called with admin rights.
  //
  // This function makes network requests and will block until those requests
  // complete successfully or fail (after some retrying).  This function is
  // not meant to be called from the chrome browser but from a background
  // utility process that does not block the user in the browser.
  virtual bool RotateWithAdminRights(const GURL& dm_server_url,
                                     const std::string& dm_token,
                                     const std::string& nonce)
      WARN_UNUSED_RESULT = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
