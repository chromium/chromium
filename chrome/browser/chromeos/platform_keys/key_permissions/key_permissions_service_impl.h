// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

namespace chromeos {
namespace platform_keys {

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
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback) override;

  void IsCorporateKey(const std::string& public_key_spki_der,
                      IsCorporateKeyCallback callback) override;

  void SetCorporateKey(const std::string& public_key_spki_der,
                       SetCorporateKeyCallback callback) override;

  PlatformKeysService* platform_keys_service() {
    return platform_keys_service_;
  }

 private:
  // Returns true if |public_key_spki_der_b64| (which is located only on a user
  // token) is marked for corporate usage.
  bool IsUserKeyCorporate(const std::string& public_key_spki_der_b64) const;

  void CanUserGrantPermissionForKeyWithLocations(
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback,
      const std::vector<TokenId>& key_locations,
      Status key_locations_retrieval_status);
  void CanUserGrantPermissionForKeyWithLocationsAndFlag(
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback,
      const std::vector<TokenId>& key_locations,
      Status key_locations_retrieval_status,
      bool is_corporate_key);

  void IsCorporateKeyWithLocations(const std::string& public_key_spki_der,
                                   IsCorporateKeyCallback callback,
                                   const std::vector<TokenId>& key_locations,
                                   Status key_locations_retrieval_status);
  void IsCorporateKeyWithKpmResponse(IsCorporateKeyCallback callback,
                                     base::Optional<bool> allowed,
                                     Status status);

  void SetCorporateKeyWithLocations(const std::string& public_key_spki_der,
                                    SetCorporateKeyCallback callback,
                                    const std::vector<TokenId>& key_locations,
                                    Status key_locations_retrieval_status);

  const bool is_regular_user_profile_;
  const bool profile_is_managed_;
  PlatformKeysService* const platform_keys_service_;
  KeyPermissionsManager* const profile_key_permissions_manager_;
  base::WeakPtrFactory<KeyPermissionsServiceImpl> weak_factory_{this};
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_
