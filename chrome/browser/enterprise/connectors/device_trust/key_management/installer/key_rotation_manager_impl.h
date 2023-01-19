// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

namespace enterprise_connectors {

class KeyPersistenceDelegate;

class KeyRotationManagerImpl : public KeyRotationManager {
 public:
  KeyRotationManagerImpl(
      std::unique_ptr<KeyNetworkDelegate> network_delegate,
      std::unique_ptr<KeyPersistenceDelegate> persistence_delegate);
  ~KeyRotationManagerImpl() override;

  // KeyRotationManager:
  void Rotate(
      const GURL& dm_server_url,
      const std::string& dm_token,
      const std::string& nonce,
      base::OnceCallback<void(KeyRotationResult)> result_callback) override;

 private:
  // Gets the `response_code` from the upload key request and continues
  // the key rotation process. `result_callback` returns the rotation result.
  // The `old_key_pair` is only required in key rotation flows and will be used
  // to restore local storage if upload failed.
  void OnDmServerResponse(
      std::unique_ptr<SigningKeyPair> old_key_pair,
      base::OnceCallback<void(KeyRotationResult)> result_callback,
      KeyNetworkDelegate::HttpResponseCode response_code);

  std::unique_ptr<KeyNetworkDelegate> network_delegate_;
  std::unique_ptr<KeyPersistenceDelegate> persistence_delegate_;
  base::WeakPtrFactory<KeyRotationManagerImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_MANAGER_IMPL_H_
