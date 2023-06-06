// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/customize_chrome/customize_chrome_feature_promo_helper.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"

void CustomizeChromeFeaturePromoHelper::RecordCustomizeChromeFeatureUsage(
    content::WebContents* web_contents) {
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (tracker) {
    tracker->NotifyEvent(feature_engagement::events::kCustomizeChromeOpened);
  }
}

void CustomizeChromeFeaturePromoHelper::MaybeShowCustomizeChromeFeaturePromo(
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopCustomizeChromeFeature)) {
    auto* browser_window =
        BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
    if (browser_window) {
      browser_window->MaybeShowFeaturePromo(
          feature_engagement::kIPHDesktopCustomizeChromeFeature);
    }
  }
}

void CustomizeChromeFeaturePromoHelper::CloseCustomizeChromeFeaturePromo(
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopCustomizeChromeFeature)) {
    auto* browser_window =
        BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
    if (browser_window) {
      browser_window->CloseFeaturePromo(
          feature_engagement::kIPHDesktopCustomizeChromeFeature);
    }
  }
}

bool CustomizeChromeFeaturePromoHelper::IsSigninModalDialogOpen(
    content::WebContents* web_contents) {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser->signin_view_controller()->ShowsModalDialog();
}
