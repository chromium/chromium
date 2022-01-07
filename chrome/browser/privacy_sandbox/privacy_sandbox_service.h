// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace content_settings {
class CookieSettings;
}

// Service which encapsulates logic related to displaying and controlling the
// users Privacy Sandbox settings. This service contains the chrome/ specific
// logic used by the UI, including decision making around what the users'
// Privacy Sandbox settings should be based on their existing settings.
// Ultimately the decisions made by this service are consumed (through
// preferences and content settings) by the PrivacySandboxSettings located in
// components/privacy_sandbox/, which in turn makes them available to Privacy
// Sandbox APIs.
class PrivacySandboxService : public KeyedService,
                              public policy::PolicyService::Observer,
                              public syncer::SyncServiceObserver,
                              public signin::IdentityManager::Observer {
 public:
  PrivacySandboxService(PrivacySandboxSettings* privacy_sandbox_settings,
                        content_settings::CookieSettings* cookie_settings,
                        PrefService* pref_service,
                        policy::PolicyService* policy_service,
                        syncer::SyncService* sync_service,
                        signin::IdentityManager* identity_manager,
                        federated_learning::FlocIdProvider* floc_id_provider);
  ~PrivacySandboxService() override;

  // Returns a description of FLoC ready for display to the user. Correctly
  // takes into account the FLoC feature parameters when determining the number
  // of days between cohort calculations.
  std::u16string GetFlocDescriptionForDisplay() const;

  // Returns the current FLoC cohort identifier for the associated profile in
  // string format suitable for direct display to the user. If the cohort is
  // not valid, the appropriate descriptive string is returned instead.
  std::u16string GetFlocIdForDisplay() const;

  // Returns when the user's current FLoC cohort identifier will next be updated
  // in a string format suitable for direct display to the user. If no compute
  // is scheduled, the appropriate descriptive string is returned instead.
  std::u16string GetFlocIdNextUpdateForDisplay(const base::Time& current_time);

  // Returns the display ready string explaining what happens when the user
  // resets the FLoC cohort identifier.
  std::u16string GetFlocResetExplanationForDisplay() const;

  // Returns a display ready string explaining the current status of FloC. E.g.
  // the effective state of the Finch experiment, and the user's setting.
  std::u16string GetFlocStatusForDisplay() const;

  // Returns whether the user's current FLoC ID can be reset. This requires that
  // the FLoC feature be enabled and FLoC be enabled in preferences. It does not
  // require that the current ID is valid, as resetting the ID also resets the
  // compute timer, it should be available whenever FLoC is active.
  bool IsFlocIdResettable() const;

  // Sets the time when history is accessible for FLoC calculation to the
  // current time and resets the time to the next FLoC id calculation. If
  // |user_initiated| is true, records the associated User Metrics Action.
  void ResetFlocId(bool user_initiated) const;

  // Returns whether the FLoC preference is enabled. This should only be used
  // for displaying the preference state to the user, and should *not* be used
  // for determining whether FLoC is allowed or not.
  bool IsFlocPrefEnabled() const;

  // Sets the FLoC preference to |enabled|.
  void SetFlocPrefEnabled(bool enabled) const;

  // Disables the Privacy Sandbox completely if |enabled| is false, if |enabled|
  // is true, more granular checks will still be performed to determine if
  // specific APIs are available in specific contexts.
  void SetPrivacySandboxEnabled(bool enabled);

  // Used by the UI to check if the API is enabled. Checks the primary
  // pref directly.
  bool IsPrivacySandboxEnabled();

  // Returns whether the state of the API is managed.
  bool IsPrivacySandboxManaged();

  // Called when a preference relevant to the the Privacy Sandbox is changed.
  void OnPrivacySandboxPrefChanged();

  // KeyedService:
  void Shutdown() override;

  // policy::PolicyService::Observer:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

  // signin::IdentityManager::Observer:
  // TODO(crbug.com/1167680): This is only required to capture failure scenarios
  // that affect sync, yet aren't reported via SyncServiceObserver.
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ReconciliationOutcome);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationNoSync);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationSyncComplete);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationPersistentSyncError);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationNoDisable);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncSuccess);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationIdentityFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncIssueThenManaged);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           NoReconciliationAlreadyRun);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           NoReconciliationSandboxSettingsDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           MetricsLoggingOccursCorrectly);

  // Contains all possible privacy sandbox states, recorded on startup.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the SettingsPrivacySandboxEnabled enum in
  // histograms/enums.xml.
  enum class SettingsPrivacySandboxEnabled {
    kPSEnabledAllowAll = 0,
    kPSEnabledBlock3P = 1,
    kPSEnabledBlockAll = 2,
    kPSDisabledAllowAll = 3,
    kPSDisabledBlock3P = 4,
    kPSDisabledBlockAll = 5,
    kPSDisabledPolicyBlock3P = 6,
    kPSDisabledPolicyBlockAll = 7,
    kPSEnabledFlocDisabledAllowAll = 8,
    kPSEnabledFlocDisabledBlock3P = 9,
    kPSEnabledFlocDisabledBlockAll = 10,
    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kPSEnabledFlocDisabledBlockAll,
  };

  // Inspects the current sync state and settings to determine if the Privacy
  // Sandbox prefs should be reconciled. Calls ReconcilePrivacySandbox()
  // immediately if appropriate, or may register sync and identity observers to
  // call ReconcilePrivacySandbox() later as appropriate.
  void MaybeReconcilePrivacySandboxPref();

  // Selectively disable the Privacy Sandbox preference based on the local and
  // synced state. Reconcilliation is only performed once per synced profile.
  // As the sandbox is default enabled, reconcilliation will only ever opt a
  // user out of the sandbox.
  void ReconcilePrivacySandboxPref();

  // Stops any observation of services being performed by this class.
  void StopObserving();

  // Helper function to actually make the metrics call for
  // LogPrivacySandboxState.
  void RecordPrivacySandboxHistogram(SettingsPrivacySandboxEnabled state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

 private:
  raw_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<policy::PolicyService> policy_service_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<federated_learning::FlocIdProvider> floc_id_provider_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  PrefChangeRegistrar user_prefs_registrar_;

  // A manual record of whether policy_service_ is being observerd.
  // Unfortunately PolicyService does not support scoped observers.
  bool policy_service_observed_ = false;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
