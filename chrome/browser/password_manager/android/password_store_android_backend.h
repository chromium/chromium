// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_

#include <memory>
#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Update enums.xml whenever updating
// this enum.
enum class UnifiedPasswordManagerActiveStatus {
  // UPM is active.
  kActive = 0,
  // UPM is inactive because passwords sync is off.
  kInactiveSyncOff = 1,
  // UPM is inactive because the client has been unenrolled due to unresolvable
  // errors
  kInactiveUnenrolledDueToErrors = 2,

  kMaxValue = kInactiveUnenrolledDueToErrors
};

// Android-specific password store backend that delegates every request to
// Google Mobile Service.
// It uses a `PasswordStoreAndroidBackendBridge` to send API requests for each
// method it implements from `PasswordStoreBackend`. The response will invoke a
// consumer method with an originally provided `JobId`. Based on that `JobId`,
// this class maps ongoing jobs to the callbacks of the methods that originally
// required the job since JNI itself can't preserve the callbacks.
class PasswordStoreAndroidBackend
    : public PasswordStoreBackend,
      public PasswordStoreAndroidBackendBridge::Consumer {
 public:
  PasswordStoreAndroidBackend(std::unique_ptr<SyncDelegate> sync_delegate,
                              PrefService* prefs);
  PasswordStoreAndroidBackend(
      base::PassKey<class PasswordStoreAndroidBackendTest>,
      std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
      std::unique_ptr<SyncDelegate> sync_delegate,
      std::unique_ptr<PasswordSyncControllerDelegateAndroid>
          sync_controller_delegate,
      PrefService* prefs);
  ~PasswordStoreAndroidBackend() override;

 private:
  SEQUENCE_CHECKER(main_sequence_checker_);

  class ClearAllLocalPasswordsMetricRecorder;

  // Wraps the handler for an asynchronous job (if successful) and invokes the
  // supplied metrics recorded upon completion. An object of this type shall be
  // created and stored in |request_for_job_| once an asynchronous begins, and
  // destroyed once the job is finished.
  class JobReturnHandler {
   public:
    using ErrorReply = base::OnceClosure;

    JobReturnHandler(LoginsOrErrorReply callback,
                     PasswordStoreBackendMetricsRecorder metrics_recorder,
                     bool is_unenrolled_from_upm);
    JobReturnHandler(PasswordChangesOrErrorReply callback,
                     PasswordStoreBackendMetricsRecorder metrics_recorder,
                     bool is_unenrolled_from_upm);
    JobReturnHandler(JobReturnHandler&&);
    JobReturnHandler& operator=(JobReturnHandler&&) = delete;
    ~JobReturnHandler();

    template <typename T>
    bool Holds() const {
      return absl::holds_alternative<T>(success_callback_);
    }

    template <typename T>
    T&& Get() && {
      return std::move(absl::get<T>(success_callback_));
    }

    void RecordMetrics(absl::optional<AndroidBackendError> error) const;
    base::TimeDelta GetElapsedTimeSinceStart() const;

   private:
    absl::variant<LoginsOrErrorReply, PasswordChangesOrErrorReply>
        success_callback_;
    PasswordStoreBackendMetricsRecorder metrics_recorder_;
    const bool is_unenrolled_from_upm_;
  };

  using JobId = PasswordStoreAndroidBackendBridge::JobId;
  // Using a small_map should ensure that we handle rare cases with many jobs
  // like a bulk deletion just as well as the normal, rather small job load.
  using JobMap = base::small_map<
      std::unordered_map<JobId, JobReturnHandler, JobId::Hasher>>;

  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived stored_passwords_changed,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
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
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  // Implements PasswordStoreAndroidBackendBridge::Consumer interface.
  void OnCompleteWithLogins(PasswordStoreAndroidBackendBridge::JobId job_id,
                            std::vector<PasswordForm> passwords) override;
  void OnLoginsChanged(PasswordStoreAndroidBackendBridge::JobId task_id,
                       PasswordChanges changes) override;
  void OnError(PasswordStoreAndroidBackendBridge::JobId job_id,
               AndroidBackendError error) override;

  template <typename Callback>
  void QueueNewJob(JobId job_id, Callback callback, MetricInfix metric_infix);
  absl::optional<JobReturnHandler> GetAndEraseJob(JobId job_id);

  // Gets logins matching |form|.
  void GetLoginsAsync(const PasswordFormDigest& form,
                      bool include_psl,
                      LoginsOrErrorReply callback);

  // Filters |logins| created between |delete_begin| and |delete_end| time
  // that match |url_filer| and asynchronously removes them.
  void FilterAndRemoveLogins(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply reply,
      LoginsResultOrError result);

  // Filters logins that match |origin_filer| and asynchronously disables
  // autosignin by updating stored logins.
  void FilterAndDisableAutoSignIn(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      PasswordChangesOrErrorReply completion,
      LoginsResultOrError result);

  // Creates a metrics recorder that records latency and success metrics for
  // logins retrieval operation with |metric_infix| name prior to calling
  // |callback|.
  static LoginsOrErrorReply ReportMetricsAndInvokeCallbackForLoginsRetrieval(
      const MetricInfix& metric_infix,
      LoginsOrErrorReply callback);

  // Creates a metrics recorder that records latency and success metrics for
  // store modification operation with |metric_infix| name prior to
  // calling |callback|.
  static PasswordChangesOrErrorReply
  ReportMetricsAndInvokeCallbackForStoreModifications(
      const MetricInfix& metric_infix,
      PasswordChangesOrErrorReply callback);

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status) for |account|.
  void GetAllLoginsForAccount(
      PasswordStoreAndroidBackendBridge::Account account,
      LoginsOrErrorReply callback);

  // Removes |form| from |account|.
  void RemoveLoginForAccount(const PasswordForm& form,
                             PasswordStoreAndroidBackendBridge::Account account,
                             PasswordChangesOrErrorReply callback);

  // Invoked synchronously by `lifecycle_helper_` when Chrome is foregrounded.
  // This should not cover the initial startup since the registration for the
  // event happens afterwads and is not repeated. A "foreground session" starts
  // when a Chrome activity resumes for the first time.
  void OnForegroundSessionStart();

  // Clears all `request_for_job_`s that haven't been completed yet at a moment
  // when it's unlikely that they will still finish. It records an error for
  // each task cleared this way that it could have failed.
  void ClearZombieTasks();

  // Observer to propagate potential password changes to.
  RemoteChangesReceived stored_passwords_changed_;

  // Helper that receives lifecycle events via JNI and synchronously invokes a
  // passed callback, e.g. `OnForegroundSessionStart`.
  std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper_;

  // TaskRunner to run responses on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Used to store callbacks for each invoked jobs since callbacks can't be
  // called via JNI directly.
  JobMap request_for_job_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // This object is the proxy to the JNI bridge that performs the API requests.
  std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge_;

  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Delegate to obtain sync status, and syncing account.
  std::unique_ptr<SyncDelegate> sync_delegate_;

  // Delegate to handle sync events.
  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
      sync_controller_delegate_;

  raw_ptr<PrefService> prefs_ = nullptr;

  base::WeakPtrFactory<PasswordStoreAndroidBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
