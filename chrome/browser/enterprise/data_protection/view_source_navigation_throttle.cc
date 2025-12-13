// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/view_source_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace enterprise_data_protection {

namespace {

GURL GetURLWithViewSourcePrefix(content::NavigationHandle* handle) {
  return GURL(content::kViewSourceScheme + std::string(":") +
              handle->GetURL().spec());
}

bool IsEnterpriseLookupEnabled(Profile* profile) {
  // Some tests return a non-null pointer for the enterprise lookup service,
  // so we need to defensively check if enterprise lookup is enabled.
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  bool has_valid_dm_token =
      connectors_service &&
      connectors_service->GetDMTokenForRealTimeUrlCheck().has_value();
  return safe_browsing::RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
      profile->GetPrefs(), has_valid_dm_token, profile->IsOffTheRecord(),
      profile->IsGuestSession());
}

// Helper function to create an UnsafeResource from an RTLookupResponse.
// Returns true if a blockable threat was found and the resource was populated.
// Assumes threat_info in rt_lookup_response is sorted by decreasing severity.
bool CreateUnsafeResourceFromRTLookupResponse(
    content::NavigationHandle* handle,
    const safe_browsing::RTLookupResponse& rt_lookup_response,
    security_interstitials::UnsafeResource& out_resource) {
  if (rt_lookup_response.threat_info_size() == 0) {
    return false;  // No threat info.
  }

  // Since threats are sorted by severity, we can take the first one
  // that translates to a blockable SBThreatType.
  const auto& first_threat_info = rt_lookup_response.threat_info(0);
  safe_browsing::SBThreatType threat_type = safe_browsing::
      RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
          first_threat_info.threat_type(), first_threat_info.verdict_type());

  // Check if the most severe threat is one we should block on.
  // For this throttle, we primarily care about enterprise policies.
  if (threat_type ==
          safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK ||
      threat_type ==
          safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN) {
    out_resource.url = GetURLWithViewSourcePrefix(handle);
    out_resource.threat_type = threat_type;
    out_resource.threat_source =
        safe_browsing::ThreatSource::URL_REAL_TIME_CHECK;
    out_resource.rt_lookup_response = rt_lookup_response;
    out_resource.navigation_id = handle->GetNavigationId();
    out_resource.rfh_locator.frame_tree_node_id =
        handle->GetFrameTreeNodeId().value();

    return true;
  }

  return false;  // The most severe threat is not one we are blocking for.
}

}  // namespace

// static
void ViewSourceNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry,
    safe_browsing::SafeBrowsingUIManager* ui_manager) {
  if (!ui_manager) {
    return;
  }

  // Only outer-most main frames show the interstitial through the navigation
  // throttle. In other cases, the interstitial is shown via
  // BaseUIManager::DisplayBlockingPage.
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() && !handle.IsInPrerenderedMainFrame()) {
    return;
  }

  if (!handle.GetNavigationEntry() ||
      !handle.GetNavigationEntry()->IsViewSourceMode()) {
    return;
  }

  registry.AddThrottle(
      base::WrapUnique(new ViewSourceNavigationThrottle(registry, ui_manager)));
}

ViewSourceNavigationThrottle::ViewSourceNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    safe_browsing::SafeBrowsingUIManager* manager)
    : content::NavigationThrottle(registry), manager_(manager) {
  content::NavigationHandle* handle = navigation_handle();
  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (IsEnterpriseLookupEnabled(profile)) {
    url_lookup_service_ =
        safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
            GetForProfile(profile)
                ->GetWeakPtr();
  }
}

inline ViewSourceNavigationThrottle::~ViewSourceNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ViewSourceNavigationThrottle::WillStartRequest() {
  return FireRealtimeLookup();
}

content::NavigationThrottle::ThrottleCheckResult
ViewSourceNavigationThrottle::WillRedirectRequest() {
  return FireRealtimeLookup();
}

content::NavigationThrottle::ThrottleCheckResult
ViewSourceNavigationThrottle::WillProcessResponse() {
  DCHECK(manager_);

  content::NavigationHandle* handle = navigation_handle();

  security_interstitials::UnsafeResource resource;
  auto url = GetURLWithViewSourcePrefix(handle);

  safe_browsing::ThreatSeverity severity =
      manager_->GetSeverestThreatForRedirectChain(
          {url}, handle->GetNavigationId(), resource);

  // Unsafe resource will show a blocking page
  if (severity != std::numeric_limits<safe_browsing::ThreatSeverity>::max() &&
      resource.threat_type !=
          safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE) {
    // Subframes and nested frame trees will show an interstitial directly
    // from BaseUIManager::DisplayBlockingPage.
    DCHECK(handle->IsInPrimaryMainFrame() ||
           handle->IsInPrerenderedMainFrame());

    security_interstitials::SecurityInterstitialPage* blocking_page =
        manager_->CreateBlockingPage(
            handle->GetWebContents(), url, {resource},
            /*forward_extension_event=*/true,
            /*blocked_page_shown_timestamp=*/std::nullopt);
    std::string error_page_content = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, base::WrapUnique(blocking_page));

    return content::NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
  }

  return content::NavigationThrottle::PROCEED;
}

const char* ViewSourceNavigationThrottle::GetNameForLogging() {
  return "ViewSourceNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
ViewSourceNavigationThrottle::FireRealtimeLookup() {
  if (!url_lookup_service_) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationHandle* handle = navigation_handle();
  auto url = GetURLWithViewSourcePrefix(handle);
  content::WebContents* web_contents = handle->GetWebContents();

  url_lookup_service_->StartMaybeCachedLookup(
      url,
      base::BindOnce(&ViewSourceNavigationThrottle::OnRealTimeLookupComplete,
                     weak_factory_.GetWeakPtr(), handle),
      base::SequencedTaskRunner::GetCurrentDefault(),
      sessions::SessionTabHelper::IdForTab(web_contents),
      /*referring_app_info=*/std::nullopt, /*use_cache=*/false);

  return content::NavigationThrottle::DEFER;
}

void ViewSourceNavigationThrottle::OnRealTimeLookupComplete(
    content::NavigationHandle* handle,
    bool is_success,
    bool is_cached,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  if (!is_success || !rt_lookup_response) {
    Resume();
    return;
  }
  security_interstitials::UnsafeResource resource;
  if (CreateUnsafeResourceFromRTLookupResponse(handle, *rt_lookup_response,
                                               resource)) {
    manager_->DisplayBlockingPage(resource);
  }

  Resume();
}

}  // namespace enterprise_data_protection
