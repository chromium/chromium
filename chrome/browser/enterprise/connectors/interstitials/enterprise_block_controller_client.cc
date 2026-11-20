// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

namespace {
std::unique_ptr<security_interstitials::MetricsHelper> GetMetricsHelper(
    content::WebContents* web_contents,
    const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "enterprise_block";

  // Passing the `HistoryService` lets `MetricsHelper` look up prior visits to
  // the host so that `interstitial.enterprise_block.decision.repeat_visit` is
  // recorded.
  return std::make_unique<security_interstitials::MetricsHelper>(
      url, settings,
      HistoryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS));
}
}  // namespace

EnterpriseBlockControllerClient::EnterpriseBlockControllerClient(
    content::WebContents* web_contents,
    const GURL& request_url)
    : SecurityInterstitialControllerClient(
          web_contents,
          GetMetricsHelper(web_contents, request_url),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          chrome::ChromeUINewTabURLAsGURL(),
          /*settings_page_helper=*/nullptr),
      request_url_(request_url) {}

EnterpriseBlockControllerClient::~EnterpriseBlockControllerClient() = default;

void EnterpriseBlockControllerClient::GoBack() {
  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
}
