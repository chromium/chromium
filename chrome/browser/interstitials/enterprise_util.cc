// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
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

  return extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
      browser_context);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

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

void SetReferrerChain(
    content::WebContents* web_contents,
    const GURL& page_url,
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>&
        referrer_chain) {
  safe_browsing::SafeBrowsingNavigationObserverManager*
      navigation_observer_manager =
          safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
              GetForBrowserContext(web_contents->GetBrowserContext());
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  safe_browsing::ReferrerChainProvider::AttributionResult attribution_result =
      navigation_observer_manager->IdentifyReferrerChainByPendingEventURL(
          page_url, enterprise_connectors::kReferrerUserGestureLimit,
          &referrer_chain);
  if (attribution_result ==
      safe_browsing::ReferrerChainProvider::NAVIGATION_EVENT_NOT_FOUND) {
    CHECK(referrer_chain.empty());
    navigation_observer_manager->IdentifyReferrerChainByEventURL(
        page_url, tab_id, enterprise_connectors::kReferrerUserGestureLimit,
        &referrer_chain);
  }
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

}  // namespace

void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      GetSafeBrowsingEventRouter(web_contents);
  if (!safe_browsing_event_router) {
    return;
  }

  safe_browsing_event_router->OnSecurityInterstitialShown(page_url, reason,
                                                          net_error_code);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid)) {
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  PrefService* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();
  enterprise_connectors::ReportingEventRouter* reporting_event_router =
      GetReportingEventRouter(web_contents);

  if (!reporting_event_router) {
    return;
  }

  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
      referrer_chain;
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    SetReferrerChain(web_contents, page_url, referrer_chain);
  }

  reporting_event_router->OnSecurityInterstitialShown(
      page_url, reason, net_error_code,
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled),
      referrer_chain);

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
}

void MaybeTriggerSecurityInterstitialProceededEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      GetSafeBrowsingEventRouter(web_contents);
  if (!safe_browsing_event_router) {
    return;
  }
  safe_browsing_event_router->OnSecurityInterstitialProceeded(page_url, reason,
                                                              net_error_code);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid)) {
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  enterprise_connectors::ReportingEventRouter* reporting_event_router =
      GetReportingEventRouter(web_contents);
  if (!reporting_event_router) {
    return;
  }

  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
      referrer_chain;
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    SetReferrerChain(web_contents, page_url, referrer_chain);
  }

  reporting_event_router->OnSecurityInterstitialProceeded(
      page_url, reason, net_error_code, referrer_chain);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void MaybeTriggerUrlFilteringInterstitialEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& threat_type,
    safe_browsing::RTLookupResponse rt_lookup_response) {
  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
      referrer_chain;
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  enterprise_connectors::ReportingEventRouter* router =
      GetReportingEventRouter(web_contents);

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    SetReferrerChain(web_contents, page_url, referrer_chain);
  }

  router->OnUrlFilteringInterstitial(page_url, threat_type, rt_lookup_response,
                                     referrer_chain);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid) ||
      base::FeatureList::IsEnabled(
          enterprise_connectors::
              kEnterpriseUrlFilteringEventReportingOnAndroid)) {
    enterprise_connectors::ReportingEventRouter* router =
        GetReportingEventRouter(web_contents);

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      SetReferrerChain(web_contents, page_url, referrer_chain);
    }

    router->OnUrlFilteringInterstitial(page_url, threat_type,
                                       rt_lookup_response, referrer_chain);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
