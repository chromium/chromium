// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

namespace ash::platform_keys {

enum class KeyUsage { kArc, kCorporate };

// If the process of updating the key permissions has been done successfully, a
// success |status| will be returned. If an error has occurred, an error
// |status| will be returned.
using AllowKeyForUsageCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

// If the key usage allowance has been retrieved successfully, |allowed| will be
// set to the result and a success |status| will be returned. If an error
// has occurred, an error |status| will be returned and |allowed| will be
// nullopt.
using IsKeyAllowedForUsageCallback =
    base::OnceCallback<void(std::optional<bool> allowed,
                            chromeos::platform_keys::Status status)>;

// ** KeyPermissionsManager (KPM) instances **
// Every KPM instance is responsible for managing key permissions of keys
// residing on a specific token.
// KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager() returns
// system token KPM instance.
// KeyPermissionsManagerImpl::GetUserPrivateTokenKeyPermissionsManager(profile)
// returns user token KPM instance.
//
// ** Key Permissions Management **
// KPM instances keep chaps updated about key permissions. Permissions of a
// certain key will be saved as a key attribute value for the key attribute type
// pkcs11_custom_attributes::kCkaChromeOsKeyPermissions. For information about
// the format the attribute value, please refer to:
// third_party/cros_system_api/constants/pkcs11_custom_attributes.h
//
// ** Key Permissions **
// KPM instances keep track of two flags for a key: "corporate" and "arc".
// "corporate": This flag can be set using AllowKeyForUsage function.
// "arc" flag is managed solely by KPM instances and can't be set by chrome
// components.
// A key is allowed for ARC usage if:
// 1- The key is corporate and
// 2- ARC is enabled and
// 3- there exists an ARC app A that is installed and
// 4- app A is mentioned in KeyPermissions policy.
//
// ** One-time key permissions migration **
// This is the process of migrating key permissions saved in chrome prefs to
// chaps. This process is done once for the system token and once per user
// token.
class KeyPermissionsManager {
 public:
  // Don't use this constructor directly. Use
  // KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager or
  // KeyPermissionsManagerImpl::GetUserPrivateTokenKeyPermissionsManager
  // instead.
  KeyPermissionsManager() = default;
  KeyPermissionsManager(const KeyPermissionsManager&) = delete;
  KeyPermissionsManager& operator=(const KeyPermissionsManager&) = delete;
  virtual ~KeyPermissionsManager() = default;

  // Allows |public_key_spki_der| to be used for |usage|.
  // Note: currently, kArc usage allowance can't be granted using this function.
  virtual void AllowKeyForUsage(AllowKeyForUsageCallback callback,
                                KeyUsage usage,
                                std::vector<uint8_t> public_key_spki_der) = 0;

  // Returns true if |public_key_spki_der| is allowed for |key_usage|.
  virtual void IsKeyAllowedForUsage(
      IsKeyAllowedForUsageCallback callback,
      KeyUsage key_usage,
      std::vector<uint8_t> public_key_spki_der) = 0;

  // Returns true if corporate keys are allowed for ARC usages. Currently,
  // either all corporate keys on a token are allowed for ARC usage or none of
  // them.
  virtual bool AreCorporateKeysAllowedForArcUsage() const = 0;

  // Key permissions managers allow two-phase shutdown. This function can be
  // called as the first shutdown phase before the destructor. This is useful
  // for key permissions managers wrapped by browser context keyed services, so
  // that the keyed services can ask the underlying key permissions managers to
  // drop references of services that depend on the context that will be
  // destroyed.
  virtual void Shutdown() = 0;
};

}  // namespace ash::platform_keys

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_H_
