// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace password_manager {

// This backend forwards requests to two backends in order to compare and record
// their results and time differences. The main backend fulfills the  request
// while the shadow backend is only queried to record shadow traffic.
class PasswordStoreProxyBackend : public PasswordStoreBackend {
 public:
  // `built_in_backend` and `android_backend` must not be null and must outlive
  // this object as long as Shutdown() is not called.
  PasswordStoreProxyBackend(PasswordStoreBackend* built_in_backend,
                            PasswordStoreBackend* android_backend,
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
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
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
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  // Forwards the (possible) forms changes caused by a remote event to the
  // main backend.
  void OnRemoteFormChangesReceived(
      CallbackOriginatesFromAndroidBackend originatesFromAndroid,
      RemoteChangesReceived remote_form_changes_received,
      absl::optional<PasswordStoreChangeList> changes);

  // Forwards sync status changes by the backend facilitating them.
  void OnSyncEnabledOrDisabled(
      CallbackOriginatesFromAndroidBackend originatesFromAndroid,
      base::RepeatingClosure sync_enabled_or_disabled_cb);

  // Helper used to determine main *and* shadow backends. Some UPM experiment
  // groups use shadow traffic to compare the two backends, other may need it
  // to execute login deletions on both backends, to avoid recovery of deleted
  // data.
  bool UsesAndroidBackendAsMainBackend();

  // Retries to execute operation on |built_in_backend| in case of an
  // unrecoverable error inside |android_backend|. |retry_callback| is the
  // pending operation with binded parameters, |result_callback| is the original
  // operation callback.
  // |ResultT| is the resulting type of the backend operation that will be
  // passed to the result callback. Could be either |LoginsResultOrError| or
  // |PasswordChangesOrError|.
  template <typename ResultT>
  void MaybeFallbackOnOperation(
      base::OnceCallback<void(base::OnceCallback<void(ResultT)> callback)>
          retry_callback,
      const base::StrongAlias<struct MethodNameTag, std::string>& method_name,
      base::OnceCallback<void(ResultT)> result_callback,
      ResultT result);

  PasswordStoreBackend* main_backend();
  PasswordStoreBackend* shadow_backend();

  const raw_ptr<PasswordStoreBackend> built_in_backend_;
  const raw_ptr<PasswordStoreBackend> android_backend_;
  raw_ptr<PrefService> const prefs_ = nullptr;
  raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

  base::WeakPtrFactory<PasswordStoreProxyBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_PROXY_BACKEND_H_
