// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_empty_backend.h"

#include "components/password_manager/core/browser/password_store/password_data_type_controller_delegate_android.h"

namespace password_manager {

namespace {
template <typename Response, typename CallbackType>
void ReplyWithEmptyList(CallbackType callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Response()));
}
}  // namespace

PasswordStoreEmptyBackend::PasswordStoreEmptyBackend() {}

PasswordStoreEmptyBackend::~PasswordStoreEmptyBackend() {}

void PasswordStoreEmptyBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  std::move(completion).Run(/*success*/ true);
}

void PasswordStoreEmptyBackend::Shutdown(base::OnceClosure shutdown_completed) {
  std::move(shutdown_completed).Run();
}

bool PasswordStoreEmptyBackend::IsAbleToSavePasswords() {
  return false;
}

void PasswordStoreEmptyBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  ReplyWithEmptyList<LoginsResult>(std::move(callback));
}

void PasswordStoreEmptyBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    LoginsOrErrorReply callback) {
  ReplyWithEmptyList<LoginsResult>(std::move(callback));
}

void PasswordStoreEmptyBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  ReplyWithEmptyList<LoginsResult>(std::move(callback));
}

void PasswordStoreEmptyBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  ReplyWithEmptyList<LoginsResult>(std::move(callback));
}

void PasswordStoreEmptyBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  ReplyWithEmptyList<LoginsResult>(std::move(callback));
}

void PasswordStoreEmptyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  NOTREACHED() << "The empty backend isn't able to save passwords.";
}

void PasswordStoreEmptyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  NOTREACHED() << "The empty backend isn't able to save passwords.";
}

void PasswordStoreEmptyBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  // There is no way to get a form from this backend to call
  // `RemoveLoginAsync`, because it's an empty backend.
  NOTREACHED() << "The empty backend doesn't store any data.";
}

void PasswordStoreEmptyBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  // This is used by "Delete Browsing Data", which doesn't have a way to know
  // that the store is backend by an empty backend.
  ReplyWithEmptyList<PasswordStoreChangeList>(std::move(callback));
}

void PasswordStoreEmptyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // Technically there is no auto sign-in enabled, since there is nothing stored
  // so it's safe to just invoke `completion` here.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(completion));
}

SmartBubbleStatsStore* PasswordStoreEmptyBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreEmptyBackend::CreateSyncControllerDelegate() {
  return std::make_unique<PasswordDataTypeControllerDelegateAndroid>();
}

void PasswordStoreEmptyBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {}

base::WeakPtr<PasswordStoreBackend> PasswordStoreEmptyBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
