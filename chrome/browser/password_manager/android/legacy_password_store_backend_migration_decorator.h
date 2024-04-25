// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LEGACY_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LEGACY_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/android/built_in_backend_to_android_backend_migrator.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace password_manager {

// This is the backend that should be used on Android platform until the full
// migration to the Android backend is launched. Internally, this backend
// owns two backends: the built-in and the Android backend. In addition
// to delegating all backend responsibilities, it is responsible for migrating
// credentials between both backends as well as instantiating any proxy backends
// that are used for shadowing the traffic.
class LegacyPasswordStoreBackendMigrationDecorator : public PasswordStoreBackend {
 public:
  LegacyPasswordStoreBackendMigrationDecorator(
      std::unique_ptr<PasswordStoreBackend> built_in_backend,
      std::unique_ptr<PasswordStoreBackend> android_backend,
      PrefService* prefs);
  LegacyPasswordStoreBackendMigrationDecorator(
      const LegacyPasswordStoreBackendMigrationDecorator&) = delete;
  LegacyPasswordStoreBackendMigrationDecorator(
      LegacyPasswordStoreBackendMigrationDecorator&&) = delete;
  LegacyPasswordStoreBackendMigrationDecorator& operator=(
      const LegacyPasswordStoreBackendMigrationDecorator&) = delete;
  LegacyPasswordStoreBackendMigrationDecorator& operator=(
      LegacyPasswordStoreBackendMigrationDecorator&&) = delete;
  ~LegacyPasswordStoreBackendMigrationDecorator() override;

  BuiltInBackendToAndroidBackendMigrator::MigrationType
  migration_in_progress_type() const;

 private:
  class PasswordSyncSettingsHelper {
   public:
    PasswordSyncSettingsHelper();

    // Remembers the initial sync setting to track its changes later.
    // Should be called after SyncService is initialized.
    void CachePasswordSyncSettingOnStartup(syncer::SyncService* sync);

    // Called when sync settings were applied to confirm change of state.
    bool ShouldActOnSyncStatusChanges();

    // Returns sync_util::IsSyncFeatureEnabledIncludingPasswords value.
    bool IsSyncFeatureEnabledIncludingPasswords();

   private:

    // Pref service.
    const raw_ptr<PrefService> prefs_ = nullptr;

    // Set when sync_service is already initialized and can be interacted with.
    raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

    // Cached value of the configured password sync setting. Updated when the
    // user is changing sync settings, and may from
    // |password_sync_applied_setting_| at that moment.
    bool password_sync_configured_setting_ = false;
  };

  // Implements PasswordStoreBackend interface.
  void InitBackend(AffiliatedMatchHelper* affiliated_match_helper,
                   RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  bool IsAbleToSavePasswords() override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(std::string account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void GetGroupedMatchingLoginsAsync(const PasswordFormDigest& form_digest,
                                     LoginsOrErrorReply callback) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const base::Location& location,
                        const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

  // Starts migration process.
  void StartMigrationIfNecessary();

  // React on sync changes to keep GMS Core local storage up-to-date.
  // Called when the changed setting is applied.
  // TODO(crbug.com/) Remove this method when no longer needed.
  void SyncStatusChanged();

  // Proxy backend to which all responsibilities are being delegated.
  std::unique_ptr<PasswordStoreBackend> active_backend_;

  raw_ptr<PasswordStoreBackend> built_in_backend_;
  raw_ptr<PasswordStoreBackend> android_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;

  // Listener for sync settings changes.
  PasswordSyncSettingsHelper sync_settings_helper_;

  base::WeakPtrFactory<LegacyPasswordStoreBackendMigrationDecorator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LEGACY_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
