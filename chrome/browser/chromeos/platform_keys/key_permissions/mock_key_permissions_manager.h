// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager.h"
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
              AllowKeyForUsage,
              (AllowKeyForUsageCallback callback,
               KeyUsage usage,
               const std::string& public_key_spki_der),
              (override));

  MOCK_METHOD(void,
              IsKeyAllowedForUsage,
              (IsKeyAllowedForUsageCallback callback,
               KeyUsage key_usage,
               const std::string& public_key_spki_der),
              (override));

  MOCK_METHOD(bool, AreCorporateKeysAllowedForArcUsage, (), (const, override));

  MOCK_METHOD(void, Shutdown, (), (override));
};

}  // namespace platform_keys
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when
// //chromeos/browser/chromeos/platform_keys moved to ash
namespace ash {
namespace platform_keys {
using ::chromeos::platform_keys::MockKeyPermissionsManager;
}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_MANAGER_H_
