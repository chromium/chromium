// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_LOCAL_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_LOCAL_BACKEND_H_

#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"

namespace syncer {
class SyncService;
class DataTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class AffiliatedMatchHelper;
class PasswordAffiliationSourceAdapter;

// This class processes passwords stored in local storage (not associated to any
// account).
class PasswordStoreAndroidLocalBackend : public PasswordStoreBackend,
                                         public PasswordStoreAndroidBackend {
 public:
  PasswordStoreAndroidLocalBackend(
      PrefService* prefs,
      PasswordAffiliationSourceAdapter& password_affiliation_adapter);

  // Only for testing.
  PasswordStoreAndroidLocalBackend(
      std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
      PrefService* prefs,
      PasswordAffiliationSourceAdapter& password_affiliation_adapter);
  ~PasswordStoreAndroidLocalBackend() override;

  // PasswordStoreBackend implementation.
  void InitBackend(AffiliatedMatchHelper* affiliated_match_helper,
                   RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  bool IsAbleToSavePasswords() override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(std::string account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void GetGroupedMatchingLoginsAsync(const PasswordFormDigest& form_digest,
                                     LoginsOrErrorReply callback) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const base::Location& location,
                        const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

 private:
  // PasswordStoreAndroidBackend implementation.
  void RecoverOnError(AndroidBackendAPIErrorCode error) override;
  void OnCallToGMSCoreSucceeded() override;
  std::string GetAccountToRetryOperation() override;
  PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
  GetStorageType() override;

  bool should_disable_saving_due_to_error_ = false;

  base::WeakPtrFactory<PasswordStoreAndroidLocalBackend> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_LOCAL_BACKEND_H_
