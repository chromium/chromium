// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <iostream>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::NavigationHandle;

namespace {

// ServerBounceDetectionState gets attached to NavigationHandle (which is a
// SupportsUserData subclass) to store data needed to detect stateful
// server-side redirects.
class ServerBounceDetectionState
    : public content::NavigationHandleUserData<ServerBounceDetectionState> {
 public:
  // The WebContents' previously committed URL at the time the navigation
  // started. Needed in case a parallel navigation commits.
  GURL initial_url;
  CookieAccessFilter filter;

 private:
  explicit ServerBounceDetectionState(
      content::NavigationHandle& navigation_handle) {}

  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// The amount of time since finishing navigation to a page that a client-side
// redirect must happen within to count as a stateful bounce (provided that all
// other criteria are met as well).
const int kBounceThresholdSeconds = 10;
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ServerBounceDetectionState);

// The TickClock that a new DIPSBounceDetector will use internally. Exposed as a
// global so that browser tests (which don't call the DIPSBounceDetector
// constructor directly) can inject a fake clock.
base::TickClock* g_clock = nullptr;

std::string GetSite(const GURL& url) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.host() : domain;
}

RedirectCategory ClassifyRedirect(CookieAccessType access, double engagement) {
  switch (access) {
    case CookieAccessType::kNone:
      return engagement > 0 ? RedirectCategory::kNoCookies_HasEngagement
                            : RedirectCategory::kNoCookies_NoEngagement;
    case CookieAccessType::kRead:
      return engagement > 0 ? RedirectCategory::kReadCookies_HasEngagement
                            : RedirectCategory::kReadCookies_NoEngagement;
    case CookieAccessType::kWrite:
      return engagement > 0 ? RedirectCategory::kWriteCookies_HasEngagement
                            : RedirectCategory::kWriteCookies_NoEngagement;
    case CookieAccessType::kReadWrite:
      return engagement > 0 ? RedirectCategory::kReadWriteCookies_HasEngagement
                            : RedirectCategory::kReadWriteCookies_NoEngagement;
  }
}

inline void UmaHistogramBounceCategory(RedirectCategory category,
                                       DIPSCookieMode mode,
                                       DIPSRedirectType type) {
  const std::string histogram_name =
      base::StrCat({"Privacy.DIPS.BounceCategory", GetHistogramPiece(type),
                    GetHistogramSuffix(mode)});
  base::UmaHistogramEnumeration(histogram_name, category);
}

inline void UmaHistogramCookieAccessFilterResult(bool result,
                                                 DIPSCookieMode mode) {
  const std::string histogram_name = base::StrCat(
      {"Privacy.DIPS.CookieAccessFilterResult", GetHistogramSuffix(mode)});
  base::UmaHistogramBoolean(histogram_name, result);
}

inline void UmaHistogramTimeToBounce(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.TimeFromNavigationCommitToClientBounce",
                          sample);
}

}  // namespace

DIPSBounceDetector::DIPSBounceDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSBounceDetector>(*web_contents),
      dips_service_(DIPSService::Get(web_contents->GetBrowserContext())),
      site_engagement_service_(site_engagement::SiteEngagementService::Get(
          web_contents->GetBrowserContext())),
      // It's safe to use unretained because these callbacks are owned by this.
      stateful_client_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulClientRedirect,
                              base::Unretained(this))),
      stateful_server_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulServerRedirect,
                              base::Unretained(this))),
      stateful_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulRedirect,
                              base::Unretained(this))),
      clock_(g_clock ? g_clock : base::DefaultTickClock::GetInstance()) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

/*static*/
base::TickClock* DIPSBounceDetector::SetTickClockForTesting(
    base::TickClock* clock) {
  return std::exchange(g_clock, clock);
}

DIPSCookieMode DIPSBounceDetector::GetCookieMode() const {
  return GetDIPSCookieMode(
      web_contents()->GetBrowserContext()->IsOffTheRecord(),
      dips_service_->ShouldBlockThirdPartyCookies());
}

void DIPSBounceDetector::HandleStatefulRedirect(const GURL& prev_url,
                                                const GURL& url,
                                                const GURL& next_url,
                                                CookieAccessType access,
                                                DIPSRedirectType type) {
  const std::string site = GetSite(url);
  // TODO(rtarpine): all the calls to HandleStatefulRedirect() for a redirect
  // chain call GetSite() on the same prev_url and next_url. Be more efficient.
  if (site == GetSite(prev_url) || site == GetSite(next_url)) {
    // Ignore same-site redirects.
    return;
  }

  double score = site_engagement_service_->GetScore(url);
  RedirectCategory category = ClassifyRedirect(access, score);
  UmaHistogramBounceCategory(category, GetCookieMode(), type);
}

void DIPSBounceDetector::HandleStatefulClientRedirect(
    const GURL& prev_url,
    const GURL& url,
    const GURL& next_url,
    base::TimeDelta bounce_time,
    CookieAccessType access) {
  // Time between page load and client-side redirect starting is only tracked
  // for stateful bounces.
  if (access != CookieAccessType::kNone)
    UmaHistogramTimeToBounce(bounce_time);
  stateful_redirect_handler_.Run(prev_url, url, next_url, access,
                                 DIPSRedirectType::kClient);
}

void DIPSBounceDetector::HandleStatefulServerRedirect(
    const GURL& prev_url,
    content::NavigationHandle* navigation_handle,
    int redirect_index,
    CookieAccessType access) {
  const auto& redirect_chain = navigation_handle->GetRedirectChain();
  const GURL& url = redirect_chain[redirect_index];
  // We are called from DidFinishNavigation() so GetURL() returns the final URL.
  // XXX For 204 No Content responses, should we actually use `prev_url`, since
  // it's what the user actually sees?
  const GURL& next_url = navigation_handle->GetURL();
  stateful_redirect_handler_.Run(prev_url, url, next_url, access,
                                 DIPSRedirectType::kServer);
}

void DIPSBounceDetector::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  base::TimeTicks now = clock_->NowTicks();
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (client_detection_state_.has_value()) {
    base::TimeDelta bounce_time = now - client_detection_state_->page_load_time;

    if (!navigation_handle->HasUserGesture() &&
        navigation_handle->IsRendererInitiated() &&
        (bounce_time <
         base::TimeDelta(base::Seconds(kBounceThresholdSeconds)))) {
      stateful_client_redirect_handler_.Run(
          client_detection_state_->previous_url,
          web_contents()->GetLastCommittedURL(), navigation_handle->GetURL(),
          bounce_time, client_detection_state_->cookie_access_type);
    }
  }

  auto* server_state =
      ServerBounceDetectionState::GetOrCreateForNavigationHandle(
          *navigation_handle);
  server_state->initial_url = web_contents()->GetLastCommittedURL();
}

void DIPSBounceDetector::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (client_detection_state_ &&
      GetSite(details.url) == client_detection_state_->current_site) {
    client_detection_state_->cookie_access_type =
        client_detection_state_->cookie_access_type |
        (details.type == Type::kChange ? CookieAccessType::kWrite
                                       : CookieAccessType::kRead);
  }
}

void DIPSBounceDetector::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  auto* state =
      ServerBounceDetectionState::GetForNavigationHandle(*navigation_handle);
  if (state) {
    state->filter.AddAccess(details.url, details.type);
  }
}

void DIPSBounceDetector::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  base::TimeTicks now = clock_->NowTicks();
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Iff the primary page changed, reset the client detection state while
  // storing the page load time and previous_url. A primary page change is
  // verified by checking IsInPrimaryMainFrame, !IsSameDocument, and
  // HasCommitted. HasCommitted is the only one not previously checked here.
  if (navigation_handle->HasCommitted()) {
    client_detection_state_ =
        ClientBounceDetectionState(navigation_handle->GetPreviousMainFrameURL(),
                                   GetSite(navigation_handle->GetURL()), now);
  }

  auto* server_state =
      ServerBounceDetectionState::GetForNavigationHandle(*navigation_handle);

  if (server_state) {
    std::vector<CookieAccessType> access_types;
    bool filter_success = server_state->filter.Filter(
        navigation_handle->GetRedirectChain(), &access_types);
    UmaHistogramCookieAccessFilterResult(filter_success, GetCookieMode());
    if (filter_success) {
      // Only collect metrics on server redirects if
      // CookieAccessFilter::Filter() succeeded, because otherwise the results
      // might be incorrect.
      for (size_t i = 0; i < access_types.size() - 1; i++) {
        stateful_server_redirect_handler_.Run(
            server_state->initial_url, navigation_handle, i, access_types[i]);
      }
    }

    if (navigation_handle->HasCommitted()) {
      // The last entry in navigation_handle->GetRedirectChain() is actually the
      // page being committed (i.e., not a redirect). If its HTTP request or
      // response accessed cookies, record this in our client detection state.
      //
      // Note that we do this even if !filter_success, because it might still
      // provide information on the committed page -- it annotates every URL it
      // can.
      client_detection_state_->cookie_access_type = access_types.back();
    }
  }
}

void DIPSBounceDetector::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (client_detection_state_.has_value())
    client_detection_state_->received_user_activation = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);
