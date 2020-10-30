// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace chromeos {
namespace platform_keys {

class PlatformKeysService;

class KeyPermissionsManagerImpl : public KeyPermissionsManager {
 public:
  // Updates chaps with the current "corporate" flag for keys on a certain
  // token.
  class KeyPermissionsInChapsUpdater {
   public:
    // The updater possible modes.
    enum class Mode {
      // Used for the one-time key permissions migration step. For more
      // information regarding the one-time migration step, please refer to
      // KeyPermissionsManager documentation.
      kMigratePermissionsFromPrefs
    };

    // |key_permissions_manager| must not be null and must outlive the updater
    // instance.
    explicit KeyPermissionsInChapsUpdater(
        Mode mode,
        KeyPermissionsManagerImpl* key_permissions_manager);
    KeyPermissionsInChapsUpdater(const KeyPermissionsInChapsUpdater&) = delete;
    KeyPermissionsInChapsUpdater& operator=(
        const KeyPermissionsInChapsUpdater&) = delete;
    ~KeyPermissionsInChapsUpdater();

    // If the update operation has been done successfully, a success
    // |update_status| will be returned. An error |update_status| will be
    // returned otherwise.
    using UpdateCallback = base::OnceCallback<void(Status update_status)>;
    // Updates the key permissions in chaps according to |mode_|.
    void Update(UpdateCallback callback);

   private:
    void UpdateWithAllKeys(std::vector<std::string> public_key_spki_der_list,
                           Status keys_retrieval_status);
    void UpdateNextKey();
    void UpdatePermissionsForKey(const std::string& public_key_spki_der);
    void UpdatePermissionsForKeyWithCorporateFlag(
        const std::string& public_key_spki_der,
        base::Optional<bool> corporate_usage_allowed,
        Status corporate_usage_retrieval_status);
    void OnKeyPermissionsUpdated(Status permissions_update_status);

    const Mode mode_;
    KeyPermissionsManagerImpl* const key_permissions_manager_;
    base::queue<std::string> public_key_spki_der_queue_;
    bool update_started_ = false;
    UpdateCallback callback_;
    base::WeakPtrFactory<KeyPermissionsInChapsUpdater> weak_ptr_factory_{this};
  };

  // Returns a key permissions manager that manages keys residing on the system
  // token.
  static KeyPermissionsManager* GetSystemTokenKeyPermissionsManager();
  // Returns a key permissions manager that manages keys residing on the user
  // token corresponding to |profile|.
  static KeyPermissionsManager* GetUserPrivateTokenKeyPermissionsManager(
      Profile* profile);

  // When called with a non-nullptr |system_token_kpm_for_testing|, subsequent
  // calls to GetSystemTokenKeyPermissionsManager() will return the passed
  // pointer. When called with nullptr, subsequent calls to
  // GetSystemTokenKeyPermissionsManager() will return the default system token
  // key permissions manager again. The caller is responsible that this is
  // called with nullptr before an object previously passed in is destroyed.
  static void SetSystemTokenKeyPermissionsManagerForTesting(
      KeyPermissionsManager* system_token_kpm_for_testing);

  // Used by ChromeBrowserMainPartsChromeos to create a system-wide key
  // permissions manager instance.
  static std::unique_ptr<KeyPermissionsManager>
  CreateSystemTokenKeyPermissionsManager();

  // Registers system-wide prefs.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // This is mainly used for testing the one-time migration with keys on the
  // slot. Disabling one-time migration will give the test a chance to generate
  // keys for testing before re-enabling one-time migration. Note: re-enabling
  // one-time migration through this method won't trigger the migration process,
  // it will only allow it to run the next time the KPM instance is created.
  static void SetOneTimeMigrationEnabledForTesting(bool enabled);

  // Don't use this constructor directly. Use
  // GetSystemTokenKeyPermissionsManager or
  // GetUserPrivateTokenKeyPermissionsManager instead.
  KeyPermissionsManagerImpl(TokenId token_id,
                            PlatformKeysService* platform_keys_service,
                            PrefService* pref_service);
  KeyPermissionsManagerImpl(const KeyPermissionsManagerImpl&) = delete;
  KeyPermissionsManagerImpl& operator=(const KeyPermissionsManagerImpl&) =
      delete;
  ~KeyPermissionsManagerImpl() override;

  void AllowKeyForUsage(AllowKeyForUsageCallback callback,
                        KeyUsage usage,
                        const std::string& public_key_spki_der) override;
  void IsKeyAllowedForUsage(IsKeyAllowedForUsageCallback callback,
                            KeyUsage usage,
                            const std::string& public_key_spki_der) override;

 private:
  void OnGotTokens(std::unique_ptr<std::vector<TokenId>> token_ids,
                   Status status);

  void StartOneTimeMigration();
  void OnOneTimeMigrationDone(Status migration_status);

  bool IsOneTimeMigrationDone() const;

  void MigrateFlagsWithAllKeys(
      std::vector<std::string> public_key_spki_der_list,
      Status all_keys_retrieval_status);
  void MigrateFlagsWithQueueOfKeys(base::queue<std::string> queue);
  void OnFlagsMigratedForKey(base::queue<std::string> queue,
                             Status last_key_flags_migration_status);

  void AllowKeyForCorporateUsage(AllowKeyForUsageCallback callback,
                                 const std::string& public_key_spki_der);

  void OnKeyPermissionsRetrieved(
      IsKeyAllowedForUsageCallback callback,
      const base::Optional<std::string>& attribute_value,
      Status status);

  void IsKeyAllowedForUsageWithPermissions(
      IsKeyAllowedForUsageCallback callback,
      KeyUsage usage,
      const base::Optional<std::string>& serialized_key_permissions,
      Status key_attribute_retrieval_status);

  // Called when the token is ready and the one-time migration is done.
  void OnReadyForQueries();

  // The token for which the key permissions manager instance is responsible.
  const TokenId token_id_;
  // True if the token is ready and the one-time migration is done.
  // List of queries waiting for the token to be ready and the one-time
  // migration to be done.
  bool ready_for_queries_ = false;
  // A list of queries that will be performed after the token is ready and the
  // one-time migration is done.
  std::vector<base::OnceClosure> queries_waiting_list_;
  // If not nullptr, then this is the only updater running.
  std::unique_ptr<KeyPermissionsInChapsUpdater>
      key_permissions_in_chaps_updater_;
  PlatformKeysService* const platform_keys_service_ = nullptr;
  PrefService* const pref_service_ = nullptr;
  base::WeakPtrFactory<KeyPermissionsManagerImpl> weak_ptr_factory_{this};
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
