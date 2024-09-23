// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_backend_migration_decorator.h"

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/built_in_backend_to_android_backend_migrator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

PasswordStoreBackendMigrationDecorator::PasswordStoreBackendMigrationDecorator(
    std::unique_ptr<PasswordStoreBackend> built_in_backend,
    std::unique_ptr<PasswordStoreBackend> android_backend,
    PrefService* prefs)
    : built_in_backend_(std::move(built_in_backend)),
      android_backend_(std::move(android_backend)),
      prefs_(prefs) {
  CHECK(built_in_backend_);
  CHECK(android_backend_);
  migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
      built_in_backend_.get(), android_backend_.get(), prefs_);
}

PasswordStoreBackendMigrationDecorator::
    ~PasswordStoreBackendMigrationDecorator() = default;

void PasswordStoreBackendMigrationDecorator::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingCallback<void(bool)> pending_initialization_calls =
      base::BarrierCallback<bool>(
          /*num_callbacks=*/2,
          base::BindOnce([](const std::vector<bool>& results) {
            return base::ranges::all_of(results, std::identity());
          }).Then(std::move(completion)));
  auto remote_changes_callback = base::BindRepeating(
      &PasswordStoreBackendMigrationDecorator::OnRemoteFormChangesReceived,
      weak_ptr_factory_.GetWeakPtr(), remote_form_changes_received);

  built_in_backend_->InitBackend(
      affiliated_match_helper,
      base::BindRepeating(remote_changes_callback,
                          built_in_backend_->AsWeakPtr()),
      std::move(sync_enabled_or_disabled_cb), pending_initialization_calls);
  android_backend_->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      base::BindRepeating(remote_changes_callback,
                          android_backend_->AsWeakPtr()),
      base::NullCallback(), pending_initialization_calls);
  if (password_manager::features::kSimulateFailedMigration.Get()) {
    // Don't try to migrate to simulate a failed migration. This causes the
    // pref to remain 'kOffAndMigrationPending' and no passwords to be migrated.
    return;
  }
  metrics_util::LogLocalPwdMigrationProgressState(
      metrics_util::LocalPwdMigrationProgressState::kScheduled);
  // Post delayed task to start migration of local passwords to avoid extra load
  // on start-up.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                         StartMigrationOfLocalPasswords,
                     migrator_->GetWeakPtr()),
      kLocalPasswordsMigrationToAndroidBackendDelay);
}

void PasswordStoreBackendMigrationDecorator::Shutdown(
    base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  migrator_.reset();

  auto shutdown_closure =
      base::BarrierClosure(2, std::move(shutdown_completed));
  built_in_backend_->Shutdown(
      base::BindOnce(shutdown_closure)
          .Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(built_in_backend_)))));
  android_backend_->Shutdown(
      base::BindOnce(shutdown_closure)
          .Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(android_backend_)))));
}

bool PasswordStoreBackendMigrationDecorator::IsAbleToSavePasswords() {
  // Suppress saving while the migration of local passwords is ongoing, to avoid
  // the migration "forgetting" any new passwords.
  return active_backend()->IsAbleToSavePasswords() &&
         !(migrator_ && migrator_->migration_in_progress());
}

void PasswordStoreBackendMigrationDecorator::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend()->GetAllLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::
    GetAllLoginsWithAffiliationAndBrandingAsync(LoginsOrErrorReply callback) {
  active_backend()->GetAllLoginsWithAffiliationAndBrandingAsync(
      std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend()->GetAutofillableLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void PasswordStoreBackendMigrationDecorator::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  active_backend()->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                            forms);
}

void PasswordStoreBackendMigrationDecorator::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  active_backend()->GetGroupedMatchingLoginsAsync(std::move(form_digest),
                                                  std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend()->AddLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend()->UpdateLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  active_backend()->RemoveLoginAsync(location, form, std::move(callback));
  if (UsesSplitStoresAndUPMForLocal(prefs_)) {
    built_in_backend_->RemoveLoginAsync(location, form, base::DoNothing());
  }
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  active_backend()->RemoveLoginsByURLAndTimeAsync(
      location, url_filter, delete_begin, delete_end,
      std::move(sync_completion), std::move(callback));
  if (UsesSplitStoresAndUPMForLocal(prefs_)) {
    built_in_backend_->RemoveLoginsByURLAndTimeAsync(
        location, url_filter, delete_begin, delete_end, base::NullCallback(),
        base::DoNothing());
  }
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  active_backend()->RemoveLoginsCreatedBetweenAsync(
      location, delete_begin, delete_end, std::move(callback));
  if (UsesSplitStoresAndUPMForLocal(prefs_)) {
    built_in_backend_->RemoveLoginsCreatedBetweenAsync(
        location, delete_begin, delete_end, base::DoNothing());
  }
}

void PasswordStoreBackendMigrationDecorator::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  active_backend()->DisableAutoSignInForOriginsAsync(origin_filter,
                                                     std::move(completion));
}

SmartBubbleStatsStore*
PasswordStoreBackendMigrationDecorator::GetSmartBubbleStatsStore() {
  return nullptr;
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreBackendMigrationDecorator::CreateSyncControllerDelegate() {
  return built_in_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreBackendMigrationDecorator::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  // TODO: b/323880741 - make sure `migrator_` doesn't require `sync_service`
  // for local password migration.
  if (migrator_) {
    migrator_->OnSyncServiceInitialized(sync_service);
  }
}

void PasswordStoreBackendMigrationDecorator::
    RecordAddLoginAsyncCalledFromTheStore() {
  active_backend()->RecordAddLoginAsyncCalledFromTheStore();
}

void PasswordStoreBackendMigrationDecorator::
    RecordUpdateLoginAsyncCalledFromTheStore() {
  active_backend()->RecordUpdateLoginAsyncCalledFromTheStore();
}

base::WeakPtr<PasswordStoreBackend>
PasswordStoreBackendMigrationDecorator::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreBackendMigrationDecorator::OnRemoteFormChangesReceived(
    RemoteChangesReceived remote_form_changes_received,
    base::WeakPtr<PasswordStoreBackend> from_backend,
    std::optional<PasswordStoreChangeList> changes) {
  if (from_backend && from_backend.get() == active_backend()) {
    remote_form_changes_received.Run(std::move(changes));
  }
}

PasswordStoreBackend* PasswordStoreBackendMigrationDecorator::active_backend() {
  return UsesSplitStoresAndUPMForLocal(prefs_) ? android_backend_.get()
                                               : built_in_backend_.get();
}

}  // namespace password_manager
