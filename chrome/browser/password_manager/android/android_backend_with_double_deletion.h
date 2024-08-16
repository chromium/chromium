// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ANDROID_BACKEND_WITH_DOUBLE_DELETION_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ANDROID_BACKEND_WITH_DOUBLE_DELETION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"

// This backend is created only when user enabled split store. It redirects all
// calls to `android_backend`. But password deletions happen on
// `built_in_backend` AND `android_backend`.
class AndroidBackendWithDoubleDeletion final
    : public password_manager::PasswordStoreBackend {
 public:
  AndroidBackendWithDoubleDeletion(
      std::unique_ptr<PasswordStoreBackend> built_in_backend,
      std::unique_ptr<PasswordStoreBackend> android_backend);
  AndroidBackendWithDoubleDeletion(const AndroidBackendWithDoubleDeletion&) =
      delete;
  AndroidBackendWithDoubleDeletion(AndroidBackendWithDoubleDeletion&&) = delete;
  AndroidBackendWithDoubleDeletion& operator=(
      const AndroidBackendWithDoubleDeletion&) = delete;
  AndroidBackendWithDoubleDeletion& operator=(
      AndroidBackendWithDoubleDeletion&&) = delete;
  ~AndroidBackendWithDoubleDeletion() override;

 private:
  // Implements PasswordStoreBackend interface.
  void InitBackend(
      password_manager::AffiliatedMatchHelper* affiliated_match_helper,
      RemoteChangesReceived remote_form_changes_received,
      base::RepeatingClosure sync_enabled_or_disabled_cb,
      base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  bool IsAbleToSavePasswords() override;
  void GetAllLoginsAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(
      std::string account,
      password_manager::LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      password_manager::LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<password_manager::PasswordFormDigest>& forms) override;
  void GetGroupedMatchingLoginsAsync(
      const password_manager::PasswordFormDigest& form_digest,
      password_manager::LoginsOrErrorReply callback) override;
  void AddLoginAsync(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(
      const base::Location& location,
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  password_manager::SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  base::WeakPtr<password_manager::PasswordStoreBackend> AsWeakPtr() override;

  std::unique_ptr<password_manager::PasswordStoreBackend> built_in_backend_;
  std::unique_ptr<password_manager::PasswordStoreBackend> android_backend_;

  base::WeakPtrFactory<AndroidBackendWithDoubleDeletion> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ANDROID_BACKEND_WITH_DOUBLE_DELETION_H_
