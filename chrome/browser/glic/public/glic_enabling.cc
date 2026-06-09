// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include <optional>
#include <ranges>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"  // nogncheck
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"       // nogncheck
#include "components/user_manager/user_type.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
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

// Comma separated list of countries to enable Glic, by default, if country
// filtering is enabled.

#if BUILDFLAG(IS_ANDROID)
constexpr char kDefaultEnabledCountries[] = "us";
#else
constexpr char kDefaultEnabledCountries[] = "us,ca,nz,in";
#endif

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

// Comma separated list of locales to enable Glic, by default, if locale
// filtering is enabled.
constexpr char kDefaultEnabledLocales[] =
#if BUILDFLAG(IS_ANDROID)
    "en-US"
#else
    "af,am,bg,bn,ca,cs,da,de,el,es,es-419,et,fi,fil,fr,gu,hi,hr,hu,id,it,ja,kn,"
    "ko,lt,lv,ml,mr,ms,nl,no,pl,pt-BR,pt-PT,ro,ru,sk,sl,sr,sv,sw,ta,te,th,tr,"
    "uk,vi,zh-CN,zh-TW,en-GB,en-US"
#if BUILDFLAG(IS_CHROMEOS)
    ",eu,gl,is,zu"
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_ANDROID)
    ;

namespace {

constexpr int kExperimentalTriggeringVersion = 1;

signin::Tribool CanUseGeminiInChrome(AccountCapabilities& capabilities) {
  return capabilities.can_use_gemini_in_chrome();
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

bool GetCountryEnablement(GlicGlobalEnabling::Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(features::kGlicCountryFiltering)) {
    base::UmaHistogramEnumeration(
        "Glic.CountryFilteringResult",
        GlicFilteringResult::kAllowedFilteringDisabled);
    return true;
  }
  const std::vector<std::string> enabled_countries =
      GetFieldTrialParamAsSplitString(features::kGlicCountryFiltering,
                                      "enabled_countries",
                                      kDefaultEnabledCountries);

  const std::vector<std::string> disabled_countries =
      GetFieldTrialParamAsSplitString(features::kGlicCountryFiltering,
                                      "disabled_countries", "");

  const bool use_session_country = base::FeatureList::IsEnabled(
      features::kGlicUseSessionCountryForFiltering);

  const std::string permanent_country_code = delegate.GetPermanentCountryCode();
  auto permanent_country_matches = [&](const std::string& c) {
    return base::EqualsCaseInsensitiveASCII(c, permanent_country_code);
  };

  const std::string session_country_code = delegate.GetSessionCountryCode();
  auto session_country_matches = [&](const std::string& c) {
    return base::EqualsCaseInsensitiveASCII(c, session_country_code);
  };

  if (std::ranges::any_of(disabled_countries, permanent_country_matches) ||
      (use_session_country &&
       std::ranges::any_of(disabled_countries, session_country_matches))) {
    base::UmaHistogramEnumeration("Glic.CountryFilteringResult",
                                  GlicFilteringResult::kBlockedInExclusionList);
    return false;
  }

  if (enabled_countries.size() == 1 && enabled_countries[0] == "*") {
    base::UmaHistogramEnumeration(
        "Glic.CountryFilteringResult",
        GlicFilteringResult::kAllowedWildcardInclusion);
    return true;
  }

  if (std::ranges::any_of(enabled_countries, permanent_country_matches) ||
      (use_session_country &&
       std::ranges::any_of(enabled_countries, session_country_matches))) {
    base::UmaHistogramEnumeration("Glic.CountryFilteringResult",
                                  GlicFilteringResult::kAllowedInInclusionList);
    return true;
  }

  base::UmaHistogramEnumeration(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList);
  return false;
}

bool GetLocaleEnablement(GlicGlobalEnabling::Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(features::kGlicLocaleFiltering)) {
    base::UmaHistogramEnumeration(
        "Glic.LocaleFilteringResult",
        GlicFilteringResult::kAllowedFilteringDisabled);
    return true;
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
    base::UmaHistogramEnumeration("Glic.LocaleFilteringResult",
                                  GlicFilteringResult::kBlockedInExclusionList);
    return false;
  }

  if (enabled_locales.size() == 1 && enabled_locales[0] == "*") {
    base::UmaHistogramEnumeration(
        "Glic.LocaleFilteringResult",
        GlicFilteringResult::kAllowedWildcardInclusion);
    return true;
  }

  if (std::ranges::any_of(enabled_locales, matches_locale)) {
    base::UmaHistogramEnumeration("Glic.LocaleFilteringResult",
                                  GlicFilteringResult::kAllowedInInclusionList);
    return true;
  }

  base::UmaHistogramEnumeration(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList);
  return false;
}

bool g_bypass_enablement_checks_for_testing = false;

using DisabledReason = GlicEnabling::ProfileEnablement::DisabledReason;
using FeatureDisabledReason =
    GlicEnabling::ProfileEnablement::FeatureDisabledReason;

void RecordDisabledReasonsWith(
    const GlicEnabling::ProfileEnablement& enablement,
    base::FunctionRef<void(DisabledReason)> record_reason) {
  if (!enablement.feature_enabled) {
    record_reason(DisabledReason::kFeatureDisabled);
  }
  if (!enablement.is_regular_profile) {
    record_reason(DisabledReason::kNotRegularProfile);
  }
  if (!enablement.is_rolled_out) {
    record_reason(DisabledReason::kNotRolledOut);
  }
  if (!enablement.primary_account_is_capable) {
    record_reason(DisabledReason::kPrimaryAccountNotCapable);
  }
  if (!enablement.allowed_by_chrome_policy) {
    record_reason(DisabledReason::kDisallowedByChromePolicy);
  }
  if (!enablement.allowed_by_remote_admin) {
    record_reason(DisabledReason::kDisallowedByRemoteAdmin);
  }
  if (!enablement.allowed_by_remote_other) {
    record_reason(DisabledReason::kDisallowedByRemoteOther);
  }
}

void RecordFeatureDisabledReasonsWith(
    const GlicEnabling::ProfileEnablement& enablement,
    base::FunctionRef<void(FeatureDisabledReason)> record_reason) {
  if (!enablement.feature_flag_enabled) {
    record_reason(FeatureDisabledReason::kFeatureFlagDisabled);
  }
  if (!enablement.allowed_by_country_filter) {
    record_reason(FeatureDisabledReason::kCountryDisabled);
  }
  if (!enablement.allowed_by_locale_filter) {
    record_reason(FeatureDisabledReason::kLocaleDisabled);
  }
  if (!enablement.system_requirement_met) {
    record_reason(FeatureDisabledReason::kSystemRequirementNotMet);
  }
  if (!enablement.os_version_supported) {
    record_reason(FeatureDisabledReason::kOsVersionNotSupported);
  }
}

}  // namespace

// static
void GlicEnabling::SetBypassEnablementChecksForTesting(bool bypass) {
  g_bypass_enablement_checks_for_testing = bypass;
}

std::string GlicGlobalEnabling::Delegate::GetPermanentCountryCode() {
  std::string permanent_country_code =
      base::ToLowerASCII(variations::GetCurrentCountryCode(
          g_browser_process->variations_service()));
  return permanent_country_code;
}

std::string GlicGlobalEnabling::Delegate::GetSessionCountryCode() {
  std::string latest_country;
  if (g_browser_process->variations_service()) {
    latest_country = base::ToLowerASCII(
        g_browser_process->variations_service()->GetLatestCountry());
  }
  return latest_country;
}

std::string GlicGlobalEnabling::Delegate::GetLocale() {
  // Allow null startup_data for tests.
  auto* startup_data = g_browser_process->startup_data();
  if (!startup_data) {
    return std::string();
  }
  return base::ToLowerASCII(
      startup_data->chrome_feature_list_creator()->actual_locale());
}

GlicEnabling::ProfileEnablement::ProfileEnablement() = default;
GlicEnabling::ProfileEnablement::ProfileEnablement(ProfileEnablement&&) =
    default;
GlicEnabling::ProfileEnablement::~ProfileEnablement() = default;

void GlicEnabling::ProfileEnablement::RecordMetrics(
    const std::string& suffix) const {
  bool is_enabled = IsEnabled();
  base::UmaHistogramBoolean(
      base::StrCat({"Glic.ProfileEnablement.IsEnabled.", suffix}), is_enabled);

  auto record_reason = [&](DisabledReason reason) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.ProfileEnablement.DisabledReason.", suffix}),
        reason);
  };

  RecordDisabledReasonsWith(*this, record_reason);
  if (!feature_enabled) {
    RecordFeatureDisabledReason(suffix);
  }

  if (is_enabled) {
    base::UmaHistogramBoolean(
        base::StrCat({"Glic.ProfileEnablement.IsConsented.", suffix}),
        fre_is_consented);
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Glic.ProfileEnablement.EligibleForLive.", suffix}),
      EligibleForLive());
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Glic.ProfileEnablement.IsPrimaryAccountFullySignedIn.", suffix}),
      primary_account_is_fully_signed_in);
  if (suffix == "Startup" && anchor_entrypoint_override_active) {
    auto record_disabled_reason = [&](FeatureDisabledReason reason) {
      base::UmaHistogramEnumeration(
          base::StrCat({"Glic.ProfileEnablement."
                        "AnchoredDespiteEligibilityFailureReason.",
                        suffix}),
          reason);
    };

    RecordFeatureDisabledReasonsWith(*this, record_disabled_reason);
  }

  base::UmaHistogramBoolean(
      base::StrCat(
          {"Glic.ProfileEnablement.IsPrimaryAccountNeedsSignedIn.", suffix}),
      primary_account_needs_signed_in);
}

void GlicEnabling::ProfileEnablement::RecordFeatureDisabledReason(
    const std::string& suffix) const {
  auto record_reason = [&](FeatureDisabledReason reason) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.GlobalEnabling.FeatureDisabledReason.", suffix}),
        reason);
  };

  RecordFeatureDisabledReasonsWith(*this, record_reason);
}

void GlicEnabling::ProfileEnablement::RecordStartupMetrics() const {
  RecordMetrics("Startup");
}

void GlicEnabling::ProfileEnablement::RecordSteadyStateMetrics() const {
  RecordMetrics("SteadyState");
}

bool GlicEnabling::ProfileEnablement::IsProfileEligible() const {
  return feature_enabled && is_regular_profile;
}

bool GlicEnabling::ProfileEnablement::IsEnabled() const {
  bool base_checks = IsProfileEligible() && is_rolled_out &&
                     primary_account_is_capable && !DisallowedByAdmin() &&
                     allowed_by_remote_other;

  if (!base_checks) {
    return false;
  }

  return allowed_by_country_filter && allowed_by_locale_filter;
}

bool GlicEnabling::ProfileEnablement::IsEnabledAndConsented() const {
  return IsEnabled() && fre_is_consented;
}

bool GlicEnabling::ProfileEnablement::ShouldShowSettingsPage() const {
  const bool show_ai_settings_for_testing = base::FeatureList::IsEnabled(
      optimization_guide::features::kAiSettingsPageForceAvailable);

  // If the feature is disabled by enterprise policy, the settings page
  // should be shown (it will be shown in a policy-disabled state) only if
  // all other non-enterprise conditions are met: the account has all
  // appropriate permissions and has previously completed the FRE before the
  // policy went into effect. The settings page should also be shown if the
  // settings testing flag is enabled.
  return show_ai_settings_for_testing ||
         (IsProfileEligible() && is_rolled_out && primary_account_is_capable &&
          allowed_by_remote_other && fre_is_consented);
}

bool GlicEnabling::ProfileEnablement::ShouldShowGlicButton() const {
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

bool GlicEnabling::ProfileEnablement::EligibleForLive() const {
  return IsProfileEligible() && live_allowed;
}

bool GlicEnabling::ProfileEnablement::EligibleForShareImage() const {
  return IsProfileEligible() && share_image_allowed;
}

bool GlicEnabling::ProfileEnablement::EligibleForGeminiEnterpriseSettings()
    const {
  return IsProfileEligible() && gemini_enterprise_settings.has_value();
}

bool GlicEnabling::ProfileEnablement::DisallowedByAdmin() const {
  return !allowed_by_chrome_policy || !allowed_by_remote_admin;
}

// static
GlicEnabling::ProfileEnablement GlicEnabling::EnablementForProfile(
    Profile* profile) {
  ProfileEnablement result;

  if (g_bypass_enablement_checks_for_testing) {
    return result;
  }

  GlicGlobalEnabling& global_enabling =
      g_browser_process->GetFeatures()->glic_global_enabling();

  result.feature_flag_enabled = base::FeatureList::IsEnabled(features::kGlic);
  result.allowed_by_country_filter = global_enabling.IsCountryEnabled();
  result.allowed_by_locale_filter = global_enabling.IsLocaleEnabled();
  result.system_requirement_met = global_enabling.IsSystemRequirementMet();
  result.fre_is_consented = HasConsentedForProfile(profile);
  result.os_version_supported = global_enabling.IsOsVersionSupported();

  bool global_criteria_met = global_enabling.IsEnabledByGlobalCriteria();
  if (!global_criteria_met) {
    result.anchor_entrypoint_override_active =
        IsAnchoredButIneligible(global_criteria_met, result.fre_is_consented);
    if (!result.anchor_entrypoint_override_active) {
      result.feature_enabled = false;
      return result;
    }
  }

  if (!profile || !profile->IsRegularProfile()) {
    result.is_regular_profile = false;
    return result;
  }

  // Certain checks are bypassed if --glic-dev is passed.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kGlicDev)) {
    if (!base::FeatureList::IsEnabled(features::kGlicRollout) &&
        !IsEligibleForGlicTieredRollout(profile)) {
      result.is_rolled_out = false;
    }

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    AccountInfo primary_account =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));

    // Not having a primary account is considered not fully signed in if the
    // kGlicShowForSignedOut feature is enabled. Otherwise, it is ineligible.
    if (primary_account.IsEmpty()) {
      if (base::FeatureList::IsEnabled(features::kGlicShowForSignedOut) &&
          !WasPreviouslyNotAllowed(profile)) {
        result.primary_account_needs_signed_in = true;
      } else {
        result.primary_account_is_capable = false;
      }
      result.live_allowed = false;
      result.share_image_allowed = false;
    } else {
      // Check if the profile is currently paused.
      if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              primary_account.account_id)) {
        result.primary_account_is_fully_signed_in = false;
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
          (CanUseGeminiInChrome(primary_account.capabilities) !=
           signin::Tribool::kUnknown)) {
        capability_value = CanUseGeminiInChrome(primary_account.capabilities);
      }
      result.primary_account_is_capable =
          (capability_value == signin::Tribool::kTrue);

      // If the feature is overridden by a field trial, and the user's
      // eligibility is known and different for the two capabilities, add them
      // to a synthetic trial.
      base::FieldTrial* field_trial = base::FeatureList::GetFieldTrial(
          switches::kGlicEligibilitySeparateAccountCapability);
      if (field_trial &&
          (CanUseGeminiInChrome(primary_account.capabilities) !=
           signin::Tribool::kUnknown) &&
          (primary_account.capabilities.can_use_model_execution_features() !=
           signin::Tribool::kUnknown) &&
          (CanUseGeminiInChrome(primary_account.capabilities) !=
           primary_account.capabilities.can_use_model_execution_features())) {
        g_browser_process->GetFeatures()
            ->glic_synthetic_trial_manager()
            ->SetSyntheticExperimentState(
                kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName,
                field_trial->GetGroupNameWithoutActivation());
      }

      result.live_allowed =
          primary_account.capabilities.can_use_model_execution_features() ==
          signin::Tribool::kTrue;

      result.share_image_allowed =
          primary_account.capabilities.can_use_model_execution_features() ==
          signin::Tribool::kTrue;
    }
  }

  result.gemini_enterprise_settings = GetGeminiEnterpriseSettings(profile);

  if (profile->GetPrefs()->GetInteger(::prefs::kGeminiSettings) !=
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled)) {
    result.allowed_by_chrome_policy = false;
  }

  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    if (auto cached_user_status =
            GlicUserStatusFetcher::GetCachedUserStatus(profile);
        cached_user_status.has_value()) {
      switch (cached_user_status->user_status_code) {
        case UserStatusCode::DISABLED_BY_ADMIN:
          result.allowed_by_remote_admin = false;
          break;
        case UserStatusCode::DISABLED_OTHER:
          result.allowed_by_remote_other = false;
          break;
        case UserStatusCode::ENABLED:
          break;
        case UserStatusCode::SERVER_UNAVAILABLE:
          // We never cache SERVER_UNAVAILABLE.
          NOTREACHED();
      }
    }
  }

  return result;
}

GlicGlobalEnabling::GlicGlobalEnabling(Delegate& delegate) {
  locale_enablement_ = GetLocaleEnablement(delegate);
  country_enablement_ = GetCountryEnablement(delegate);
}

GlicGlobalEnabling::~GlicGlobalEnabling() = default;

bool GlicGlobalEnabling::IsSystemRequirementMet() const {
  static const bool supported_system_requirements = [] {
    if (base::SysInfo::AmountOfTotalPhysicalMemory().AsDeprecatedByteCount() <
        base::MiB(features::kGlicMinRequiredRamMb.Get())) {
      return false;
    }
#if BUILDFLAG(IS_CHROMEOS)
    constexpr base::ByteCount kMinimumMemoryThreshold = base::GiB(8);
    const bool bypass_cbx_requirement =
        GlicEnabling::IsLikelyDogfoodClient() &&
        base::SysInfo::AmountOfTotalPhysicalMemory().AsDeprecatedByteCount() >=
            kMinimumMemoryThreshold;

    return (bypass_cbx_requirement ||
            base::FeatureList::IsEnabled(
                chromeos::features::kFeatureManagementGlic));
#else
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }();

  return supported_system_requirements;
}

bool GlicGlobalEnabling::IsOsVersionSupported() const {
  static const bool supported_os_version = [] {
#if BUILDFLAG(IS_ANDROID)
    // Glic requires Foreground Services (FGS) to run, which has strict
    // requirements starting from Android S (see b/515767943).
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      return false;
    }
    return true;
#else
    return true;
#endif
  }();

  return supported_os_version;
}

bool GlicGlobalEnabling::IsEnabledByGlobalCriteria() {
  if (g_bypass_enablement_checks_for_testing) {
    return true;
  }
  // It is important that this value not change at runtime in production. Any
  // future updates to this function must maintain that property.
  bool is_enabled = base::FeatureList::IsEnabled(features::kGlic) &&
                    locale_enablement_.value_or(true) &&
                    country_enablement_.value_or(true);

  return is_enabled && IsOsVersionSupported() && IsSystemRequirementMet();
}

// static
bool GlicEnabling::IsEnabledByGlobalCriteria() {
  return g_browser_process->GetFeatures()
      ->glic_global_enabling()
      .IsEnabledByGlobalCriteria();
}

// static
bool GlicEnabling::IsLikelyDogfoodClient() {
  if (base::FeatureList::IsEnabled(features::kGlicIgnoreDogfoodClient)) {
    return false;
  }
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  return variations_service && variations_service->IsLikelyDogfoodClient();
}

// static
bool GlicEnabling::IsProfileEligible(Profile* profile) {
  if (g_bypass_enablement_checks_for_testing) {
    return true;
  }

  // Glic is supported only in regular profiles (i.e. disabled in incognito,
  // guest, system profile, etc.).
  if (!profile || !profile->IsRegularProfile()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (!IsChromeOSProfileEligible(profile)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool global_criteria = IsEnabledByGlobalCriteria();
  bool consented = HasConsentedForProfile(profile);

  // A profile is eligible if global criteria are met OR if the user previously
  // onboarded and the anchor override is active (to keep the entry point
  // visible for error states).
  return global_criteria || IsAnchoredButIneligible(global_criteria, consented);
}

// static
bool GlicEnabling::IsAnchoredButIneligible(bool global_criteria_met,
                                           bool consented) {
  return !global_criteria_met && consented &&
         base::FeatureList::IsEnabled(
             features::kGlicAnchorEntryPointForOnboardedUsers);
}

// static
void GlicEnabling::RecordProfileIneligibilityMetricsAtStartup(
    Profile* profile) {
  if (g_bypass_enablement_checks_for_testing) {
    return;
  }

  // Only record related metrics if the profile is ineligible.
  if (IsProfileEligible(profile)) {
    return;
  }

  base::UmaHistogramBoolean("Glic.ProfileEnablement.IsEnabled.Startup", false);

  // Log specific causes of ineligibility.
  if (!IsEnabledByGlobalCriteria()) {
    base::UmaHistogramEnumeration(
        "Glic.ProfileEnablement.DisabledReason.Startup",
        DisabledReason::kFeatureDisabled);
  }
  // Aside from flag enablement, `profile` can also be ineligible if it is not
  // a regular profile, or it fails ChromeOS-specific checks in
  // `IsProfileEligible`.
  bool not_regular_profile = false;
  if (!profile || !(profile->IsRegularProfile())) {
    not_regular_profile = true;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (!IsChromeOSProfileEligible(profile)) {
    not_regular_profile = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (not_regular_profile) {
    base::UmaHistogramEnumeration(
        "Glic.ProfileEnablement.DisabledReason.Startup",
        DisabledReason::kNotRegularProfile);
  }
}

// static
bool GlicEnabling::IsEnabledForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabled();
}

// static
bool GlicEnabling::WasPreviouslyNotAllowed(Profile* profile) {
  return profile &&
         profile->GetPrefs()->GetBoolean(prefs::kGlicPreviouslyNotAllowed);
}

// static
bool GlicEnabling::HasConsentedForProfile(Profile* profile) {
  return profile && GetCompletedFre(profile) == prefs::FreStatus::kCompleted;
}

// static
bool GlicEnabling::IsEnabledAndConsentForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabledAndConsented();
}

// static
bool GlicEnabling::DidDismissForProfile(Profile* profile) {
  return profile && GetCompletedFre(profile) == prefs::FreStatus::kIncomplete;
}

// static
bool GlicEnabling::IsReadyForProfile(Profile* profile) {
  return GetProfileReadyState(profile) == mojom::ProfileReadyState::kReady;
}

// static
mojom::ProfileReadyState GlicEnabling::GetProfileReadyState(Profile* profile) {
  // The order of these checks is important. Higher priority states (like admin
  // disablement or sign-in requirements) should be returned first.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    return mojom::ProfileReadyState::kReady;
  }

  const ProfileEnablement enablement = EnablementForProfile(profile);
  if (enablement.DisallowedByAdmin()) {
    return mojom::ProfileReadyState::kDisabledByAdmin;
  }

  if (!enablement.primary_account_is_fully_signed_in ||
      enablement.primary_account_needs_signed_in) {
    return mojom::ProfileReadyState::kSignInRequired;
  }

  // If the account is not capable, we only want to return a specific error
  // state if the entry point is "anchored" (i.e. the user has onboarded
  // previously and we want to show the button in an error state instead of
  // hiding it).
  if (!enablement.primary_account_is_capable) {
    if (enablement.anchor_entrypoint_override_active) {
      return mojom::ProfileReadyState::kIneligibleAccount;
    } else {
      return mojom::ProfileReadyState::kIneligible;
    }
  }

  // Similar to account capability, for country filtering we only show a
  // specific mismatch error if the entry point is anchored.
  if (!enablement.allowed_by_country_filter) {
    if (enablement.anchor_entrypoint_override_active) {
      return mojom::ProfileReadyState::kLocationMismatch;
    } else {
      return mojom::ProfileReadyState::kIneligible;
    }
  }

  // If the account is anchored but we haven't identified a more specific
  // reason for ineligibility above (like country or account capability),
  // return a general ineligible account state. This prevents an anchored
  // button from ever showing a 'Ready' state.
  if (enablement.anchor_entrypoint_override_active) {
    return mojom::ProfileReadyState::kIneligibleAccount;
  }

  // Fallthrough for any other reason the profile might not be enabled.
  if (!enablement.IsEnabled()) {
    return mojom::ProfileReadyState::kIneligible;
  }

  return mojom::ProfileReadyState::kReady;
}

// static
bool GlicEnabling::IsEligibleForGlicTieredRollout(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kGlicTieredRollout) &&
      profile->GetPrefs()->GetBoolean(prefs::kGlicRolloutEligibility)) {
    return true;
  }

  subscription_eligibility::SubscriptionEligibilityService*
      subscription_eligibility_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(profile);
  if (!subscription_eligibility_service) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kGlicTieredRolloutV2) &&
         features::GetGlicTieredRolloutV2EligibleTiers().contains(
             subscription_eligibility_service->GetAiSubscriptionTier());
}

// static
bool GlicEnabling::IsInternalsWebUIEnabled(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kGlic) &&
         profile->IsRegularProfile();
}

// static
bool GlicEnabling::ShouldShowSettingsPage(Profile* profile) {
  return EnablementForProfile(profile).ShouldShowSettingsPage();
}

bool GlicEnabling::ShouldShowWebActuationToggle() const {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAlwaysShowWebActuationToggle)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicWebActuationSetting)) {
    return false;
  }

  // If the account is ineligible, hide the toggle.
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  if (!glic_service) {
    return false;
  }
  if (!glic_service->HasActorPolicyChecker()) {
    return false;
  }
  if (glic_service->actor_policy_checker().CannotActOnWebReason() ==
      glic::GlicActorPolicyChecker::CannotActReason::
          kAccountCapabilityIneligible) {
    return false;
  }

  bool is_managed = glic::GlicActorPolicyChecker::IsBrowserManaged(*profile_);

  bool is_enterprise_account = false;
  if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
    is_enterprise_account = glic::GlicActorPolicyChecker::IsEnterpriseAccount(
        *profile_, actor_service->GetJournal());
  }

  // Enterprise Case: Align toggle visibility with GlicActorPolicyChecker.
  if (is_managed || is_enterprise_account) {
    return glic_service->actor_policy_checker().CanActOnWeb();
  }

  // Google one User
  // If not managed, we check consumer subscription tiers.
  const base::flat_set<int32_t>& allowed_tiers =
      glic::GlicActorPolicyChecker::GetActorEligibleTiers();
  // If no tiers are allowed, the toggle should never be shown.
  if (allowed_tiers.empty()) {
    return false;
  }

  // NOTE: kGlicWebActuationSettingsToggle controls toggle visibility based
  // solely on subscription eligibility. If this feature is disabled, the
  // toggle remains visible only if the user has previously accepted the
  // consent card.
  if (base::FeatureList::IsEnabled(features::kGlicWebActuationSettingsToggle)) {
    // Always show the toggle for internal dogfooders, mirroring the bypass in
    // GlicActorPolicyChecker.
    if (glic::GlicEnabling::IsLikelyDogfoodClient()) {
      return true;
    }
    // Strict subscription check for external users.
    auto* subscription_service = subscription_eligibility::
        SubscriptionEligibilityServiceFactory::GetForProfile(profile_);
    CHECK(subscription_service);
    return allowed_tiers.contains(
        subscription_service->GetAiSubscriptionTier());
  }
  // Show the toggle if the user has explicitly modified the preference before
  // (via accepting the consent card).
  if (!glic_service->enabling().IsUserEnabledActuationOnWebDefault()) {
    return true;
  }
  return false;
}

bool GlicEnabling::ShouldShowGlicButton(Profile* profile) {
  return EnablementForProfile(profile).ShouldShowGlicButton();
}

void GlicEnabling::OnGlicSettingsPolicyChanged() {
  // Update the overall enabled status as the policy has changed.
  UpdateEnabledStatus();
}

#if BUILDFLAG(IS_CHROMEOS)
// static
bool GlicEnabling::IsChromeOSProfileEligible(Profile* profile) {
  if (!ash::IsUserBrowserContext(profile)) {
    // We only allow regular user session profiles.
    // E.g. disallowed on login screen.
    return false;
  }
  auto* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (user == nullptr) {
    // When there is no signed in user on ChromeOS, assume that the profile is
    // not eligible.
    return false;
  }
  switch (user->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      // These are ok to use Glic.
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
  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
bool GlicEnabling::IsAutoOpenForPdfEnabled(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kAutoOpenGlicForPdf)) {
    return false;
  }

  if (HasConsentedForProfile(profile)) {
    return true;
  }

  return features::kAutoOpenGlicForPdfWithOnboarding.Get();
}

// static
bool GlicEnabling::IsContextualMenuItemEnabled(Profile* profile) {
  bool enabled = IsEnabledForProfile(profile) &&
                 base::FeatureList::IsEnabled(features::kGlicContextMenu);
  base::UmaHistogramBoolean("Glic.WebContentContextMenu.Enabled", enabled);
  return enabled;
}

// static
bool GlicEnabling::IsSelectionPromptEnabledForProfile(Profile* profile) {
  return IsEnabledForProfile(profile) &&
         base::FeatureList::IsEnabled(features::kGlicSelectionPrompt);
}

// static
bool GlicEnabling::IsLiveAndFloatyEnabledByFlags() {
  // Despite the name, when off, this disables live mode and floaty.
  return base::FeatureList::IsEnabled(features::kGlicLiveMode);
}

// static
bool GlicEnabling::IsShareImageEnabledForProfile(Profile* profile) {
  auto enablement = EnablementForProfile(profile);
  return enablement.IsEnabled() && enablement.EligibleForShareImage() &&
         base::FeatureList::IsEnabled(features::kGlicShareImage);
}

namespace {
std::optional<glic::mojom::GeminiEnterpriseSettings>
ParseGeminiEnterpriseSettings(const base::DictValue& dict) {
  const std::string* project_id = dict.FindString("project_id");
  const std::string* app_id = dict.FindString("app_id");
  const std::string* location = dict.FindString("location");
  if (project_id && app_id && location) {
    glic::mojom::GeminiEnterpriseSettings settings;
    settings.project_id = *project_id;
    settings.app_id = *app_id;
    settings.location = *location;
    return settings;
  }
  return std::nullopt;
}
}  // namespace

// static
std::optional<glic::mojom::GeminiEnterpriseSettings>
GlicEnabling::GetGeminiEnterpriseSettings(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicGeminiEnterpriseSettingsEnabled)) {
    return std::nullopt;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  // TODO(b/517605114): Remove this command line switch override before launch.
  if (command_line->HasSwitch(
          switches::kGlicGeminiEnterpriseSettingsOverride)) {
    std::string switch_value = command_line->GetSwitchValueASCII(
        switches::kGlicGeminiEnterpriseSettingsOverride);
    auto parsed_json = base::JSONReader::Read(
        switch_value, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (parsed_json && parsed_json->is_dict()) {
      if (auto settings =
              ParseGeminiEnterpriseSettings(parsed_json->GetDict());
          settings.has_value()) {
        return settings;
      } else {
        LOG(ERROR) << "Gemini Enterprise settings override is missing "
                      "required fields.";
      }
    } else {
      LOG(ERROR) << "Gemini Enterprise settings override is not a valid "
                    "JSON dictionary.";
    }
  }

  const base::DictValue& pref_dict =
      profile->GetPrefs()->GetDict(glic::prefs::kGlicGeminiEnterpriseSettings);
  return ParseGeminiEnterpriseSettings(pref_dict);
}

// static
std::unique_ptr<GlicEnabling> GlicEnabling::CreateForTesting(  // IN-TEST
    Profile* profile,
    ProfileAttributesStorage* profile_attributes_storage) {
  return std::make_unique<GlicEnabling>(base::PassKey<GlicEnabling>(), profile,
                                        profile_attributes_storage);
}

GlicEnabling::GlicEnabling(
    base::PassKey<GlicKeyedService, GlicEnabling> pass_key,
    Profile* profile,
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
  pref_registrar_.Add(
      prefs::kGlicUserEnabledActuationOnWeb,
      base::BindRepeating(&GlicEnabling::OnUserEnabledActuationOnWebChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kGlicExperimentalTriggeringEnabled,
      base::BindRepeating(&GlicEnabling::OnExperimentalTriggeringEnabledChanged,
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

  subscription_eligibility::SubscriptionEligibilityService*
      subscription_eligibility_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(profile);
  if (subscription_eligibility_service) {
    subscription_eligibility_service_observation_.Observe(
        subscription_eligibility_service);
  }

  static_assert(std::is_final_v<GlicEnabling>,
                "If you want to inherit from GlicEnabling, ensure this "
                "function call does not violate the 'no virtual functions in "
                "constructors' style guide rule. Consider a 2-phase setup");
  last_experimental_triggering_state_ = GetExperimentalTriggeringState();
}
GlicEnabling::~GlicEnabling() = default;

bool GlicEnabling::IsAllowed() {
  return IsEnabledForProfile(profile_);
}

bool GlicEnabling::HasConsented() const {
  return GetCompletedFre() == prefs::FreStatus::kCompleted;
}

// static
prefs::FreStatus GlicEnabling::GetCompletedFre(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringOptInBypass)) {
    return prefs::FreStatus::kCompleted;
  }
  return static_cast<prefs::FreStatus>(
      profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre));
}

prefs::FreStatus GlicEnabling::GetCompletedFre() const {
  return GetCompletedFre(profile_);
}

void GlicEnabling::SetCompletedFre(prefs::FreStatus status) {
  profile_->GetPrefs()->SetInteger(prefs::kGlicCompletedFre,
                                   static_cast<int>(status));
}

bool GlicEnabling::GetUserEnabledActuationOnWeb() const {
  if (base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringOptInBypass)) {
    return true;
  }
  return profile_->GetPrefs()->GetBoolean(
      prefs::kGlicUserEnabledActuationOnWeb);
}

bool GlicEnabling::IsUserEnabledActuationOnWebDefault() const {
  const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
      prefs::kGlicUserEnabledActuationOnWeb);
  return pref && pref->IsDefaultValue();
}

bool GlicEnabling::IsExperimentalTriggeringEnabledDefault() const {
  const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
      prefs::kGlicExperimentalTriggeringEnabled);
  return pref && pref->IsDefaultValue();
}

bool GlicEnabling::IsExperimentalTriggeringUserControlled() const {
  const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
      prefs::kGlicExperimentalTriggeringEnabled);
  return pref && !pref->IsManaged();
}

void GlicEnabling::SetUserEnabledActuationOnWeb(bool enabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                   enabled);
}

bool GlicEnabling::GetExperimentalTriggeringEnabled() const {
  if (base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringOptInBypass)) {
    return true;
  }
  return profile_->GetPrefs()->GetBoolean(
      prefs::kGlicExperimentalTriggeringEnabled);
}

syncer::DeviceInfo::GlicExperimentalTriggeringState
GlicEnabling::GetExperimentalTriggeringState() const {
  if (!IsEnabledForProfile(profile_)) {
    return syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable;
  }

  if (!base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering)) {
    return syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable;
  }
  bool is_device_managed = false;

  // TODO(crbug.com/510420396): Refactor this out as the logic is the same in
  // glic policy checker
  // Check if the browser or the device is managed
  auto* management_service_factory =
      policy::ManagementServiceFactory::GetInstance();
  auto* browser_management_service =
      management_service_factory->GetForProfile(profile_);
  auto* platform_management_service =
      management_service_factory->GetForPlatform();
  if ((browser_management_service && browser_management_service->IsManaged()) ||
      (platform_management_service &&
       platform_management_service->IsManaged())) {
    is_device_managed = true;
  }

  bool has_managed_account = false;

  // Check if enterprise account
  if (!is_device_managed) {
    bool is_enterprise_account_data_protected = false;
    if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
      std::optional<glic::CachedUserStatus> cached_user_status =
          glic::GlicUserStatusFetcher::GetCachedUserStatus(profile_);
      if (cached_user_status.has_value()) {
        is_enterprise_account_data_protected =
            cached_user_status->is_enterprise_account_data_protected;
      } else {
        // NOTE: Do not return false as a fail-closed here. CachedUserStatus is
        // only fetched when `is_managed` of
        // GlicUserStatusFetcher::UpdateUserStatus is true. Returning false
        // means gating all the non-enterprise accounts from actuation.
      }
    }

    signin::Tribool account_is_managed_tribool = signin::Tribool::kUnknown;
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    if (identity_manager) {
      // `account_info` is empty if the user has not signed in.
      const CoreAccountInfo account_info =
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin);
      const AccountInfo extended_account_info =
          identity_manager->FindExtendedAccountInfoByAccountId(
              account_info.account_id);

      account_is_managed_tribool = extended_account_info.IsManaged();
    }

    has_managed_account = is_enterprise_account_data_protected ||
                          account_is_managed_tribool == signin::Tribool::kTrue;
  }

  bool is_managed = is_device_managed || has_managed_account;

  // Apply policy if managed, unless it's a dogfood client.
  if (is_managed && !IsLikelyDogfoodClient()) {
    // Check policy
    auto* pref_service = profile_->GetPrefs();
    auto policy_state = static_cast<glic::prefs::GlicSparkPolicyState>(
        pref_service->GetInteger(glic::prefs::kGlicSparkPolicySettings));
    if (policy_state != glic::prefs::GlicSparkPolicyState::kEnabled) {
      return syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable;
    }
  }

  if (HasConsented() && GetUserEnabledActuationOnWeb() &&
      GetExperimentalTriggeringEnabled()) {
    return syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady;
  }
  return syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn;
}

std::optional<int> GlicEnabling::GetExperimentalTriggeringVersion() const {
  if (GetExperimentalTriggeringState() ==
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable) {
    return std::nullopt;
  }
  return kExperimentalTriggeringVersion;
}

RequiredExperimentalOptIn GlicEnabling::GetRequiredExperimentalOptIn() const {
  if (!HasConsented()) {
    return RequiredExperimentalOptIn::kGlic;
  }
  if (!GetUserEnabledActuationOnWeb()) {
    return RequiredExperimentalOptIn::kActuation;
  }
  if (!GetExperimentalTriggeringEnabled()) {
    return RequiredExperimentalOptIn::kExperimental;
  }
  return RequiredExperimentalOptIn::kNotNeeded;
}

void GlicEnabling::SetExperimentalTriggeringEnabled(bool enabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                   enabled);
}

void GlicEnabling::MaybeRecordStartupMetrics() {
  if (recorded_startup_metrics_) {
    return;
  }

  recorded_startup_metrics_ = true;
  EnablementForProfile(profile_).RecordStartupMetrics();
}

base::CallbackListSubscription GlicEnabling::RegisterAllowedChanged(
    EnableChangedCallback callback) {
  return enable_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterOnConsentChanged(
    ConsentChangedCallback callback) {
  return consent_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicEnabling::RegisterOnUserEnabledActuationOnWebChanged(
    UserEnabledActuationOnWebChangedCallback callback) {
  return user_enabled_actuation_on_web_changed_callback_list_.Add(
      std::move(callback));
}

void GlicEnabling::OnUserEnabledActuationOnWebChanged() {
  user_enabled_actuation_on_web_changed_callback_list_.Notify();
  MaybeNotifyExperimentalTriggeringStateChanged();
}

base::CallbackListSubscription
GlicEnabling::RegisterOnExperimentalTriggeringEnabledChanged(
    ExperimentalTriggeringEnabledChangedCallback callback) {
  return experimental_triggering_enabled_changed_callback_list_.Add(
      std::move(callback));
}

void GlicEnabling::OnExperimentalTriggeringEnabledChanged() {
  experimental_triggering_enabled_changed_callback_list_.Notify();
  MaybeNotifyExperimentalTriggeringStateChanged();
}

base::CallbackListSubscription
GlicEnabling::RegisterOnExperimentalTriggeringStateChanged(
    ExperimentalTriggeringStateChangedCallback callback) {
  return experimental_triggering_state_changed_callback_list_.Add(
      std::move(callback));
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
#if BUILDFLAG(IS_ANDROID)
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    SetCompletedFre(prefs::FreStatus::kNotStarted);
    profile_->GetPrefs()->ClearPref(prefs::kGlicUserEnabledActuationOnWeb);
    profile_->GetPrefs()->ClearPref(prefs::kGlicGeolocationEnabled);
  }
#endif
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

void GlicEnabling::OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) {
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
  if (IsAllowed()) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicPreviouslyNotAllowed, false);
  } else {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    if (identity_manager &&
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      profile_->GetPrefs()->SetBoolean(prefs::kGlicPreviouslyNotAllowed, true);
    }
  }
  if (ProfileAttributesEntry* entry =
          profile_attributes_storage_->GetProfileAttributesWithPath(
              profile_->GetPath())) {
    entry->SetIsGlicEligible(IsAllowed());
  }
  enable_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
  profile_ready_state_changed_callback_list_.Notify();
  MaybeNotifyExperimentalTriggeringStateChanged();
}

void GlicEnabling::UpdateConsentStatus() {
  consent_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
  profile_ready_state_changed_callback_list_.Notify();
  MaybeNotifyExperimentalTriggeringStateChanged();
}

void GlicEnabling::MaybeNotifyExperimentalTriggeringStateChanged() {
  auto new_state = GetExperimentalTriggeringState();
  if (new_state != last_experimental_triggering_state_) {
    last_experimental_triggering_state_ = new_state;
    experimental_triggering_state_changed_callback_list_.Notify();
  }
}

}  // namespace glic
