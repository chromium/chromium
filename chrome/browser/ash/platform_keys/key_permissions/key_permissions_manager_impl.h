// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/platform_keys/key_permissions/arc_key_permissions_manager_delegate.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace ash::platform_keys {

class PlatformKeysService;

class KeyPermissionsManagerImpl : public KeyPermissionsManager,
                                  public ArcKpmDelegate::Observer {
 public:
  // Updates chaps with the current "arc" and "corporate" flags for keys on a
  // certain token.
  class KeyPermissionsInChapsUpdater {
   public:
    // The updater possible modes.
    enum class Mode {
      // Used for the one-time key permissions migration step. For more
      // information regarding the one-time migration step, please refer to
      // KeyPermissionsManager documentation.
      kMigratePermissionsFromPrefs,
      // Used for updating ARC usage flag in chaps after the one-time key
      // permissions migration step is done and when ARC usage allowance
      // changes for keys on a token.
      kUpdateArcUsageFlag
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
    using UpdateCallback =
        base::OnceCallback<void(chromeos::platform_keys::Status update_status)>;
    // Updates the key permissions in chaps according to |mode_|.
    void Update(UpdateCallback callback);

   private:
    void UpdateWithAllKeys(
        std::vector<std::vector<uint8_t>> public_key_spki_der_list,
        chromeos::platform_keys::Status keys_retrieval_status);
    void UpdateNextKey();
    void OnUpdateFinished();
    void UpdatePermissionsForKey(std::vector<uint8_t> public_key_spki_der);
    void UpdatePermissionsForKeyWithCorporateFlag(
        std::vector<uint8_t> public_key_spki_der,
        absl::optional<bool> corporate_usage_allowed,
        chromeos::platform_keys::Status corporate_usage_retrieval_status);
    void OnKeyPermissionsUpdated(
        chromeos::platform_keys::Status permissions_update_status);

    const Mode mode_;
    const raw_ptr<KeyPermissionsManagerImpl, ExperimentalAsh>
        key_permissions_manager_;
    base::queue<std::vector<uint8_t>> public_key_spki_der_queue_;
    bool update_started_ = false;
    UpdateCallback callback_;
    // The time when the Update() method was called.
    base::TimeTicks update_start_time_;

    base::WeakPtrFactory<KeyPermissionsInChapsUpdater> weak_ptr_factory_{this};
  };

  // Returns a key permissions manager that manages keys residing on the system
  // token.
  static KeyPermissionsManager* GetSystemTokenKeyPermissionsManager();
  // Returns a key permissions manager that manages keys residing on the user
  // token corresponding to |profile|.
  // Note: A nullptr will be returned for non-regular user profiles.
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

  // Used by `ChromeBrowserMainPartsAsh` to create a system-wide key
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
  KeyPermissionsManagerImpl(
      chromeos::platform_keys::TokenId token_id,
      std::unique_ptr<ArcKpmDelegate> arc_usage_manager_delegate,
      PlatformKeysService* platform_keys_service,
      PrefService* pref_service);
  KeyPermissionsManagerImpl(const KeyPermissionsManagerImpl&) = delete;
  KeyPermissionsManagerImpl& operator=(const KeyPermissionsManagerImpl&) =
      delete;
  ~KeyPermissionsManagerImpl() override;

  void AllowKeyForUsage(AllowKeyForUsageCallback callback,
                        KeyUsage usage,
                        std::vector<uint8_t> public_key_spki_der) override;
  void IsKeyAllowedForUsage(IsKeyAllowedForUsageCallback callback,
                            KeyUsage usage,
                            std::vector<uint8_t> public_key_spki_der) override;
  bool AreCorporateKeysAllowedForArcUsage() const override;

  void Shutdown() override;

 private:
  // ArcKpmDelegate::Observer
  void OnArcUsageAllowanceForCorporateKeysChanged(bool allowed) override;

  void OnGotTokens(
      std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>> token_ids,
      chromeos::platform_keys::Status status);

  // Updates the permissions of the keys residing on |token_id| in chaps. If
  // this method is called while an update is already running, it will cancel
  // the running update and start a new one.
  void UpdateKeyPermissionsInChaps();
  void OnKeyPermissionsInChapsUpdated(
      chromeos::platform_keys::Status update_status);

  void StartOneTimeMigration();
  void OnOneTimeMigrationDone(chromeos::platform_keys::Status migration_status);
  bool IsOneTimeMigrationDone() const;

  void AllowKeyForCorporateUsage(AllowKeyForUsageCallback callback,
                                 std::vector<uint8_t> public_key_spki_der);

  void OnKeyPermissionsRetrieved(
      IsKeyAllowedForUsageCallback callback,
      const absl::optional<std::string>& attribute_value,
      chromeos::platform_keys::Status status);

  void IsKeyAllowedForUsageWithPermissions(
      IsKeyAllowedForUsageCallback callback,
      KeyUsage usage,
      absl::optional<std::vector<uint8_t>> serialized_key_permissions,
      chromeos::platform_keys::Status key_attribute_retrieval_status);

  // Called when the token is ready and the one-time migration is done.
  void OnReadyForQueries();

  // The token for which the key permissions manager instance is responsible.
  const chromeos::platform_keys::TokenId token_id_;
  // True if ARC usage is allowed for corporate keys according to
  // |arc_usage_manager_delegate_|.
  bool arc_usage_allowed_for_corporate_keys_ = false;
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
  // The ARC usage manager delegate for |token_id_|.
  std::unique_ptr<ArcKpmDelegate> arc_usage_manager_delegate_;
  raw_ptr<PlatformKeysService, ExperimentalAsh> platform_keys_service_ =
      nullptr;
  raw_ptr<PrefService, DanglingUntriaged | ExperimentalAsh> pref_service_ =
      nullptr;
  base::ScopedObservation<ArcKpmDelegate, ArcKpmDelegate::Observer>
      arc_usage_manager_delegate_observation_{this};
  base::WeakPtrFactory<KeyPermissionsManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::platform_keys

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_IMPL_H_
