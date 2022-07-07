// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <vector>

#include "base/feature_list.h"
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
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-shared.h"

using blink::mojom::EngagementLevel;
using content::NavigationHandle;

namespace {

// Controls whether UKM metrics are collected for DIPS.
const base::Feature kDipsUkm CONSTINIT{"DipsUkm",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

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
// redirect must happen within to count as a bounce (provided that all other
// criteria are met as well).
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

RedirectCategory ClassifyRedirect(CookieAccessType access,
                                  EngagementLevel engagement) {
  switch (access) {
    case CookieAccessType::kNone:
      return engagement > EngagementLevel::NONE
                 ? RedirectCategory::kNoCookies_HasEngagement
                 : RedirectCategory::kNoCookies_NoEngagement;
    case CookieAccessType::kRead:
      return engagement > EngagementLevel::NONE
                 ? RedirectCategory::kReadCookies_HasEngagement
                 : RedirectCategory::kReadCookies_NoEngagement;
    case CookieAccessType::kWrite:
      return engagement > EngagementLevel::NONE
                 ? RedirectCategory::kWriteCookies_HasEngagement
                 : RedirectCategory::kWriteCookies_NoEngagement;
    case CookieAccessType::kReadWrite:
      return engagement > EngagementLevel::NONE
                 ? RedirectCategory::kReadWriteCookies_HasEngagement
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
      // It's safe to use unretained because the callback is owned by this.
      redirect_handler_(base::BindRepeating(&DIPSBounceDetector::HandleRedirect,
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

ukm::SourceId DIPSBounceDetector::GetRedirectSourceId(
    NavigationHandle* navigation_handle,
    int index) {
  return ukm::UkmRecorder::GetSourceIdForRedirectUrl(
      base::PassKey<DIPSBounceDetector>(),
      navigation_handle->GetRedirectChain()[index]);
}

DIPSRedirectChainInfo::DIPSRedirectChainInfo(const GURL& initial_url,
                                             const GURL& final_url,
                                             int length)
    : initial_url(initial_url),
      initial_site(GetSite(initial_url)),
      final_url(final_url),
      final_site(GetSite(final_url)),
      initial_and_final_sites_same(initial_site == final_site),
      length(length) {}

DIPSRedirectChainInfo::~DIPSRedirectChainInfo() = default;

DIPSRedirectInfo::DIPSRedirectInfo(const GURL& url,
                                   DIPSRedirectType redirect_type,
                                   CookieAccessType access_type,
                                   int index,
                                   ukm::SourceId source_id)
    : DIPSRedirectInfo(url,
                       redirect_type,
                       access_type,
                       index,
                       source_id,
                       /*client_bounce_delay=*/base::TimeDelta(),
                       /*has_sticky_activation=*/false) {
  // This constructor should only be called for server-side redirects;
  // client-side redirects should call the constructor with extra arguments.
  DCHECK_EQ(redirect_type, DIPSRedirectType::kServer);
}

DIPSRedirectInfo::DIPSRedirectInfo(const GURL& url,
                                   DIPSRedirectType redirect_type,
                                   CookieAccessType access_type,
                                   int index,
                                   ukm::SourceId source_id,
                                   base::TimeDelta client_bounce_delay,
                                   bool has_sticky_activation)
    : url(url),
      redirect_type(redirect_type),
      access_type(access_type),
      index(index),
      source_id(source_id),
      client_bounce_delay(client_bounce_delay),
      has_sticky_activation(has_sticky_activation) {}

DIPSRedirectInfo::~DIPSRedirectInfo() = default;

void DIPSBounceDetector::HandleRedirect(const DIPSRedirectInfo& redirect,
                                        const DIPSRedirectChainInfo& chain) {
  const std::string site = GetSite(redirect.url);
  EngagementLevel level =
      site_engagement_service_->GetEngagementLevel(redirect.url);
  bool initial_site_same = (site == chain.initial_site);
  bool final_site_same = (site == chain.final_site);
  DCHECK_LE(0, redirect.index);
  DCHECK_LT(redirect.index, chain.length);

  if (base::FeatureList::IsEnabled(kDipsUkm)) {
    ukm::builders::DIPS_Redirect(redirect.source_id)
        .SetSiteEngagementLevel(static_cast<int64_t>(level))
        .SetRedirectType(static_cast<int64_t>(redirect.redirect_type))
        .SetCookieAccessType(static_cast<int64_t>(redirect.access_type))
        .SetRedirectAndInitialSiteSame(initial_site_same)
        .SetRedirectAndFinalSiteSame(final_site_same)
        .SetInitialAndFinalSitesSame(chain.initial_and_final_sites_same)
        .SetRedirectChainIndex(redirect.index)
        .SetRedirectChainLength(chain.length)
        .SetClientBounceDelay(
            BucketizeBounceDelay(redirect.client_bounce_delay))
        .SetHasStickyActivation(redirect.has_sticky_activation)
        .Record(ukm::UkmRecorder::Get());
  }

  if (initial_site_same || final_site_same) {
    // Don't record UMA metrics for same-site redirects.
    return;
  }

  RedirectCategory category = ClassifyRedirect(redirect.access_type, level);
  UmaHistogramBounceCategory(category, GetCookieMode(), redirect.redirect_type);
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
      // Time between page load and client-side redirect starting is only
      // tracked for stateful bounces.
      if (client_detection_state_->cookie_access_type !=
          CookieAccessType::kNone)
        UmaHistogramTimeToBounce(bounce_time);

      DIPSRedirectChainInfo chain(
          /*initial_url=*/client_detection_state_->previous_url,
          /*final_url=*/navigation_handle->GetURL(),
          /*length=*/1);
      DIPSRedirectInfo redirect(
          /*url=*/web_contents()->GetLastCommittedURL(),
          /*redirect_type=*/DIPSRedirectType::kClient,
          /*access_type=*/client_detection_state_->cookie_access_type,
          /*index=*/0,
          /*source_id=*/
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
          /*client_bounce_delay=*/bounce_time,
          /*has_sticky_activation=*/
          client_detection_state_->received_user_activation);
      redirect_handler_.Run(redirect, chain);
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
        (details.type == content::CookieAccessDetails::Type::kChange
             ? CookieAccessType::kWrite
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
    client_detection_state_ = ClientBounceDetectionState(
        navigation_handle->GetPreviousPrimaryMainFrameURL(),
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
      DIPSRedirectChainInfo chain(/*initial_url=*/server_state->initial_url,
                                  /*final_url=*/navigation_handle->GetURL(),
                                  /*length=*/access_types.size() - 1);

      for (size_t i = 0; i < access_types.size() - 1; i++) {
        DIPSRedirectInfo redirect(
            /*url=*/navigation_handle->GetRedirectChain()[i],
            /*redirect_type=*/DIPSRedirectType::kServer,
            /*access_type=*/access_types[i],
            /*index=*/i,
            /*source_id=*/GetRedirectSourceId(navigation_handle, i));
        redirect_handler_.Run(redirect, chain);
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
