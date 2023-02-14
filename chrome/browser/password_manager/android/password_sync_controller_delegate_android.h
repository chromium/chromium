// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class PasswordSyncControllerDelegateAndroid
    : public syncer::ModelTypeControllerDelegate,
      public syncer::SyncServiceObserver,
      public PasswordSyncControllerDelegateBridge::Consumer {
 public:
  explicit PasswordSyncControllerDelegateAndroid(
      std::unique_ptr<PasswordSyncControllerDelegateBridge> bridge,
      base::OnceClosure on_sync_shutdown);
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
  void ClearMetadataWhileStopped() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // PasswordStoreAndroidBackendBridge::Consumer implementation.
  void OnCredentialManagerNotified() override;
  void OnCredentialManagerError(const AndroidBackendError& error,
                                int api_error_code) override;

  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateProxyModelControllerDelegate();

  // Updates |is_sync_enabled| to hold the initial sync setting.
  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

 private:
  using IsSyncEnabled = base::StrongAlias<struct IsSyncEnabledTag, bool>;

  // Notify credential manager about current account on startup or if
  // password sync setting has changed.
  void UpdateCredentialManagerSyncStatus(syncer::SyncService* sync_service);

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtrToBaseClass();

  const std::unique_ptr<PasswordSyncControllerDelegateBridge> bridge_;

  // Current sync status, absl::nullopt until OnSyncServiceInitialized() is
  // called. This value is used to distinguish between sync setup on startup and
  // when user turns on sync manually.
  absl::optional<IsSyncEnabled> is_sync_enabled_;

  // Last sync status set in CredentialManager.
  absl::optional<IsSyncEnabled> credential_manager_sync_setting_;

  base::OnceClosure on_sync_shutdown_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  base::WeakPtrFactory<PasswordSyncControllerDelegateAndroid> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
