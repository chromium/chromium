// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_

#include <memory>
#include <string>

class GURL;

namespace enterprise_connectors {

class KeyPersistenceDelegate;
class KeyNetworkDelegate;

// Interface for the object in charge of handling key rotation logic inside the
// installer.
class KeyRotationManager {
 public:
  virtual ~KeyRotationManager() = default;

  static std::unique_ptr<KeyRotationManager> Create(
      std::unique_ptr<KeyNetworkDelegate> network_delegate);

  static std::unique_ptr<KeyRotationManager> CreateForTesting(
      std::unique_ptr<KeyNetworkDelegate> network_delegate,
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);

  // Rotates the key pair.  If no key pair already exists, simply creates a
  // new one.  `dm_token` the DM token to use when sending the new public key to
  // the DM server.  This function will fail if not called with admin rights.
  //
  // This function makes network requests and will block until those requests
  // complete successfully or fail (after some retrying). This function is
  // not meant to be called from the chrome browser but from a background
  // utility process that does not block the user in the browser.
  [[nodiscard]] virtual bool RotateWithAdminRights(
      const GURL& dm_server_url,
      const std::string& dm_token,
      const std::string& nonce) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_H_
