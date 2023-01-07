// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_

#include "components/safe_browsing/content/browser/safe_browsing_controller_client.h"

namespace content {
class WebContents;
}

namespace security_interstitials {
class SettingsPageHelper;
}

class PrefService;

// Provides embedder-specific logic for the Safe Browsing interstitial page
// controller.
class ChromeControllerClient
    : public safe_browsing::SafeBrowsingControllerClient {
 public:
  ChromeControllerClient(
      content::WebContents* web_contents,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      PrefService* prefs,
      const std::string& app_locale,
      const GURL& default_safe_page,
      std::unique_ptr<security_interstitials::SettingsPageHelper>
          settings_page_helper);

  ChromeControllerClient(const ChromeControllerClient&) = delete;
  ChromeControllerClient& operator=(const ChromeControllerClient&) = delete;

  ~ChromeControllerClient() override;

  void Proceed() override;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_
