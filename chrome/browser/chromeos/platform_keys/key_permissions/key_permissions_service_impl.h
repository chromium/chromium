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
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

class PrefService;

namespace base {
class Value;
}

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
  // Implementation of PermissionsForExtension.
  class PermissionsForExtensionImpl : public PermissionsForExtension {
   public:
    // |key_permissions_service| must not be null and outlive this object.
    // Methods of this object refer implicitly to the extension with the id
    // |extension_id|. Don't use this constructor directly. Call
    // |KeyPermissionsService::GetPermissionsForExtension| instead.
    PermissionsForExtensionImpl(
        const std::string& extension_id,
        std::unique_ptr<base::Value> state_store_value,
        PrefService* profile_prefs,
        policy::PolicyService* profile_policies,
        KeyPermissionsServiceImpl* key_permissions_service_impl);

    PermissionsForExtensionImpl(const PermissionsForExtensionImpl& other) =
        delete;
    PermissionsForExtensionImpl& operator=(
        const PermissionsForExtensionImpl& other) = delete;
    ~PermissionsForExtensionImpl() override;

    void CanUseKeyForSigning(const std::string& public_key_spki_der,
                             CanUseKeyForSigningCallback callback) override;

    void SetKeyUsedForSigning(const std::string& public_key_spki_der,
                              SetKeyUsedForSigningCallback callback) override;

    void RegisterKeyForCorporateUsage(
        const std::string& public_key_spki_der,
        RegisterKeyForCorporateUsageCallback callback) override;

    void SetUserGrantedPermission(
        const std::string& public_key_spki_der,
        SetUserGrantedPermissionCallback callback) override;

   private:
    struct KeyEntry;

    // Writes the current |state_store_entries_| to the state store of
    // |extension_id_|.
    void WriteToStateStore();

    // Reads a KeyEntry list from |state| and stores them in
    // |state_store_entries_|.
    void KeyEntriesFromState(const base::Value& state);

    // Converts |state_store_entries_| to a base::Value for storing in the state
    // store.
    std::unique_ptr<base::Value> KeyEntriesToState();

    // Returns an existing entry for |public_key_spki_der_b64| from
    // |state_store_entries_|. If there is no existing entry, creates, adds and
    // returns a new entry.
    // |public_key_spki_der| must be the base64 encoding of the DER of a Subject
    // Public Key Info.
    KeyPermissionsServiceImpl::PermissionsForExtensionImpl::KeyEntry*
    GetStateStoreEntry(const std::string& public_key_spki_der_b64);

    bool PolicyAllowsCorporateKeyUsage() const;

    void CanUseKeyForSigningWithLocations(
        const std::string& public_key_spki_der,
        CanUseKeyForSigningCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status);
    void CanUseKeyForSigningWithFlags(CanUseKeyForSigningCallback callback,
                                      bool sign_unlimited_allowed,
                                      bool is_corporate_key);

    void SetKeyUsedForSigningWithLocations(
        const std::string& public_key_spki_der,
        SetKeyUsedForSigningCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status);
    void RegisterKeyForCorporateUsageWithLocations(
        const std::string& public_key_spki_der,
        RegisterKeyForCorporateUsageCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status);

    void SetUserGrantedPermissionWithLocations(
        const std::string& public_key_spki_der,
        SetUserGrantedPermissionCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status);
    void SetUserGrantedPermissionWithLocationsAndFlag(
        const std::string& public_key_spki_der,
        SetUserGrantedPermissionCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status,
        bool can_user_grant_permission);

    const std::string extension_id_;
    std::vector<KeyEntry> state_store_entries_;
    PrefService* const profile_prefs_;
    policy::PolicyService* const profile_policies_;
    KeyPermissionsServiceImpl* const key_permissions_service_;
    base::WeakPtrFactory<PermissionsForExtensionImpl> weak_factory_{this};
  };

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

  void GetPermissionsForExtension(
      const std::string& extension_id,
      GetPermissionsForExtensionCallback callback) override;

  void CanUserGrantPermissionForKey(
      const std::string& public_key_spki_der,
      CanUserGrantPermissionForKeyCallback callback) const override;

  void IsCorporateKey(const std::string& public_key_spki_der,
                      IsCorporateKeyCallback callback) const override;

  void SetCorporateKey(const std::string& public_key_spki_der,
                       SetCorporateKeyCallback callback) const override;

  // Returns the list of apps and extensions ids allowed to use corporate usage
  // keys by policy in |profile_policies|.
  static std::vector<std::string> GetCorporateKeyUsageAllowedAppIds(
      policy::PolicyService* const profile_policies);

 private:
  // Creates a PermissionsForExtension object from |extension_id| and |value|
  // and passes the object to |callback|.
  void CreatePermissionObjectAndPassToCallback(
      const std::string& extension_id,
      GetPermissionsForExtensionCallback callback,
      std::unique_ptr<base::Value> value);

  // Writes |value| to the state store of the extension with id |extension_id|.
  void SetPlatformKeysOfExtension(const std::string& extension_id,
                                  std::unique_ptr<base::Value> value);

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
