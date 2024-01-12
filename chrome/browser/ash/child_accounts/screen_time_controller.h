// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/time_limit_notifier.h"
#include "chrome/browser/ash/child_accounts/usage_time_limit_processor.h"
#include "chrome/browser/ash/child_accounts/usage_time_state_notifier.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class TickClock;
class OneShotTimer;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
enum class ParentCodeValidationResult;

// The controller to track each user's screen time usage and inquiry time limit
// processor (a component to calculate state based on policy settings and time
// usage) when necessary to determine the current lock screen state.
// Schedule notifications and lock/unlock screen based on the processor output.
class ScreenTimeController
    : public KeyedService,
      public parent_access::ParentAccessService::Observer,
      public session_manager::SessionManagerObserver,
      public UsageTimeStateNotifier::Observer,
      public system::TimezoneSettings::Observer,
      public SystemClockClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when daily screen time limit is |kUsageTimeLimitWarningTime| or
    // less to finish.
    virtual void UsageTimeLimitWarning() = 0;
  };

  // Registers preferences.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit ScreenTimeController(content::BrowserContext* context);

  ScreenTimeController(const ScreenTimeController&) = delete;
  ScreenTimeController& operator=(const ScreenTimeController&) = delete;

  ~ScreenTimeController() override;

  // Returns the child's screen time duration. This is how long the child has
  // used the device today (since the last reset).
  virtual base::TimeDelta GetScreenTimeDuration();

  // Method intended for testing purposes only.
  void SetClocksForTesting(
      const base::Clock* clock,
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Call UsageTimeLimitWarning for each observer for testing.
  void NotifyUsageTimeLimitWarningForTesting();

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Call time limit processor for new state.
  void CheckTimeLimit(const std::string& source);

  // Request to lock the screen and show the time limits message when the screen
  // is locked.
  void ForceScreenLockByPolicy();

  // Enables the time limits message in the lock screen and performs tasks that
  // need to run after the screen is locked.
  // |active_policy|: Which policy is locking the device, only valid when
  //                  |visible| is true.
  // |next_unlock_time|: When user will be able to unlock the screen, only valid
  //                     when |visible| is true.
  void OnScreenLockByPolicy(usage_time_limit::PolicyType active_policy,
                            base::Time next_unlock_time);

  // Disables the time limits message in the lock screen.
  void OnScreenLockByPolicyEnd();

  // Called when the policy of time limits changes.
  void OnPolicyChanged();

  // Reset any currently running timers.
  void ResetStateTimers();
  void ResetInSessionTimers();
  void ResetWarningTimers();

  // Schedule a call for UsageTimeLimitWarning.
  void ScheduleUsageTimeLimitWarning(const usage_time_limit::State& state);

  // Save the |state| to |prefs::kScreenTimeLastState|.
  void SaveCurrentStateToPref(const usage_time_limit::State& state);

  // Get the last calculated |state| from |prefs::kScreenTimeLastState|, if it
  // exists.
  std::optional<usage_time_limit::State> GetLastStateFromPref();

  // Called when the usage time limit is |kUsageTimeLimitWarningTime| or less to
  // finish. It should call the method UsageTimeLimitWarning for each observer.
  void UsageTimeLimitWarning();

  // Converts a usage_time_limit::PolicyType to its TimeLimitNotifier::LimitType
  // equivalent.
  std::optional<TimeLimitNotifier::LimitType> ConvertPolicyType(
      usage_time_limit::PolicyType policy_type);

  // parent_access::ParentAccessService::Observer:
  void OnAccessCodeValidation(ParentCodeValidationResult result,
                              std::optional<AccountId> account_id) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // UsageTimeStateNotifier::Observer:
  void OnUsageTimeStateChange(
      const UsageTimeStateNotifier::UsageTimeState state) override;

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // SystemClockClient::Observer:
  void SystemClockUpdated() override;

  raw_ptr<content::BrowserContext> context_;
  raw_ptr<PrefService> pref_service_;

  base::ObserverList<Observer> observers_;

  // Points to the base::DefaultClock by default.
  raw_ptr<const base::Clock> clock_;

  // Timer scheduled for when the next lock screen state change event is
  // expected to happen, e.g. when bedtime is over or the usage limit ends.
  std::unique_ptr<base::OneShotTimer> next_state_timer_;

  // Timer to schedule the usage time limit warning and call the
  // UsageTimeLimitWarning for each observer. This should happen
  // |kUsageTimeLimitWarningTime| minutes or less before the device is locked by
  // usage limit.
  std::unique_ptr<base::OneShotTimer> usage_time_limit_warning_timer_;

  // Contains the last time limit policy processed by this class. Used to
  // generate notifications when the policy changes.
  base::Value::Dict last_policy_;

  // Used to set up timers when a time limit is approaching.
  TimeLimitNotifier time_limit_notifier_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
