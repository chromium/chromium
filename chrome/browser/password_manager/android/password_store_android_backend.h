// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_

#include <memory>
#include <optional>
#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"

class PrefService;

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

// This enum is used in the JobReturnHandler for tracking the store operation
// that started the job so that the correct operation can be retried when the
// job encountered an error.
enum class PasswordStoreOperation {
  // Operations that are safe to retry because they are non-modifying and not
  // visible to the user.
  kGetAllLoginsAsync = 0,
  kGetAutofillableLoginsAsync = 1,

  // Obsolete
  // kGetAllLoginsForAccountAsync = 2,

  // Operation that is non-modifying, but not safe to retry because it is
  // user-visible.
  kFillMatchingLoginsAsync = 3,

  // Operations that are not safe to retry because they are modifying.
  kAddLoginAsync = 4,
  kUpdateLoginAsync = 5,

  // Obsolete
  // kRemoveLoginForAccount = 6,

  kRemoveLoginAsync = 7,
  kRemoveLoginsByURLAndTimeAsync = 8,
  kRemoveLoginsCreatedBetweenAsync = 9,
  kDisableAutoSignInForOriginsAsync = 10,
  // Deprecated
  // kClearAllLocalPasswords = 11,

  // Operation that is non-modifying, but not safe to retry because it is
  // user-visible.
  kGetGroupedMatchingLoginsAsync = 12,
  kGetAllLoginsWithBrandingInfoAsync = 13,

  kMaxValue = kGetAllLoginsWithBrandingInfoAsync,
};

// Android-specific password store backend that delegates every request to
// Google Mobile Service.
// It uses a `PasswordStoreAndroidBackendDispatcherBridge` to send API requests
// for each method it implements from `PasswordStoreBackend`. The response will
// invoke a consumer method via `PasswordStoreAndroidBackendReceiverBridge` with
// an originally provided `JobId`. Based on that `JobId`, this class maps
// ongoing jobs to the callbacks of the methods that originally required the job
// since JNI itself can't preserve the callbacks.
class PasswordStoreAndroidBackend
    : public PasswordStoreAndroidBackendReceiverBridge::Consumer {
 protected:
  PasswordStoreAndroidBackend(
      std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
      PrefService* prefs);
  ~PasswordStoreAndroidBackend() override;

  // Internal methods corresponding to PasswordStoreBackendInterface that take
  // the account corresponding to the GMS Core storage as a param.
  void Init(
      PasswordStoreBackend::RemoteChangesReceived remote_form_changes_received);
  void Shutdown(base::OnceClosure shutdown_completed);
  void GetAllLoginsInternal(std::string account,
                            LoginsOrErrorReply callback,
                            PasswordStoreOperation operation =
                                PasswordStoreOperation::kGetAllLoginsAsync,
                            base::TimeDelta delay = base::Seconds(0));
  void GetAutofillableLoginsInternal(
      std::string account,
      LoginsOrErrorReply callback,
      PasswordStoreOperation operation =
          PasswordStoreOperation::kGetAutofillableLoginsAsync,
      base::TimeDelta delay = base::Seconds(0));
  void GetAllLoginsWithAffiliationAndBrandingInternal(
      std::string account,
      LoginsOrErrorReply callback);
  void GetLoginsInternal(std::string account,
                         const PasswordFormDigest& form,
                         bool include_psl,
                         LoginsOrErrorReply callback);
  void AddLoginInternal(std::string account,
                        const PasswordForm& form,
                        PasswordChangesOrErrorReply callback);
  void UpdateLoginInternal(std::string account,
                           const PasswordForm& form,
                           PasswordChangesOrErrorReply callback);
  void RemoveLoginInternal(std::string account,
                           const PasswordForm& form,
                           PasswordChangesOrErrorReply callback);
  void FillMatchingLoginsInternal(std::string account,
                                  LoginsOrErrorReply callback,
                                  bool include_psl,
                                  const std::vector<PasswordFormDigest>& forms);
  void GetGroupedMatchingLoginsInternal(std::string account,
                                        const PasswordFormDigest& form_digest,
                                        LoginsOrErrorReply callback);
  void RemoveLoginsByURLAndTimeInternal(
      std::string account,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback);
  void RemoveLoginsCreatedBetweenInternal(std::string account,
                                          base::Time delete_begin,
                                          base::Time delete_end,
                                          PasswordChangesOrErrorReply callback);
  void DisableAutoSignInForOriginsInternal(
      std::string account,
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion);

  // Cancels all the queued jobs because of `reason` and replies to them with
  // `reply_error`.
  void ClearAllTasksAndReplyWithReason(
      const AndroidBackendError& reason,
      const PasswordStoreBackendError& reply_error);

  PasswordStoreAndroidBackendBridgeHelper* bridge_helper() {
    return bridge_helper_.get();
  }

  PrefService* prefs() { return prefs_; }

  // Subclasses can override this method
  // to have a special handling for different errors.
  virtual void RecoverOnError(AndroidBackendAPIErrorCode error) = 0;
  // Subclasses can override this method to react when GMSCore responds
  // successfully.
  virtual void OnCallToGMSCoreSucceeded() = 0;
  // Subclasses have to provide an account which will be used for retries.
  virtual std::string GetAccountToRetryOperation() = 0;
  // Subclasses have to provide a store backend type that is used for tracking
  // metrics that are split for local and account.
  virtual PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
  GetStorageType() = 0;

 private:
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Wraps the handler for an asynchronous job (if successful or scheduled to be
  // retried) and invokes the supplied metrics recorded upon completion. An
  // object of this type shall be created and stored in |request_for_job_| once
  // an asynchronous task begins, and destroyed once the job is finished.
  // The handler stores an |operation| which determines whether the method
  // matching the |operation| can be retried due to an error. The operations are
  // retried with exponential backoff. The |delay| with which the job was
  // started is stored in the handler so that the next retry of the job can
  // increase the |delay|. NOTE: Currently only retries for operations which
  // match 1-to-1 to methods are supported.
  class JobReturnHandler {
   public:
    using ErrorReply = base::OnceClosure;

    JobReturnHandler(LoginsOrErrorReply callback,
                     PasswordStoreBackendMetricsRecorder metrics_recorder,
                     base::TimeDelta delay,
                     PasswordStoreOperation operation);
    JobReturnHandler(PasswordChangesOrErrorReply callback,
                     PasswordStoreBackendMetricsRecorder metrics_recorder,
                     base::TimeDelta delay,
                     PasswordStoreOperation operation);
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

    void RecordMetrics(std::optional<AndroidBackendError> error) const;
    base::TimeDelta GetElapsedTimeSinceStart() const;

    base::TimeDelta GetDelay();
    PasswordStoreOperation GetOperation();

   private:
    absl::variant<LoginsOrErrorReply, PasswordChangesOrErrorReply>
        success_callback_;
    PasswordStoreBackendMetricsRecorder metrics_recorder_;
    base::TimeDelta delay_;
    PasswordStoreOperation operation_;
  };

  // Wraps the `callback` to retry after the previous attempt to call
  // the backend failed. Used to be able to cancel a posted delayed
  // operation while also replying to its `reply_callback` that the
  // delayed retry was cancelled. In order to cancel the retry, it's
  // enough to destroy the wrapper object, which will invalidate the weak
  // pointer bound to the delayed task.
  class CancellableRetryCallback {
   public:
    CancellableRetryCallback(base::OnceCallback<void(LoginsOrErrorReply,
                                                     PasswordStoreOperation,
                                                     base::TimeDelta)> callback,
                             PasswordStoreOperation operation,
                             LoginsOrErrorReply reply_callback,
                             base::TimeDelta current_delay);
    CancellableRetryCallback(CancellableRetryCallback&&) = delete;
    CancellableRetryCallback& operator=(CancellableRetryCallback&&) = delete;
    ~CancellableRetryCallback();

    base::WeakPtr<CancellableRetryCallback> AsWeakPtr();

    void Run();
    LoginsOrErrorReply GetReplyCallbackAndCancel();

   private:
    base::OnceCallback<
        void(LoginsOrErrorReply, PasswordStoreOperation, base::TimeDelta)>
        callback_;
    PasswordStoreOperation operation_;
    LoginsOrErrorReply reply_callback_;
    base::TimeDelta current_delay_;
    base::WeakPtrFactory<CancellableRetryCallback> weak_ptr_factory_{this};
  };

  using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;
  // Using a small_map should ensure that we handle rare cases with many jobs
  // like a bulk deletion just as well as the normal, rather small job load.
  using JobMap = base::small_map<
      std::unordered_map<JobId, JobReturnHandler, JobId::Hasher>>;

  using DelayedRetryId = base::IdType32<CancellableRetryCallback>;

  base::OnceCallback<
      void(LoginsOrErrorReply, PasswordStoreOperation, base::TimeDelta)>
  GetRetryCallbackForOperation(PasswordStoreOperation operation);
  // Implements the retry mechanism for the operations that are safe to retry.
  // It attempts to retry the |operation|. The given |delay| comes from the
  // previous attempt to run the operation. The delay before the next retry will
  // be double the value of |delay|, except for the first retry that has a delay
  // of 1 second.
  void RetryOperation(PasswordStoreOperation operation,
                      AndroidBackendAPIErrorCode api_error_code,
                      base::TimeDelta delay,
                      LoginsOrErrorReply reply);
  void CleanupRetryAfterRun(DelayedRetryId retry_id);

  // Implements PasswordStoreAndroidBackendDispatcherBridge::Consumer interface.
  void OnCompleteWithLogins(
      PasswordStoreAndroidBackendDispatcherBridge::JobId job_id,
      std::vector<PasswordForm> passwords) override;
  void OnLoginsChanged(
      PasswordStoreAndroidBackendDispatcherBridge::JobId task_id,
      PasswordChanges changes) override;
  void OnError(PasswordStoreAndroidBackendDispatcherBridge::JobId job_id,
               AndroidBackendError error) override;

  template <typename Callback>
  // Calling this method can be delayed in the case when a retry is scheduled.
  // Since the retry logic implements exponential backoff the duration of the
  // |delay| is passed to QueueNewJob from its caller such that the |delay| can
  // be stored in the JobReturnHandler and retrievable from it. QueueNewJob
  // doesn't introduce any delay.
  void QueueNewJob(JobId job_id,
                   Callback callback,
                   MethodName method_name,
                   PasswordStoreOperation operation,
                   base::TimeDelta delay);
  std::optional<JobReturnHandler> GetAndEraseJob(JobId job_id);

  // Filters |logins| created between |delete_begin| and |delete_end| time
  // that match |url_filer| and asynchronously removes them.
  // |operation| is the PasswordStoreOperation  that invoked this method and
  // |delay| is the amount of time by which the call to this method was delayed.
  void FilterAndRemoveLogins(
      std::string account,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply reply,
      LoginsResultOrError result);

  // Filters logins that match |origin_filer| and asynchronously disables
  // autosignin by updating stored logins.
  void FilterAndDisableAutoSignIn(
      std::string account,
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      PasswordChangesOrErrorReply completion,
      LoginsResultOrError result);

  // Creates a metrics recorder that records latency and success metrics for
  // logins retrieval operation with |method_name| name prior to calling
  // |callback|.
  static LoginsOrErrorReply ReportMetricsAndInvokeCallbackForLoginsRetrieval(
      const MethodName& method_name,
      LoginsOrErrorReply callback,
      PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
          store_type);

  // Creates a metrics recorder that records latency and success metrics for
  // store modification operation with |method_name| name prior to
  // calling |callback|.
  static PasswordChangesOrErrorReply
  ReportMetricsAndInvokeCallbackForStoreModifications(
      const MethodName& method_name,
      PasswordChangesOrErrorReply callback,
      PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
          store_type);

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
  PasswordStoreBackend::RemoteChangesReceived stored_passwords_changed_;

  // Helper that receives lifecycle events via JNI and synchronously invokes a
  // passed callback, e.g. `OnForegroundSessionStart`.
  std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper_;

  // TaskRunner to run responses on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Used to store callbacks for each invoked jobs since callbacks can't be
  // called via JNI directly.
  JobMap request_for_job_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // This object is the proxy to the dispatcher JNI bridge that performs the API
  // requests.
  std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper_;

  // Used to store retry callbacks that will be executed as part of a
  // posted delayed task.
  std::map<DelayedRetryId, std::unique_ptr<CancellableRetryCallback>>
      scheduled_retries_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // The id of the latest scheduled retry. Incremented when a new retry is
  // scheduled.
  DelayedRetryId::Generator delayed_retry_id_generator_;

  raw_ptr<PrefService> prefs_ = nullptr;

  base::Time initialized_at_ = base::Time::Now();

  // This will be set to false once the first foregrounding has been handled.
  bool should_delay_refresh_on_foregrounding_ = true;

  base::WeakPtrFactory<PasswordStoreAndroidBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
