// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

namespace {
const char kHelpCenterLink[] = "cpn_safe_browsing";

}  // namespace

SafeBrowsingBlockingPage*
ChromeSafeBrowsingBlockingPageFactory::CreateSafeBrowsingPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
    bool should_trigger_reporting) {
  // Create appropriate display options for this blocking page.
  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
  bool is_extended_reporting_opt_in_allowed =
      IsExtendedReportingOptInAllowed(*prefs);
  bool is_proceed_anyway_disabled =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);

  // Determine if any prefs need to be updated prior to showing the security
  // interstitial. This must happen before querying IsScout to populate the
  // Display Options below.
  safe_browsing::UpdatePrefsBeforeSecurityInterstitial(prefs);

  security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
      display_options(BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
                      is_extended_reporting_opt_in_allowed,
                      web_contents->GetBrowserContext()->IsOffTheRecord(),
                      IsExtendedReportingEnabled(*prefs),
                      IsExtendedReportingPolicyManaged(*prefs),
                      IsEnhancedProtectionEnabled(*prefs),
                      is_proceed_anyway_disabled,
                      true,  // should_open_links_in_new_tab
                      true,  // always_show_back_to_safety
                      true,  // is_enhanced_protection_message_enabled
                      IsSafeBrowsingPolicyManaged(*prefs), kHelpCenterLink);

  return new SafeBrowsingBlockingPage(ui_manager, web_contents, main_frame_url,
                                      unsafe_resources, display_options,
                                      should_trigger_reporting);
}

ChromeSafeBrowsingBlockingPageFactory::ChromeSafeBrowsingBlockingPageFactory() =
    default;

}  // namespace safe_browsing
