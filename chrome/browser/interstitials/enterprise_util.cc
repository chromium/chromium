// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/connectors/core/features.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
extensions::SafeBrowsingPrivateEventRouter* GetSafeBrowsingEventRouter(
    content::WebContents* web_contents) {
  // |web_contents| can be null in tests.
  if (!web_contents)
    return nullptr;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // In guest profile, IsOffTheRecord also returns true. So we need an
  // additional check on IsGuestSession to ensure the event is sent in guest
  // mode.
  if (profile->IsOffTheRecord() && !profile->IsGuestSession()) {
    return nullptr;
  }

  return extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
      browser_context);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
enterprise_connectors::ReportingEventRouter* GetReportingEventRouter(
    content::WebContents* web_contents) {
  // |web_contents| can be null in tests.
  if (!web_contents) {
    return nullptr;
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // In guest profile, IsOffTheRecord also returns true. So we need an
  // additional check on IsGuestSession to ensure the event is sent in guest
  // mode.
  if (profile->IsOffTheRecord() && !profile->IsGuestSession()) {
    return nullptr;
  }

  return enterprise_connectors::ReportingEventRouterFactory::
      GetForBrowserContext(browser_context);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

}  // namespace

void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetSafeBrowsingEventRouter(web_contents);
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
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetSafeBrowsingEventRouter(web_contents);
  if (!event_router)
    return;
  event_router->OnSecurityInterstitialProceeded(page_url, reason,
                                                net_error_code);
#endif
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void MaybeTriggerUrlFilteringInterstitialEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& threat_type,
    safe_browsing::RTLookupResponse rt_lookup_response) {
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  enterprise_connectors::ReportingEventRouter* router =
      GetReportingEventRouter(web_contents);

  router->OnUrlFilteringInterstitial(page_url, threat_type, rt_lookup_response);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid) ||
      base::FeatureList::IsEnabled(
          enterprise_connectors::
              kEnterpriseUrlFilteringEventReportingOnAndroid)) {
    enterprise_connectors::ReportingEventRouter* router =
        GetReportingEventRouter(web_contents);

    router->OnUrlFilteringInterstitial(page_url, threat_type,
                                       rt_lookup_response);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
