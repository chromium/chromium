// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_OS_AND_POLICIES_UPDATE_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_OS_AND_POLICIES_UPDATE_CHECKER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/scheduled_update_checker/task_executor_with_retries.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace policy {

namespace update_checker_internal {

// The maximum iterations allowed to check for and download an update if the
// operation fails. Used with |os_and_policies_update_checker_|.
constexpr int kMaxOsAndPoliciesUpdateCheckerRetryIterations = 2;

// Interval at which |os_and_policies_update_checker_| retries checking for and
// downloading updates.
constexpr base::TimeDelta kOsAndPoliciesUpdateCheckerRetryTime =
    base::TimeDelta::FromMinutes(10);

// Time for which |OsAndPoliciesUpdateChecker| will wait for a valid network
// before querying the update server for updates. After this time it will return
// a failure. During testing it was noted that on average 1 minute seemed to be
// the delay after which a network would be detected by Chrome.
constexpr base::TimeDelta kWaitForNetworkTimeout =
    base::TimeDelta::FromMinutes(5);

}  // namespace update_checker_internal

// This class is used by the scheduled update check policy to perform the actual
// device update check.
class OsAndPoliciesUpdateChecker
    : public chromeos::UpdateEngineClient::Observer,
      public chromeos::NetworkStateHandlerObserver {
 public:
  OsAndPoliciesUpdateChecker(
      chromeos::NetworkStateHandler* network_state_handler);
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

  // chromeos::NetworkStateHandlerObserver overrides.
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

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
  void OnUpdateCheckStarted(
      chromeos::UpdateEngineClient::UpdateCheckResult result);

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
  chromeos::NetworkStateHandler* const network_state_handler_;

  // Scheduled and retries |StartUpdateCheck|.
  TaskExecutorWithRetries update_check_task_executor_;

  // Timer to wait for a valid network after |Start| is called.
  base::OneShotTimer wait_for_network_timer_;

  // Timer to abort any pending operations and call
  // |update_check_completion_cb_| with false.
  base::OneShotTimer timeout_timer_;

  // Not owned.
  chromeos::UpdateEngineClient* const update_engine_client_;

  base::WeakPtrFactory<OsAndPoliciesUpdateChecker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OsAndPoliciesUpdateChecker);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_OS_AND_POLICIES_UPDATE_CHECKER_H_
