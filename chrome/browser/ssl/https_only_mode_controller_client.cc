// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_controller_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<security_interstitials::MetricsHelper>
HttpsOnlyModeControllerClient::GetMetricsHelper(const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "https_first_mode";
  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}

HttpsOnlyModeControllerClient::HttpsOnlyModeControllerClient(
    content::WebContents* web_contents,
    const GURL& request_url)
    : SecurityInterstitialControllerClient(
          web_contents,
          GetMetricsHelper(request_url),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL),
          /*settings_page_helper=*/nullptr),
      web_contents_(web_contents),
      request_url_(request_url) {}

HttpsOnlyModeControllerClient::~HttpsOnlyModeControllerClient() = default;

void HttpsOnlyModeControllerClient::GoBack() {
  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
}

void HttpsOnlyModeControllerClient::Proceed() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  // StatefulSSLHostStateDelegate can be null during tests.
  if (state) {
    // Notifies the browser process when a HTTP exception is allowed in
    // HTTPS-First Mode.
    web_contents_->SetAlwaysSendSubresourceNotifications();

    state->AllowHttpForHost(
        request_url_.host(),
        web_contents_->GetPrimaryMainFrame()->GetStoragePartition());
  }
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents_);
  tab_helper->set_is_navigation_upgraded(false);
  tab_helper->set_is_navigation_fallback(false);
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
  // The failed https navigation will remain as a forward entry, so it needs to
  // be removed.
  web_contents_->GetController().PruneForwardEntries();
}
