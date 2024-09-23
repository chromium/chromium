// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace metrics {

// State manager for per-user metrics collection. Listens to user
// creation/deletion and manages user prefs accordingly.
//
// When a particular user is created (login), all metrics collected so far
// will be flushed to local state. Before a user is destroyed (logout), all
// metrics will be flushed to the user dir.
//
// It is assumed that there can only be at most one user logged in at once. This
// assumption is only true in Ash Chrome.
//
// This class integrates into the MetricsService in order to separately handle
// UMA reporting consent for unmanaged secondary users. Profile prefs are
// used to handle the consent for each user. These profile settings interact
// with the local state pref that controls the overall device reporting
// consent, and UMA uploading logic.
//
// Ownership status needs to be asynchronously retrieved first in order to know
// whether the device has no ownership yet, or whether the device is owned and
// we are controlling consent for a secondary user.
//
// This class does not manage the device owner reporting consent.
// Device owner consent is handled separately by
// |ash::StatsReportingController|. In the future, we may want to consider
// simplifying the code by using a single class to manage both device owner
// consent and secondary user consent.
class PerUserStateManagerChromeOS
    : public user_manager::UserManager::UserSessionStateObserver,
      public user_manager::UserManager::Observer,
      public ash::SessionTerminationManager::Observer {
 public:
  // Callback to handle changes in user metrics consent.
  using MetricsConsentHandler = base::RepeatingCallback<void(bool)>;

  // Does not own params passed by pointer. Caller should ensure that the
  // lifetimes of the weak pointers exceed that of |this|.
  PerUserStateManagerChromeOS(
      MetricsServiceClient* metrics_service_client,
      user_manager::UserManager* user_manager,
      PrefService* local_state,
      const MetricsLogStore::StorageLimits& storage_limits,
      const std::string& signing_key);

  // Does not own |metrics_service_client| and |local_state|. Lifetime of
  // these raw pointers should be handled by the caller.
  PerUserStateManagerChromeOS(MetricsServiceClient* metrics_service_client,
                              PrefService* local_state);

  PerUserStateManagerChromeOS(const PerUserStateManagerChromeOS& other) =
      delete;
  PerUserStateManagerChromeOS& operator=(
      const PerUserStateManagerChromeOS& other) = delete;
  ~PerUserStateManagerChromeOS() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the user_id of the current logged in user. If no user is logged in,
  // returns std::nullopt. If a user has logged in and has opted-out, will
  // return std::nullopt.
  //
  // If the user has opted-into metrics collection and is not ephemeral, then
  // this will return the pseudo-anonymous identifier associated with the user.
  std::optional<std::string> GetCurrentUserId() const;

  // Returns the consent of the current logged in user only if current user's
  // consent should be applied to metrics reporting.
  //
  // The cases in which this occurs are:
  //
  //    1) Regular non-owner users on non-managed devices.
  //    2) Guest users.
  //
  // If no user is logged in, returns std::nullopt. True means that the user
  // has opted-into metrics collection during the session and False means that
  // the user has opted-out.
  std::optional<bool> GetCurrentUserReportingConsentIfApplicable() const;

  // Sets the metric consent for the current logged in user. If no user is
  // logged in, no-ops.
  //
  // This method will reset the client id if a user toggles from a non-consent
  // to consent state AND the user had consented to metrics collection in the
  // past. This is to preserve the pseudo-anonymity of <user_id, client_id>
  // identifier.
  //
  // This call should be used to toggle consent from the UI or during OOBE flow
  // for the current user.
  void SetCurrentUserMetricsConsent(bool metrics_consent);

  // Returns true if |user| should have the ability to toggle user metrics
  // collection for themselves.
  //
  // This will return false for managed device users as well as guest users.
  bool IsUserAllowedToChangeConsent(user_manager::User* user) const;

  // Adds an observer |callback| to be called when a user consent should be
  // applied. This happens either when an applicable user logs in or an
  // applicable user changes metrics consent.
  base::CallbackListSubscription AddObserver(
      const MetricsConsentHandler& callback);

  // Sets behavior of IsReportingPolicyManaged() for testing.
  //
  // TODO(crbug/1269950): Investigate why ash::LoginManagerTest does not work
  // with ash::ScopedStubInstallAttributes. Remove this function once resolved
  // as it is hack to force PerUserStateManagerChromeOS to return a fixed value.
  static void SetIsManagedForTesting(bool is_managed);

  // Resets the logged in user state for testing.
  void ResetStateForTesting();

 protected:
  // These methods are marked virtual to stub out for testing.

  // Sets the user log store to use |log_store|. Default uses
  // |metrics_service_client_| implementation.
  virtual void SetUserLogStore(std::unique_ptr<UnsentLogStore> log_store);

  // Unsets the user log store. Default uses |metrics_service_client_|
  // implementation.
  virtual void UnsetUserLogStore();

  // Resets the client ID. Should be called when user consent is turned off->on
  // and the user has opted-in metrics consent in the past. Default uses
  // |metrics_service_client_| implementation.
  virtual void ForceClientIdReset();

  // Returns true if the reporting policy is managed.
  virtual bool IsReportingPolicyManaged() const;

  // Returns true if user log store has been set to be used to persist metric
  // logs.
  virtual bool HasUserLogStore() const;

  // Returns true if the device is owned either by a policy or a local owner.
  //
  // Does not guarantee that the ownership status is known and will return false
  // if the status is unknown.
  //
  // See //chrome/browser/ash/settings/device_settings_service.h for more
  // details as to when a device is considered owned and how a device becomes
  // owned.
  virtual bool IsDeviceOwned() const;

  // Returns true if the device status is known.
  virtual bool IsDeviceStatusKnown() const;

  // These methods are protected to avoid dependency on DeviceSettingsService
  // during testing.

  // Ensures that ownership status is known before proceeding with using
  // profile prefs.
  virtual void WaitForOwnershipStatus();

  // Returns true if a user log store in the user cryptohome should be used for
  // the current logged in user.
  // Certain users (ie demo mode sessions with metrics consent on) should not
  // use a user log store since the user log store will be stored on the
  // temporary cryptohome and will be deleted at the end of the session.
  // Demo mode sessions with metric consent on should be stored in local state
  // to be persistent.
  bool ShouldUseUserLogStore() const;

  // Loads appropriate prefs from |current_user_| and creates new log storage
  // using profile prefs.
  //
  // Will only be called when OwnershipStatus is known. This guarantees that
  // we avoid race conditions where the ownership status is still unknown due
  // to policy fetch on browser restart.
  // The status will either be kOwnershipNone, or kOwnershipTaken.
  void InitializeProfileMetricsState(
      ash::DeviceSettingsService::OwnershipStatus status);

 private:
  // Possible states for |this|.
  enum class State {
    // Right after construction.
    CONSTRUCTED = 0,

    // ActiveUserChanged on non-trivial user. The profile for the user is not
    // immediately created.
    USER_LOGIN = 1,

    // User profile has been created and ready to use.
    USER_PROFILE_READY = 2,

    // User log store has been initialized, if applicable. Per-user consent
    // now can be changed if the user has permissions to change consent.
    //
    // Terminal state.
    USER_LOG_STORE_HANDLED = 3,
  };

  // UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // UserManager::Observer:
  void OnUserToBeRemoved(const AccountId& account_id) override;

  // ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;

  // Updates the current user ID to |new_user_id|. Updates both the profile pref
  // as well as local state pref.
  void UpdateCurrentUserId(const std::string& new_user_id);

  // Resets the state of |this| to that of no logged in user.
  void ResetState();

  // Returns the prefs for the current logged in user.
  PrefService* GetCurrentUserPrefs() const;

  // Builds a unsent log store for |current_user_| and assigns it to be used as
  // the primary log store for ongoing logs.
  void AssignUserLogStore();

  // Sets the reporting state for metrics collection. Notifies observers that
  // user metrics consent has changed to |metrics_consent|.
  void SetReportingState(bool metrics_consent);

  // Notifies observers of the per-user state change |metrics_consent|.
  void NotifyObservers(bool metrics_consent);

  // Updates local state prefs based on |metrics_enabled|. If |metrics_enabled|
  // is true,
  //
  //    1) Client ID will be reset if the user has ever had metrics reporting
  //       enabled. This is to preserve the pseudo-anonymous identifier
  //       <client_id, user_id>.
  void UpdateLocalStatePrefs(bool metrics_enabled);

  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingCallbackList<void(bool)> callback_list_;

  // Raw pointer to Metrics service client that should own |this|.
  const raw_ptr<MetricsServiceClient> metrics_service_client_;

  // Raw pointer to user manager. User manager is used to listen to login/logout
  // events as well as retrieve metadata about users. |user_manager_| should
  // outlive |this|.
  const raw_ptr<user_manager::UserManager> user_manager_;

  // Raw pointer to local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // Logs parameters that control log storage requirements and restrictions.
  const MetricsLogStore::StorageLimits storage_limits_;

  // Signing key to encrypt logs.
  const std::string signing_key_;

  // Pointer to the current logged-in user.
  raw_ptr<user_manager::User> current_user_ = nullptr;

  // Current state for |this|.
  State state_ = State::CONSTRUCTED;

  // Task runner. Used to persist state to daemon-store.
  scoped_refptr<base::SequencedTaskRunner> task_runner_ = nullptr;

  base::WeakPtrFactory<PerUserStateManagerChromeOS> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_
