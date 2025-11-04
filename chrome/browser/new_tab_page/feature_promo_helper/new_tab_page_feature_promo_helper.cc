// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "ui/base/ui_base_features.h"

namespace {

const void* const kCustomizeChromeAutoOpenedUserDataKey =
    &kCustomizeChromeAutoOpenedUserDataKey;

class CustomizeChromeAutoOpenedUserData : public base::SupportsUserData::Data {
 public:
  static CustomizeChromeAutoOpenedUserData* GetOrCreateForProfile(
      Profile* profile) {
    auto* data = static_cast<CustomizeChromeAutoOpenedUserData*>(
        profile->GetUserData(kCustomizeChromeAutoOpenedUserDataKey));
    if (!data) {
      data = new CustomizeChromeAutoOpenedUserData();
      profile->SetUserData(kCustomizeChromeAutoOpenedUserDataKey,
                           base::WrapUnique(data));
    }
    return data;
  }

  int times_opened() const { return times_opened_; }
  void IncrementTimesOpened() { times_opened_ += 1; }

 private:
  CustomizeChromeAutoOpenedUserData() = default;

  int times_opened_ = 0;
};

void ShowCustomizeChromeSidePanel(Profile* profile) {
  CustomizeChromeAutoOpenedUserData::GetOrCreateForProfile(profile)
      ->IncrementTimesOpened();
  profile->GetPrefs()->SetInteger(
      prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount,
      profile->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount) +
          1);

  actions::ActionManager::Get()
      .FindAction(kActionSidePanelShowCustomizeChrome)
      ->InvokeAction(
          actions::ActionInvocationContext::Builder()
              .SetProperty(
                  kSidePanelOpenTriggerKey,
                  static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                      SidePanelOpenTrigger::
                          kNewTabPageAutomaticCustomizeChrome))
              .Build());
}

void OnShowIPHResult(Profile* profile,
                     user_education::FeaturePromoResult result) {
  // If pref was not set before, this is first opening, so panel should be
  // shown only after the IPH.
  if (result == user_education::FeaturePromoResult::Success() &&
      !profile->GetPrefs()->GetBoolean(
          prefs::kNtpCustomizeChromeIPHAutoOpened)) {
    profile->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeIPHAutoOpened,
                                    true);
    ShowCustomizeChromeSidePanel(profile);
  }
}

NTPCustomizeChromePromoEligibility CanShowCustomizeChromePromo(
    Profile* profile) {
  auto* background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(profile);
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  if (background_service->GetCustomBackground() ||
      theme_service->GetThemeID() != ThemeHelper::kDefaultThemeID) {
    return NTPCustomizeChromePromoEligibility::kChromeCustomizedAlready;
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kNtpCustomizeChromeExplicitlyClosed)) {
    return NTPCustomizeChromePromoEligibility::
        kCustomizeChromeClosedExplicitlyByUser;
  }

  if (profile->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeButtonOpenCount) > 0) {
    return NTPCustomizeChromePromoEligibility::kCustomizeChromeOpenedByUser;
  }

  // If no max auto open count is set, then we are showing a different variation
  // of the promo (not involving auto opening of the Side Panel), for which the
  // user is considered eligible at this point.
  if (ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get() == 0) {
    return NTPCustomizeChromePromoEligibility::kCanShowPromo;
  }

  if (ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get() <= 0 ||
      ntp_features::kNtpCustomizeChromeAutoShownSessionMaxCount.Get() <= 0) {
    return NTPCustomizeChromePromoEligibility::kFeatureConfigMismatch;
  }

  if (profile->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount) >=
      ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get()) {
    return NTPCustomizeChromePromoEligibility::kReachedTotalMaxCountAlready;
  }

  if (CustomizeChromeAutoOpenedUserData::GetOrCreateForProfile(profile)
          ->times_opened() >=
      ntp_features::kNtpCustomizeChromeAutoShownSessionMaxCount.Get()) {
    return NTPCustomizeChromePromoEligibility::kReachedSessionMaxCountAlready;
  }

  return NTPCustomizeChromePromoEligibility::kCanShowPromo;
}

}  // namespace

void NewTabPageFeaturePromoHelper::RecordPromoFeatureUsageAndClosePromo(
    const base::Feature& feature,
    content::WebContents* web_contents) {
  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents)) {
    interface->NotifyFeaturePromoFeatureUsed(
        feature, FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

// For testing purposes only.
void NewTabPageFeaturePromoHelper::
    SetDefaultSearchProviderIsGoogleForTesting(bool value) {
  default_search_provider_is_google_ = value;
}

bool NewTabPageFeaturePromoHelper::DefaultSearchProviderIsGoogle(
    Profile* profile) {
  if (default_search_provider_is_google_.has_value()) {
    return default_search_provider_is_google_.value();
  }
  return search::DefaultSearchProviderIsGoogle(profile);
}

void NewTabPageFeaturePromoHelper::MaybeShowFeaturePromo(
    user_education::FeaturePromoParams params,
    content::WebContents* web_contents) {
  if (!DefaultSearchProviderIsGoogle(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    return;
  }
  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents)) {
    interface->MaybeShowFeaturePromo(std::move(params));
  }
}

bool NewTabPageFeaturePromoHelper::IsSigninModalDialogOpen(
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithTab(web_contents);
  // `browser` might be NULL if the new tab is immediately dragged out of the
  // window.
  return browser ? browser->GetFeatures()
                       .signin_view_controller()
                       ->ShowsModalDialog()
                 : false;
}

void NewTabPageFeaturePromoHelper::MaybeTriggerAutomaticCustomizeChromePromo(
    content::WebContents* web_contents) {
  auto* browser_interface = webui::GetBrowserWindowInterface(web_contents);
  if (!browser_interface ||
      browser_interface->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))) {
    return;
  }

  auto* interface = BrowserUserEducationInterface::From(browser_interface);
  if (!interface) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto promo_eligibility = CanShowCustomizeChromePromo(profile);
  base::UmaHistogramEnumeration("NewTabPage.CustomizeChromePromoEligibility",
                                promo_eligibility);
  if (promo_eligibility != NTPCustomizeChromePromoEligibility::kCanShowPromo ||
      !base::FeatureList::IsEnabled(
          ntp_features::kNtpCustomizeChromeAutoOpen)) {
    return;
  }

  // Variation where we do not open the Side Panel automatically; instead we
  // show a tutorial.
  if (ntp_features::kNtpCustomizeChromeAutoShownMaxCount.Get() == 0) {
    interface->MaybeShowFeaturePromo(
        feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature);
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature);
  if (!profile->GetPrefs()->GetBoolean(
          prefs::kNtpCustomizeChromeIPHAutoOpened)) {
    params.show_promo_result_callback =
        base::BindOnce(&OnShowIPHResult, profile);

    interface->MaybeShowFeaturePromo(std::move(params));
    return;
  }

  interface->MaybeShowFeaturePromo(std::move(params));
  ShowCustomizeChromeSidePanel(profile);
}
