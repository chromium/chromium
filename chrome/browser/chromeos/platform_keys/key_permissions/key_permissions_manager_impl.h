// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager.h"
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

class KeyPermissionsManagerImpl : public KeyPermissionsManager {
 public:
  // Implementation of PermissionsForExtension.
  class PermissionsForExtensionImpl : public PermissionsForExtension {
   public:
    // |key_permissions| must not be null and outlive this object.
    // Methods of this object refer implicitly to the extension with the id
    // |extension_id|. Don't use this constructor directly. Call
    // |KeyPermissionsManager::GetPermissionsForExtension| instead.
    PermissionsForExtensionImpl(const std::string& extension_id,
                                std::unique_ptr<base::Value> state_store_value,
                                PrefService* profile_prefs,
                                policy::PolicyService* profile_policies,
                                KeyPermissionsManagerImpl* key_permissions);

    PermissionsForExtensionImpl(const PermissionsForExtensionImpl& other) =
        delete;
    PermissionsForExtensionImpl& operator=(
        const PermissionsForExtensionImpl& other) = delete;
    ~PermissionsForExtensionImpl() override;

    bool CanUseKeyForSigning(
        const std::string& public_key_spki_der,
        const std::vector<platform_keys::TokenId>& key_locations) override;

    void RegisterKeyForCorporateUsage(
        const std::string& public_key_spki_der,
        const std::vector<platform_keys::TokenId>& key_locations) override;

    void SetUserGrantedPermission(
        const std::string& public_key_spki_der,
        const std::vector<platform_keys::TokenId>& key_locations) override;

    void SetKeyUsedForSigning(
        const std::string& public_key_spki_der,
        const std::vector<platform_keys::TokenId>& key_locations) override;

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
    KeyPermissionsManagerImpl::PermissionsForExtensionImpl::KeyEntry*
    GetStateStoreEntry(const std::string& public_key_spki_der_b64);

    bool PolicyAllowsCorporateKeyUsage() const;

    const std::string extension_id_;
    std::vector<KeyEntry> state_store_entries_;
    PrefService* const profile_prefs_;
    policy::PolicyService* const profile_policies_;
    KeyPermissionsManagerImpl* const key_permissions_;
  };

  // |profile_prefs| and |extensions_state_store| must not be null and must
  // outlive this object.
  // If |profile_is_managed| is false, |profile_policies| is ignored. Otherwise,
  // |profile_policies| must not be null and must outlive this object.
  // |profile_is_managed| determines the default usage and permissions for
  // keys without explicitly assigned usage.
  KeyPermissionsManagerImpl(bool profile_is_managed,
                            PrefService* profile_prefs,
                            policy::PolicyService* profile_policies,
                            extensions::StateStore* extensions_state_store);

  ~KeyPermissionsManagerImpl() override;

  KeyPermissionsManagerImpl(const KeyPermissionsManagerImpl& other) = delete;
  KeyPermissionsManagerImpl& operator=(const KeyPermissionsManagerImpl& other) =
      delete;

  void GetPermissionsForExtension(const std::string& extension_id,
                                  const PermissionsCallback& callback) override;

  bool CanUserGrantPermissionFor(
      const std::string& public_key_spki_der,
      const std::vector<platform_keys::TokenId>& key_locations) const override;

  bool IsCorporateKey(
      const std::string& public_key_spki_der,
      const std::vector<platform_keys::TokenId>& key_locations) const override;

  void SetCorporateKey(const std::string& public_key_spki_der,
                       platform_keys::TokenId key_location) const override;

  // Returns true if |public_key_spki_der_b64| is a corporate usage key.
  // TOOD(http://crbug.com/1127284): Remove this and migrate callers to
  // IsCorporateKey().
  static bool IsCorporateKeyForProfile(
      const std::string& public_key_spki_der_b64,
      const PrefService* const profile_prefs);

  // Returns the list of apps and extensions ids allowed to use corporate usage
  // keys by policy in |profile_policies|.
  static std::vector<std::string> GetCorporateKeyUsageAllowedAppIds(
      policy::PolicyService* const profile_policies);

 private:
  // Creates a PermissionsForExtension object from |extension_id| and |value|
  // and passes the object to |callback|.
  void CreatePermissionObjectAndPassToCallback(
      const std::string& extension_id,
      const PermissionsCallback& callback,
      std::unique_ptr<base::Value> value);

  // Writes |value| to the state store of the extension with id |extension_id|.
  void SetPlatformKeysOfExtension(const std::string& extension_id,
                                  std::unique_ptr<base::Value> value);

  const bool profile_is_managed_;
  PrefService* const profile_prefs_;
  policy::PolicyService* const profile_policies_;
  extensions::StateStore* const extensions_state_store_;
  base::WeakPtrFactory<KeyPermissionsManagerImpl> weak_factory_{this};
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
