// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"

namespace {

// Returns a scoped refptr to the SafeBrowsingService's database manager, if
// available. Otherwise returns nullptr.
const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
GetDatabaseManagerFromSafeBrowsingService() {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  return safe_browsing_service ? safe_browsing_service->database_manager()
                               : nullptr;
}

}  // namespace

void CreateSubresourceFilterWebContentsHelper(
    content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterWebContentsHelper::
      CreateForWebContents(
          web_contents,
          SubresourceFilterProfileContextFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())),
          GetDatabaseManagerFromSafeBrowsingService(), dealer);
}
