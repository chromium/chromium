// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class PerUserStateManagerChromeOS
    : public user_manager::UserManager::UserSessionStateObserver,
      public user_manager::UserManager::Observer {
 public:
  // Does not own params passed by pointer. Caller should ensure that the
  // lifetimes of the weak pointers exceed that of |this|.
  PerUserStateManagerChromeOS(
      MetricsServiceClient* metrics_service_client_,
      user_manager::UserManager* user_manager,
      PrefService* local_state,
      const MetricsLogStore::StorageLimits& storage_limits,
      const std::string& signing_key);

  // Does not own |metrics_service_client| and |local_state|. Lifetime of
  // these raw pointers should be managed by the caller.
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
  // returns absl::nullopt.
  //
  // If the user has opted-into metrics collection and is not ephemeral, then
  // this will return the pseudo-anonymous identifier associated with the user.
  // If the user has opted-out, then this will return empty string instead of
  // absl::nullopt since it is used to write to the pref.
  absl::optional<std::string> GetCurrentUserId() const;

  // Returns the consent of the current logged in user. If no user is logged in,
  // returns absl::nullopt. True means that the user has opted-into metrics
  // collection during the session and False means that the user has opted-out.
  absl::optional<bool> GetCurrentUserConsent() const;

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
  //
  // Initialization must be deferred in some cases since the initial consent
  // will determine where the logs will be stored. If
  // ShouldWaitForFirstConsent() is true for the current user, then
  // initialization will be deferred until this function is called at least
  // once. For specific cases, refer to documentation in
  // ShouldWaitForFirstConsent().
  void SetCurrentUserMetricsConsent(bool metrics_consent);

  // Returns true if a user log store in the user cryptohome should be used for
  // the current logged in user.
  //
  // Certain users (ie ephemeral sessions with metrics consent on) should not
  // use a user log store since the user log store will be stored on the
  // temporary cryptohome and will be deleted at the end of the session.
  // Ephemeral sessions with metric consent on should be stored in local state
  // to be persistent.
  bool ShouldUseUserLogStore() const;

  // Returns true if |user| should have the ability to toggle user metrics
  // collection for themselves.
  //
  // This will return false for managed device users as well as guest users.
  bool IsUserAllowedToChangeConsent(user_manager::User* user) const;

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

  // Sets the reporting state for metrics collection. Default uses
  // |metrics_service_client_| implementation. Should not be called if
  // IsReportingPolicyManaged() is true.
  virtual void SetReportingState(bool metrics_consent);

  // Returns true if the reporting policy is managed.
  virtual bool IsReportingPolicyManaged() const;

  // Returns the device metrics consent. If ownership has not been taken, will
  // return false.
  virtual bool GetDeviceMetricsConsent() const;

  // Returns true if user log store has been set to be used to persist metric
  // logs.
  virtual bool HasUserLogStore() const;

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

    // Waiting on the first consent to determine if a user log store should be
    // used or not.
    //
    // Guest sessions before device consent is set should use log store if guest
    // session disables metrics reporting since the temporary cryptohome
    // partition will be deleted at the end of the guest session.
    //
    // This state is skipped for all other users.
    WAITING_ON_FIRST_CONSENT = 3,

    // User log store has been initialized, if applicable. Per-user consent
    // now can be changed if the user has permissions to change consent.
    //
    // Terminal state.
    USER_LOG_STORE_HANDLED = 4,
  };

  // UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // UserManager::Observer:
  void OnUserToBeRemoved(const AccountId& account_id) override;

  // Loads appropriate prefs from |current_user_| and creates new log storage
  // using profile prefs.
  void InitializeProfileMetricsState();

  // Called when the metrics consent is set for the first time and
  // |ShouldWaitForFirstConsent()| returns true.
  //
  // Sets the log store if |metrics_consent| is false. If true, then logs should
  // be written to local state to be persisted.
  void OnFirstConsent(bool metrics_consent);

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

  // Returns true if log store creation should be deferred until first consent
  // is set. This will return true if |current_user_| is a guest and the device
  // has no owner and is not managed.
  //
  // Normally, a guest session's metrics consent is determined by the owner's
  // consent and whether to use the user log store or local state log store can
  // be immediately determined at the start of a session. However, if no owner
  // is set, this cannot be determined immediately and initialization of which
  // log store to use for the session will be deferred until the guest user has
  // selected metrics consent during OOBE flow.
  bool ShouldWaitForFirstConsent() const;

  // Raw pointer to Metrics service client that should own |this|.
  MetricsServiceClient* const metrics_service_client_;

  // Raw pointer to user manager. User manager is used to listen to login/logout
  // events as well as retrieve metadata about users. |user_manager_| should
  // outlive |this|.
  user_manager::UserManager* const user_manager_;

  // Raw pointer to local state prefs store.
  PrefService* const local_state_;

  // Logs parameters that control log storage requirements and restrictions.
  const MetricsLogStore::StorageLimits storage_limits_;

  // Signing key to encrypt logs.
  const std::string signing_key_;

  // Pointer to the current logged-in user.
  user_manager::User* current_user_ = nullptr;

  // Current state for |this|.
  State state_ = State::CONSTRUCTED;

  base::WeakPtrFactory<PerUserStateManagerChromeOS> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PER_USER_STATE_MANAGER_CHROMEOS_H_
