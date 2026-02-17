// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_hats_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/channel_info.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr char kChannel[] = "Channel";
constexpr char kChromeVersion[] = "Chrome Version";
constexpr char kNumberOfChromeProfiles[] = "Number of Chrome Profiles";
constexpr char kNumberOfGoogleAccounts[] = "Number of Google Accounts";
constexpr char kDismissedSigninBubbleType[] =
    "Data type Sign-in Bubble Dismissed";
constexpr char kIdentityState[] = "Sign-in Status";

// Launches a HaTS survey for the profile associated with `browser`.
void LaunchHatsSurveyForBrowser(const std::string& trigger, Browser* browser) {
  if (!browser) {
    return;
  }
  signin::LaunchHatsSurveyForProfile(trigger, browser->GetProfile());
}

std::string GetDismissedBubbleType(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kAddressBubble:
      return "Address Bubble";
    case signin_metrics::AccessPoint::kBookmarkBubble:
      return "Bookmark Bubble";
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
      return "Extension Install Bubble";
    case signin_metrics::AccessPoint::kPasswordBubble:
      return "Password Bubble";
    default:
      return "Other";
  }
}

SurveyStringData GetSurveyStringData(const std::string& trigger,
                                     Profile* profile,
                                     std::optional<signin_metrics::AccessPoint>
                                         access_point_for_data_type_promo) {
  SurveyStringData data;
  data.emplace(
      kChannel,
      std::string(version_info::GetChannelString(chrome::GetChannel())));

  data.emplace(kChromeVersion, version_info::GetVersion().GetString());

  if (trigger == kHatsSurveyTriggerIdentitySigninPromoBubbleDismissed) {
    CHECK(access_point_for_data_type_promo.has_value());
    data.emplace(
        kDismissedSigninBubbleType,
        GetDismissedBubbleType(access_point_for_data_type_promo.value()));
  }

  // For bucketing, report "5+" if the number of profiles is larger than 5.
  const size_t num_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  data.emplace(kNumberOfChromeProfiles,
               num_profiles > 5 ? "5+" : base::NumberToString(num_profiles));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // For bucketing, report "5+" if the number of accounts is larger than 5.
  const size_t num_google_accounts =
      identity_manager->GetAccountsWithRefreshTokens().size();
  data.emplace(kNumberOfGoogleAccounts,
               num_google_accounts > 5
                   ? "5+"
                   : base::NumberToString(num_google_accounts));

  data.emplace(kIdentityState,
               signin_util::SignedInStateToString(
                   signin_util::GetSignedInState(identity_manager)));

  return data;
}

// Returns true if surveys are allowed for the current application locale.
bool IsLocaleAllowedForSurvey() {
  static constexpr auto kAllowedSurveyLocales =
      base::MakeFixedFlatSet<std::string_view>({
          "de",
          "en",
          "en-GB",
          "en-US",
          "fr",
          "fr-CA",
          "ja",
          "pt",
      });
  return kAllowedSurveyLocales.contains(
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
}

// Returns true if the survey corresponding to `trigger` should be enabled.
// Surveys are gated by both a feature flag and locale eligibility.
// However, if the feature flag is overridden (e.g., by Finch or command-line),
// the locale check is bypassed to allow server-side control.
//
// The given survey might still be suppressed if it has a conflicting feature
// enabled (used to coordinate surveys associated with similar Chrome triggers).
bool IsSurveyEnabledForHatsTrigger(const std::string& trigger) {
  static const base::NoDestructor<
      absl::flat_hash_map<std::string_view, const base::Feature*>>
      kHatsTriggerFeatureMap(
          {{kHatsSurveyTriggerIdentityAddressBubbleSignin,
            &switches::kChromeIdentitySurveyAddressBubbleSignin},
           {kHatsSurveyTriggerIdentityDiceWebSigninAccepted,
            &switches::kChromeIdentitySurveyDiceWebSigninAccepted},
           {kHatsSurveyTriggerIdentityDiceWebSigninDeclined,
            &switches::kChromeIdentitySurveyDiceWebSigninDeclined},
           {kHatsSurveyTriggerIdentityFirstRunSignin,
            &switches::kChromeIdentitySurveyFirstRunSignin},
           {kHatsSurveyTriggerIdentityPasswordBubbleSignin,
            &switches::kChromeIdentitySurveyPasswordBubbleSignin},
           {kHatsSurveyTriggerIdentityProfileMenuDismissed,
            &switches::kChromeIdentitySurveyProfileMenuDismissed},
           {kHatsSurveyTriggerIdentityProfileMenuSignin,
            &switches::kChromeIdentitySurveyProfileMenuSignin},
           {kHatsSurveyTriggerIdentityProfilePickerAddProfileSignin,
            &switches::kChromeIdentitySurveyProfilePickerAddProfileSignin},
           {kHatsSurveyTriggerIdentitySigninInterceptProfileSeparation,
            &switches::kChromeIdentitySurveySigninInterceptProfileSeparation},
           {kHatsSurveyTriggerIdentitySigninPromoBubbleDismissed,
            &switches::kChromeIdentitySurveySigninPromoBubbleDismissed},
           {kHatsSurveyTriggerIdentitySwitchProfileFromProfileMenu,
            &switches::kChromeIdentitySurveySwitchProfileFromProfileMenu},
           {kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker,
            &switches::kChromeIdentitySurveySwitchProfileFromProfilePicker},
           {kHatsSurveyTriggerIdentityFirstRunCompleted,
            &switches::kBeforeFirstRunDesktopRefreshSurvey}});
  // Map of HaTS features that are conflicting with each other. Keys are
  // features that are suppressed if the corresponding value feature is
  // enabled.
  static const base::NoDestructor<
      absl::flat_hash_map<const base::Feature*, const base::Feature*>>
      kConflictingFeaturesMap(
          {{&switches::kChromeIdentitySurveyFirstRunSignin,
            &switches::kBeforeFirstRunDesktopRefreshSurvey}});

  const auto* feature = base::FindPtrOrNull(*kHatsTriggerFeatureMap, trigger);

  if (!feature) {
    // No matching feature for the given trigger.
    return false;
  }

  if (kConflictingFeaturesMap->contains(feature) &&
      base::FeatureList::IsEnabled(*kConflictingFeaturesMap->at(feature))) {
    // If the feature has a conflicting feature, suppress the survey.
    return false;
  }

  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverridden(feature->name)) {
    // If the feature state is overridden (e.g., by Finch or command-line),
    // bypass the locale check. This allows Finch to enable surveys for
    // locales not listed in `IsLocaleAllowedForSurvey`.
    return base::FeatureList::IsEnabled(*feature);
  }

  // If not overridden, the survey is enabled only if the feature is on
  // AND the locale is eligible.
  return IsLocaleAllowedForSurvey() && base::FeatureList::IsEnabled(*feature);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Attempts to launch the survey, whose SurveyStringData is provided at the last
// possible moment by `data_factory`. This avoids unnecessary work in case the
// survey can't be launched anyway.
void LaunchHatsSurveyForProfileInternal(
    const std::string& trigger,
    Profile* profile,
    bool defer_if_no_browser,
    base::OnceCallback<SurveyStringData()> data_factory) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (!profile || !IsSurveyEnabledForHatsTrigger(trigger)) {
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    // An active browser is needed to launch the survey.
    if (defer_if_no_browser) {
      // If no browser is active, defer the survey launch until a browser
      // becomes available.
      // TODO(crbug.com/427971911): Fix test crashes due to the dangling
      // pointer.
      new profiles::BrowserAddedForProfileObserver(
          profile, base::BindOnce(&LaunchHatsSurveyForBrowser, trigger));
    }
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    // HaTS service is not available for OTR profiles.
    return;
  }

  hats_service->LaunchDelayedSurvey(
      trigger,
      switches::kChromeIdentitySurveyLaunchWithDelayDuration.Get()
          .InMilliseconds(),
      /*product_specific_bits_data=*/{}, std::move(data_factory).Run());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

}  // namespace

namespace signin {

void LaunchHatsSurveyForProfile(const std::string& trigger,
                                Profile* profile,
                                bool defer_if_no_browser,
                                std::optional<signin_metrics::AccessPoint>
                                    access_point_for_data_type_promo) {
  LaunchHatsSurveyForProfileInternal(
      trigger, profile, defer_if_no_browser,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      base::BindOnce(&GetSurveyStringData, trigger, profile,
                     access_point_for_data_type_promo)
#else
      base::BindOnce([]() { return SurveyStringData(); })
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  );
}

void LaunchHatsSurveyForProfile(const std::string& trigger,
                                Profile* profile,
                                bool defer_if_no_browser,
                                SurveyStringData data) {
  LaunchHatsSurveyForProfileInternal(
      trigger, profile, defer_if_no_browser,
      base::BindOnce([](SurveyStringData data) { return data; },
                     std::move(data)));
}

}  // namespace signin
