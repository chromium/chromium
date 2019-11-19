// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
extensions::SafeBrowsingPrivateEventRouter* GetEventRouter(
    content::WebContents* web_contents) {
  // |web_contents| can be null in tests.
  if (!web_contents)
    return nullptr;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (browser_context->IsOffTheRecord())
    return nullptr;

  return extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
      browser_context);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetEventRouter(web_contents);
  if (!event_router)
    return;
  event_router->OnSecurityInterstitialShown(page_url, reason, net_error_code);
#endif
}

void MaybeTriggerSecurityInterstitialProceededEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetEventRouter(web_contents);
  if (!event_router)
    return;
  event_router->OnSecurityInterstitialProceeded(page_url, reason,
                                                net_error_code);
#endif
}

std::string GetThreatTypeStringForInterstitial(
    safe_browsing::SBThreatType threat_type) {
  switch (threat_type) {
    case safe_browsing::SB_THREAT_TYPE_URL_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return "PHISHING";
    case safe_browsing::SB_THREAT_TYPE_URL_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return "MALWARE";
    case safe_browsing::SB_THREAT_TYPE_URL_UNWANTED:
    case safe_browsing::SB_THREAT_TYPE_BILLING:
      return "HARMFUL";
    case safe_browsing::SB_THREAT_TYPE_UNUSED:
    case safe_browsing::SB_THREAT_TYPE_SAFE:
    case safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_EXTENSION:
    case safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE:
    case safe_browsing::SB_THREAT_TYPE_API_ABUSE:
    case safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case safe_browsing::SB_THREAT_TYPE_CSD_WHITELIST:
    case safe_browsing::
        DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_APK_DOWNLOAD:
    case safe_browsing::SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      NOTREACHED();
      break;
  }
  return std::string();
}
