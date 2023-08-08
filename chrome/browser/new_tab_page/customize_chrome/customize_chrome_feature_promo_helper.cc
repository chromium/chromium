// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/customize_chrome/customize_chrome_feature_promo_helper.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/ui_base_features.h"

void CustomizeChromeFeaturePromoHelper::RecordCustomizeChromeFeatureUsage(
    content::WebContents* web_contents) {
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (tracker) {
    tracker->NotifyEvent(feature_engagement::events::kCustomizeChromeOpened);
  }
}

// For testing purposes only.
void CustomizeChromeFeaturePromoHelper::
    SetDefaultSearchProviderIsGoogleForTesting(bool value) {
  default_search_provider_is_google_ = value;
}

bool CustomizeChromeFeaturePromoHelper::DefaultSearchProviderIsGoogle(
    Profile* profile) {
  if (default_search_provider_is_google_.has_value()) {
    return default_search_provider_is_google_.value();
  }
  return search::DefaultSearchProviderIsGoogle(profile);
}

void CustomizeChromeFeaturePromoHelper::MaybeShowCustomizeChromeFeaturePromo(
    content::WebContents* web_contents) {
  const base::Feature& customize_chrome_feature =
      features::IsChromeRefresh2023()
          ? feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature
          : feature_engagement::kIPHDesktopCustomizeChromeFeature;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !DefaultSearchProviderIsGoogle(browser->profile())) {
    return;
  }
  if (auto* const browser_window = browser->window()) {
    browser_window->MaybeShowFeaturePromo(customize_chrome_feature);
  }
}

void CustomizeChromeFeaturePromoHelper::CloseCustomizeChromeFeaturePromo(
    content::WebContents* web_contents) {
  const base::Feature& customize_chrome_feature =
      features::IsChromeRefresh2023()
          ? feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature
          : feature_engagement::kIPHDesktopCustomizeChromeFeature;
  if (auto* const browser_window =
          BrowserWindow::FindBrowserWindowWithWebContents(web_contents)) {
    browser_window->CloseFeaturePromo(customize_chrome_feature);
  }
}

bool CustomizeChromeFeaturePromoHelper::IsSigninModalDialogOpen(
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser->signin_view_controller()->ShowsModalDialog();
}
