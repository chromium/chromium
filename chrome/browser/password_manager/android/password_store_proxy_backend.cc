// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace password_manager {

namespace {

void InvokeCallbackWithCombinedStatus(base::OnceCallback<void(bool)> completion,
                                      std::vector<bool> statuses) {
  std::move(completion).Run(base::ranges::all_of(statuses, std::identity()));
}

void RecordPasswordDeletionResult(PasswordChangesOrError result) {
  bool is_operation_successful = true;
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    is_operation_successful = false;
  }
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStoreProxyBackend.PasswordRemovalStatus",
      is_operation_successful);
  if (!is_operation_successful) {
    return;
  }

  PasswordChanges changes = absl::get<PasswordChanges>(std::move(result));

  if (changes.has_value()) {
    base::UmaHistogramCounts1000(
        "PasswordManager.PasswordStoreProxyBackend.RemovedPasswordCount",
        changes.value().size());
  }
}

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    std::unique_ptr<PasswordStoreBackend> built_in_backend,
    std::unique_ptr<PasswordStoreBackend> android_backend,
    PrefService* prefs)
    : built_in_backend_(std::move(built_in_backend)),
      android_backend_(std::move(android_backend)),
      prefs_(prefs) {
  CHECK(!password_manager::UsesSplitStoresAndUPMForLocal(prefs_));
}

PasswordStoreProxyBackend::~PasswordStoreProxyBackend() = default;

void PasswordStoreProxyBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingCallback<void(bool)> pending_initialization_calls =
      base::BarrierCallback<bool>(
          /*num_callbacks=*/2, base::BindOnce(&InvokeCallbackWithCombinedStatus,
                                              std::move(completion)));

  // Both backends need to be initialized, so using the helpers for main/shadow
  // backend is unnecessary and won't work since the sync status may not be
  // available yet.
  built_in_backend_->InitBackend(
      affiliated_match_helper,
      base::BindRepeating(
          &PasswordStoreProxyBackend::OnRemoteFormChangesReceived,
          weak_ptr_factory_.GetWeakPtr(),
          CallbackOriginatesFromAndroidBackend(false),
          remote_form_changes_received),
      std::move(sync_enabled_or_disabled_cb),
      base::BindOnce(pending_initialization_calls));

  android_backend_->InitBackend(
      affiliated_match_helper,
      base::BindRepeating(
          &PasswordStoreProxyBackend::OnRemoteFormChangesReceived,
          weak_ptr_factory_.GetWeakPtr(),
          CallbackOriginatesFromAndroidBackend(true),
          std::move(remote_form_changes_received)),
      base::NullCallback(), base::BindOnce(pending_initialization_calls));
}

void PasswordStoreProxyBackend::Shutdown(base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::RepeatingClosure pending_shutdown_calls = base::BarrierClosure(
      /*num_closures=*/2, std::move(shutdown_completed));
  android_backend_->Shutdown(pending_shutdown_calls);
  built_in_backend_->Shutdown(pending_shutdown_calls);
  android_backend_.reset();
  built_in_backend_.reset();
}

bool PasswordStoreProxyBackend::IsAbleToSavePasswords() {
  // shadow_backend()->IsAbleToSavePasswords() doesn't matter because it's a
  // fallback.
  return main_backend()->IsAbleToSavePasswords();
}
void PasswordStoreProxyBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  main_backend()->GetAllLoginsAsync(std::move(callback));
}

void PasswordStoreProxyBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    LoginsOrErrorReply callback) {
  main_backend()->GetAllLoginsWithAffiliationAndBrandingAsync(
      std::move(callback));
}

void PasswordStoreProxyBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  main_backend()->GetAutofillableLoginsAsync(std::move(callback));
}

void PasswordStoreProxyBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED_IN_MIGRATION();
}

void PasswordStoreProxyBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  main_backend()->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                          forms);
}

void PasswordStoreProxyBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  main_backend()->GetGroupedMatchingLoginsAsync(form_digest,
                                                std::move(callback));
}

void PasswordStoreProxyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordChangesOrErrorReply result_callback;
  main_backend()->AddLoginAsync(form, std::move(callback));
}

void PasswordStoreProxyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordChangesOrErrorReply result_callback;
  main_backend()->UpdateLoginAsync(form, std::move(callback));
}

void PasswordStoreProxyBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  main_backend()->RemoveLoginAsync(location, form, std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginAsync(location, form, base::DoNothing());
  }
}

void PasswordStoreProxyBackend::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  // The `sync_completion` callback is only relevant for account passwords
  // which don't exist on Android, so it is not passed in and can be ignored
  // later.
  CHECK(!sync_completion);
  main_backend()->RemoveLoginsByURLAndTimeAsync(
      location, url_filter, delete_begin, delete_end, base::NullCallback(),
      std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginsByURLAndTimeAsync(
        location, url_filter, std::move(delete_begin), std::move(delete_end),
        base::NullCallback(), base::DoNothing());
  }
}

void PasswordStoreProxyBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  main_backend()->RemoveLoginsCreatedBetweenAsync(
      location, delete_begin, delete_end, std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginsCreatedBetweenAsync(
        location, std::move(delete_begin), std::move(delete_end),
        base::DoNothing());
  }
}

void PasswordStoreProxyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(crbug.com/40208332): Implement error handling, when actual
  // store changes will be received from the store.
  main_backend()->DisableAutoSignInForOriginsAsync(origin_filter,
                                                   std::move(completion));
}

SmartBubbleStatsStore* PasswordStoreProxyBackend::GetSmartBubbleStatsStore() {
  return main_backend()->GetSmartBubbleStatsStore();
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreProxyBackend::CreateSyncControllerDelegate() {
  return built_in_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreProxyBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
  sync_service_->AddObserver(this);
  android_backend_->OnSyncServiceInitialized(sync_service);
  MaybeClearBuiltInBackend();

  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    // Reset initial UPM migration if password sync is disabled.
    prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                       0);
  }
}

void PasswordStoreProxyBackend::RecordAddLoginAsyncCalledFromTheStore() {
  main_backend()->RecordAddLoginAsyncCalledFromTheStore();
}

void PasswordStoreProxyBackend::RecordUpdateLoginAsyncCalledFromTheStore() {
  main_backend()->RecordUpdateLoginAsyncCalledFromTheStore();
}

base::WeakPtr<PasswordStoreBackend> PasswordStoreProxyBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PasswordStoreBackend* PasswordStoreProxyBackend::main_backend() {
  return UsesAndroidBackendAsMainBackend() ? android_backend_.get()
                                           : built_in_backend_.get();
}

PasswordStoreBackend* PasswordStoreProxyBackend::shadow_backend() {
  return UsesAndroidBackendAsMainBackend() ? built_in_backend_.get()
                                           : android_backend_.get();
}

void PasswordStoreProxyBackend::OnStateChanged(syncer::SyncService* sync) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    // Reset initial UPM migration if password sync is disabled.
    prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                       0);
  }
}

void PasswordStoreProxyBackend::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  sync_service->RemoveObserver(this);
  sync_service_ = nullptr;
}

void PasswordStoreProxyBackend::OnRemoteFormChangesReceived(
    CallbackOriginatesFromAndroidBackend originates_from_android,
    RemoteChangesReceived remote_form_changes_received,
    std::optional<PasswordStoreChangeList> changes) {
  // `remote_form_changes_received` is used to inform observers about changes in
  // the backend. This check guarantees observers are informed only about
  // changes in the main backend.
  if (originates_from_android.value() == UsesAndroidBackendAsMainBackend()) {
    remote_form_changes_received.Run(std::move(changes));
  }
}

bool PasswordStoreProxyBackend::UsesAndroidBackendAsMainBackend() {
  CHECK(sync_service_);
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    return false;
  }

  // If there are no passwords in the `LoginDatabase` UPM can be enabled
  // regardless of other factors since if there are no passwords no migration is
  // required.
  if (prefs_->GetBoolean(prefs::kEmptyProfileStoreLoginDatabase)) {
    return true;
  }

  // There are passwords in the `LoginDatabase`. In order to ensure that those
  // passwords are available in the `android_backend_` the user has to not be
  // unrolled and has to have finished the initial migration.
  if (prefs_->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors) ||
      prefs_->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices) == 0) {
    return false;
  }

  return true;
}

void PasswordStoreProxyBackend::MaybeClearBuiltInBackend() {
  CHECK(!password_manager::UsesSplitStoresAndUPMForLocal(prefs_));

  // Don't do anything if password syncing is not enabled.
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    return;
  }

  // Don't do anything if the user didn't complete initial UPM migration or was
  // unenrolled in the past.
  if (prefs_->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices) == 0 ||
      prefs_->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors)) {
    return;
  }

  built_in_backend_->RemoveLoginsCreatedBetweenAsync(
      FROM_HERE, base::Time(), base::Time::Max(),
      base::BindOnce(&RecordPasswordDeletionResult));
}

}  // namespace password_manager
