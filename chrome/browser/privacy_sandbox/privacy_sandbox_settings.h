// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "net/cookies/cookie_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class HostContentSettingsMap;
class PrefService;

namespace content_settings {
class CookieSettings;
}

namespace url {
class Origin;
}

// A service which acts as a intermediary between Privacy Sandbox APIs and the
// preferences and content settings which define when they are allowed to be
// accessed.
// TODO (crbug.com/1154686): Move this and other Privacy Sandbox items into
// components.
class PrivacySandboxSettings : public KeyedService,
                               public policy::PolicyService::Observer,
                               public syncer::SyncServiceObserver,
                               public signin::IdentityManager::Observer {
 public:
  class Observer {
   public:
    virtual void OnFlocDataAccessibleSinceUpdated(bool reset_compute_timer) = 0;
  };

  PrivacySandboxSettings(HostContentSettingsMap* host_content_settings_map,
                         content_settings::CookieSettings* cookie_settings,
                         PrefService* pref_service,
                         policy::PolicyService* policy_service,
                         syncer::SyncService* sync_service,
                         signin::IdentityManager* identity_manager);
  ~PrivacySandboxSettings() override;

  // Returns true when the privacy sandbox settings feature is enabled. This
  // function, rather than direct inspection of the feature itself, should be
  // used to determine if the privacy sandbox is available to users.
  // TODO(crbug.com/1174572) Remove this when one API is fully launched.
  static bool PrivacySandboxSettingsFunctional();

  // Returns whether FLoC is allowed at all. If false, FLoC calculations should
  // not occur. If true, the more specific function, IsFlocAllowedForContext(),
  // should be consulted for the relevant context.
  bool IsFlocAllowed() const;

  // Determines whether FLoC is allowable in a particular context.
  // |top_frame_origin| is used to check for content settings which could both
  // affect 1P and 3P contexts.
  bool IsFlocAllowedForContext(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin) const;

  // Returns the point in time from which history is eligible to be used when
  // calculating a user's FLoC ID. Reset when a user clears all cookies, or
  // when the browser restarts with "Clear on exit" enabled. The returned time
  // will have been fuzzed for local privacy, and so may be in the future, in
  // which case no history is eligible.
  base::Time FlocDataAccessibleSince() const;

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
  // This is static to break a circular dependency between this service, and
  // the FlocIdProvider service. This pushes the requirement to the caller to
  // ensure the profile is not in a shutdown state before calling.
  // TODO(crbug.com/1210405): This is indicative of an architecture issue and
  // should be changed or broken out into an explicit helper.
  static std::u16string GetFlocIdNextUpdateForDisplay(
      federated_learning::FlocIdProvider* floc_id_provider,
      PrefService* pref_service,
      const base::Time& current_time);

  // Returns the display ready string explanaing what happens when the user
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
  // current time and resets the time to the next FLoC id calculation
  void ResetFlocId() const;

  // Returns whether the FLoC preference is enabled. This should only be used
  // for displaying the preference state to the user, and should *not* be used
  // for determining whether FLoC is allowed or not.
  bool IsFlocPrefEnabled() const;

  // Sets the FLoC preference to |enabled|.
  void SetFlocPrefEnabled(bool enabled) const;

  // Determines whether Conversion Measurement is allowable in a particular
  // context. Should be called at both impression & conversion. At each of these
  // points |top_frame_origin| is the same as either the impression origin or
  // the conversion origin respectively.
  bool IsConversionMeasurementAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const;

  // Called before sending the associated conversion report to
  // |reporting_origin|. Re-checks that |reporting_origin| is allowable as a 3P
  // on both |impression_origin| and |conversion_origin|.
  bool ShouldSendConversionReport(const url::Origin& impression_origin,
                                  const url::Origin& conversion_origin,
                                  const url::Origin& reporting_origin) const;

  // Determine whether |auction_party| can register an interest group, or sell /
  // buy in an auction, on |top_frame_origin|.
  bool IsFledgeAllowed(const url::Origin& top_frame_origin,
                       const GURL& auction_party);

  // Filter |auction_parties| down to those that may participate as a buyer for
  // auctions run on |top_frame_origin|. Logically equivalent to calling
  // IsFledgeAllowed() for each element of |auction_parties|.
  std::vector<GURL> FilterFledgeAllowedParties(
      const url::Origin& top_frame_origin,
      const std::vector<GURL>& auction_parties);

  // Returns whether the Privacy Sandbox is "generally" available. A return
  // value of false indicates that the sandbox is completely disabled. A return
  // value of true *must* be followed up by the appropriate context specific
  // check. If the sandbox experiment is disabled, this check is equivalent to
  // |!cookie_settings_->ShouldBlockThirdPartyCookies()|; but if the experiment
  // is enabled, this will check prefs::kPrivacySandboxApisEnabled instead.
  bool IsPrivacySandboxAllowed();

  // Used by the UI to check if the API is enabled. Unlike the method above,
  // this method only checks the pref directly.
  bool IsPrivacySandboxEnabled();

  // Returns whether the state of the API is managed.
  bool IsPrivacySandboxManaged();

  // Gets invoked by the UI when the user manually changed the state of the API.
  void SetPrivacySandboxEnabled(bool enabled);

  // Called when there's a broad cookies clearing action. For example, this
  // should be called on "Clear browsing data", but shouldn't be called on the
  // Clear-Site-Data header, as it's restricted to a specific site.
  void OnCookiesCleared();

  // Called when a preference relevant to the the Privacy Sandbox is changed.
  void OnPrivacySandboxPrefChanged();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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
  friend class PrivacySandboxSettingsTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest, ReconciliationOutcome);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           ImmediateReconciliationNoSync);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           ImmediateReconciliationSyncComplete);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           ImmediateReconciliationPersistentSyncError);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           ImmediateReconciliationNoDisable);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           DelayedReconciliationSyncSuccess);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           DelayedReconciliationSyncFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           DelayedReconciliationIdentityFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           DelayedReconciliationSyncIssueThenManaged);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           NoReconciliationAlreadyRun);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           NoReconciliationSandboxSettingsDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
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

  // Determines based on the current features, preferences and provided
  // |cookie_settings| whether Privacy Sandbox APIs are generally allowable for
  // |url| on |top_frame_origin|. Individual APIs may perform additional checks
  // for allowability (such as incognito) ontop of this. |cookie_settings| is
  // provided as a parameter to allow callers to cache it between calls.
  bool IsPrivacySandboxAllowedForContext(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin,
      const ContentSettingsForOneType& cookie_settings) const;

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

  // Sets the time when history is accessible for FLoC calculation to the
  // current time, optionally resetting the time to the next FLoC id calculation
  // if |reset_calculate_timer| is true.
  void SetFlocDataAccessibleFromNow(bool reset_calculate_timer) const;

  // Stops any observation of services being performed by this class.
  void StopObserving();

  // Helper function to actually make the metrics call for
  // LogPrivacySandboxState.
  void RecordPrivacySandboxHistogram(SettingsPrivacySandboxEnabled state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  HostContentSettingsMap* host_content_settings_map_;
  content_settings::CookieSettings* cookie_settings_;
  PrefService* pref_service_;
  policy::PolicyService* policy_service_;
  syncer::SyncService* sync_service_;
  signin::IdentityManager* identity_manager_;

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

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
