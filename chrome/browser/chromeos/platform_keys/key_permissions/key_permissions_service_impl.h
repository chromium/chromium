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
#include "extensions/browser/state_store.h"

class PrefService;

namespace extensions {
class StateStore;
}

namespace policy {
class PolicyService;
}

namespace chromeos {
namespace platform_keys {

class PlatformKeysService;

// TODO(crbug.com/1130949): Convert KeyPermissionsServiceImpl operations into
// classes.
class KeyPermissionsServiceImpl : public KeyPermissionsService {
 public:
  // |profile_prefs| and |extensions_state_store| must not be null and must
  // outlive this object.
  // If |profile_is_managed| is false, |profile_policies| is ignored. Otherwise,
  // |profile_policies| must not be null and must outlive this object.
  // |profile_is_managed| determines the default usage and permissions for
  // keys without explicitly assigned usage.
  KeyPermissionsServiceImpl(bool profile_is_managed,
                            PrefService* profile_prefs,
                            policy::PolicyService* profile_policies,
                            extensions::StateStore* extensions_state_store,
                            PlatformKeysService* platform_keys_service);

  ~KeyPermissionsServiceImpl() override;

  KeyPermissionsServiceImpl(const KeyPermissionsServiceImpl& other) = delete;
  KeyPermissionsServiceImpl& operator=(const KeyPermissionsServiceImpl& other) =
      delete;

  void CanUserGrantPermissionForKey(
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback) const override;

  void IsCorporateKey(const std::string& public_key_spki_der,
                      IsCorporateKeyCallback callback) const override;

  void SetCorporateKey(const std::string& public_key_spki_der,
                       SetCorporateKeyCallback callback) const override;

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
      Status key_locations_retrieval_status) const;
  void CanUserGrantPermissionForKeyWithLocationsAndFlag(
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback,
      const std::vector<TokenId>& key_locations,
      Status key_locations_retrieval_status,
      bool is_corporate_key);

  void IsCorporateKeyWithLocations(const std::string& public_key_spki_der,
                                   IsCorporateKeyCallback callback,
                                   const std::vector<TokenId>& key_locations,
                                   Status key_locations_retrieval_status) const;

  void SetCorporateKeyWithLocations(
      const std::string& public_key_spki_der,
      SetCorporateKeyCallback callback,
      const std::vector<TokenId>& key_locations,
      Status key_locations_retrieval_status) const;

  const bool profile_is_managed_;
  PrefService* const profile_prefs_;
  policy::PolicyService* const profile_policies_;
  extensions::StateStore* const extensions_state_store_;
  PlatformKeysService* const platform_keys_service_;
  base::WeakPtrFactory<KeyPermissionsServiceImpl> weak_factory_{this};
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_IMPL_H_
