// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_

#include <memory>
#include <unordered_map>

#include "base/containers/small_map.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

// Android-specific password store backend that delegates every request to
// Google Mobile Service.
// It uses a `PasswordStoreAndroidBackendBridge` to send API requests for each
// method it implements from `PasswordStoreBackend`. The response will invoke a
// consumer method with an originally provided `TaskId`. Based on that `TaskId`,
// this class maps ongoing tasks to the callbacks of the methods that originally
// required the task since JNI itself can't preserve the callbacks.
class PasswordStoreAndroidBackend
    : public PasswordStoreBackend,
      public PasswordStoreAndroidBackendBridge::Consumer {
 public:
  explicit PasswordStoreAndroidBackend(
      std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge);
  ~PasswordStoreAndroidBackend() override;

 private:
  // Stub class for handling sync events.
  class SyncModelTypeControllerDelegate
      : public syncer::ModelTypeControllerDelegate {
   public:
    SyncModelTypeControllerDelegate();
    SyncModelTypeControllerDelegate(const SyncModelTypeControllerDelegate&) =
        delete;
    SyncModelTypeControllerDelegate(SyncModelTypeControllerDelegate&&) = delete;
    SyncModelTypeControllerDelegate& operator=(
        const SyncModelTypeControllerDelegate&) = delete;
    SyncModelTypeControllerDelegate& operator=(
        SyncModelTypeControllerDelegate&&) = delete;
    ~SyncModelTypeControllerDelegate() override;

    base::WeakPtr<SyncModelTypeControllerDelegate> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    // syncer::ModelTypeControllerDelegate implementation
    void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                        StartCallback callback) override {}
    void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override {}
    void GetAllNodesForDebugging(AllNodesCallback callback) override {}
    void GetTypeEntitiesCountForDebugging(
        base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
        const override {}
    void RecordMemoryUsageAndCountsHistograms() override {}

    base::WeakPtrFactory<SyncModelTypeControllerDelegate> weak_ptr_factory_{
        this};
  };

  using TaskId = PasswordStoreAndroidBackendBridge::TaskId;
  using ReplyVariant = absl::variant<LoginsReply, PasswordStoreChangeListReply>;
  // Using a small_map should ensure that we handle rare cases with many tasks
  // like a bulk deletion just as well as the normal, rather small task load.
  using TaskMap =
      base::small_map<std::unordered_map<TaskId, ReplyVariant, TaskId::Hasher>>;

  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void GetAllLoginsAsync(LoginsReply callback) override;
  void GetAutofillableLoginsAsync(LoginsReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegateFactory() override;

  // Implements PasswordStoreAndroidBackendBridge::Consumer interface.
  void OnCompleteWithLogins(PasswordStoreAndroidBackendBridge::TaskId task_id,
                            std::vector<PasswordForm> passwords) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

  ReplyVariant GetAndEraseTask(TaskId task_id);

  // Observer to propagate remote form changes to.
  RemoteChangesReceived remote_form_changes_received_;

  // Delegate to handle sync events.
  SyncModelTypeControllerDelegate sync_controller_delegate_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Used to store callbacks for each invoked tasks since callbacks can't be
  // called via JNI directly.
  TaskMap request_for_task_;

  // This object is the proxy to the JNI bridge that performs the API requests.
  std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
