// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-shared.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

using blink::mojom::EngagementLevel;
using content::NavigationHandle;

ServerBounceDetectionState::ServerBounceDetectionState() = default;
ServerBounceDetectionState::~ServerBounceDetectionState() = default;
ServerBounceDetectionState::ServerBounceDetectionState(
    NavigationHandle& navigation_handle) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ServerBounceDetectionState);

namespace {

// Controls whether UKM metrics are collected for DIPS.
BASE_FEATURE(kDipsUkm, "DipsUkm", base::FEATURE_ENABLED_BY_DEFAULT);

// The amount of time since finishing navigation to a page that a client-side
// redirect must happen within to count as a bounce (provided that all other
// criteria are met as well).
const int kBounceThresholdSeconds = 10;

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

/* static */
void DIPSWebContentsObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (profile->IsSystemProfile())
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS creates various irregular profiles (login, lock screen...); they
  // are of type kRegular (returns true for `Profile::IsRegular()`), that aren't
  // used to browse the web and users can't configure. Don't collect metrics
  // about them.
  if (!ash::ProfileHelper::IsUserProfile(profile))
    return;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DIPSWebContentsObserver::CreateForWebContents(web_contents);
}

DIPSWebContentsObserver::DIPSWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSWebContentsObserver>(*web_contents),
      dips_service_(DIPSService::Get(web_contents->GetBrowserContext())),
      site_engagement_service_(site_engagement::SiteEngagementService::Get(
          web_contents->GetBrowserContext())),
      detector_(this,
                base::DefaultTickClock::GetInstance(),
                base::DefaultClock::GetInstance()) {}

DIPSWebContentsObserver::~DIPSWebContentsObserver() = default;

DIPSBounceDetector::DIPSBounceDetector(DIPSBounceDetectorDelegate* delegate,
                                       const base::TickClock* tick_clock,
                                       const base::Clock* clock)
    : tick_clock_(tick_clock),
      clock_(clock),
      delegate_(delegate),
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

DIPSBounceDetectorDelegate::~DIPSBounceDetectorDelegate() = default;

DIPSNavigationHandle::~DIPSNavigationHandle() = default;

DIPSCookieMode DIPSWebContentsObserver::GetCookieMode() const {
  return GetDIPSCookieMode(
      web_contents()->GetBrowserContext()->IsOffTheRecord(),
      dips_service_->ShouldBlockThirdPartyCookies());
}

ukm::SourceId DIPSNavigationHandle::GetRedirectSourceId(int index) const {
  return ukm::UkmRecorder::GetSourceIdForRedirectUrl(
      base::PassKey<DIPSNavigationHandle>(), GetRedirectChain()[index]);
}

DIPSRedirectChainInfo::DIPSRedirectChainInfo(const GURL& initial_url,
                                             const GURL& final_url,
                                             int length)
    : initial_url(initial_url),
      initial_site(GetSiteForDIPS(initial_url)),
      final_url(final_url),
      final_site(GetSiteForDIPS(final_url)),
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
  const std::string site = GetSiteForDIPS(redirect.url);
  EngagementLevel level = delegate_->GetEngagementLevel(redirect.url);
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

  // Record this bounce in the DIPS database.
  if (redirect.access_type != CookieAccessType::kUnknown) {
    DIPSRecordedEvent bounce = redirect.access_type > CookieAccessType::kRead
                                   ? DIPSRecordedEvent::kStatefulBounce
                                   : DIPSRecordedEvent::kStatelessBounce;
    delegate_->RecordEvent(bounce, redirect.url, clock_->Now());
  }

  RedirectCategory category = ClassifyRedirect(redirect.access_type, level);
  UmaHistogramBounceCategory(category, delegate_->GetCookieMode(),
                             redirect.redirect_type);
}

void DIPSWebContentsObserver::RecordEvent(DIPSRecordedEvent event,
                                          const GURL& url,
                                          const base::Time& time) {
  switch (event) {
    case DIPSRecordedEvent::kStorage: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordStorage)
          .WithArgs(url, time, GetCookieMode());
      return;
    }
    case DIPSRecordedEvent::kInteraction: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordInteraction)
          .WithArgs(url, time, GetCookieMode());
      return;
    }
    case DIPSRecordedEvent::kStatelessBounce: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordStatelessBounce)
          .WithArgs(url, time);
      return;
    }
    case DIPSRecordedEvent::kStatefulBounce: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordStatefulBounce)
          .WithArgs(url, time);
      return;
    }
  }
}

const GURL& DIPSWebContentsObserver::GetLastCommittedURL() const {
  return web_contents()->GetLastCommittedURL();
}

ukm::SourceId DIPSWebContentsObserver::GetPageUkmSourceId() const {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

blink::mojom::EngagementLevel DIPSWebContentsObserver::GetEngagementLevel(
    const GURL& url) const {
  return site_engagement_service_->GetEngagementLevel(url);
}

// A thin wrapper around NavigationHandle to implement DIPSNavigationHandle.
class DIPSNavigationHandleImpl : public DIPSNavigationHandle {
 public:
  explicit DIPSNavigationHandleImpl(NavigationHandle* handle)
      : handle_(handle) {}

  bool HasUserGesture() const override {
    return handle_->HasUserGesture() || !handle_->IsRendererInitiated();
  }

  ServerBounceDetectionState* GetServerState() override {
    return ServerBounceDetectionState::GetOrCreateForNavigationHandle(*handle_);
  }

  bool HasCommitted() const override { return handle_->HasCommitted(); }

  const GURL& GetPreviousPrimaryMainFrameURL() const override {
    return handle_->GetPreviousPrimaryMainFrameURL();
  }

  const std::vector<GURL>& GetRedirectChain() const override {
    return handle_->GetRedirectChain();
  }

 private:
  raw_ptr<NavigationHandle> handle_;
};

void DIPSWebContentsObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.DidStartNavigation(&dips_handle);
}

void DIPSBounceDetector::DidStartNavigation(
    DIPSNavigationHandle* navigation_handle) {
  base::TimeTicks now = tick_clock_->NowTicks();

  DIPSRedirectInfoPtr client_redirect;
  if (client_detection_state_.has_value()) {
    base::TimeDelta bounce_time = now - client_detection_state_->page_load_time;

    if (!navigation_handle->HasUserGesture() &&
        (bounce_time <
         base::TimeDelta(base::Seconds(kBounceThresholdSeconds)))) {
      // Time between page load and client-side redirect starting is only
      // tracked for stateful bounces.
      if (client_detection_state_->cookie_access_type > CookieAccessType::kNone)
        UmaHistogramTimeToBounce(bounce_time);

      client_redirect = std::make_unique<DIPSRedirectInfo>(
          /*url=*/delegate_->GetLastCommittedURL(),
          /*redirect_type=*/DIPSRedirectType::kClient,
          /*access_type=*/client_detection_state_->cookie_access_type,
          /*index=*/redirect_context_.size(),
          /*source_id=*/
          delegate_->GetPageUkmSourceId(),
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

  ServerBounceDetectionState* server_state =
      navigation_handle->GetServerState();

  if (client_redirect) {
    server_state->navigation_start = std::move(client_redirect);
  } else {
    server_state->navigation_start = delegate_->GetLastCommittedURL();
  }
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  detector_.OnClientCookiesAccessed(details.url, details.type);
}

void DIPSBounceDetector::OnClientCookiesAccessed(const GURL& url,
                                                 CookieOperation op) {
  if (op == CookieOperation::kChange) {
    delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, clock_->Now());
  }
  if (client_detection_state_ &&
      GetSiteForDIPS(url) == client_detection_state_->current_site) {
    client_detection_state_->cookie_access_type =
        client_detection_state_->cookie_access_type |
        (op == CookieOperation::kChange ? CookieAccessType::kWrite
                                        : CookieAccessType::kRead);
  }
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.OnServerCookiesAccessed(&dips_handle, details.url, details.type);
}

void DIPSBounceDetector::OnServerCookiesAccessed(
    DIPSNavigationHandle* navigation_handle,
    const GURL& url,
    CookieOperation op) {
  if (op == CookieOperation::kChange) {
    delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, clock_->Now());
  }
  ServerBounceDetectionState* state = navigation_handle->GetServerState();
  if (state) {
    state->filter.AddAccess(url, op);
  }
}

void DIPSWebContentsObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.DidFinishNavigation(&dips_handle);
}

void DIPSBounceDetector::DidFinishNavigation(
    DIPSNavigationHandle* navigation_handle) {
  base::TimeTicks now = tick_clock_->NowTicks();
  // Iff the primary page changed, reset the client detection state while
  // storing the page load time and previous_url. A primary page change is
  // verified by checking IsInPrimaryMainFrame, !IsSameDocument, and
  // HasCommitted. HasCommitted is the only one not previously checked here.
  if (navigation_handle->HasCommitted()) {
    client_detection_state_ = ClientBounceDetectionState(
        navigation_handle->GetPreviousPrimaryMainFrameURL(),
        GetSiteForDIPS(navigation_handle->GetURL()), now);
  }

  ServerBounceDetectionState* server_state =
      navigation_handle->GetServerState();

  if (!server_state) {
    return;
  }

  std::vector<DIPSRedirectInfoPtr> redirects;
  std::vector<CookieAccessType> access_types;
  bool filter_success = server_state->filter.Filter(
      navigation_handle->GetRedirectChain(), &access_types);
  UmaHistogramCookieAccessFilterResult(filter_success,
                                       delegate_->GetCookieMode());

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
        /*source_id=*/navigation_handle->GetRedirectSourceId(i)));
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
// TODO(kaklilu): Follow up on how this interacts with Fenced Frames.
void DIPSWebContentsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  // Ignore iframe activations since we only care for its associated main-frame
  // interactions on the top-level site.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  detector_.OnUserActivation();
}

void DIPSBounceDetector::OnUserActivation() {
  GURL url = delegate_->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  if (client_detection_state_.has_value())
    client_detection_state_->received_user_activation = true;

  delegate_->RecordEvent(DIPSRecordedEvent::kInteraction, url, clock_->Now());
}

void DIPSWebContentsObserver::WebContentsDestroyed() {
  detector_.BeforeDestruction();
}

void DIPSBounceDetector::BeforeDestruction() {
  // Handle the current chain before the tab closes and the state is lost.
  redirect_context_.EndChain(delegate_->GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSWebContentsObserver);
