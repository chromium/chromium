// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "components/safe_browsing/safe_browsing_controller_client.h"

namespace content {
class WebContents;
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
      const GURL& default_safe_page);
  ~ChromeControllerClient() override;

  void Proceed() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeControllerClient);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CONTROLLER_CLIENT_H_
