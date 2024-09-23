// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace password_manager {

// This backend forwards requests to two backends in order to compare and record
// their results and time differences. The main backend fulfills the  request
// while the shadow backend is only queried to record shadow traffic.
class PasswordStoreProxyBackend final : public PasswordStoreBackend,
                                        public syncer::SyncServiceObserver {
 public:
  // `built_in_backend` and `android_backend` must not be null and must outlive
  // this object as long as Shutdown() is not called.
  PasswordStoreProxyBackend(
      std::unique_ptr<PasswordStoreBackend> built_in_backend,
      std::unique_ptr<PasswordStoreBackend> android_backend,
      PrefService* prefs);
  PasswordStoreProxyBackend(const PasswordStoreProxyBackend&) = delete;
  PasswordStoreProxyBackend(PasswordStoreProxyBackend&&) = delete;
  PasswordStoreProxyBackend& operator=(const PasswordStoreProxyBackend&) =
      delete;
  PasswordStoreProxyBackend& operator=(PasswordStoreProxyBackend&&) = delete;
  ~PasswordStoreProxyBackend() override;

 private:
  using CallbackOriginatesFromAndroidBackend =
      base::StrongAlias<struct CallbackOriginatesFromAndroidBackendTag, bool>;

  // Implements PasswordStoreBackend interface.
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
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // Forwards the (possible) forms changes caused by a remote event to the
  // main backend.
  void OnRemoteFormChangesReceived(
      CallbackOriginatesFromAndroidBackend originatesFromAndroid,
      RemoteChangesReceived remote_form_changes_received,
      std::optional<PasswordStoreChangeList> changes);

  // Helper used to determine on which backend to run operations.
  bool UsesAndroidBackendAsMainBackend();

  // Clears all passwords from `built_in_backend_` if all conditions bellow are
  // satisfied:
  // - Password sync is enabled
  // - initial UPM migration was finished and there was no unenrollment
  void MaybeClearBuiltInBackend();

  PasswordStoreBackend* main_backend();
  PasswordStoreBackend* shadow_backend();

  std::unique_ptr<PasswordStoreBackend> built_in_backend_;
  std::unique_ptr<PasswordStoreBackend> android_backend_;
  raw_ptr<PrefService> const prefs_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  base::WeakPtrFactory<PasswordStoreProxyBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_
