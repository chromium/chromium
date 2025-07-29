// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_hats_util.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/signin/public/base/signin_switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
struct ChromeIdentityHatsTriggerFeatureMappingEntry {
  const char* trigger;
  const raw_ptr<const base::Feature> feature;
};

const ChromeIdentityHatsTriggerFeatureMappingEntry
    kChromeIdentityHatsTriggerFeatureMapping[] = {
        {kHatsSurveyTriggerIdentityAddressBubbleSignin,
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
};

// Launches a HaTS survey for the profile associated with `browser`.
void LaunchSigninHatsSurveyForBrowser(const std::string& trigger,
                                      Browser* browser) {
  if (!browser) {
    return;
  }
  signin::LaunchSigninHatsSurveyForProfile(trigger, browser->GetProfile());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

namespace signin {

bool IsFeatureEnabledForSigninHatsTrigger(const std::string& trigger) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  for (const auto& entry : kChromeIdentityHatsTriggerFeatureMapping) {
    if (trigger == entry.trigger) {
      return base::FeatureList::IsEnabled(*entry.feature);
    }
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // No matching feature for the given trigger, or the current platform does not
  // support signin surveys.
  return false;
}

void LaunchSigninHatsSurveyForProfile(const std::string& trigger,
                                      Profile* profile,
                                      bool defer_if_no_browser) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (!profile || !IsFeatureEnabledForSigninHatsTrigger(trigger)) {
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
          profile, base::BindOnce(&LaunchSigninHatsSurveyForBrowser, trigger));
    }
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    // HaTS service is not available for OTR profiles.
    return;
  }

  // TODO(crbug.com/427971911): add product-specific data.
  hats_service->LaunchDelayedSurvey(
      trigger, switches::kChromeIdentitySurveyLaunchWithDelayDuration.Get()
                   .InMilliseconds());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

}  // namespace signin
