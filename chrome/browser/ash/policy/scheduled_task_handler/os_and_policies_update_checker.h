// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_OS_AND_POLICIES_UPDATE_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_OS_AND_POLICIES_UPDATE_CHECKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/task_executor_with_retries.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {
class NetworkStateHandler;
}  // namespace ash

namespace policy {

namespace update_checker_internal {

// The maximum iterations allowed to check for and download an update if the
// operation fails. Used with |os_and_policies_update_checker_|.
constexpr int kMaxOsAndPoliciesUpdateCheckerRetryIterations = 2;

// Interval at which |os_and_policies_update_checker_| retries checking for and
// downloading updates.
constexpr base::TimeDelta kOsAndPoliciesUpdateCheckerRetryTime =
    base::Minutes(10);

// Time for which |OsAndPoliciesUpdateChecker| will wait for a valid network
// before querying the update server for updates. After this time it will return
// a failure. During testing it was noted that on average 1 minute seemed to be
// the delay after which a network would be detected by Chrome.
constexpr base::TimeDelta kWaitForNetworkTimeout = base::Minutes(5);

}  // namespace update_checker_internal

// This class is used by the scheduled update check policy to perform the actual
// device update check.
class OsAndPoliciesUpdateChecker : public ash::UpdateEngineClient::Observer,
                                   public ash::NetworkStateHandlerObserver {
 public:
  explicit OsAndPoliciesUpdateChecker(
      ash::NetworkStateHandler* network_state_handler);

  OsAndPoliciesUpdateChecker(const OsAndPoliciesUpdateChecker&) = delete;
  OsAndPoliciesUpdateChecker& operator=(const OsAndPoliciesUpdateChecker&) =
      delete;

  ~OsAndPoliciesUpdateChecker() override;

  using UpdateCheckCompletionCallback = base::OnceCallback<void(bool result)>;

  // Starts an update check and possible download. Once the update check is
  // finished it refreshes policies and finally calls |cb| to indicate success
  // or failure when the process is complete. Calls |cb| with false if |timeout|
  // passed without the operation completing. Overrides any previous calls to
  // |Start|.
  void Start(UpdateCheckCompletionCallback cb, base::TimeDelta timeout);

  // Stops any pending update checks or policy refreshes. Calls
  // |update_check_completion_cb_| with false. It is safe to call |Start| after
  // this.
  void Stop();

  // Returns true if |Start| has been called and not been |Stop|ped.
  bool IsRunning() const;

  // ash::NetworkStateHandlerObserver overrides.
  void DefaultNetworkChanged(const ash::NetworkState* network) override;

 private:
  // Schedules update check by using |update_check_task_executor_|.
  void ScheduleUpdateCheck();

  // Runs |update_check_completion_cb_| with |update_check_result| and runs
  // |ResetState|.
  void RunCompletionCallbackAndResetState(bool update_check_result);

  // Runs when |wait_for_network_timer_| expires i.e. a network hasn't been
  // detected after the maximum time out.
  void OnNetworkWaitTimeout();

  // Runs when |update_check_task_executor_::Start| has failed after retries.
  void OnUpdateCheckFailure();

  // Requests update engine to do an update check.
  void StartUpdateCheck();

  // UpdateEngineClient::Observer overrides.
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  // Tells whether starting an update check succeeded or not.
  void OnUpdateCheckStarted(ash::UpdateEngineClient::UpdateCheckResult result);

  // Refreshes policies. |update_check_result| represents the status of the
  // previous stage i.e. an OS update check and download.
  void RefreshPolicies(bool update_check_result);

  // Called when the API call to refresh policies is completed.
  // |update_check_result| represents the result of the update check which
  // triggered this policy refresh.
  void OnRefreshPoliciesCompletion(bool update_check_result);

  // Resets all state and cancels any pending update checks.
  void ResetState();

  // Ignore fist IDLE status that is sent when the update check is initiated.
  bool ignore_idle_status_ = true;

  // Set to true when |Start| is called and false when |Stop| is called.
  bool is_running_ = false;

  // Callback passed to |Start|. Called if |StartUpdateCheck| is unsuccessful
  // after retries or when an update check finishes successfully.
  UpdateCheckCompletionCallback update_check_completion_cb_;

  // Not owned.
  const raw_ptr<ash::NetworkStateHandler> network_state_handler_;
  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Scheduled and retries |StartUpdateCheck|.
  TaskExecutorWithRetries update_check_task_executor_;

  // Timer to wait for a valid network after |Start| is called.
  base::OneShotTimer wait_for_network_timer_;

  // Timer to abort any pending operations and call
  // |update_check_completion_cb_| with false.
  base::OneShotTimer timeout_timer_;

  // Not owned.
  const raw_ptr<ash::UpdateEngineClient> update_engine_client_;

  base::WeakPtrFactory<OsAndPoliciesUpdateChecker> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_OS_AND_POLICIES_UPDATE_CHECKER_H_
