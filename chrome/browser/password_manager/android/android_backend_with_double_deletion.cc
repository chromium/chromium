// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/android_backend_with_double_deletion.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace {

using password_manager::LoginsOrErrorReply;
using password_manager::PasswordChangesOrErrorReply;
using password_manager::PasswordForm;
using password_manager::PasswordFormDigest;

}  // namespace

AndroidBackendWithDoubleDeletion::AndroidBackendWithDoubleDeletion(
    std::unique_ptr<PasswordStoreBackend> built_in_backend,
    std::unique_ptr<PasswordStoreBackend> android_backend)
    : built_in_backend_(std::move(built_in_backend)),
      android_backend_(std::move(android_backend)) {}

AndroidBackendWithDoubleDeletion::~AndroidBackendWithDoubleDeletion() = default;

void AndroidBackendWithDoubleDeletion::InitBackend(
    password_manager::AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  // `sync_enabled_or_disabled_cb` can be invoked by `built_in_backend_` only if
  // M4 feature flag is disabled. After M4 is enabled LoginDatabase has no
  // connection to the sync engine, preventing any internal changes on sync
  // events.
  built_in_backend_->InitBackend(affiliated_match_helper, base::DoNothing(),
                                 sync_enabled_or_disabled_cb,
                                 base::DoNothing());

  // `sync_enabled_or_disabled_cb` can be invoked by `android_backend_` only if
  // M4 feature flag is enabled and the `android_backend_` is an account
  // backend. The callback is invoked when sync status changes are detected.
  android_backend_->InitBackend(
      affiliated_match_helper, std::move(remote_form_changes_received),
      std::move(sync_enabled_or_disabled_cb), std::move(completion));
}

void AndroidBackendWithDoubleDeletion::Shutdown(
    base::OnceClosure shutdown_completed) {
  weak_ptr_factory_.InvalidateWeakPtrs();
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

bool AndroidBackendWithDoubleDeletion::IsAbleToSavePasswords() {
  return android_backend_->IsAbleToSavePasswords();
}
void AndroidBackendWithDoubleDeletion::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  android_backend_->GetAllLoginsAsync(std::move(callback));
}

void AndroidBackendWithDoubleDeletion::
    GetAllLoginsWithAffiliationAndBrandingAsync(LoginsOrErrorReply callback) {
  android_backend_->GetAllLoginsWithAffiliationAndBrandingAsync(
      std::move(callback));
}

void AndroidBackendWithDoubleDeletion::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  android_backend_->GetAutofillableLoginsAsync(std::move(callback));
}

void AndroidBackendWithDoubleDeletion::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void AndroidBackendWithDoubleDeletion::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  android_backend_->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                            forms);
}

void AndroidBackendWithDoubleDeletion::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  android_backend_->GetGroupedMatchingLoginsAsync(form_digest,
                                                  std::move(callback));
}

void AndroidBackendWithDoubleDeletion::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  android_backend_->AddLoginAsync(form, std::move(callback));
}

void AndroidBackendWithDoubleDeletion::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  android_backend_->UpdateLoginAsync(form, std::move(callback));
}

void AndroidBackendWithDoubleDeletion::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  android_backend_->RemoveLoginAsync(location, form, std::move(callback));
  built_in_backend_->RemoveLoginAsync(location, form, base::DoNothing());
}

void AndroidBackendWithDoubleDeletion::RemoveLoginsByURLAndTimeAsync(
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
  android_backend_->RemoveLoginsByURLAndTimeAsync(
      location, url_filter, delete_begin, delete_end, base::NullCallback(),
      std::move(callback));
  built_in_backend_->RemoveLoginsByURLAndTimeAsync(
      location, url_filter, delete_begin, delete_end, base::NullCallback(),
      base::DoNothing());
}

void AndroidBackendWithDoubleDeletion::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  android_backend_->RemoveLoginsCreatedBetweenAsync(
      location, delete_begin, delete_end, std::move(callback));
  built_in_backend_->RemoveLoginsCreatedBetweenAsync(
      location, delete_begin, delete_end, base::DoNothing());
}

void AndroidBackendWithDoubleDeletion::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  android_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                     std::move(completion));
}

password_manager::SmartBubbleStatsStore*
AndroidBackendWithDoubleDeletion::GetSmartBubbleStatsStore() {
  return android_backend_->GetSmartBubbleStatsStore();
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
AndroidBackendWithDoubleDeletion::CreateSyncControllerDelegate() {
  return built_in_backend_->CreateSyncControllerDelegate();
}

void AndroidBackendWithDoubleDeletion::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  android_backend_->OnSyncServiceInitialized(sync_service);
}

void AndroidBackendWithDoubleDeletion::RecordAddLoginAsyncCalledFromTheStore() {
  android_backend_->RecordAddLoginAsyncCalledFromTheStore();
}

void AndroidBackendWithDoubleDeletion::
    RecordUpdateLoginAsyncCalledFromTheStore() {
  android_backend_->RecordUpdateLoginAsyncCalledFromTheStore();
}

base::WeakPtr<password_manager::PasswordStoreBackend>
AndroidBackendWithDoubleDeletion::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
