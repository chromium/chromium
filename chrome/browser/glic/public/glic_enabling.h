// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;
class ProfileAttributesStorage;

namespace glic {
namespace prefs {
enum class SettingsPolicyState;
}
namespace mojom {
// TODO(crbug.com/406500707): This forward declaration is needed because we use
// allow_circular_includes_from. Our build rules should be refactored to avoid
// this.
enum class ProfileReadyState : int32_t;
}  // namespace mojom

// This class provides a central location for checking if GLIC is enabled. It
// allows for future expansion to include other ways the feature may be disabled
// such as based on user preferences or system settings.
//
// There are multiple notions of "enabled". The highest level is
// IsEnabledByFlags which controls whether any global-Glic infrastructure is
// created. If flags are off, nothing Glic-related should be created.
//
// If flags are enabled, various global objects are created as well as a
// GlicKeyedService for each "eligible" profile. Eligible profiles exclude
// incognito, guest mode, system profile, etc. i.e. include only real user
// profiles. An eligible profile will create various Glic decorations and views
// for the profile's browser windows, regardless of whether Glic is actually
// "enabled" for the given profile. If disabled, those decorations should remain
// inert.  The GlicKeyedService is created for all eligible profiles so it can
// listen for changes to prefs which control the per-profile Glic-Enabled state.
//
// Finally, an eligible profile may be Glic-Enabled. In this state, Glic UI is
// visible and usable by the user. This state can change at runtime so Glic
// entry points should depend on this state.
class GlicEnabling : public signin::IdentityManager::Observer {
 public:
  // Returns whether the global Glic feature is enabled for Chrome. This status
  // will not change at runtime.
  static bool IsEnabledByFlags();

  // Some profiles - such as incognito, guest, system profile, etc. - are never
  // eligible to use Glic. This function returns true if a profile is eligible
  // for Glic, that is, it can potentially be enabled, regardless of whether it
  // is currently enabled or not. Always returns false if IsEnabledByFlags is
  // off. This will never change for a given profile.
  static bool IsProfileEligible(const Profile* profile);

  // This is a convenience method for code outside of //chrome/browser/glic.
  // Code inside should use instance method IsAllowed() instead.
  static bool IsEnabledForProfile(Profile* profile);

  // Returns true if the profile has completed the FRE.
  static bool HasConsentedForProfile(Profile* profile);

  // Returns true if the given profile has Glic enabled and has completed the
  // FRE. True implies that IsEnabledByFlags(), IsProfileEligible(profile), and
  // IsEnabledForProfile(profile) are also true. This value can change at
  // runtime.
  static bool IsEnabledAndConsentForProfile(Profile* profile);

  // Returns true if the given profile was shown the FRE but did not complete
  // it. This value can change at runtime.
  static bool DidDismissForProfile(Profile* profile);

  // Whether or not the profile is currently ready for Glic. This means no
  // additional steps must be taken before opening Glic.
  static bool IsReadyForProfile(Profile* profile);

  // Same as IsReadyForProfile, but returns a more detailed state.
  static mojom::ProfileReadyState GetProfileReadyState(Profile* profile);

  // Whether the profile is in the glic tiered rollout population.
  static bool IsEligibleForGlicTieredRollout(Profile* profile);

  // The settings page is shown when:
  // * Flags are enabled
  // * The profile is eligible (regular, non-incognito, non-guest, etc.)
  // * The profile has model execution privileges
  // * The profile has completed the first run experience
  static bool ShouldShowSettingsPage(Profile* profile);

  // Whether the FRE screen is displayed in the same window as the chat app.
  static bool IsUnifiedFreEnabled(Profile* profile);

  // Whether the required feature flags for multi-instance - kGlicMultiInstance,
  // kGlicMultiTab, and kGlicMultitabUnderlines - are enabled. When calling, be
  // sure that IsMultiInstanceEnabled() should not be used instead.
  static bool IsMultiInstanceEnabledByFlags();

  // Returns true if glic is enabled for the profile, the feature is enabled,
  // and the account is non-enterprise (or for glic dev).
  static bool IsShareImageEnabledForProfile(Profile* profile);

  // Whether the required feature flags for multi-instance are enabled, or
  // multi-instance should be enabled due to subscription tier. This serves as
  // the default enablement check for the multi-instance feature and should be
  // used in most cases.
  static bool IsMultiInstanceEnabled();

  // Whether the result of
  // `GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout` was true the
  // first time this function was called during the current run of Chrome.
  static bool IsEligibleForGlicMultiInstanceTieredRolloutThisRun();

  // Whether any loaded profile is, or has ever been, of a subscription tier
  // that should enable multi-instance. `additional_profile` may be provided by
  // the caller in case it has not been fully loaded.
  // NOTE: new usages of this API should be extremely limited. Checking the
  // feature enablement of multi-instance should go through
  // IsMultiInstanceEnabled() instead. Please contact @cuianthony before using.
  static bool GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
      Profile* additional_profile);

  struct ProfileEnablement {
    // These conditions are checked first and may prevent following checks from
    // occurring.
    bool feature_disabled : 1 = false;
    bool not_regular_profile : 1 = false;

    // These are checked separately, so may be present in various combinations.
    bool not_rolled_out : 1 = false;
    bool primary_account_not_capable : 1 = false;
    bool disallowed_by_chrome_policy : 1 = false;
    bool disallowed_by_remote_admin : 1 = false;
    bool disallowed_by_remote_other : 1 = false;
    bool not_consented : 1 = false;

    bool IsProfileEligible() const {
      return !feature_disabled && !not_regular_profile;
    }

    bool IsEnabled() const {
      return IsProfileEligible() && !not_rolled_out &&
             !primary_account_not_capable && !DisallowedByAdmin() &&
             !disallowed_by_remote_other;
    }

    bool IsEnabledAndConsented() const { return IsEnabled() && !not_consented; }

    bool ShouldShowSettingsPage() const {
      const bool show_ai_settings_for_testing = base::FeatureList::IsEnabled(
          optimization_guide::features::kAiSettingsPageForceAvailable);

      // If the feature is disabled by enterprise policy, the settings page
      // should be shown (it will be shown in a policy-disabled state) only if
      // all other non-enterprise conditions are met: the account has all
      // appropriate permissions and has previously completed the FRE before the
      // policy went into effect. The settings page should also be shown if the
      // settings testing flag is enabled.
      return show_ai_settings_for_testing ||
             (IsProfileEligible() && !not_rolled_out &&
              !primary_account_not_capable && !disallowed_by_remote_other &&
              !not_consented);
    }

    bool DisallowedByAdmin() const {
      return disallowed_by_chrome_policy || disallowed_by_remote_admin;
    }
  };
  static ProfileEnablement EnablementForProfile(Profile* profile);

  // Whether the user's country and locale are in a location that Glic is rolled
  // out to.
  static bool IsInRolloutLocation();

  explicit GlicEnabling(Profile* profile,
                        ProfileAttributesStorage* profile_attributes_storage);
  ~GlicEnabling() override;

  // Returns true if the given profile is allowed to use glic. This means that
  // IsProfileEligible() returns true and:
  //   * the profile is signed in
  //   * can_use_model_execution is true
  //   * glic is allowed by enterprise policy.
  // This value can change at runtime.
  //
  // Once a profile is allowed to run glic, there are several more checks that
  // are required to use glic although many callsites may not care about all of
  // these:
  //   * FRE has been passed. There is no way to permanently decline FRE, as
  //     it's only invoked on user interaction with glic entry points.
  //   * Entry point specific flags (e.g. kGlicPinnedToTabstrip).
  //   * Profile is not paused.
  // If all entry-points have been disabled, then glic is functionally disabled.
  bool IsAllowed();

  // Returns true if the given profile has completed the FRE and false
  // otherwise.
  bool HasConsented();

  void SetGlicUserStatusUrlForTest(const GURL& test_url) {
    glic_user_status_fetcher_->SetGlicUserStatusUrlForTest(test_url);
  }

  void SetUserStatusFetchOverrideForTest(
      GlicUserStatusFetcher::FetchOverrideCallback fetch_override) {
    glic_user_status_fetcher_->SetFetchOverrideForTest(
        std::move(fetch_override));
  }

  // Updates the user status when information suggests that it might have
  // changed recently. This is internally debounced to avoid excessive
  // requests, for signals that might be received multiple times.
  void UpdateUserStatusWithThrottling() {
    glic_user_status_fetcher_->UpdateUserStatusWithThrottling();
  }

  // This is called anytime IsAllowed() might return a different value.
  using EnableChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterAllowedChanged(
      EnableChangedCallback callback);

  using ConsentChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnConsentChanged(
      ConsentChangedCallback callback);

  // This is called anytime ShouldShowSettingsPage() might return a different
  // value.
  using ShowSettingsPageChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnShowSettingsPageChanged(
      ShowSettingsPageChangedCallback callback);

  using ProfileReadyStateChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterProfileReadyStateChanged(
      ProfileReadyStateChangedCallback callback);

 private:
  void OnGlicSettingsPolicyChanged();

  // IdentityManagerObserver:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Detects changes to capabilities.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  // Detects potential changes to tiered rollout status.
  void OnTieredRolloutStatusMaybeChanged();

  // Detects paused state.
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  void UpdateEnabledStatus();
  void UpdateConsentStatus();

  raw_ptr<Profile> profile_;
  raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  using EnableChangedCallbackList = base::RepeatingCallbackList<void()>;
  EnableChangedCallbackList enable_changed_callback_list_;
  using OnConsentChangeCallbackList = base::RepeatingCallbackList<void()>;
  OnConsentChangeCallbackList consent_changed_callback_list_;
  using OnShowSettingsPageChangeCallbackList =
      base::RepeatingCallbackList<void()>;
  OnShowSettingsPageChangeCallbackList
      show_settings_page_changed_callback_list_;
  using ProfileReadyStateChangedCallbackList =
      base::RepeatingCallbackList<void()>;
  ProfileReadyStateChangedCallbackList
      profile_ready_state_changed_callback_list_;
  PrefChangeRegistrar pref_registrar_;
  std::unique_ptr<GlicUserStatusFetcher> glic_user_status_fetcher_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_
