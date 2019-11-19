// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

// static
std::unique_ptr<security_interstitials::MetricsHelper>
LookalikeUrlControllerClient::GetMetricsHelper(const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "lookalike";

  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}

LookalikeUrlControllerClient::LookalikeUrlControllerClient(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& safe_url)
    : SecurityInterstitialControllerClient(
          web_contents,
          GetMetricsHelper(request_url),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL)),
      request_url_(request_url),
      safe_url_(safe_url) {}

LookalikeUrlControllerClient::~LookalikeUrlControllerClient() {}

void LookalikeUrlControllerClient::GoBack() {
  // We don't offer 'go back', but rather redirect to the legitimate site.
  content::OpenURLParams params(safe_url_, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);

  // Prevent the back button from returning to the bad site.
  params.should_replace_current_entry = true;
  web_contents_->OpenURL(params);
}

void LookalikeUrlControllerClient::Proceed() {
  LookalikeUrlTabStorage::GetOrCreate(web_contents_)
      ->AllowDomain(request_url_.host());
  Reload();
}
