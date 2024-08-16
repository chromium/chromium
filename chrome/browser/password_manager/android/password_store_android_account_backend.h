// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_ACCOUNT_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_ACCOUNT_BACKEND_H_

#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

class AffiliatedMatchHelper;
class PasswordAffiliationSourceAdapter;

// This class processes passwords only from an account.
class PasswordStoreAndroidAccountBackend : public PasswordStoreBackend,
                                           public PasswordStoreAndroidBackend {
 public:
  // `is_account_store` allows to control whether the backend is used by profile
  // or account password store.
  PasswordStoreAndroidAccountBackend(
      PrefService* prefs,
      PasswordAffiliationSourceAdapter* password_affiliation_adapter,
      password_manager::IsAccountStore is_account_store);

  PasswordStoreAndroidAccountBackend(
      base::PassKey<class PasswordStoreAndroidAccountBackendTest>,
      std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
      std::unique_ptr<PasswordSyncControllerDelegateAndroid>
          sync_controller_delegate,
      PrefService* prefs,
      PasswordAffiliationSourceAdapter* password_affiliation_adapter);
  ~PasswordStoreAndroidAccountBackend() override;

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

  // If |forms_or_error| contains forms, it retrieves and fills in affiliation
  // and branding information for Android credentials in the forms and invokes
  // |callback| with the result. If an error was received instead, it directly
  // invokes |callback| with it, as no forms could be fetched. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(
      LoginsOrErrorReply callback,
      LoginsResultOrError forms_or_error);

  // Called when password sync flips from disabled to enabled and vice-versa.
  // If the sync status changes, all pending jobs should be replied to
  // with the corresponding error since their results are no longer relevant.
  // This should also interrupt retry chains or other pre-scheduled
  // chains of calls to the store.
  void OnPasswordsSyncStateChanged();

  // Clears |sync_service_| when syncer::SyncServiceObserver::OnSyncShutdown is
  // called.
  void SyncShutdown();

  const raw_ptr<PasswordAffiliationSourceAdapter> password_affiliation_adapter_;
  raw_ptr<AffiliatedMatchHelper> affiliated_match_helper_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Legacy delegate to handle sync events.
  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
      sync_controller_delegate_;

  bool should_disable_saving_due_to_error_ = false;

  base::RepeatingClosure sync_enabled_or_disabled_cb_;

  base::WeakPtrFactory<PasswordStoreAndroidAccountBackend> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_ACCOUNT_BACKEND_H_
