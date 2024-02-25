// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_KEY_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_KEY_PERMISSIONS_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class StateStore;
}

namespace policy {
class PolicyService;
}

namespace content {
class BrowserContext;
}

namespace chromeos::platform_keys {

// PlatformKeys is a field stored in each extension's state store. It saves
// signing permissions of keys in the context of a (Profile, Extension) pair.
// The current format of data that is written to the PlatformKeys field is a
// list of serialized KeyEntry objects:
//   { 'SPKI': string,
//     'signOnce': bool,  // if not present, defaults to false
//     'signUnlimited': bool  // if not present, defaults to false
//   }
//
// Do not change this constant as clients will lose their existing state.
const char kStateStorePlatformKeys[] = "PlatformKeys";

using CanUseKeyForSigningCallback = base::OnceCallback<void(bool allowed)>;

using RegisterKeyForCorporateUsageCallback =
    base::OnceCallback<void(bool is_error,
                            crosapi::mojom::KeystoreError error)>;

using SetUserGrantedPermissionCallback =
    base::OnceCallback<void(bool is_error,
                            crosapi::mojom::KeystoreError error)>;

using SetKeyUsedForSigningCallback =
    base::OnceCallback<void(bool is_error,
                            crosapi::mojom::KeystoreError error)>;

// ** ExtensionKeyPermissionsService Responsibility **
// - Managing signing permissions for a (Profile, Extension) pair.
// - Answering signing permissions queries for a (Profile, Extension) pair.
//
// The permission model depends on whether the user account is managed or not.
//
// ** If the user account is not managed **
// The user is under full control of the keys that are generated or imported
// while the device is not managed. For that, a user can grant a specific
// extension the permission to sign arbitrary data with a specific key for an
// unlimited number of times.
//
// ** If the user account is managed **
// The administrator is in charge of granting access to keys that are meant for
// corporate usage.
// The KeyPermissions policy allows the administrator to list exactly the
// extensions that are allowed to use corporate keys. Non-corporate keys are not
// affected.
//
// ** One-off Permission for the Certification Requests **
// Independent of the above, the extension that generates a key using the
// chrome.enterprise.platformKeys API is allowed to sign arbitrary data with the
// private key for a single time in order to create a certification request.
// The assumption is that certification requests usually require a signature of
// data including the public key. So the one-off permission implies that once a
// certificate authority creates the certificate of the generated key, the
// generating extension isn't able to use the key anymore except if explicitly
// permitted by the administrator.
//
// ** IMPORTANT: Synchronization / Possible Race Conditions **
// This class reads from the extensions::StateStore only once on creation and
// caches the result. Further reads are done using the cached result. This can
// lead to race conditions once there can exist more than one instance of the
// service for the same (Profile, Extension) pair. Currently this will not
// happen as the only class owning ExtensionKeyPermissionsServices is
// ExtensionPlatformKeysService, which never owns two
// ExtensionKeyPermissionsServices at the same time.
class ExtensionKeyPermissionsService {
 public:
  // |key_permissions_service| must not be null and outlive this object.
  // Methods of this object refer implicitly to the extension with the id
  // |extension_id|. Don't use this constructor directly. Call
  // |ExtensionKeyPermissionsServiceFactory::GetForBrowserContextAndExtension|
  // instead.
  ExtensionKeyPermissionsService(const std::string& extension_id,
                                 extensions::StateStore* state_store,
                                 base::Value::List state_store_value,
                                 policy::PolicyService* profile_policies,
                                 content::BrowserContext* browser_context);

  ExtensionKeyPermissionsService(const ExtensionKeyPermissionsService&) =
      delete;
  ExtensionKeyPermissionsService& operator=(
      const ExtensionKeyPermissionsService&) = delete;
  ~ExtensionKeyPermissionsService();

  // Returns true if the private key matching |public_key_spki_der| can be
  // used for signing by the extension with id |extension_id_|.
  // |key_locations| must describe locations available to the user the private
  // key is stored on.
  void CanUseKeyForSigning(const std::vector<uint8_t>& public_key_spki_der,
                           CanUseKeyForSigningCallback callback);

  // Must be called when the extension with id |extension_id| used the private
  // key matching |public_key_spki_der| for signing. |key_locations| must
  // describe locations available to the user the private key is stored on.
  // Updates the permissions accordingly.  E.g. if this extension generated
  // the key and no other permission was granted then the permission to sign
  // with this key is removed.
  void SetKeyUsedForSigning(const std::vector<uint8_t>& public_key_spki_der,
                            SetKeyUsedForSigningCallback callback);

  // Registers the private key matching |public_key_spki_der| as being generated
  // by the extension with id |extension_id| and marks it for corporate usage.
  // |key_locations| must describe locations available to the user the private
  // key is stored on.
  void RegisterKeyForCorporateUsage(
      const std::vector<uint8_t>& public_key_spki_der,
      RegisterKeyForCorporateUsageCallback callback);

  // Sets the user granted permission that the extension with id
  // |extension_id| can use the private key matching |public_key_spki_der| for
  // signing. |key_locations| must describe locations available to the user
  // the private key is stored on.
  void SetUserGrantedPermission(const std::vector<uint8_t>& public_key_spki_der,
                                SetUserGrantedPermissionCallback callback);

  // Returns the list of apps and extensions ids allowed to use corporate usage
  // keys by policy in |profile_policies|.
  static std::vector<std::string> GetCorporateKeyUsageAllowedAppIds(
      policy::PolicyService* const profile_policies);

 private:
  struct KeyEntry {
    explicit KeyEntry(const std::string& public_key_spki_der_b64)
        : spki_b64(public_key_spki_der_b64) {}

    // The base64-encoded DER of a X.509 Subject Public Key Info.
    std::string spki_b64;

    // True if the key can be used once for singing.
    // This permission is granted if an extension generated a key using the
    // enterprise.platformKeys API, so that it can build a certification
    // request. After the first signing operation this permission will be
    // revoked.
    bool sign_once = false;

    // True if the key can be used for signing an unlimited number of times.
    // This permission is granted by the user to allow the extension to use the
    // key for signing through the enterprise.platformKeys or platformKeys API.
    // This permission is granted until revoked by the user or the policy.
    bool sign_unlimited = false;
  };

  void OnGotExtensionValue(std::optional<base::Value> value);

  // Writes the current |state_store_entries_| to the state store of
  // |extension_id_|.
  void WriteToStateStore();

  // Reads a KeyEntry list from |state| and stores them in
  // |state_store_entries_|.
  void KeyEntriesFromState(const base::Value::List& state);

  // Converts |state_store_entries_| to a base::Value for storing in the state
  // store.
  base::Value::List KeyEntriesToState();

  // Returns an existing entry for |public_key_spki_der_b64| from
  // |state_store_entries_|. If there is no existing entry, creates, adds and
  // returns a new entry.
  // |public_key_spki_der| must be the base64 encoding of the DER of a Subject
  // Public Key Info.
  KeyEntry* GetStateStoreEntry(const std::string& public_key_spki_der_b64);

  // Writes |value| to the state store of the extension.
  void SetPlatformKeysInStateStore(std::optional<base::Value> value);

  bool PolicyAllowsCorporateKeyUsage() const;

  void CanUseKeyForSigningWithFlags(
      CanUseKeyForSigningCallback callback,
      bool sign_unlimited_allowed,
      crosapi::mojom::GetKeyTagsResultPtr key_tags);

  void SetUserGrantedPermissionWithFlag(
      const std::vector<uint8_t>& public_key_spki_der,
      SetUserGrantedPermissionCallback callback,
      bool can_user_grant_permission);

  const extensions::ExtensionId extension_id_;
  raw_ptr<extensions::StateStore, FlakyDanglingUntriaged>
      extensions_state_store_ = nullptr;
  std::vector<KeyEntry> state_store_entries_;
  const raw_ptr<policy::PolicyService> profile_policies_;
  const raw_ptr<crosapi::mojom::KeystoreService> keystore_service_ = nullptr;
  base::WeakPtrFactory<ExtensionKeyPermissionsService> weak_factory_{this};
};

}  // namespace chromeos::platform_keys

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_KEY_PERMISSIONS_SERVICE_H_
