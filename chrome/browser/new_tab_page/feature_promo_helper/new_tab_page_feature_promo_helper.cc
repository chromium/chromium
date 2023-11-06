// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/ui_base_features.h"

void NewTabPageFeaturePromoHelper::RecordFeatureUsage(
    const std::string& event,
    content::WebContents* web_contents) {
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (tracker) {
    tracker->NotifyEvent(event);
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
    const base::Feature& iph_feature,
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !DefaultSearchProviderIsGoogle(browser->profile())) {
    return;
  }
  if (auto* const browser_window = browser->window()) {
    browser_window->MaybeShowFeaturePromo(iph_feature);
  }
}

void NewTabPageFeaturePromoHelper::CloseFeaturePromo(
    const base::Feature& iph_feature,
    content::WebContents* web_contents) {
  if (auto* const browser_window =
          BrowserWindow::FindBrowserWindowWithWebContents(web_contents)) {
    browser_window->CloseFeaturePromo(iph_feature);
  }
}

bool NewTabPageFeaturePromoHelper::IsSigninModalDialogOpen(
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithTab(web_contents);
  return browser->signin_view_controller()->ShowsModalDialog();
}
