// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"

#if defined(OS_ANDROID)
#include "components/subresource_filter/android/ads_blocked_infobar_delegate.h"
#endif

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() = default;

// static
void ChromeSubresourceFilterClient::
    CreateThrottleManagerWithClientForWebContents(
        content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterThrottleManager::
      CreateForWebContents(
          web_contents,
          std::make_unique<ChromeSubresourceFilterClient>(web_contents),
          SubresourceFilterProfileContextFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())),
          dealer);
}

void ChromeSubresourceFilterClient::ShowNotification() {
#if defined(OS_ANDROID)
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);
    subresource_filter::AdsBlockedInfobarDelegate::Create(infobar_service);
#endif
}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
ChromeSubresourceFilterClient::GetSafeBrowsingDatabaseManager() {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  return safe_browsing_service ? safe_browsing_service->database_manager()
                               : nullptr;
}
