// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace platform_keys {

class MockKeyPermissionsManager : public KeyPermissionsManager {
 public:
  MockKeyPermissionsManager();
  MockKeyPermissionsManager(const MockKeyPermissionsManager&) = delete;
  MockKeyPermissionsManager& operator=(const MockKeyPermissionsManager&) =
      delete;
  ~MockKeyPermissionsManager() override;

  MOCK_METHOD(void,
              GetPermissionsForExtension,
              (const std::string& extension_id,
               const PermissionsCallback& callback),
              (override));

  MOCK_METHOD(bool,
              CanUserGrantPermissionFor,
              (const std::string& public_key_spki_der,
               const std::vector<platform_keys::TokenId>& key_locations),
              (const override));

  MOCK_METHOD(bool,
              IsCorporateKey,
              (const std::string& public_key_spki_der_b64,
               const std::vector<platform_keys::TokenId>& key_locations),
              (const override));

  MOCK_METHOD(void,
              SetCorporateKey,
              (const std::string& public_key_spki_der_b64,
               platform_keys::TokenId key_location),
              (const override));
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
