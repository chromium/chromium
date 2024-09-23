// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace password_manager {

class BuiltInBackendToAndroidBackendMigrator;

// Exposed here for testing.
inline constexpr base::TimeDelta kLocalPasswordsMigrationToAndroidBackendDelay =
    base::Seconds(5);

// This backend migrates local passwords from `built_in_backend` to
// `android_backend`. Migration is scheduled after InitBackend() call. While
// migration is ongoing password saving is suppressed. Before migration is
// finished `built_in_backend` is used for all operations. After migration is
// complete `android_backend` is used instead, although password deletions are
// still propagated to `built_in_backend` as well.
class PasswordStoreBackendMigrationDecorator : public PasswordStoreBackend {
 public:
  PasswordStoreBackendMigrationDecorator(
      std::unique_ptr<PasswordStoreBackend> built_in_backend,
      std::unique_ptr<PasswordStoreBackend> android_backend,
      PrefService* prefs);
  PasswordStoreBackendMigrationDecorator(
      const PasswordStoreBackendMigrationDecorator&) = delete;
  PasswordStoreBackendMigrationDecorator(
      PasswordStoreBackendMigrationDecorator&&) = delete;
  PasswordStoreBackendMigrationDecorator& operator=(
      const PasswordStoreBackendMigrationDecorator&) = delete;
  PasswordStoreBackendMigrationDecorator& operator=(
      PasswordStoreBackendMigrationDecorator&&) = delete;
  ~PasswordStoreBackendMigrationDecorator() override;

 private:
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
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

  // Forwards the (possible) forms changes caused by a remote event to the
  // backend. Only changes from the active_backend() are taken into
  // consideration. For `android_backend_` they can happen on foreground event.
  void OnRemoteFormChangesReceived(
      RemoteChangesReceived remote_form_changes_received,
      base::WeakPtr<PasswordStoreBackend> from_backend,
      std::optional<PasswordStoreChangeList> changes);

  PasswordStoreBackend* active_backend();

  std::unique_ptr<PasswordStoreBackend> built_in_backend_;
  std::unique_ptr<PasswordStoreBackend> android_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  // Helper class responsible for actually migrating passwords from one backend
  // to another.
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;

  base::WeakPtrFactory<PasswordStoreBackendMigrationDecorator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
