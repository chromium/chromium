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
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace password_manager {

namespace {

using sync_util::IsSyncFeatureEnabledIncludingPasswords;

bool ShouldErrorResultInFallback(PasswordStoreBackendError error) {
  switch (error.recovery_type) {
    case PasswordStoreBackendErrorRecoveryType::kUnrecoverable:
      return true;
    case PasswordStoreBackendErrorRecoveryType::kRecoverable:
      return false;
  }
}


using MethodName = base::StrongAlias<struct MethodNameTag, std::string>;

void InvokeCallbackWithCombinedStatus(base::OnceCallback<void(bool)> completion,
                                      std::vector<bool> statuses) {
  std::move(completion).Run(base::ranges::all_of(statuses, std::identity()));
}

std::string GetFallbackMetricNameForMethod(const MethodName& method_name) {
  return base::StrCat({"PasswordManager.PasswordStoreProxyBackend.",
                       method_name.value(), ".Fallback"});
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
  NOTREACHED();
}

void PasswordStoreProxyBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  LoginsOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend()) {
    // Lambda is used to reorder |FillMatchingLoginsAsync| arguments so all but
    // the |reply_callback| could be binded.
    auto execute_on_built_in_backend = base::BindOnce(
        [](PasswordStoreBackend* backend, bool include_psl,
           const std::vector<PasswordFormDigest>& forms,
           LoginsOrErrorReply reply_callback) {
          backend->FillMatchingLoginsAsync(std::move(reply_callback),
                                           include_psl, forms);
        },
        base::Unretained(built_in_backend_.get()), include_psl, forms);

    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeFallbackOnOperation<
            LoginsResultOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("FillMatchingLoginsAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->FillMatchingLoginsAsync(std::move(result_callback),
                                          include_psl, forms);
}

void PasswordStoreProxyBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  LoginsOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::GetGroupedMatchingLoginsAsync,
                       base::Unretained(built_in_backend_.get()), form_digest);

    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeFallbackOnOperation<
            LoginsResultOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("GetGroupedMatchingLoginsAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->GetGroupedMatchingLoginsAsync(form_digest,
                                                std::move(result_callback));
}

void PasswordStoreProxyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::AddLoginAsync,
                       base::Unretained(built_in_backend_.get()), form);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeFallbackOnOperation<
            PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("AddLoginAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->AddLoginAsync(form, std::move(result_callback));
}

void PasswordStoreProxyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::UpdateLoginAsync,
                       base::Unretained(built_in_backend_.get()), form);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeFallbackOnOperation<
            PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("UpdateLoginAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->UpdateLoginAsync(form, std::move(result_callback));
}

void PasswordStoreProxyBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  main_backend()->RemoveLoginAsync(form, std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginAsync(form, base::DoNothing());
  }
}

void PasswordStoreProxyBackend::RemoveLoginsByURLAndTimeAsync(
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
      url_filter, delete_begin, delete_end, base::NullCallback(),
      std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginsByURLAndTimeAsync(
        url_filter, std::move(delete_begin), std::move(delete_end),
        base::NullCallback(), base::DoNothing());
  }
}

void PasswordStoreProxyBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  main_backend()->RemoveLoginsCreatedBetweenAsync(delete_begin, delete_end,
                                                  std::move(callback));
  if (UsesAndroidBackendAsMainBackend()) {
    shadow_backend()->RemoveLoginsCreatedBetweenAsync(
        std::move(delete_begin), std::move(delete_end), base::DoNothing());
  }
}

void PasswordStoreProxyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(https://crbug.com/1278807): Implement error handling, when actual
  // store changes will be received from the store.
  main_backend()->DisableAutoSignInForOriginsAsync(origin_filter,
                                                   std::move(completion));
}

SmartBubbleStatsStore* PasswordStoreProxyBackend::GetSmartBubbleStatsStore() {
  return main_backend()->GetSmartBubbleStatsStore();
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
PasswordStoreProxyBackend::CreateSyncControllerDelegate() {
  return built_in_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreProxyBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
  sync_service_->AddObserver(this);
  android_backend_->OnSyncServiceInitialized(sync_service);
  MaybeClearBuiltInBackend();

  if (!IsSyncFeatureEnabledIncludingPasswords(sync_service_)) {
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

template <typename ResultT>
void PasswordStoreProxyBackend::MaybeFallbackOnOperation(
    base::OnceCallback<void(base::OnceCallback<void(ResultT)> callback)>
        fallback_callback,
    const MethodName& method_name,
    base::OnceCallback<void(ResultT)> result_callback,
    ResultT result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result) &&
      ShouldErrorResultInFallback(
          absl::get<PasswordStoreBackendError>(result))) {
    base::UmaHistogramBoolean(GetFallbackMetricNameForMethod(method_name),
                              true);
    std::move(fallback_callback).Run(std::move(result_callback));
  } else {
    std::move(result_callback).Run(std::move(result));
  }
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
  if (!IsSyncFeatureEnabledIncludingPasswords(sync_service_)) {
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
  if (!IsSyncFeatureEnabledIncludingPasswords(sync_service_)) {
    return false;
  }

  bool is_unenrolled =
      prefs_->GetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);

  if (!base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncOnlyInGMSCore)) {
    // If M4 feature flag is disabled use `android_backend` as long as there was
    // no unenrollment.
    return !is_unenrolled;
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
  if (is_unenrolled ||
      prefs_->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices) == 0) {
    return false;
  }

  return true;
}

void PasswordStoreProxyBackend::MaybeClearBuiltInBackend() {
  CHECK(!password_manager::UsesSplitStoresAndUPMForLocal(prefs_));

  // Don't do anything if `kUnifiedPasswordManagerSyncOnlyInGMSCore` feature is
  // not enabled.
  if (!base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncOnlyInGMSCore)) {
    return;
  }

  // Don't do anything if password syncing is not enabled.
  if (!IsSyncFeatureEnabledIncludingPasswords(sync_service_)) {
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

  if (base::FeatureList::IsEnabled(features::kClearLoginDatabaseForUPMUsers)) {
    built_in_backend_->RemoveLoginsCreatedBetweenAsync(
        base::Time(), base::Time::Max(),
        base::BindOnce(&RecordPasswordDeletionResult));
  }
}

}  // namespace password_manager
