// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/legacy_password_store_backend_migration_decorator.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/built_in_backend_to_android_backend_migrator.h"
#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

namespace {

// TODO(crbug.com/40067770): Migrate away from `ConsentLevel::kSync` on Android.
using sync_util::IsSyncFeatureEnabledIncludingPasswords;
using MigrationType = BuiltInBackendToAndroidBackendMigrator::MigrationType;

// Time in seconds by which the passwords migration from the built-in backend to
// the Android backend is delayed.
constexpr int kMigrationToAndroidBackendDelay = 30;

}  // namespace

LegacyPasswordStoreBackendMigrationDecorator::
    LegacyPasswordStoreBackendMigrationDecorator(
        std::unique_ptr<PasswordStoreBackend> built_in_backend,
        std::unique_ptr<PasswordStoreBackend> android_backend,
        PrefService* prefs)
    : built_in_backend_(built_in_backend.get()),
      android_backend_(android_backend.get()),
      prefs_(prefs) {
  // LegacyPasswordStoreBackendMigrationDecorator should not be created after
  // stores split under any circumstances.
  CHECK(!password_manager::UsesSplitStoresAndUPMForLocal(prefs_));
  CHECK(built_in_backend_);
  CHECK(android_backend_);
  active_backend_ = std::make_unique<PasswordStoreProxyBackend>(
      std::move(built_in_backend), std::move(android_backend), prefs_);
}

LegacyPasswordStoreBackendMigrationDecorator::
    ~LegacyPasswordStoreBackendMigrationDecorator() = default;

BuiltInBackendToAndroidBackendMigrator::MigrationType
LegacyPasswordStoreBackendMigrationDecorator::migration_in_progress_type()
    const {
  return migrator_->migration_in_progress_type();
}

LegacyPasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    PasswordSyncSettingsHelper() {}

void LegacyPasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    CachePasswordSyncSettingOnStartup(syncer::SyncService* sync) {
  sync_service_ = sync;
  // TODO(crbug.com/40067770): Migrate away from `ConsentLevel::kSync` on
  // Android.
  password_sync_configured_setting_ =
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync);
}

bool LegacyPasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    ShouldActOnSyncStatusChanges() {
  CHECK(sync_service_);

  // TODO(crbug.com/40067770): Migrate away from `ConsentLevel::kSync` on
  // Android.
  bool is_password_sync_enabled =
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service_);

  // Return false if the setting didn't change.
  if (password_sync_configured_setting_ == is_password_sync_enabled) {
    return false;
  }

  password_sync_configured_setting_ = is_password_sync_enabled;
  return true;
}

bool LegacyPasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    IsSyncFeatureEnabledIncludingPasswords() {
  CHECK(sync_service_);
  // TODO(crbug.com/40067770): Migrate away from `ConsentLevel::kSync` on
  // Android.
  return sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service_);
}

void LegacyPasswordStoreBackendMigrationDecorator::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingClosure handle_sync_status_change = base::BindRepeating(
      &LegacyPasswordStoreBackendMigrationDecorator::SyncStatusChanged,
      weak_ptr_factory_.GetWeakPtr());

  // |sync_enabled_or_disabled_cb| is called on a background sequence so it
  // should be posted to the main sequence before invoking
  // LegacyPasswordStoreBackendMigrationDecorator::SyncStatusChanged().
  base::RepeatingClosure handle_sync_status_change_on_main_thread =
      base::BindRepeating(
          base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
          base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
          std::move(handle_sync_status_change));

  // Inject nested callback to listen for sync status changes.
  sync_enabled_or_disabled_cb =
      std::move(handle_sync_status_change_on_main_thread)
          .Then(std::move(sync_enabled_or_disabled_cb));

  active_backend_->InitBackend(
      affiliated_match_helper, std::move(remote_form_changes_received),
      std::move(sync_enabled_or_disabled_cb), std::move(completion));

  migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
      built_in_backend_.get(), android_backend_.get(), prefs_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LegacyPasswordStoreBackendMigrationDecorator::
                         StartMigrationIfNecessary,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kMigrationToAndroidBackendDelay));
}

void LegacyPasswordStoreBackendMigrationDecorator::Shutdown(
    base::OnceClosure shutdown_completed) {
  migrator_.reset();
  built_in_backend_ = nullptr;
  android_backend_ = nullptr;
  // Calling Shutdown() on active_backend_ will take care of calling
  // Shutdown() on relevant backends.
  active_backend_->Shutdown(
      base::BindOnce(
          [](std::unique_ptr<PasswordStoreBackend> combined_backend) {
            // All the backends must be destroyed only after
            // |active_backend_| signals that Shutdown is over. It can be
            // done asynchronously and after
            // LegacyPasswordStoreBackendMigrationDecorator destruction.
            combined_backend.reset();
          },
          std::move(active_backend_))
          .Then(std::move(shutdown_completed)));
}

bool LegacyPasswordStoreBackendMigrationDecorator::IsAbleToSavePasswords() {
  return active_backend_->IsAbleToSavePasswords();
}

void LegacyPasswordStoreBackendMigrationDecorator::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAllLoginsAsync(std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::
    GetAllLoginsWithAffiliationAndBrandingAsync(LoginsOrErrorReply callback) {
  active_backend_->GetAllLoginsWithAffiliationAndBrandingAsync(
      std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAutofillableLoginsAsync(std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void LegacyPasswordStoreBackendMigrationDecorator::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  active_backend_->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                           forms);
}

void LegacyPasswordStoreBackendMigrationDecorator::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  active_backend_->GetGroupedMatchingLoginsAsync(std::move(form_digest),
                                                 std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend_->AddLoginAsync(form, std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend_->UpdateLoginAsync(form, std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend_->RemoveLoginAsync(form, std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  active_backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, std::move(delete_begin), std::move(delete_end),
      std::move(sync_completion), std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  active_backend_->RemoveLoginsCreatedBetweenAsync(
      std::move(delete_begin), std::move(delete_end), std::move(callback));
}

void LegacyPasswordStoreBackendMigrationDecorator::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  active_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                    std::move(completion));
}

SmartBubbleStatsStore*
LegacyPasswordStoreBackendMigrationDecorator::GetSmartBubbleStatsStore() {
  return active_backend_->GetSmartBubbleStatsStore();
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
LegacyPasswordStoreBackendMigrationDecorator::CreateSyncControllerDelegate() {
  return built_in_backend_->CreateSyncControllerDelegate();
}

void LegacyPasswordStoreBackendMigrationDecorator::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_settings_helper_.CachePasswordSyncSettingOnStartup(sync_service);
  active_backend_->OnSyncServiceInitialized(sync_service);
  if (migrator_)
    migrator_->OnSyncServiceInitialized(sync_service);
}

void LegacyPasswordStoreBackendMigrationDecorator::
    RecordAddLoginAsyncCalledFromTheStore() {
  active_backend_->RecordAddLoginAsyncCalledFromTheStore();
}

void LegacyPasswordStoreBackendMigrationDecorator::
    RecordUpdateLoginAsyncCalledFromTheStore() {
  active_backend_->RecordUpdateLoginAsyncCalledFromTheStore();
}

base::WeakPtr<PasswordStoreBackend>
LegacyPasswordStoreBackendMigrationDecorator::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void LegacyPasswordStoreBackendMigrationDecorator::StartMigrationIfNecessary() {
  // TODO(crbug.com/40067770): Migrate away from `ConsentLevel::kSync` on
  // Android.
  bool password_sync_enabled =
      sync_settings_helper_.IsSyncFeatureEnabledIncludingPasswords();

  if (prefs_->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors) &&
      password_sync_enabled) {
    int reenrollment_attempts = prefs_->GetInteger(
        prefs::kTimesAttemptedToReenrollToGoogleMobileServices);
    prefs_->SetInteger(prefs::kTimesAttemptedToReenrollToGoogleMobileServices,
                       reenrollment_attempts + 1);
    migrator_->StartAccountMigrationIfNecessary(
        MigrationType::kReenrollmentAttempt);
    return;
  }

  if (prefs_->GetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange) &&
      !password_sync_enabled) {
    // Sync was disabled at the end of the last session, but migration from
    // the android backend to the built-in backend didn't happen. It's not
    // safe to attempt to call the android backend to migrate logins. Disable
    // autosignin for all logins to avoid using outdated settings.
    built_in_backend_->DisableAutoSignInForOriginsAsync(
        base::BindRepeating([](const GURL& url) { return true; }),
        base::DoNothing());
    prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, false);
    return;
  }

  if (password_sync_enabled &&
      prefs_->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices) == 0) {
    migrator_->StartAccountMigrationIfNecessary(
        MigrationType::kInitialForSyncUsers);
  }
}

void LegacyPasswordStoreBackendMigrationDecorator::SyncStatusChanged() {
  if (prefs_->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors)) {
    return;
  }

  bool act_on_password_changes =
      sync_settings_helper_.ShouldActOnSyncStatusChanges();
  prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange,
                     act_on_password_changes);

  if (act_on_password_changes) {
    // Non-syncable data needs to be migrated to the new active backend.
    if (sync_settings_helper_.IsSyncFeatureEnabledIncludingPasswords()) {
      migrator_->StartAccountMigrationIfNecessary(
          MigrationType::kNonSyncableToAndroidBackend);
    } else {
      prefs_->SetInteger(password_manager::prefs::
                             kCurrentMigrationVersionToGoogleMobileServices,
                         0);
      migrator_->StartAccountMigrationIfNecessary(
          MigrationType::kNonSyncableToBuiltInBackend);
    }
  }
}

}  // namespace password_manager
