// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"

#include "base/android/build_info.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_store/password_data_type_controller_delegate_android.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

namespace {

constexpr char kUPMActiveHistogram[] =
    "PasswordManager.UnifiedPasswordManager.ActiveStatus2";

std::string GetSyncingAccount(const syncer::SyncService* sync_service) {
  CHECK(sync_service);
  return password_manager::sync_util::HasChosenToSyncPasswords(sync_service)
             ? sync_service->GetAccountInfo().email
             : std::string();
}

void LogUPMActiveStatus(syncer::SyncService* sync_service, PrefService* prefs) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service)) {
    base::UmaHistogramEnumeration(
        kUPMActiveHistogram,
        UnifiedPasswordManagerActiveStatus::kInactiveSyncOff);
    return;
  }

  if (password_manager_upm_eviction::IsCurrentUserEvicted(prefs)) {
    base::UmaHistogramEnumeration(
        kUPMActiveHistogram,
        UnifiedPasswordManagerActiveStatus::kInactiveUnenrolledDueToErrors);
    return;
  }

  base::UmaHistogramEnumeration(kUPMActiveHistogram,
                                UnifiedPasswordManagerActiveStatus::kActive);
}

template <typename Response, typename CallbackType>
void ReplyWithEmptyList(CallbackType callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Response()));
}

}  // namespace

PasswordStoreAndroidAccountBackend::PasswordStoreAndroidAccountBackend(
    PrefService* prefs,
    PasswordAffiliationSourceAdapter* password_affiliation_adapter,
    password_manager::IsAccountStore is_account_store)
    : PasswordStoreAndroidBackend(
          PasswordStoreAndroidBackendBridgeHelper::Create(is_account_store),
          std::make_unique<PasswordManagerLifecycleHelperImpl>(),
          prefs),
      password_affiliation_adapter_(password_affiliation_adapter) {
  sync_controller_delegate_ =
      std::make_unique<PasswordSyncControllerDelegateAndroid>(
          std::make_unique<PasswordSyncControllerDelegateBridgeImpl>());
  sync_controller_delegate_->SetSyncObserverCallbacks(
      base::BindRepeating(
          &PasswordStoreAndroidAccountBackend::OnPasswordsSyncStateChanged,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordStoreAndroidAccountBackend::SyncShutdown,
                     weak_ptr_factory_.GetWeakPtr()));
}

PasswordStoreAndroidAccountBackend::PasswordStoreAndroidAccountBackend(
    base::PassKey<class PasswordStoreAndroidAccountBackendTest> key,
    std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
    std::unique_ptr<PasswordSyncControllerDelegateAndroid>
        sync_controller_delegate,
    PrefService* prefs,
    PasswordAffiliationSourceAdapter* password_affiliation_adapter)
    : PasswordStoreAndroidBackend(std::move(bridge_helper),
                                  std::move(lifecycle_helper),
                                  prefs),
      password_affiliation_adapter_(password_affiliation_adapter) {
  sync_controller_delegate_ = std::move(sync_controller_delegate);
  sync_controller_delegate_->SetSyncObserverCallbacks(
      base::BindRepeating(
          &PasswordStoreAndroidAccountBackend::OnPasswordsSyncStateChanged,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordStoreAndroidAccountBackend::SyncShutdown,
                     weak_ptr_factory_.GetWeakPtr()));
}

PasswordStoreAndroidAccountBackend::~PasswordStoreAndroidAccountBackend() =
    default;

void PasswordStoreAndroidAccountBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  Init(std::move(remote_form_changes_received));
  CHECK(completion);
  affiliated_match_helper_ = affiliated_match_helper;
  sync_enabled_or_disabled_cb_ = std::move(sync_enabled_or_disabled_cb);
  std::move(completion).Run(/*success*/ true);
}

void PasswordStoreAndroidAccountBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  affiliated_match_helper_ = nullptr;
  sync_service_ = nullptr;
  PasswordStoreAndroidBackend::Shutdown(std::move(shutdown_completed));
}

bool PasswordStoreAndroidAccountBackend::IsAbleToSavePasswords() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordSavingDisabledDueToGMSCoreError",
      should_disable_saving_due_to_error_);
  return sync_service_ != nullptr && !should_disable_saving_due_to_error_;
}

void PasswordStoreAndroidAccountBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<LoginsResult>(std::move(callback));
    return;
  }
  GetAllLoginsInternal(GetSyncingAccount(sync_service_), std::move(callback));
}

void PasswordStoreAndroidAccountBackend::
    GetAllLoginsWithAffiliationAndBrandingAsync(LoginsOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<LoginsResult>(std::move(callback));
    return;
  }
  if (bridge_helper()->CanUseGetAllLoginsWithBrandingInfoAPI()) {
    GetAllLoginsWithAffiliationAndBrandingInternal(
        GetSyncingAccount(sync_service_), std::move(callback));
    return;
  }
  auto affiliation_injection =
      base::BindOnce(&PasswordStoreAndroidAccountBackend::
                         InjectAffiliationAndBrandingInformation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  GetAllLoginsInternal(GetSyncingAccount(sync_service_),
                       std::move(affiliation_injection));
}

void PasswordStoreAndroidAccountBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<LoginsResult>(std::move(callback));
    return;
  }
  GetAutofillableLoginsInternal(GetSyncingAccount(sync_service_),
                                std::move(callback));
}

void PasswordStoreAndroidAccountBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  CHECK(!account.empty());
  // This method is only used before the store split, to migrate non-syncable
  // data back to the built-in backend after password sync turns off.
  CHECK(!password_manager::UsesSplitStoresAndUPMForLocal(prefs()));
  GetAllLoginsInternal(std::move(account), std::move(callback));
}

void PasswordStoreAndroidAccountBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<LoginsResult>(std::move(callback));
    return;
  }
  FillMatchingLoginsInternal(GetSyncingAccount(sync_service_),
                             std::move(callback), include_psl, forms);
}

void PasswordStoreAndroidAccountBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<LoginsResult>(std::move(callback));
    return;
  }
  if (bridge_helper()->CanUseGetAffiliatedPasswordsAPI()) {
    GetGroupedMatchingLoginsInternal(GetSyncingAccount(sync_service_),
                                     form_digest, std::move(callback));
    return;
  }

  GetLoginsWithAffiliationsRequestHandler(
      form_digest, this, affiliated_match_helper_.get(), std::move(callback));
}

void PasswordStoreAndroidAccountBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  CHECK(password_manager::sync_util::HasChosenToSyncPasswords(sync_service_));
  AddLoginInternal(GetSyncingAccount(sync_service_), form, std::move(callback));
}

void PasswordStoreAndroidAccountBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  CHECK(password_manager::sync_util::HasChosenToSyncPasswords(sync_service_));
  UpdateLoginInternal(GetSyncingAccount(sync_service_), form,
                      std::move(callback));
}

void PasswordStoreAndroidAccountBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<PasswordStoreChangeList>(std::move(callback));
    return;
  }
  RemoveLoginInternal(GetSyncingAccount(sync_service_), form,
                      std::move(callback));
}

void PasswordStoreAndroidAccountBackend::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<PasswordStoreChangeList>(std::move(callback));
    return;
  }
  RemoveLoginsByURLAndTimeInternal(GetSyncingAccount(sync_service_), url_filter,
                                   delete_begin, delete_end,
                                   std::move(callback));
}

void PasswordStoreAndroidAccountBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service_)) {
    ReplyWithEmptyList<PasswordStoreChangeList>(std::move(callback));
    return;
  }
  RemoveLoginsCreatedBetweenInternal(GetSyncingAccount(sync_service_),
                                     delete_begin, delete_end,
                                     std::move(callback));
}

void PasswordStoreAndroidAccountBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  CHECK(password_manager::sync_util::HasChosenToSyncPasswords(sync_service_));
  DisableAutoSignInForOriginsInternal(GetSyncingAccount(sync_service_),
                                      origin_filter, std::move(completion));
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreAndroidAccountBackend::CreateSyncControllerDelegate() {
  return std::make_unique<PasswordDataTypeControllerDelegateAndroid>();
}

SmartBubbleStatsStore*
PasswordStoreAndroidAccountBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

base::WeakPtr<PasswordStoreBackend>
PasswordStoreAndroidAccountBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreAndroidAccountBackend::RecoverOnError(
    AndroidBackendAPIErrorCode error) {
  CHECK(sync_service_);
  if (error == AndroidBackendAPIErrorCode::kPassphraseRequired) {
    sync_service_->SendExplicitPassphraseToPlatformClient();
  }
  should_disable_saving_due_to_error_ = true;
}

void PasswordStoreAndroidAccountBackend::OnCallToGMSCoreSucceeded() {
  // Since the API call has succeeded, it's safe to reenable saving.
  should_disable_saving_due_to_error_ = false;
}

std::string PasswordStoreAndroidAccountBackend::GetAccountToRetryOperation() {
  CHECK(sync_service_);
  return GetSyncingAccount(sync_service_);
}

PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
PasswordStoreAndroidAccountBackend::GetStorageType() {
  return PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
      kAccount;
}

void PasswordStoreAndroidAccountBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  // TODO(crbug.com/40847054) Check if this might be called multiple times
  // without a need for it. If it is don't repeatedly initialize the sync
  // service to make it clear that it's not needed to do so for future readers
  // of the code.
  if (!sync_service_) {
    LogUPMActiveStatus(sync_service, prefs());
  }
  sync_service_ = sync_service;
  sync_controller_delegate_->OnSyncServiceInitialized(sync_service);

  // Stop fetching affiliations if AndroidBackend can be used and branding info
  // can be obtained directly from the GMS Core backend.
  if (!prefs()->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors) &&
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service_) &&
      bridge_helper()->CanUseGetAllLoginsWithBrandingInfoAPI() &&
      password_affiliation_adapter_) {
    password_affiliation_adapter_->DisableSource();
  }
}

void PasswordStoreAndroidAccountBackend::
    RecordAddLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.AccountBackend.AddLoginCalledOnStore",
      true);
}

void PasswordStoreAndroidAccountBackend::
    RecordUpdateLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.AccountBackend.UpdateLoginCalledOnStore",
      true);
}

void PasswordStoreAndroidAccountBackend::
    InjectAffiliationAndBrandingInformation(
        LoginsOrErrorReply callback,
        LoginsResultOrError forms_or_error) {
  if (!affiliated_match_helper_ ||
      absl::holds_alternative<PasswordStoreBackendError>(forms_or_error) ||
      absl::get<LoginsResult>(forms_or_error).empty()) {
    std::move(callback).Run(std::move(forms_or_error));
    return;
  }
  affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
      std::move(absl::get<LoginsResult>(forms_or_error)), std::move(callback));
}

void PasswordStoreAndroidAccountBackend::OnPasswordsSyncStateChanged() {
  // Invoke `sync_enabled_or_disabled_cb_` only if M4 feature flag is enabled
  // since Chrome no longer actively syncs passwords post M4.
  if (sync_enabled_or_disabled_cb_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, sync_enabled_or_disabled_cb_);
  }

  // Reply with a recoverable error, because this isn't a persistent issue,
  // only a transient state
  ClearAllTasksAndReplyWithReason(
      AndroidBackendError(
          AndroidBackendErrorType::kCancelledPwdSyncStateChanged),
      PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized));
}

void PasswordStoreAndroidAccountBackend::SyncShutdown() {
  ClearAllTasksAndReplyWithReason(
      AndroidBackendError(
          AndroidBackendErrorType::kCancelledPwdSyncStateChanged),
      PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized));
  sync_service_ = nullptr;
}

}  // namespace password_manager
