// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"

class PrefService;

namespace password_manager {

// Instantiate this object to migrate all stored passwords between the built-in
// and the Android backends. Migration is potentially an expensive operation
// and shouldn't start during the hot phase of Chrome start.
class BuiltInBackendToAndroidBackendMigrator {
 public:
  // The type of operation triggered on backend during the migration. Used for
  // the metrics reporting.
  enum class BackendOperationForMigration {
    kAddLogin,
    kUpdateLogin,
    kRemoveLogin,
    kGetAllLogins,
  };

  // |built_in_backend| and |android_backend| must not be null and must outlive
  // the migrator.
  BuiltInBackendToAndroidBackendMigrator(PasswordStoreBackend* built_in_backend,
                                         PasswordStoreBackend* android_backend,
                                         PrefService* prefs);

  BuiltInBackendToAndroidBackendMigrator(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  ~BuiltInBackendToAndroidBackendMigrator();

  // Starts migration from |built_in_backend| to |android_backend| if time from
  // last attempt is enough.
  void StartMigrationOfLocalPasswords();

  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  bool migration_in_progress() const { return migration_in_progress_; }

  base::WeakPtr<BuiltInBackendToAndroidBackendMigrator> GetWeakPtr();

 private:
  struct IsPasswordLess;
  struct BackendAndLoginsResults;
  class MigrationMetricsReporter;

  using PasswordFormPtrFlatSet =
      base::flat_set<const PasswordForm*, IsPasswordLess>;

  // Migrates password from the profile store |built_in_backend_| to the Gms
  // core local store |android_backend_|. |result| consists of passwords from
  // the |built_in_backend_| let's call them |A|. If the password from |A| is
  // already present in |android_backend_|, then the latest version of the
  // credential is adopted by |android_backend_|.
  void MigrateLocalPasswordsBetweenAndroidAndBuiltInBackends(
      std::vector<BackendAndLoginsResults> result);

  // Updates both |built_in_backend_| and |android_backend_| such that both
  // contain the same set of passwords without deleting any password. In
  // addition, it marks the initial migration as completed.
  void MergeBuiltInBackendIntoAndroidBackend(
      PasswordFormPtrFlatSet built_in_backend_logins,
      PasswordFormPtrFlatSet android_logins);

  // Helper methods to {Add,Update,Remove} |form| in |backend|. This is used to
  // ensure that all the operations are happening inside
  // BuiltInBackendToAndroidBackendMigrator life-scope.
  void AddLoginToBackend(PasswordStoreBackend* backend,
                         const PasswordForm& form,
                         base::OnceClosure callback);
  void UpdateLoginInBackend(PasswordStoreBackend* backend,
                            const PasswordForm& form,
                            base::OnceClosure callback);
  void RemoveLoginFromBackend(PasswordStoreBackend* backend,
                              const PasswordForm& form,
                              base::OnceClosure callback);

  // If |changelist| is an empty changelist, migration is aborted by calling
  // MigrationFinished() indicating the migration is *not* successful.
  // Otherwise, |callback| is invoked. |backend| is used to know on which
  // backend the operation was performed, for the purpose of recording metrics.
  void RunCallbackOrAbortMigration(
      base::OnceClosure callback,
      const std::string& backend_infix,
      BackendOperationForMigration backend_operation,
      PasswordChangesOrError changelist);

  // Reports metrics and deletes |metrics_reporter_|
  void MigrationFinished(bool is_success);

  // Removes blocklisted forms with non-empty |username_value| or
  // |password_value| from |backend|.
  // |result_callback| is called with the |LoginsResult| containing valid forms
  // only or |PasswordStoreBackendError| if it contained in |logins_or_error|.
  // |logins_or_error| is modified in place.
  void RemoveBlocklistedFormsWithValues(PasswordStoreBackend* backend,
                                        LoginsOrErrorReply result_callback,
                                        LoginsResultOrError logins_or_error);

  // Returns the string to be used in recording metrics for this |backend|.
  std::string GetMetricInfixFromBackend(PasswordStoreBackend* backend);

  const raw_ptr<PasswordStoreBackend> built_in_backend_;
  const raw_ptr<PasswordStoreBackend> android_backend_;
  const raw_ptr<PrefService> prefs_;

  std::unique_ptr<MigrationMetricsReporter> metrics_reporter_;

  raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

  bool migration_in_progress_ = false;

  base::WeakPtrFactory<BuiltInBackendToAndroidBackendMigrator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
