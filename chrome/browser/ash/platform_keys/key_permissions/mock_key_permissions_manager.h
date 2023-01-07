// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::platform_keys {

class MockKeyPermissionsManager : public KeyPermissionsManager {
 public:
  MockKeyPermissionsManager();
  MockKeyPermissionsManager(const MockKeyPermissionsManager&) = delete;
  MockKeyPermissionsManager& operator=(const MockKeyPermissionsManager&) =
      delete;
  ~MockKeyPermissionsManager() override;

  MOCK_METHOD(void,
              AllowKeyForUsage,
              (AllowKeyForUsageCallback callback,
               KeyUsage usage,
               std::vector<uint8_t> public_key_spki_der),
              (override));

  MOCK_METHOD(void,
              IsKeyAllowedForUsage,
              (IsKeyAllowedForUsageCallback callback,
               KeyUsage key_usage,
               std::vector<uint8_t> public_key_spki_der),
              (override));

  MOCK_METHOD(bool, AreCorporateKeysAllowedForArcUsage, (), (const, override));

  MOCK_METHOD(void, Shutdown, (), (override));
};

}  // namespace ash::platform_keys

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
