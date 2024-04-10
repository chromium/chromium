// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/version.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/settings/cros_settings.h"

class PrefRegistrySimple;

namespace ash {
class NetworkStateHandler;
class UpdateRequiredNotification;
}  // namespace ash

namespace base {
class Clock;
class Time;
}  // namespace base

namespace policy {

// This class observes the device setting |kDeviceMinimumVersion|, and
// checks if respective requirement is met. If an update is not required, all
// running timers are reset. If an update is required, it calculates the
// deadline using the warning period in the policy and restarts the timer. It
// also calls UpdateRequiredNotification to show in-session notifications if an
// update is required but it cannot be downloaded due to network limitations or
// Auto Update Expiration.
class MinimumVersionPolicyHandler : public BuildStateObserver,
                                    public ash::NetworkStateHandlerObserver,
                                    public ash::UpdateEngineClient::Observer {
 public:
  static const char kRequirements[];
  static const char kChromeOsVersion[];
  static const char kWarningPeriod[];
  static const char kEolWarningPeriod[];
  static const char kUnmanagedUserRestricted[];

  class Observer {
   public:
    virtual void OnMinimumVersionStateChanged() = 0;
    virtual ~Observer() = default;
  };

  // Delegate of MinimumVersionPolicyHandler to handle the external
  // dependencies.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Checks if the user is logged in as any kiosk app or this is an
    // auto-launch kiosk device.
    virtual bool IsKioskMode() const = 0;

    // Checks if the device is enterprise managed.
    virtual bool IsDeviceEnterpriseManaged() const = 0;

    // Checks if a user is logged in.
    virtual bool IsUserLoggedIn() const = 0;

    // Checks if the user logged in is managed and not a child user.
    virtual bool IsUserEnterpriseManaged() const = 0;

    // Checks if we are currently on the login screen.
    virtual bool IsLoginSessionState() const = 0;

    // Checks if login is in progress before starting user session.
    virtual bool IsLoginInProgress() const = 0;

    // Shows the update required screen.
    virtual void ShowUpdateRequiredScreen() = 0;

    // Terminates the current session and restarts Chrome to show the login
    // screen.
    virtual void RestartToLoginScreen() = 0;

    // Hides update required screen and shows the login screen.
    virtual void HideUpdateRequiredScreenIfShown() = 0;

    virtual base::Version GetCurrentVersion() const = 0;
  };

  class MinimumVersionRequirement {
   public:
    MinimumVersionRequirement(const base::Version version,
                              const base::TimeDelta warning,
                              const base::TimeDelta eol_warning);

    MinimumVersionRequirement(const MinimumVersionRequirement&) = delete;

    // Method used to create an instance of MinimumVersionRequirement from
    // dictionary if it contains valid version string.
    static std::unique_ptr<MinimumVersionRequirement> CreateInstanceIfValid(
        const base::Value::Dict& dict);

    // This is used to compare two MinimumVersionRequirement objects
    // and returns 1 if the first object has version or warning time
    // or eol warning time greater than that the second, -1 if the
    // its version or warning time or eol warning time is less than the second,
    // else 0.
    int Compare(const MinimumVersionRequirement* other) const;

    base::Version version() const { return minimum_version_; }
    base::TimeDelta warning() const { return warning_time_; }
    base::TimeDelta eol_warning() const { return eol_warning_time_; }

   private:
    base::Version minimum_version_;
    base::TimeDelta warning_time_;
    base::TimeDelta eol_warning_time_;
  };

  enum class NetworkStatus { kAllowed, kMetered, kOffline };

  enum class NotificationType {
    kMeteredConnection,
    kNoConnection,
    kEolReached
  };

  explicit MinimumVersionPolicyHandler(Delegate* delegate,
                                       ash::CrosSettings* cros_settings);

  MinimumVersionPolicyHandler(const MinimumVersionPolicyHandler&) = delete;
  MinimumVersionPolicyHandler& operator=(const MinimumVersionPolicyHandler&) =
      delete;

  ~MinimumVersionPolicyHandler() override;

  // BuildStateObserver:
  void OnUpdate(const BuildState* build_state) override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const ash::NetworkState* network) override;
  void OnShuttingDown() override;

  // UpdateEngineClient::Observer:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool RequirementsAreSatisfied() const { return GetState() == nullptr; }

  const MinimumVersionRequirement* GetState() const { return state_.get(); }

  bool DeadlineReached() { return deadline_reached_; }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Show notification on login if the user is managed or
  // |unmanaged_user_restricted_| is set to true if it is the last day to
  // deadline.
  void MaybeShowNotificationOnLogin();

  // Whether banner to return back the device should be visible in Settings. It
  // is true when an update is required on a device that has reached End Of Life
  // (Auto Update Expiration) and the currently signed in user is enterprise
  // managed or an unmanaged user restricted by DeviceMinimumVersion policy.
  bool ShouldShowUpdateRequiredEolBanner() const;

  // Returns the number of days to deadline if update is required and deadline
  // has not been reached. Returns null if update is not required.
  std::optional<int> GetTimeRemainingInDays();

  // Callback used in tests and invoked after end-of-life status has been
  // fetched from the update_engine.
  void set_fetch_eol_callback_for_testing(base::OnceClosure callback) {
    fetch_eol_callback_ = std::move(callback);
  }

  base::Time update_required_deadline_for_testing() const {
    return update_required_deadline_;
  }

  bool IsDeadlineTimerRunningForTesting() const;

 private:
  // Returns |true| if the current version satisfies the given requirement.
  bool CurrentVersionSatisfies(
      const MinimumVersionRequirement& requirement) const;

  // Whether the current user should receive update required notifications and
  // force signed out on reaching the deadline. Retuns true if the user is
  // enterprise managed or |unmanaged_user_restricted_| is true.
  bool IsPolicyRestrictionAppliedForUser() const;

  void OnPolicyChanged();
  bool IsPolicyApplicable();
  void Reset();

  // Handles post update completed actions like reset timers, hide update
  // required notification and stop observing build state.
  void ResetOnUpdateCompleted();

  // Handles the state when update is required as per the policy. If on the
  // login screen, update required screen is shown, else the user is logged out
  // if the device is not updated within the given |warning_time|. The
  // |warning_time| is supplied by the policy.
  void HandleUpdateRequired(base::TimeDelta warning_time);

  void HandleUpdateNotRequired();

  // Invokes update_engine_client to fetch the end-of-life status of the device.
  void FetchEolInfo();

  // Callback after fetching end-of-life info from the update_engine_client.
  void OnFetchEolInfo(ash::UpdateEngineClient::EolInfo info);

  // Called when the warning time to apply updates has expired. If the user on
  // the login screen, the update required screen is shown else the current user
  // session is terminated to bring the user back to the login screen.
  void OnDeadlineReached();

  // Starts the timer to expire when |deadline| is reached.
  void StartDeadlineTimer(base::Time deadline);

  // Starts observing the BuildState for any updates in Chrome OS and resets the
  // state if new version satisfies the minimum version requirement.
  void StartObservingUpdate();

  // Shows notification for a managed logged in user if update is required and
  // the device can not be updated due to end-of-life or network limitations.
  void MaybeShowNotification(base::TimeDelta warning);

  // Shows notification if required and starts a timer to expire when the next
  // notification is to be shown.
  void ShowAndScheduleNotification(base::Time deadline);

  void HideNotification() const;

  void StopObservingNetwork();

  // Start updating over metered network as user has given consent through
  // notification click.
  void UpdateOverMeteredPermssionGranted();

  // Tells whether starting an update check succeeded or not.
  void OnUpdateCheckStarted(ash::UpdateEngineClient::UpdateCheckResult result);

  // Callback from UpdateEngineClient::SetUpdateOverCellularOneTimePermission().
  void OnSetUpdateOverCellularOneTimePermission(bool success);

  // Updates pref |kUpdateRequiredWarningPeriod| in local state to
  // |warning_time|. If |kUpdateRequiredTimerStartTime| is not null, it means
  // update is already required and hence, the timer start time should not be
  // updated.
  void UpdateLocalState(base::TimeDelta warning_time);

  // Resets the local state prefs to default values.
  void ResetLocalState();

  // This delegate instance is owned by the owner of
  // MinimumVersionPolicyHandler. The owner is responsible to make sure that the
  // delegate lives throughout the life of the policy handler.
  raw_ptr<Delegate> delegate_;

  // This represents the current minimum version requirement.
  // It is chosen as one of the configurations specified in the policy. It is
  // set to nullptr if the current version is higher than the minimum required
  // version in all the configurations.
  std::unique_ptr<MinimumVersionRequirement> state_;

  // If this flag is true, unmanaged user sessions receive update required
  // notifications and are force logged out when deadline is reached.
  bool unmanaged_user_restricted_ = false;

  bool eol_reached_ = false;

  // If this flag is true, user should restricted to use the session by logging
  // out and/or showing update required screen.
  bool deadline_reached_ = false;

  // Time when the policy is applied and with respect to which the deadline to
  // update the device is calculated.
  base::Time update_required_time_;

  // Deadline for updating the device post which the user is restricted from
  // using the session by force log out if a session is active and then blocking
  // sign in at the login screen.
  base::Time update_required_deadline_;

  // Fires when the deadline to update the device has reached or passed.
  base::WallClockTimer update_required_deadline_timer_;

  // Fires when next update required notification is to be shown.
  base::WallClockTimer notification_timer_;

  // Non-owning reference to CrosSettings. This class have shorter lifetime than
  // CrosSettings.
  raw_ptr<ash::CrosSettings> cros_settings_;

  const raw_ptr<base::Clock> clock_;

  base::OnceClosure fetch_eol_callback_;

  base::CallbackListSubscription policy_subscription_;

  // Handles showing in-session update required notifications on the basis of
  // current network and time to reach the deadline.
  std::unique_ptr<ash::UpdateRequiredNotification> notification_handler_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // List of registered observers.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<MinimumVersionPolicyHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_H_
