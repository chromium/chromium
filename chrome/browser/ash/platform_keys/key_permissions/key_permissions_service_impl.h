// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

namespace ash::platform_keys {

class KeyPermissionsManager;
class PlatformKeysService;

// TODO(crbug.com/1130949): Convert KeyPermissionsServiceImpl operations into
// classes.
class KeyPermissionsServiceImpl : public KeyPermissionsService {
 public:
  // |profile_is_managed| determines the default usage and permissions for
  // keys without explicitly assigned usage.
  KeyPermissionsServiceImpl(
      bool is_regular_user_profile,
      bool profile_is_managed,
      PlatformKeysService* platform_keys_service,
      KeyPermissionsManager* profile_key_permissions_manager);

  ~KeyPermissionsServiceImpl() override;

  KeyPermissionsServiceImpl(const KeyPermissionsServiceImpl& other) = delete;
  KeyPermissionsServiceImpl& operator=(const KeyPermissionsServiceImpl& other) =
      delete;

  void CanUserGrantPermissionForKey(
      std::vector<uint8_t> public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback) override;

  void IsCorporateKey(std::vector<uint8_t> public_key_spki_der,
                      IsCorporateKeyCallback callback) override;

  void SetCorporateKey(std::vector<uint8_t> public_key_spki_der,
                       SetCorporateKeyCallback callback) override;

  PlatformKeysService* platform_keys_service() {
    return platform_keys_service_;
  }

 private:
  // Returns true if |public_key_spki_der_b64| (which is located only on a user
  // token) is marked for corporate usage.
  bool IsUserKeyCorporate(const std::string& public_key_spki_der_b64) const;

  void CanUserGrantPermissionForKeyWithLocations(
      std::vector<uint8_t> public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback,
      const std::vector<chromeos::platform_keys::TokenId>& key_locations,
      chromeos::platform_keys::Status key_locations_retrieval_status);
  void CanUserGrantPermissionForKeyWithLocationsAndFlag(
      std::vector<uint8_t> public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback,
      const std::vector<chromeos::platform_keys::TokenId>& key_locations,
      std::optional<bool> corporate_key,
      chromeos::platform_keys::Status status);

  void IsCorporateKeyWithLocations(
      std::vector<uint8_t> public_key_spki_der,
      IsCorporateKeyCallback callback,
      const std::vector<chromeos::platform_keys::TokenId>& key_locations,
      chromeos::platform_keys::Status key_locations_retrieval_status);
  void IsCorporateKeyWithKpmResponse(IsCorporateKeyCallback callback,
                                     std::optional<bool> allowed,
                                     chromeos::platform_keys::Status status);

  void SetCorporateKeyWithLocations(
      std::vector<uint8_t> public_key_spki_der,
      SetCorporateKeyCallback callback,
      const std::vector<chromeos::platform_keys::TokenId>& key_locations,
      chromeos::platform_keys::Status key_locations_retrieval_status);

  const bool is_regular_user_profile_;
  const bool profile_is_managed_;
  const raw_ptr<PlatformKeysService, DanglingUntriaged> platform_keys_service_;
  const raw_ptr<KeyPermissionsManager, DanglingUntriaged>
      profile_key_permissions_manager_;
  base::WeakPtrFactory<KeyPermissionsServiceImpl> weak_factory_{this};
};

}  // namespace ash::platform_keys

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_
