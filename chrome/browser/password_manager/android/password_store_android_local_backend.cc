// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"

#include "base/android/build_info.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

PasswordStoreAndroidLocalBackend::PasswordStoreAndroidLocalBackend(
    PrefService* prefs,
    PasswordAffiliationSourceAdapter& password_affiliation_adapter)
    : PasswordStoreAndroidLocalBackend(
          // The local android backend can only be created for the profile
          // store.
          PasswordStoreAndroidBackendBridgeHelper::Create(
              password_manager::kProfileStore),
          std::make_unique<PasswordManagerLifecycleHelperImpl>(),
          prefs,
          password_affiliation_adapter) {}

PasswordStoreAndroidLocalBackend::PasswordStoreAndroidLocalBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
    PrefService* prefs,
    PasswordAffiliationSourceAdapter& password_affiliation_adapter)
    : PasswordStoreAndroidBackend(std::move(bridge_helper),
                                  std::move(lifecycle_helper),
                                  prefs) {
  // AccountBackend doesn't call `DisableSource` when sync is turned off.
  // This is why we have to explicitly call it here whenever local GMSCore is
  // created.
  password_affiliation_adapter.DisableSource();
}

PasswordStoreAndroidLocalBackend::~PasswordStoreAndroidLocalBackend() = default;

void PasswordStoreAndroidLocalBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  Init(std::move(remote_form_changes_received));
  CHECK(completion);
  std::move(completion).Run(/*success=*/true);
}

void PasswordStoreAndroidLocalBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  PasswordStoreAndroidBackend::Shutdown(std::move(shutdown_completed));
}

bool PasswordStoreAndroidLocalBackend::IsAbleToSavePasswords() {
  return !should_disable_saving_due_to_error_;
}

void PasswordStoreAndroidLocalBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  GetAllLoginsInternal(std::string(), std::move(callback));
}

void PasswordStoreAndroidLocalBackend::
    GetAllLoginsWithAffiliationAndBrandingAsync(LoginsOrErrorReply callback) {
  GetAllLoginsWithAffiliationAndBrandingInternal(std::string(),
                                                 std::move(callback));
}

void PasswordStoreAndroidLocalBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  GetAutofillableLoginsInternal(std::string(), std::move(callback));
}

void PasswordStoreAndroidLocalBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED_IN_MIGRATION();
}

void PasswordStoreAndroidLocalBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  FillMatchingLoginsInternal(std::string(), std::move(callback), include_psl,
                             forms);
}

void PasswordStoreAndroidLocalBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  GetGroupedMatchingLoginsInternal(std::string(), form_digest,
                                   std::move(callback));
}

void PasswordStoreAndroidLocalBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  AddLoginInternal(std::string(), form, std::move(callback));
}

void PasswordStoreAndroidLocalBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  UpdateLoginInternal(std::string(), form, std::move(callback));
}

void PasswordStoreAndroidLocalBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  RemoveLoginInternal(std::string(), form, std::move(callback));
}

void PasswordStoreAndroidLocalBackend::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  RemoveLoginsByURLAndTimeInternal(std::string(), url_filter, delete_begin,
                                   delete_end, std::move(callback));
}

void PasswordStoreAndroidLocalBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  RemoveLoginsCreatedBetweenInternal(std::string(), delete_begin, delete_end,
                                     std::move(callback));
}

void PasswordStoreAndroidLocalBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DisableAutoSignInForOriginsInternal(std::string(), origin_filter,
                                      std::move(completion));
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreAndroidLocalBackend::CreateSyncControllerDelegate() {
  return nullptr;
}

void PasswordStoreAndroidLocalBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {}

void PasswordStoreAndroidLocalBackend::RecordAddLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.LocalBackend.AddLoginCalledOnStore", true);
}

void PasswordStoreAndroidLocalBackend::
    RecordUpdateLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.LocalBackend.UpdateLoginCalledOnStore",
      true);
}

SmartBubbleStatsStore*
PasswordStoreAndroidLocalBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

base::WeakPtr<PasswordStoreBackend>
PasswordStoreAndroidLocalBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreAndroidLocalBackend::RecoverOnError(
    AndroidBackendAPIErrorCode error) {
  should_disable_saving_due_to_error_ = true;
}

void PasswordStoreAndroidLocalBackend::OnCallToGMSCoreSucceeded() {
  // Since the API call has succeeded, it's safe to reenable saving.
  should_disable_saving_due_to_error_ = false;
}

std::string PasswordStoreAndroidLocalBackend::GetAccountToRetryOperation() {
  return std::string();
}

PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
PasswordStoreAndroidLocalBackend::GetStorageType() {
  return PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
      kLocal;
}

}  // namespace password_manager
