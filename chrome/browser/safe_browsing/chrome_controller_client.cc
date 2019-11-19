// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_controller_client.h"

#include "base/feature_list.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

ChromeControllerClient::ChromeControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page)
    : safe_browsing::SafeBrowsingControllerClient(web_contents,
                                                  std::move(metrics_helper),
                                                  prefs,
                                                  app_locale,
                                                  default_safe_page) {}

ChromeControllerClient::~ChromeControllerClient() {}

void ChromeControllerClient::Proceed() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Hosted Apps should not be allowed to run if Safe Browsing considers them
  // dangeours. So, when users click proceed on an interstitial, move the tab
  // to a regular Chrome window and proceed as usual there.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser))
    chrome::OpenInChrome(browser);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  safe_browsing::SafeBrowsingControllerClient::Proceed();
}
