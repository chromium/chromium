// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"

namespace password_manager {

PasswordStoreAndroidLocalBackend::PasswordStoreAndroidLocalBackend()
    : PasswordStoreAndroidLocalBackend(
          // The local android backend can only be created for the profile
          // store.
          PasswordStoreAndroidBackendBridgeHelper::Create(
              password_manager::kProfileStore),
          std::make_unique<PasswordManagerLifecycleHelperImpl>()) {}

PasswordStoreAndroidLocalBackend::PasswordStoreAndroidLocalBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper)
    : PasswordStoreAndroidBackend(std::move(bridge_helper),
                                  std::move(lifecycle_helper)) {}

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

ActionableError PasswordStoreAndroidLocalBackend::GetError() {
  return last_error();
}

void PasswordStoreAndroidLocalBackend::GetAllLoginsAsync(
    BackendLoginsOrErrorReply callback) {
  GetAllLoginsInternal(std::string(),
                       AdaptLoginsResultCallback(std::move(callback)));
}

void PasswordStoreAndroidLocalBackend::
    GetAllLoginsWithAffiliationAndBrandingAsync(
        BackendLoginsOrErrorReply callback) {
  GetAllLoginsWithAffiliationAndBrandingInternal(
      std::string(), AdaptLoginsResultCallback(std::move(callback)));
}

void PasswordStoreAndroidLocalBackend::GetAutofillableLoginsAsync(
    BackendLoginsOrErrorReply callback) {
  GetAutofillableLoginsInternal(std::string(),
                                AdaptLoginsResultCallback(std::move(callback)));
}

void PasswordStoreAndroidLocalBackend::FillMatchingLoginsAsync(
    BackendLoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  FillMatchingLoginsInternal(std::string(),
                             AdaptLoginsResultCallback(std::move(callback)),
                             include_psl, forms);
}

void PasswordStoreAndroidLocalBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    BackendLoginsOrErrorReply callback) {
  GetGroupedMatchingLoginsInternal(
      std::string(), form_digest,
      AdaptLoginsResultCallback(std::move(callback)));
}

void PasswordStoreAndroidLocalBackend::AddLoginAsync(
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  AddLoginInternal(std::string(), std::move(cred), std::move(callback));
}

void PasswordStoreAndroidLocalBackend::UpdateLoginAsync(
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  UpdateLoginInternal(std::string(), std::move(cred), std::move(callback));
}

void PasswordStoreAndroidLocalBackend::RemoveLoginAsync(
    const base::Location& location,
    StoredCredential cred,
    PasswordChangesOrErrorReply callback) {
  RemoveLoginInternal(std::string(), std::move(cred), std::move(callback));
}

void PasswordStoreAndroidLocalBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  CHECK(!sync_completion);
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

SmartBubbleStatsStore*
PasswordStoreAndroidLocalBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

base::WeakPtr<PasswordStoreBackend>
PasswordStoreAndroidLocalBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreAndroidLocalBackend::RecoverOnError(
    AndroidBackendAPIErrorCode error) {}

std::string PasswordStoreAndroidLocalBackend::GetAccountToRetryOperation() {
  return std::string();
}

PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
PasswordStoreAndroidLocalBackend::GetStorageType() {
  return PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
      kLocal;
}

}  // namespace password_manager
