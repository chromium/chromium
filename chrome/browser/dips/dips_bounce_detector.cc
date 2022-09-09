// Copyright 2022 The Chromium Authors
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
  DIPSNavigationStart navigation_start;
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

RedirectCategory ClassifyRedirect(CookieAccessType access,
                                  EngagementLevel engagement) {
  switch (access) {
    case CookieAccessType::kUnknown:
      return engagement > EngagementLevel::NONE
                 ? RedirectCategory::kUnknownCookies_HasEngagement
                 : RedirectCategory::kUnknownCookies_NoEngagement;
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
      clock_(g_clock ? g_clock : base::DefaultTickClock::GetInstance()),
      // It's safe to use unretained because the callback is owned by the
      // DIPSRedirectContext which is owned by this.
      redirect_context_(base::BindRepeating(&DIPSBounceDetector::HandleRedirect,
                                            base::Unretained(this)),
                        /*initial_url=*/GURL::EmptyGURL()) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

void DIPSBounceDetector::SetRedirectHandlerForTesting(
    DIPSRedirectHandler handler) {
  redirect_context_.SetRedirectHandlerForTesting(handler);  // IN-TEST
}

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
      initial_site(GetDIPSSite(initial_url)),
      final_url(final_url),
      final_site(GetDIPSSite(final_url)),
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

DIPSRedirectContext::DIPSRedirectContext(DIPSRedirectHandler handler,
                                         const GURL& initial_url)
    : handler_(handler), initial_url_(initial_url) {}

DIPSRedirectContext::~DIPSRedirectContext() = default;

void DIPSRedirectContext::Append(
    bool committed,
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr>&& server_redirects,
    GURL final_url) {
  if (committed) {
    Append(std::move(navigation_start), std::move(server_redirects));
  } else {
    DIPSRedirectContext temp_context(handler_, initial_url_);
    temp_context.Append(std::move(navigation_start),
                        std::move(server_redirects));
    temp_context.EndChain(std::move(final_url));
  }
}

void DIPSRedirectContext::Append(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr>&& server_redirects) {
  // If there was a client-side redirect, grow the chain. Otherwise, end it.
  if (absl::holds_alternative<DIPSRedirectInfoPtr>(navigation_start)) {
    auto& client_redirect = absl::get<DIPSRedirectInfoPtr>(navigation_start);
    DCHECK_EQ(client_redirect->redirect_type, DIPSRedirectType::kClient);
    redirects_.push_back(std::move(client_redirect));
  } else {
    auto& client_url = absl::get<GURL>(navigation_start);
    // This is the most common reason for redirect chains
    // to terminate. Other reasons include: (1) navigations
    // that don't commit and (2) the user closing the tab
    // (i.e., WCO::WebContentsDestroyed())
    EndChain(std::move(client_url));
  }

  // Server-side redirects always grow the chain.
  for (auto& redirect : server_redirects) {
    DCHECK_EQ(redirect->redirect_type, DIPSRedirectType::kServer);
    redirects_.push_back(std::move(redirect));
  }
}

void DIPSRedirectContext::EndChain(GURL url) {
  if (!redirects_.empty()) {
    // Uncommitted chains may omit earlier (committed) redirects in the chain,
    // so |redirects_.size()| may not tell us the correct chain length. Instead,
    // use the index of the last item in the chain (since it was generated based
    // on the committed chain length).
    DIPSRedirectChainInfo chain(initial_url_, url,
                                redirects_.back()->index + 1);
    for (const auto& redirect : redirects_) {
      handler_.Run(*redirect, chain);
    }
    redirects_.clear();
  }

  initial_url_ = std::move(url);
}

void DIPSBounceDetector::HandleRedirect(const DIPSRedirectInfo& redirect,
                                        const DIPSRedirectChainInfo& chain) {
  const std::string site = GetDIPSSite(redirect.url);
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

  DIPSRedirectInfoPtr client_redirect;
  if (client_detection_state_.has_value()) {
    base::TimeDelta bounce_time = now - client_detection_state_->page_load_time;

    if (!navigation_handle->HasUserGesture() &&
        navigation_handle->IsRendererInitiated() &&
        (bounce_time <
         base::TimeDelta(base::Seconds(kBounceThresholdSeconds)))) {
      // Time between page load and client-side redirect starting is only
      // tracked for stateful bounces.
      if (client_detection_state_->cookie_access_type > CookieAccessType::kNone)
        UmaHistogramTimeToBounce(bounce_time);

      client_redirect = std::make_unique<DIPSRedirectInfo>(
          /*url=*/web_contents()->GetLastCommittedURL(),
          /*redirect_type=*/DIPSRedirectType::kClient,
          /*access_type=*/client_detection_state_->cookie_access_type,
          /*index=*/redirect_context_.size(),
          /*source_id=*/
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
          /*client_bounce_delay=*/bounce_time,
          /*has_sticky_activation=*/
          client_detection_state_->received_user_activation);
      // We cannot append |client_redirect| to |redirect_context_| immediately,
      // because we don't know if the navigation will commit. We must wait until
      // DidFinishNavigation().
    }
    // Similarly, we can't call redirect_context_->EndChain() yet even if this
    // navigation isn't a redirect. (Technically, if more than
    // kBounceThresholdSeconds time has passed, we can be certain that the chain
    // has ended; but for code simplicity, we ignore that.)
  }

  auto* server_state =
      ServerBounceDetectionState::GetOrCreateForNavigationHandle(
          *navigation_handle);

  if (client_redirect) {
    server_state->navigation_start = std::move(client_redirect);
  } else {
    server_state->navigation_start = web_contents()->GetLastCommittedURL();
  }
}

void DIPSBounceDetector::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (client_detection_state_ &&
      GetDIPSSite(details.url) == client_detection_state_->current_site) {
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
        GetDIPSSite(navigation_handle->GetURL()), now);
  }

  auto* server_state =
      ServerBounceDetectionState::GetForNavigationHandle(*navigation_handle);

  if (!server_state) {
    return;
  }

  std::vector<DIPSRedirectInfoPtr> redirects;
  std::vector<CookieAccessType> access_types;
  bool filter_success = server_state->filter.Filter(
      navigation_handle->GetRedirectChain(), &access_types);
  UmaHistogramCookieAccessFilterResult(filter_success, GetCookieMode());

  for (size_t i = 0; i < access_types.size() - 1; i++) {
    redirects.push_back(std::make_unique<DIPSRedirectInfo>(
        /*url=*/navigation_handle->GetRedirectChain()[i],
        /*redirect_type=*/DIPSRedirectType::kServer,
        /*access_type=*/access_types[i],
        /*index=*/
        absl::holds_alternative<DIPSRedirectInfoPtr>(
            server_state->navigation_start)
            ? redirect_context_.size() + i + 1
            : i,
        /*source_id=*/GetRedirectSourceId(navigation_handle, i)));
  }

  // This call handles all the logic for terminating the redirect chain when
  // applicable, and using a temporary redirect context if the navigation didn't
  // commit.
  redirect_context_.Append(navigation_handle->HasCommitted(),
                           std::move(server_state->navigation_start),
                           std::move(redirects), navigation_handle->GetURL());

  if (navigation_handle->HasCommitted()) {
    // The last entry in navigation_handle->GetRedirectChain() is actually the
    // page being committed (i.e., not a redirect). If its HTTP request or
    // response accessed cookies, record this in our client detection state.
    client_detection_state_->cookie_access_type = access_types.back();
  }
}

void DIPSBounceDetector::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (client_detection_state_.has_value())
    client_detection_state_->received_user_activation = true;
}

void DIPSBounceDetector::WebContentsDestroyed() {
  // Handle the current chain before the tab closes and the state is lost.
  redirect_context_.EndChain(web_contents()->GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);
