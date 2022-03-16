// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class PasswordSyncControllerDelegateAndroid
    : public syncer::ModelTypeControllerDelegate,
      public PasswordSyncControllerDelegateBridge::Consumer {
 public:
  PasswordSyncControllerDelegateAndroid(
      std::unique_ptr<PasswordSyncControllerDelegateBridge> bridge,
      PasswordStoreBackend::SyncDelegate* sync_delegate);
  PasswordSyncControllerDelegateAndroid(
      const PasswordSyncControllerDelegateAndroid&) = delete;
  PasswordSyncControllerDelegateAndroid(
      PasswordSyncControllerDelegateAndroid&&) = delete;
  PasswordSyncControllerDelegateAndroid& operator=(
      const PasswordSyncControllerDelegateAndroid&) = delete;
  PasswordSyncControllerDelegateAndroid& operator=(
      PasswordSyncControllerDelegateAndroid&&) = delete;
  ~PasswordSyncControllerDelegateAndroid() override;

  // syncer::ModelTypeControllerDelegate implementation
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // PasswordStoreAndroidBackendBridge::Consumer implementation.
  void OnCredentialManagerNotified() override;
  void OnCredentialManagerError(const AndroidBackendError& error,
                                int api_error_code) override;

  // Notifies CredentialManager to use syncing account.
  void NotifyCredentialManagerWhenSyncing();

  // Notifies CredentialManager that passwords are not synced.
  void NotifyCredentialManagerWhenNotSyncing();

  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateProxyModelControllerDelegate();

 private:
  using IsSyncEnabled = base::StrongAlias<struct IsSyncEnabledTag, bool>;

  // Updates |is_sync_enabled| and |syncing_account| to hold the actual syncing
  // status and syncing account. Must be called only after sync service was
  // instantiated.
  void UpdateSyncStatusOnStartUp();

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtrToBaseClass();

  const std::unique_ptr<PasswordSyncControllerDelegateBridge> bridge_;

  raw_ptr<PasswordStoreBackend::SyncDelegate> sync_delegate_;

  // Current sync status, absl::nullopt until UpdateSyncStatusOnStartUp() is
  // called. This value is used to distinguish between sync setup on startup and
  // when user turns on sync manually.
  absl::optional<IsSyncEnabled> is_sync_enabled_;

  // Current syncing account if one exist.
  absl::optional<std::string> syncing_account_;

  base::WeakPtrFactory<PasswordSyncControllerDelegateAndroid> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
