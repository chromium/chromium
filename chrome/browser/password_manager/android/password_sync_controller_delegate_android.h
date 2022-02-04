// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class PasswordSyncControllerDelegateAndroid
    : public syncer::ModelTypeControllerDelegate {
 public:
  PasswordSyncControllerDelegateAndroid();
  PasswordSyncControllerDelegateAndroid(
      const PasswordSyncControllerDelegateAndroid&) = delete;
  PasswordSyncControllerDelegateAndroid(
      PasswordSyncControllerDelegateAndroid&&) = delete;
  PasswordSyncControllerDelegateAndroid& operator=(
      const PasswordSyncControllerDelegateAndroid&) = delete;
  PasswordSyncControllerDelegateAndroid& operator=(
      PasswordSyncControllerDelegateAndroid&&) = delete;
  ~PasswordSyncControllerDelegateAndroid() override;

  base::WeakPtr<PasswordSyncControllerDelegateAndroid> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // syncer::ModelTypeControllerDelegate implementation
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;

  base::WeakPtrFactory<PasswordSyncControllerDelegateAndroid> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_ANDROID_H_
