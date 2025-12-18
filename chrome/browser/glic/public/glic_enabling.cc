// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include <ranges>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"  // nogncheck
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"       // nogncheck
#include "components/user_manager/user_type.h"  // nogncheck
#endif

namespace glic {

// Feature flag kGlicCountryFiltering controls whether country filtering is
// applied client side. Two finch params are used to control this, both are a
// comma separated string.
// disabled_countries:
//   - Optional, default to empty.
//   - The country must not be in this list to be enabled.
// enabled_countries:
//   - Optional, default to kDefaultEnabledCountries.
//   - If the size is 1 and the string is "*", then all countries are enabled.
//   - Otherwise, the country must be in this list to be enabled.

// Comma separated list of countries to enable GLIC, by default, if country
// filtering is enabled.
constexpr char kDefaultEnabledCountries[] = "us,au,ca,nz";

// Feature flag kGlicLocaleFiltering controls whether locale filtering is
// applied client side. Two finch params are used to control this, both are a
// comma separated string.
// disabled_locales:
//   - Optional, default to empty.
//   - The locale must not be in this list to be enabled.
// enabled_locales:
//   - Optional, default to kDefaultEnabledLocales.
//   - If the size is 1 and the string is "*", then all locales are enabled.
//   - Otherwise, the locale must be in this list to be enabled.

// Comma separated list of locales to enable GLIC, by default, if locale
// filtering is enabled.
constexpr char kDefaultEnabledLocales[] = "en-us";

namespace {

bool HasGoogleInternalProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return false;
  }
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager) {
      continue;
    }
    if (IsPrimaryAccountGoogleInternal(*identity_manager)) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> GetFieldTrialParamAsSplitString(
    const base::Feature& feature,
    const std::string& param_name,
    const std::string& default_value) {
  std::string string_list = base::GetFieldTrialParamByFeatureAsString(
      feature, param_name, default_value);
  return base::SplitString(string_list, ", \t\n'\"", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::optional<bool> GetCountryEnablement(
    GlicGlobalEnabling::Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(features::kGlicCountryFiltering)) {
    return std::nullopt;
  }
  std::vector<std::string> enabled_countries = GetFieldTrialParamAsSplitString(
      features::kGlicCountryFiltering, "enabled_countries",
      kDefaultEnabledCountries);

  std::vector<std::string> disabled_countries = GetFieldTrialParamAsSplitString(
      features::kGlicCountryFiltering, "disabled_countries", "");

  std::string country_code = delegate.GetCountryCode();
  auto country_matches = [&](const std::string& c) {
    return base::EqualsCaseInsensitiveASCII(c, country_code);
  };

  if (std::ranges::any_of(disabled_countries, country_matches)) {
    return false;
  }

  if (enabled_countries.size() == 1 && enabled_countries[0] == "*") {
    return true;
  }

  return std::ranges::any_of(enabled_countries, country_matches);
}

std::optional<bool> GetLocaleEnablement(
    GlicGlobalEnabling::Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(features::kGlicLocaleFiltering)) {
    return std::nullopt;
  }
  auto normalize_locale = [&](const std::string& locale) {
    std::string out;
    base::ReplaceChars(locale, "_", "-", &out);
    return base::ToLowerASCII(out);
  };

  std::vector<std::string> disabled_locales = GetFieldTrialParamAsSplitString(
      features::kGlicLocaleFiltering, "disabled_locales", "");

  std::vector<std::string> enabled_locales = GetFieldTrialParamAsSplitString(
      features::kGlicLocaleFiltering, "enabled_locales",
      kDefaultEnabledLocales);

  std::string locale = normalize_locale(delegate.GetLocale());
  auto matches_locale = [&](const std::string& a) {
    return normalize_locale(a) == locale;
  };

  if (std::ranges::any_of(disabled_locales, matches_locale)) {
    return false;
  }

  if (enabled_locales.size() == 1 && enabled_locales[0] == "*") {
    return true;
  }

  return std::ranges::any_of(enabled_locales, matches_locale);
}

}  // namespace

std::string GlicGlobalEnabling::Delegate::GetCountryCode() {
  std::string country_code =
      base::ToLowerASCII(variations::GetCurrentCountryCode(
          g_browser_process->variations_service()));
  DLOG_IF(WARNING, country_code.empty()) << "Couldn't get country info.";
  return country_code;
}

std::string GlicGlobalEnabling::Delegate::GetLocale() {
  return base::ToLowerASCII(
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
}

GlicEnabling::ProfileEnablement GlicEnabling::EnablementForProfile(
    Profile* profile) {
  ProfileEnablement result;

  if (!IsEnabledByFlags()) {
    result.feature_disabled = true;
    return result;
  }

  if (!profile || !profile->IsRegularProfile()) {
    result.not_regular_profile = true;
    return result;
  }

  // Certain checks are bypassed if --glic-dev is passed.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kGlicDev)) {
    if (!base::FeatureList::IsEnabled(features::kGlicRollout) &&
        !IsEligibleForGlicTieredRollout(profile)) {
      result.not_rolled_out = true;
    }

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    AccountInfo primary_account =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));

    // Not having a primary account is considered ineligible, as is kUnknown
    // for the required account capability.
    if (primary_account.IsEmpty()) {
      result.primary_account_not_capable = true;
    }

    // Check account capabilities.
    //
    // TODO(crbug.com/470004757): when cleaning up the
    // kGlicEligibilitySeparateAccountCapability feature, also remove the
    // fallback to can_use_model_execution_features().
    signin::Tribool capability_value =
        primary_account.capabilities.can_use_model_execution_features();
    if (base::FeatureList::IsEnabled(
            switches::kGlicEligibilitySeparateAccountCapability) &&
        (primary_account.capabilities.can_use_gemini_in_chrome() !=
         signin::Tribool::kUnknown)) {
      capability_value =
          primary_account.capabilities.can_use_gemini_in_chrome();
    }
    result.primary_account_not_capable =
        (capability_value != signin::Tribool::kTrue);

    // If the feature is overridden by a field trial, and the user's eligibility
    // is known and different for the two capabilities, add them to a synthetic
    // trial.
    base::FieldTrial* field_trial = base::FeatureList::GetFieldTrial(
        switches::kGlicEligibilitySeparateAccountCapability);
    if (field_trial &&
        (primary_account.capabilities.can_use_gemini_in_chrome() !=
         signin::Tribool::kUnknown) &&
        (primary_account.capabilities.can_use_model_execution_features() !=
         signin::Tribool::kUnknown) &&
        (primary_account.capabilities.can_use_gemini_in_chrome() !=
         primary_account.capabilities.can_use_model_execution_features())) {
      g_browser_process->GetFeatures()
          ->glic_synthetic_trial_manager()
          ->SetSyntheticExperimentState(
              kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName,
              field_trial->GetGroupNameWithoutActivation());
    }
  }

  if (profile->GetPrefs()->GetInteger(::prefs::kGeminiSettings) !=
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled)) {
    result.disallowed_by_chrome_policy = true;
  }

  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    if (auto cached_user_status =
            GlicUserStatusFetcher::GetCachedUserStatus(profile);
        cached_user_status.has_value()) {
      switch (cached_user_status->user_status_code) {
        case UserStatusCode::DISABLED_BY_ADMIN:
          result.disallowed_by_remote_admin = true;
          break;
        case UserStatusCode::DISABLED_OTHER:
          result.disallowed_by_remote_other = true;
          break;
        case UserStatusCode::ENABLED:
          break;
        case UserStatusCode::SERVER_UNAVAILABLE:
          // We never cache SERVER_UNAVAILABLE.
          NOTREACHED();
      }
    }
  }

  if (!HasConsentedForProfile(profile) &&
      !base::FeatureList::IsEnabled(features::kGlicTrustFirstOnboarding)) {
    result.not_consented = true;
  }

  return result;
}

GlicGlobalEnabling::GlicGlobalEnabling(Delegate& delegate) {
  locale_enablement_ = GetLocaleEnablement(delegate);
  country_enablement_ = GetCountryEnablement(delegate);
}

GlicGlobalEnabling::~GlicGlobalEnabling() = default;

bool GlicGlobalEnabling::IsEnabledByFlags() {
  bool is_enabled = base::FeatureList::IsEnabled(features::kGlic) &&
                    features::HasTabSearchToolbarButton() &&
                    locale_enablement_.value_or(true) &&
                    country_enablement_.value_or(true);
#if BUILDFLAG(IS_CHROMEOS)
  constexpr base::ByteCount kMinimumMemoryThreshold = base::GiB(8);

  // TODO(b:468055370): Remove the bypassing once the glic is fully launched.
  const bool bypass_cbx_requirement =
      base::FeatureList::IsEnabled(
          chromeos::features::kGlicEnableFor8GbDevices) &&
      base::SysInfo::AmountOfPhysicalMemory() >= kMinimumMemoryThreshold;

  is_enabled = is_enabled && (bypass_cbx_requirement ||
                              base::FeatureList::IsEnabled(
                                  chromeos::features::kFeatureManagementGlic));
#endif  // BUILDFLAG(IS_CHROMEOS)
  return is_enabled;
}

// static
bool GlicEnabling::IsInRolloutLocation() {
  // TODO(crbug.com/454702721): Getting the location on ChromeOS is done
  // differently.
  auto* variations_service = g_browser_process->variations_service();
  return variations_service->GetStoredPermanentCountry() == "us" &&
         g_browser_process->GetApplicationLocale() == "en-US";
}

bool GlicEnabling::IsEnabledByFlags() {
  return g_browser_process->GetFeatures()
      ->glic_global_enabling()
      .IsEnabledByFlags();
}

bool GlicEnabling::IsProfileEligible(const Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // Due to the tight coupling of the browser Profile and OS users in ChromeOS,
  // we check the user session type to align with other desktop browser
  // behavior.
  if (!ash::IsUserBrowserContext(profile)) {
    // We only allow regular user session profiles.
    // E.g. disallowed on login screen.
    return false;
  }
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      const_cast<Profile*>(profile));
  if (user == nullptr) {
    // When there is no signed in user on ChromeOS, assume that the profile is
    // not eligible.
    return false;
  }
  switch (user->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      // These are ok to use glic.
      break;
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskChromeApp:
    case user_manager::UserType::kKioskWebApp:
    case user_manager::UserType::kKioskIWA:
    case user_manager::UserType::kKioskArcvmApp:
      // Disallows guest session, and device local account sessions.
      return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  return IsEnabledByFlags() && profile && profile->IsRegularProfile();
}

bool GlicEnabling::IsEnabledForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabled();
}

bool GlicEnabling::HasConsentedForProfile(Profile* profile) {
  return profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre) ==
         static_cast<int>(prefs::FreStatus::kCompleted);
}

bool GlicEnabling::IsEnabledAndConsentForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabledAndConsented();
}

bool GlicEnabling::DidDismissForProfile(Profile* profile) {
  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicCompletedFre) ==
         static_cast<int>(prefs::FreStatus::kIncomplete);
}

bool GlicEnabling::IsReadyForProfile(Profile* profile) {
  return GetProfileReadyState(profile) == mojom::ProfileReadyState::kReady;
}

mojom::ProfileReadyState GlicEnabling::GetProfileReadyState(Profile* profile) {
  const ProfileEnablement enablement = EnablementForProfile(profile);
  if (enablement.DisallowedByAdmin()) {
    return mojom::ProfileReadyState::kDisabledByAdmin;
  }
  if (!enablement.IsEnabledAndConsented()) {
    return mojom::ProfileReadyState::kIneligible;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    return mojom::ProfileReadyState::kReady;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Check that profile is not currently paused.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    return mojom::ProfileReadyState::kUnknownError;
  }
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          core_account_info.account_id)) {
    return mojom::ProfileReadyState::kSignInRequired;
  }
  return mojom::ProfileReadyState::kReady;
}

bool GlicEnabling::IsEligibleForGlicTieredRollout(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kGlicTieredRollout) &&
         profile->GetPrefs()->GetBoolean(prefs::kGlicRolloutEligibility);
}

bool GlicEnabling::ShouldShowSettingsPage(Profile* profile) {
  return EnablementForProfile(profile).ShouldShowSettingsPage();
}

void GlicEnabling::OnGlicSettingsPolicyChanged() {
  // Update the overall enabled status as the policy has changed.
  UpdateEnabledStatus();
}

bool GlicEnabling::IsUnifiedFreEnabled(Profile* profile) {
  return IsMultiInstanceEnabled() &&
         base::FeatureList::IsEnabled(features::kGlicUnifiedFreScreen);
}

bool GlicEnabling::IsMultiInstanceEnabledByFlags() {
  const bool multi_instance_enabled =
      base::FeatureList::IsEnabled(features::kGlicMultiInstance);
  const bool multi_tab_enabled =
      base::FeatureList::IsEnabled(mojom::features::kGlicMultiTab);
  const bool tab_underlines_enabled =
      base::FeatureList::IsEnabled(features::kGlicMultitabUnderlines);

  if (multi_instance_enabled &&
      !(multi_tab_enabled && tab_underlines_enabled)) {
    LOG(ERROR)
        << "GlicMultiInstance is enabled without kGlicMultiTab and/or "
           "kGlicMultitabUnderlines. All of these features must be enabled to "
           "ensure proper behavior.";
  }

  return multi_instance_enabled && multi_tab_enabled && tab_underlines_enabled;
}

bool GlicEnabling::IsShareImageEnabledForProfile(Profile* profile) {
  if (!IsEnabledForProfile(profile) ||
      !base::FeatureList::IsEnabled(features::kGlicShareImage)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kGlicShareImageEnterprise)) {
    return true;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGlicDev) &&
      HasGoogleInternalProfile()) {
    return true;
  }

  auto* browser_management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  const bool is_managed =
      browser_management_service && browser_management_service->IsManaged();
  if (is_managed) {
    return false;
  }

  // LINT.IfChange(GlicCachedUserStatusScope)

  // See GlicUserStatusFetcher for details on when we update the cached value
  // and when we skip updating.
  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck) &&
      GlicUserStatusFetcher::GetCachedUserStatus(profile).has_value()) {
    return false;
  }

  // LINT.ThenChange(//chrome/browser/glic/glic_user_status_fetcher.cc:GlicCachedUserStatusScope)

  auto account_managed_status_finder = signin::AccountManagedStatusFinder(
      identity_manager,
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      base::DoNothing());

  switch (account_managed_status_finder.GetOutcome()) {
    case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
    case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown:
      return true;
    case signin::AccountManagedStatusFinderOutcome::kPending:
    case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
    case signin::AccountManagedStatusFinderOutcome::kEnterprise:
    case signin::AccountManagedStatusFinderOutcome::kError:
    case signin::AccountManagedStatusFinderOutcome::kTimeout:
      return false;
  }
}

bool GlicEnabling::IsMultiInstanceEnabled() {
  if (IsMultiInstanceEnabledByFlags()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          features::kGlicEnableMultiInstanceBasedOnTier)) {
    return false;
  }

  // MultiTab feaure enablement should still gate multi-instance enablement when
  // considering subscription tier.
  if (!base::FeatureList::IsEnabled(mojom::features::kGlicMultiTab) ||
      !base::FeatureList::IsEnabled(features::kGlicMultitabUnderlines)) {
    LOG(ERROR) << "Multi-instance functions cannot be enabled without the "
                  "kGlicMultiTab and kGlicMultitabUnderlines features. These "
                  "features must be enabled to ensure proper behavior.";
    return false;
  }

  return IsEligibleForGlicMultiInstanceTieredRolloutThisRun();
}

bool GlicEnabling::IsEligibleForGlicMultiInstanceTieredRolloutThisRun() {
  // It is necessary that `is_eligible` does not change after the first call to
  // this function during a run of Chrome, as multi-instance cannot be
  // enabled/disabled dynamically.
  static bool is_eligible =
      GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(nullptr);

  return is_eligible;
}

bool GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
    Profile* additional_profile) {
  if (!g_browser_process->local_state() ||
      !g_browser_process->profile_manager()) {
    return false;
  }

  // If multi-instance was ever enabled by tier, ensure that it stays enabled.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kGlicMultiInstanceEnabledBySubscriptionTier)) {
    return true;
  }

  // If `additional_profile` was specified, also check it.
  std::vector<Profile*> available_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  if (additional_profile) {
    available_profiles.emplace_back(additional_profile);
  }

  for (Profile* profile : available_profiles) {
    auto* subscription_eligibility_service = subscription_eligibility::
        SubscriptionEligibilityServiceFactory::GetForProfile(profile);
    int32_t profile_subscription_tier =
        subscription_eligibility_service
            ? subscription_eligibility_service->GetAiSubscriptionTier()
            : 0;
    if (profile_subscription_tier == 1 || profile_subscription_tier == 2) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kGlicMultiInstanceEnabledBySubscriptionTier, true);
      return true;
    }
  }
  return false;
}

GlicEnabling::GlicEnabling(Profile* profile,
                           ProfileAttributesStorage* profile_attributes_storage)
    : profile_(profile),
      profile_attributes_storage_(profile_attributes_storage) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      ::prefs::kGeminiSettings,
      base::BindRepeating(&GlicEnabling::OnGlicSettingsPolicyChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(prefs::kGlicCompletedFre,
                      base::BindRepeating(&GlicEnabling::UpdateConsentStatus,
                                          base::Unretained(this)));
  if (!base::FeatureList::IsEnabled(features::kGlicRollout) &&
      base::FeatureList::IsEnabled(features::kGlicTieredRollout)) {
    pref_registrar_.Add(
        prefs::kGlicRolloutEligibility,
        base::BindRepeating(&GlicEnabling::OnTieredRolloutStatusMaybeChanged,
                            base::Unretained(this)));
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);

  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    glic_user_status_fetcher_ = std::make_unique<GlicUserStatusFetcher>(
        profile_, base::BindRepeating(&GlicEnabling::UpdateEnabledStatus,
                                      base::Unretained(this)));
  }
}
GlicEnabling::~GlicEnabling() = default;

bool GlicEnabling::IsAllowed() {
  return IsEnabledForProfile(profile_);
}

bool GlicEnabling::HasConsented() {
  return HasConsentedForProfile(profile_);
}

base::CallbackListSubscription GlicEnabling::RegisterAllowedChanged(
    EnableChangedCallback callback) {
  return enable_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterOnConsentChanged(
    ConsentChangedCallback callback) {
  return consent_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterOnShowSettingsPageChanged(
    ShowSettingsPageChangedCallback callback) {
  return show_settings_page_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterProfileReadyStateChanged(
    ProfileReadyStateChangedCallback callback) {
  return profile_ready_state_changed_callback_list_.Add(std::move(callback));
}

void GlicEnabling::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnRefreshTokensLoaded() {
  UpdateEnabledStatus();
}

void GlicEnabling::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnTieredRolloutStatusMaybeChanged() {
  UpdateEnabledStatus();
}

void GlicEnabling::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  // Check that the account info here is the same as the primary account, and
  // ignore all events that are not about the primary account.
  if (identity_manager_observation_.GetSource()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin) != account_info) {
    return;
  }
  UpdateEnabledStatus();
}

void GlicEnabling::UpdateEnabledStatus() {
  if (ProfileAttributesEntry* entry =
          profile_attributes_storage_->GetProfileAttributesWithPath(
              profile_->GetPath())) {
    entry->SetIsGlicEligible(IsAllowed());
  }
  enable_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
  profile_ready_state_changed_callback_list_.Notify();
}

void GlicEnabling::UpdateConsentStatus() {
  consent_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
  profile_ready_state_changed_callback_list_.Notify();
}

}  // namespace glic
