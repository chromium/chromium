// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"

class Profile;
class ProfileAttributesStorage;

namespace glic {
namespace prefs {
enum class SettingsPolicyState;
enum class FreStatus;
}  // namespace prefs
namespace mojom {
// TODO(crbug.com/406500707): This forward declaration is needed because we use
// allow_circular_includes_from. Our build rules should be refactored to avoid
// this.
enum class ProfileReadyState : int32_t;
enum class InvocationSource : int32_t;
}  // namespace mojom

// This synthetic field trial is registered for users who are affected by the
// kGlicEligibilitySeparateAccountCapability feature.
//
// Users who have a different value for the "old" and "new" account capability
// are added to this synthetic field trial, with the group corresponding to
// their field trial group for the main
// (kGlicEligibilitySeparateAccountCapability) feature.
//
// For example:
// - GlicEligibilitySeparateAccountCapabilityAffectedUsers:Control contains
// clients in the Control group of main experiment, where at least one profile
// has a different value for the "old" and "new" account capability.
// - GlicEligibilitySeparateAccountCapabilityAffectedUsers:Enabled contains
// clients in the Enabled group of the main experiment, where at least one
// profile has a different value for the "old" and "new" account capability.
//
// Clients in the Control or Enabled groups of the main experiment, where all
// profiles have the same value for the "old" and "new" account capability, are
// not added to this synthetic field trial.
//
// This synthetic trial is re-evaluated in each session, and takes into account
// only loaded profiles.
inline constexpr char
    kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName[] =
        "GlicEligibilitySeparateAccountCapabilityAffectedUsers";

// Global state used by GlicEnabling.
class GlicGlobalEnabling {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual std::string GetPermanentCountryCode();
    virtual std::string GetSessionCountryCode();
    virtual std::string GetLocale();
  };
  explicit GlicGlobalEnabling(Delegate& delegate);
  ~GlicGlobalEnabling();
  bool IsEnabledByGlobalCriteria();
  bool IsSystemRequirementMet() const;
  bool IsOsVersionSupported() const;
  bool IsLocaleEnabled() const { return locale_enablement_.value_or(true); }
  bool IsCountryEnabled() const { return country_enablement_.value_or(true); }

 private:
  std::optional<bool> locale_enablement_;
  std::optional<bool> country_enablement_;
};

// LINT.IfChange(RequiredExperimentalOptIn)
enum class RequiredExperimentalOptIn {
  kGlic = 0,
  kActuation = 1,
  kExperimental = 2,
  kNotNeeded = 3,
  kMaxValue = kNotNeeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequiredExperimentalOptIn)

// This class provides a central location for checking if Glic is enabled. It
// allows for future expansion to include other ways the feature may be disabled
// such as based on user preferences or system settings.
//
// There are multiple notions of "enabled". The highest level is
// IsEnabledByGlobalCriteria which controls whether any global-Glic
// infrastructure is created. It checks the feature flag, country, locale, and
// system requirements. If these criteria are not met, nothing Glic-related
// should be created.
//
// If the above checks pass, various global objects are created as well as a
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
class GlicEnabling final : public signin::IdentityManager::Observer,
                           public subscription_eligibility::
                               SubscriptionEligibilityService::Observer {
 public:
  // Returns whether the global Glic feature is enabled for Chrome. This status
  // will not change at runtime.
  static bool IsEnabledByGlobalCriteria();

  // Checks whether this client is likely a dogfooder, taking the ignore dogfood
  // feature into account.
  static bool IsLikelyDogfoodClient();

  // Returns true if a profile is eligible for Glic. Some profiles - such as
  // incognito, guest, system profile, etc. - are never eligible. An eligible
  // profile is one where Glic could potentially be enabled, regardless of
  // whether it is currently enabled or not.
  //
  // NOTE: This only represents static structural suitability (i.e., the client
  // device is globally capable and the profile type is suitable). It does NOT
  // check active user account status (such as sign-in state or GAIA account
  // capabilities).
  //
  // This is a foundational, static check that does not change at runtime. It
  // controls whether Glic infrastructure (e.g., `GlicKeyedService`, UI
  // controllers) is created for the profile.
  //
  // Always returns false if `IsEnabledByGlobalCriteria()` is off.
  static bool IsProfileEligible(Profile* profile);

  // Returns true if the profile is technically ineligible due to global
  // criteria but the entry point remains anchored because the user has
  // onboarded previously.
  static bool IsAnchoredButIneligible(bool global_criteria_met, bool consented);

  // This is a convenience method for code outside of //chrome/browser/glic.
  // Code inside should use instance method IsAllowed() instead.
  static bool IsEnabledForProfile(Profile* profile);

  // Returns true if the profile has completed the FRE.
  static bool HasConsentedForProfile(Profile* profile);

  // Returns true if the given profile has Glic enabled and has completed the
  // FRE. True implies that IsEnabledByGlobalCriteria(),
  // IsProfileEligible(profile), and IsEnabledForProfile(profile) are also true.
  // This value can change at runtime.
  static bool IsEnabledAndConsentForProfile(Profile* profile);

  // Returns true if the given profile was shown the FRE but did not complete
  // it. This value can change at runtime.
  static bool DidDismissForProfile(Profile* profile);

  // Whether or not the profile is currently ready for Glic. This means no
  // additional steps must be taken before opening Glic.
  static bool IsReadyForProfile(Profile* profile);

  // Same as IsReadyForProfile, but returns a more detailed state.
  static mojom::ProfileReadyState GetProfileReadyState(Profile* profile);

  // Whether the profile is in the Glic tiered rollout population.
  static bool IsEligibleForGlicTieredRollout(Profile* profile);

  // Whether the glic internals page is enabled.
  static bool IsInternalsWebUIEnabled(Profile* profile);

  // The settings page is shown when:
  // * Flags are enabled
  // * The profile is eligible (regular, non-incognito, non-guest, etc.)
  // * The profile has model execution privileges
  // * The profile has completed the first run experience
  static bool ShouldShowSettingsPage(Profile* profile);

  // The Glic Button is shown when:
  // * Glic is enabled for the client and profile
  // * OR the user has lost access for a reason that may be recoverable.
  static bool ShouldShowGlicButton(Profile* profile);

  // Whether the auto open for pdf flow is enabled.
  static bool IsAutoOpenForPdfEnabled(Profile* profile);

  // Whether the tab web contents contextual menu item is enabled.
  static bool IsContextualMenuItemEnabled(Profile* profile);

  // Whether the selection prompt is enabled.
  static bool IsSelectionPromptEnabledForProfile(Profile* profile);

  // Returns true if Glic is enabled for the profile, the feature is enabled,
  // and the account is non-enterprise (or for Glic dev).
  static bool IsShareImageEnabledForProfile(Profile* profile);

  // Returns the Gemini Enterprise settings, taking into account command line
  // overrides.
  static std::optional<glic::mojom::GeminiEnterpriseSettings>
  GetGeminiEnterpriseSettings(Profile* profile);

  // Whether the live mode and floaty window are enabled by flags.
  static bool IsLiveAndFloatyEnabledByFlags();

  struct ProfileEnablement {
    ProfileEnablement();
    ProfileEnablement(ProfileEnablement&&);
    ~ProfileEnablement();

    // These conditions are checked first and may prevent following checks from
    // occurring.
    bool feature_enabled : 1 = true;
    bool is_regular_profile : 1 = true;

    // These are checked separately, so may be present in various combinations.
    bool is_rolled_out : 1 = true;
    bool primary_account_is_capable : 1 = true;
    bool primary_account_is_fully_signed_in : 1 = true;
    // The profile is signed out, but kGlicShowForSignedOut is enabled, so the
    // GiC panel can be shown to show the sign-in promotion.
    bool primary_account_needs_signed_in : 1 = false;
    bool allowed_by_chrome_policy : 1 = true;
    bool allowed_by_remote_admin : 1 = true;
    bool allowed_by_remote_other : 1 = true;
    bool fre_is_consented : 1 = true;

    // Whether allowed by country filtering.
    bool allowed_by_country_filter : 1 = true;

    // Whether allowed by locale filtering.
    bool allowed_by_locale_filter : 1 = true;

    // Whether the Glic feature flag is enabled.
    bool feature_flag_enabled : 1 = true;

    // Whether system requirements (relevant to ChromeOS only) for Glic are
    // met.
    bool system_requirement_met : 1 = true;

    // Whether the OS version is supported.
    bool os_version_supported : 1 = true;

    // Whether the user has onboarded with this profile previously which keeps
    // Glic partially enabled to show error states instead of hiding the button.
    bool anchor_entrypoint_override_active : 1 = false;

    // Whether live (audio) functionality is allowed for this account type.
    bool live_allowed : 1 = true;

    // Whether share image functionality is allowed for this account type.
    bool share_image_allowed : 1 = true;

    // LINT.IfChange(FeatureDisabledReason)
    enum class FeatureDisabledReason {
      kFeatureFlagDisabled = 0,
      kCountryDisabled = 1,
      kLocaleDisabled = 2,
      kSystemRequirementNotMet = 3,
      kOsVersionNotSupported = 4,
      kMaxValue = kOsVersionNotSupported,
    };
    // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicFeatureDisabledReason)

    enum class DisabledReason {
      kFeatureDisabled = 0,
      kNotRegularProfile = 1,
      kNotRolledOut = 2,
      kPrimaryAccountNotCapable = 3,
      kDisallowedByChromePolicy = 4,
      kDisallowedByRemoteAdmin = 5,
      kDisallowedByRemoteOther = 6,
      kMaxValue = kDisallowedByRemoteOther,
    };

    // Record the state of this struct to UMA.
    void RecordStartupMetrics() const { RecordMetrics("Startup"); }
    void RecordSteadyStateMetrics() const { RecordMetrics("SteadyState"); }

    bool IsProfileEligible() const {
      return feature_enabled && is_regular_profile;
    }

    // Returns true if Glic is fully enabled and allowed to run on this profile.
    // Unlike `GlicEnabling::IsProfileEligible()`, this is a dynamic check that
    // can change at runtime. It evaluates dynamic profile state such as whether
    // the user is signed in, active user account capabilities (e.g.,
    // GAIA/Gemini capabilities), rollout groups, enterprise policies, and
    // location filters.
    bool IsEnabled() const {
      bool base_checks = IsProfileEligible() && is_rolled_out &&
                         primary_account_is_capable && !DisallowedByAdmin() &&
                         allowed_by_remote_other;

      if (!base_checks) {
        return false;
      }

      return allowed_by_country_filter && allowed_by_locale_filter;
    }

    bool IsEnabledAndConsented() const {
      return IsEnabled() && fre_is_consented;
    }

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
             (IsProfileEligible() && is_rolled_out &&
              primary_account_is_capable && allowed_by_remote_other &&
              fre_is_consented);
    }

    // Returns true if the Glic button/entrypoint should be dynamically visible
    // in the UI at the current moment.
    //
    // NOTE: This represents dynamic, runtime visibility, not static structural
    // capability. During window startup construction (such as in
    // `HorizontalTabStripRegionView` or `ToolbarView`), the parent view
    // container must be created if Glic could potentially become active
    // (meaning `GlicEnabling::IsProfileEligible()` is true, even if the user
    // starts signed out). Once the container is created, this method is used by
    // `GlicButtonController` to dynamically show or hide the button inside the
    // container at runtime (e.g., rendering it immediately after sign-in
    // completes).
    //
    // Always returns false if the Glic feature is disabled by feature flag,
    // enterprise admin policy, or if the user is not in the rollout group.
    bool ShouldShowGlicButton() const {
      if (!feature_flag_enabled) {
        return false;
      }
      if (IsEnabled()) {
        return true;
      }
      if (anchor_entrypoint_override_active) {
        return !DisallowedByAdmin() && is_rolled_out;
      }
      return false;
    }

    bool EligibleForLive() const { return IsProfileEligible() && live_allowed; }

    bool EligibleForShareImage() const {
      return IsProfileEligible() && share_image_allowed;
    }

    bool DisallowedByAdmin() const {
      return !allowed_by_chrome_policy || !allowed_by_remote_admin;
    }

   private:
    // `suffix` should be either "Startup" or "SteadyState".
    void RecordMetrics(const std::string& suffix) const;
    void RecordFeatureDisabledReason(const std::string& suffix) const;
  };
  static ProfileEnablement EnablementForProfile(Profile* profile);

  explicit GlicEnabling(Profile* profile,
                        ProfileAttributesStorage* profile_attributes_storage);
  ~GlicEnabling() override;

  // Returns true if the given profile is allowed to use Glic. This is the
  // primary check to determine if Glic can be opened at all (i.e. entrypoints
  // are available). Being "allowed" to use Glic means:
  //   * `IsProfileEligible()` returns true
  //   * the profile is signed in
  //   * can_use_model_execution is true
  //   * Glic is allowed by enterprise policy.
  // This value can change at runtime. If this returns false, all entry points
  // should be hidden or disabled and Glic is functionally disabled.
  //
  // Note that once a profile is allowed to run Glic, there are several more
  // requirements for actually using Glic (i.e. opening the UI and not being
  // blocked on an error state):
  //   * FRE has been passed. There is no way to permanently decline FRE, as
  //     it's only invoked on user interaction with Glic entry points.
  //   * Profile is not paused.
  // There are also settings that affect entry points:
  //   * The tab strip GlicButton can be unpinned in settings; this state is
  //     tracked by the `kGlicPinnedToTabstrip` preference.
  //   * The OS-level entry point can be disabled in settings; this state is
  //     tracked by the `kGlicLauncherEnabled` preference. It also cannot be
  //     enabled without FRE completion.
  // Many callsites do not care about all of these additional conditions.
  bool IsAllowed();

  // Returns true if the given profile has completed the FRE and false
  // otherwise.
  bool HasConsented() const;

  // Returns the FRE status.
  static prefs::FreStatus GetCompletedFre(Profile* profile);
  prefs::FreStatus GetCompletedFre() const;
  // Sets the FRE status.
  void SetCompletedFre(prefs::FreStatus status);

  // Returns whether user enabled actuation on web.
  bool GetUserEnabledActuationOnWeb() const;
  // Returns true if the user enabled actuation on web pref is at its default
  // value.
  bool IsUserEnabledActuationOnWebDefault() const;
  // Returns true if the experimental triggering enabled pref is at its default
  // value.
  bool IsExperimentalTriggeringEnabledDefault() const;
  // Returns true if the experimental triggering enabled pref is user
  // controlled.
  bool IsExperimentalTriggeringUserControlled() const;
  // Sets whether user enabled actuation on web.
  void SetUserEnabledActuationOnWeb(bool enabled);

  // Returns whether experimental triggering is enabled. This only checks the
  // experimental triggering value, for a complete opt-in check, use
  // `IsExperimentalTriggeringFullyOptedIn()`.
  bool GetExperimentalTriggeringEnabled() const;

  // Sets whether experimental triggering is enabled.
  void SetExperimentalTriggeringEnabled(bool enabled);

  // Returns the state of experimental triggering.
  syncer::DeviceInfo::GlicExperimentalTriggeringState
  GetExperimentalTriggeringState() const;

  // Returns the version of the Glic experimental triggering protocol
  // supported by the current client, or std::nullopt if unavailable.
  std::optional<int> GetExperimentalTriggeringVersion() const;

  // Returns the required opt-in state for experimental triggering.
  RequiredExperimentalOptIn GetRequiredExperimentalOptIn() const;

  // Checks if startup metrics have already been recorded, and if not, records
  // them.
  void MaybeRecordStartupMetrics();

  // Records startup metrics related to profile ineligibility. Should only be
  // called once per profile.
  static void RecordProfileIneligibilityMetricsAtStartup(Profile* profile);

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

  // Test-only method to bypass enablement checks.
  static void SetBypassEnablementChecksForTesting(bool bypass);

  // This is called anytime IsAllowed() might return a different value.
  using EnableChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterAllowedChanged(
      EnableChangedCallback callback);

  using ConsentChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnConsentChanged(
      ConsentChangedCallback callback);

  using UserEnabledActuationOnWebChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnUserEnabledActuationOnWebChanged(
      UserEnabledActuationOnWebChangedCallback callback);

  using ExperimentalTriggeringEnabledChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnExperimentalTriggeringEnabledChanged(
      ExperimentalTriggeringEnabledChangedCallback callback);

  using ExperimentalTriggeringStateChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterOnExperimentalTriggeringStateChanged(
      ExperimentalTriggeringStateChangedCallback callback);

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
  void OnUserEnabledActuationOnWebChanged();
  void OnExperimentalTriggeringEnabledChanged();
  void MaybeNotifyExperimentalTriggeringStateChanged();

  // IdentityManagerObserver:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // subscription_eligibility::SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

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

#if BUILDFLAG(IS_CHROMEOS)
  static bool IsChromeOSProfileEligible(Profile* profile);
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool recorded_startup_metrics_ = false;

  raw_ptr<Profile> profile_;
  raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  using EnableChangedCallbackList = base::RepeatingCallbackList<void()>;
  EnableChangedCallbackList enable_changed_callback_list_;
  using OnConsentChangeCallbackList = base::RepeatingCallbackList<void()>;
  OnConsentChangeCallbackList consent_changed_callback_list_;
  using UserEnabledActuationOnWebChangedCallbackList =
      base::RepeatingCallbackList<void()>;
  UserEnabledActuationOnWebChangedCallbackList
      user_enabled_actuation_on_web_changed_callback_list_;
  using ExperimentalTriggeringEnabledChangedCallbackList =
      base::RepeatingCallbackList<void()>;
  ExperimentalTriggeringEnabledChangedCallbackList
      experimental_triggering_enabled_changed_callback_list_;
  using ExperimentalTriggeringStateChangedCallbackList =
      base::RepeatingCallbackList<void()>;
  ExperimentalTriggeringStateChangedCallbackList
      experimental_triggering_state_changed_callback_list_;
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
  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_service_observation_{this};
  syncer::DeviceInfo::GlicExperimentalTriggeringState
      last_experimental_triggering_state_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_ENABLING_H_
