// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
extensions::SafeBrowsingPrivateEventRouter* GetEventRouter(
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

#if !BUILDFLAG(IS_ANDROID)
void MaybeTriggerUrlFilteringInterstitialEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& threat_type,
    safe_browsing::RTLookupResponse rt_lookup_response) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetEventRouter(web_contents);
  if (!event_router) {
    return;
  }
  event_router->OnUrlFilteringInterstitial(page_url, threat_type,
                                           rt_lookup_response);
#endif
}
#endif
