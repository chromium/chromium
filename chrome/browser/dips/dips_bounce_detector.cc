// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/gurl.h"

using content::NavigationHandle;

ServerBounceDetectionState::ServerBounceDetectionState() = default;
ServerBounceDetectionState::~ServerBounceDetectionState() = default;
ServerBounceDetectionState::ServerBounceDetectionState(
    NavigationHandle& navigation_handle) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ServerBounceDetectionState);

ClientBounceDetectionState::ClientBounceDetectionState(
    const ClientBounceDetectionState& other) = default;
ClientBounceDetectionState::~ClientBounceDetectionState() = default;
ClientBounceDetectionState::ClientBounceDetectionState(
    GURL url,
    std::string site,
    base::TimeTicks load_time) {
  this->previous_url = std::move(url);
  this->current_site = std::move(site);
  this->page_load_time = load_time;
}

namespace {

inline void UmaHistogramTimeToBounce(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.TimeFromNavigationCommitToClientBounce",
                          sample);
}

}  // namespace

/* static */
void DIPSWebContentsObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  auto* dips_service = DIPSService::Get(web_contents->GetBrowserContext());
  if (!dips_service) {
    return;
  }

  DIPSWebContentsObserver::CreateForWebContents(web_contents, dips_service);
}

DIPSWebContentsObserver::DIPSWebContentsObserver(
    content::WebContents* web_contents,
    DIPSService* dips_service)
    : content_settings::PageSpecificContentSettings::SiteDataObserver(
          web_contents),
      content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSWebContentsObserver>(*web_contents),
      dips_service_(dips_service),
      detector_(this,
                base::DefaultTickClock::GetInstance(),
                base::DefaultClock::GetInstance()) {
  issue_reporting_callback_ = base::BindRepeating(
      &DIPSWebContentsObserver::EmitDIPSIssue, weak_factory_.GetWeakPtr());
}

DIPSWebContentsObserver::~DIPSWebContentsObserver() = default;

const base::TimeDelta DIPSBounceDetector::kTimestampUpdateInterval =
    base::Minutes(1);

DIPSBounceDetector::DIPSBounceDetector(DIPSBounceDetectorDelegate* delegate,
                                       const base::TickClock* tick_clock,
                                       const base::Clock* clock)
    : tick_clock_(tick_clock),
      clock_(clock),
      delegate_(delegate),
      // It's safe to use Unretained because the callback is owned by the
      // DIPSRedirectContext which is owned by `this`, and the delegate must
      // outlive `this`.
      committed_redirect_context_(
          base::BindRepeating(&DIPSBounceDetectorDelegate::HandleRedirectChain,
                              base::Unretained(delegate)),
          base::BindRepeating(
              &DIPSBounceDetectorDelegate::ReportRedirectorsWithoutInteraction,
              base::Unretained(delegate)),
          /*initial_url=*/GURL::EmptyGURL(),
          /*redirect_prefix_count=*/0u),
      client_bounce_detection_timer_(
          FROM_HERE,
          dips::kClientBounceDetectionTimeout.Get(),
          base::BindRepeating(
              &DIPSBounceDetector::OnClientBounceDetectionTimeout,
              base::Unretained(this)),
          tick_clock) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

DIPSBounceDetectorDelegate::~DIPSBounceDetectorDelegate() = default;

DIPSNavigationHandle::~DIPSNavigationHandle() = default;

ukm::SourceId DIPSNavigationHandle::GetRedirectSourceId(int index) const {
  return ukm::UkmRecorder::GetSourceIdForRedirectUrl(
      base::PassKey<DIPSNavigationHandle>(), GetRedirectChain()[index]);
}

DIPSRedirectContext::DIPSRedirectContext(DIPSRedirectChainHandler handler,
                                         DIPSIssueHandler issue_handler,
                                         const GURL& initial_url,
                                         size_t redirect_prefix_count)
    : handler_(handler),
      issue_handler_(issue_handler),
      initial_url_(initial_url),
      redirect_prefix_count_(redirect_prefix_count) {}

DIPSRedirectContext::~DIPSRedirectContext() = default;

void DIPSRedirectContext::AppendClientRedirect(
    DIPSRedirectInfoPtr client_redirect) {
  DCHECK_EQ(client_redirect->redirect_type, DIPSRedirectType::kClient);
  if (client_redirect->access_type > SiteDataAccessType::kNone) {
    update_offset_ = redirects_.size();
  }
  if (client_redirect->access_type > SiteDataAccessType::kRead) {
    redirectors_.insert(GetSiteForDIPS(client_redirect->url));
  }
  client_redirect->chain_index = GetRedirectChainLength();
  redirects_.push_back(std::move(client_redirect));
  TrimRedirectsFromFront();
}

void DIPSRedirectContext::AppendServerRedirects(
    std::vector<DIPSRedirectInfoPtr> server_redirects) {
  for (auto& redirect : server_redirects) {
    DCHECK_EQ(redirect->redirect_type, DIPSRedirectType::kServer);
    if (redirect->access_type > SiteDataAccessType::kNone) {
      update_offset_ = redirects_.size();
    }
    if (redirect->access_type > SiteDataAccessType::kRead) {
      redirectors_.insert(GetSiteForDIPS(redirect->url));
    }
    redirect->chain_index = GetRedirectChainLength();
    redirects_.push_back(std::move(redirect));
  }
  TrimRedirectsFromFront();
}

void DIPSRedirectContext::TrimRedirectsFromFront() {
  size_t trim_count = base::ClampSub(redirects_.size(), kDIPSRedirectChainMax);
  if (trim_count == 0) {
    return;
  }

  TrimAndHandleRedirects(trim_count);

  update_offset_ = base::ClampSub(update_offset_, trim_count);
}

void DIPSRedirectContext::ReportIssue(const GURL& final_url) {
  // Since redirectors that are the same as the start or final page won't be
  // acted on, we don't report on them.
  //
  // NOTE: This is not exactly right since the end of this navigation may not
  // necessarily be the end of the chain, if a client redirect happens. However,
  // this is better for developer experience than waiting until then, since
  // notifications come faster.
  redirectors_.erase(GetSiteForDIPS(initial_url_));
  redirectors_.erase(GetSiteForDIPS(final_url));

  issue_handler_.Run(std::move(redirectors_));

  redirectors_.clear();
}

void DIPSRedirectContext::HandleUncommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects,
    GURL final_url) {
  absl::visit(  //
      base::Overloaded{
          [&](DIPSRedirectInfoPtr client_redirect) {
            // The uncommitted navigation began with a client redirect, so its
            // chain is considered an extension of *this*
            // `DIPSRedirectContext`'s in-progress chain within the temp
            // `DIPSRedirectContext`, whilst leaving *this*
            // `DIPSRedirectContext`'s in-progress chain unchanged.
            DIPSRedirectContext temp_context(handler_, issue_handler_,
                                             initial_url_,
                                             GetRedirectChainLength());
            temp_context.AppendClientRedirect(std::move(client_redirect));
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.ReportIssue(final_url);
            temp_context.EndChain(std::move(final_url));
          },
          [&](GURL previous_nav_last_committed_url) {
            // The uncommitted navigation began *without* a client redirect, so
            // a new redirect chain within a new `DIPSRedirectContext` and
            // process it immediately (the in-progress chain in *this*
            // `DIPSRedirectContext` is irrelevant).
            DIPSRedirectContext temp_context(handler_, issue_handler_,
                                             previous_nav_last_committed_url,
                                             /*redirect_prefix_count=*/0);
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.ReportIssue(final_url);
            temp_context.EndChain(std::move(final_url));
          },
      },
      std::move(navigation_start));
}

void DIPSRedirectContext::AppendCommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects,
    const GURL& final_url) {
  // If there was a client-side redirect before
  // `DIPSBounceDetector::client_bounce_detection_timer_` timedout, grow the
  // chain. Otherwise, end it.
  absl::visit(  //
      base::Overloaded{
          [this](DIPSRedirectInfoPtr client_redirect) {
            // The committed navigation began with a client redirect, so extend
            // the in-progress redirect chain.
            AppendClientRedirect(std::move(client_redirect));
          },
          [this](GURL previous_nav_last_committed_url) {
            // The committed navigation began *without* a client redirect, so
            // end the in-progress redirect chain and start a new one.
            EndChain(previous_nav_last_committed_url);
          },
      },
      std::move(navigation_start));

  // Server-side redirects always grow the chain.
  AppendServerRedirects(std::move(server_redirects));
  ReportIssue(final_url);
}

void DIPSRedirectContext::TrimAndHandleRedirects(size_t trim_count) {
  DCHECK_GE(redirects_.size(), trim_count);

  // Use an empty final_URL. This processes the redirect as different from the
  // final URL, which allows recording in the DIPS database.
  auto chain = std::make_unique<DIPSRedirectChainInfo>(
      initial_url_,
      /*final_url=*/GURL(), GetRedirectChainLength(),
      /*is_partial_chain=*/true);

  std::vector<DIPSRedirectInfoPtr> redirect_subchain;
  for (size_t ind = 0; ind < trim_count; ind++) {
    redirect_subchain.push_back(std::move(redirects_.at(ind)));
  }

  redirects_.erase(redirects_.begin(), redirects_.begin() + trim_count);
  redirect_prefix_count_ += trim_count;

  handler_.Run(std::move(redirect_subchain), std::move(chain));
}

void DIPSRedirectContext::EndChain(GURL final_url) {
  if (!initial_url_.is_empty()) {
    auto chain = std::make_unique<DIPSRedirectChainInfo>(
        initial_url_, final_url, GetRedirectChainLength(),
        /*is_partial_chain=*/false);
    handler_.Run(std::move(redirects_), std::move(chain));
  }

  initial_url_ = std::move(final_url);
  redirects_.clear();
  update_offset_ = 0;
}

bool DIPSRedirectContext::AddLateCookieAccess(GURL url, CookieOperation op) {
  while (update_offset_ < redirects_.size()) {
    if (redirects_[update_offset_]->url == url) {
      redirects_[update_offset_]->access_type =
          redirects_[update_offset_]->access_type | ToSiteDataAccessType(op);

      // This cookie access might indicate a stateful bounce and ideally we'd
      // report an issue to notify the user, but the navigation already
      // committed and any relevant notifications were already emitted, so it's
      // too late.

      return true;
    }

    update_offset_++;
  }

  return false;
}

void DIPSWebContentsObserver::EmitDIPSIssue(
    const std::set<std::string>& sites) {
  if (sites.empty()) {
    return;
  }

  auto details = blink::mojom::InspectorIssueDetails::New();
  auto bounce_tracking_issue_details =
      blink::mojom::BounceTrackingIssueDetails::New();

  bounce_tracking_issue_details->tracking_sites.reserve(sites.size());
  for (const auto& site : sites) {
    bounce_tracking_issue_details->tracking_sites.push_back(site);
  }

  details->bounce_tracking_issue_details =
      std::move(bounce_tracking_issue_details);

  WebContentsObserver::web_contents()
      ->GetPrimaryMainFrame()
      ->ReportInspectorIssue(blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kBounceTrackingIssue,
          std::move(details)));
}

void DIPSWebContentsObserver::ReportRedirectorsWithoutInteraction(
    const std::set<std::string>& sites) {
  if (sites.size() == 0) {
    return;
  }

  dips_service_->storage()
      ->AsyncCall(&DIPSStorage::FilterSitesWithoutInteraction)
      .WithArgs(sites)
      .Then(issue_reporting_callback_);
}

void DIPSWebContentsObserver::RecordEvent(DIPSRecordedEvent event,
                                          const GURL& url,
                                          const base::Time& time) {
  switch (event) {
    case DIPSRecordedEvent::kStorage: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordStorage)
          .WithArgs(url, time, dips_service_->GetCookieMode());
      return;
    }
    case DIPSRecordedEvent::kInteraction: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordInteraction)
          .WithArgs(url, time, dips_service_->GetCookieMode());
      return;
    }
  }
}

const GURL& DIPSWebContentsObserver::GetLastCommittedURL() const {
  return WebContentsObserver::web_contents()->GetLastCommittedURL();
}

ukm::SourceId DIPSWebContentsObserver::GetPageUkmSourceId() const {
  return WebContentsObserver::web_contents()
      ->GetPrimaryMainFrame()
      ->GetPageUkmSourceId();
}

void DIPSWebContentsObserver::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  dips_service_->HandleRedirectChain(
      std::move(redirects), std::move(chain),
      base::BindRepeating(
          &DIPSWebContentsObserver::IncrementPageSpecificBounceCount,
          weak_factory_.GetWeakPtr()));
}

void DIPSWebContentsObserver::IncrementPageSpecificBounceCount(
    const GURL& final_url) {
  // Do nothing if the current URL doesn't match the final URL of the chain.
  // This means that the user has navigated away from the bounce destination, so
  // we don't want to update settings for the wrong site.
  if (WebContentsObserver::web_contents()->GetURL() != final_url) {
    return;
  }

  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      WebContentsObserver::web_contents()->GetPrimaryPage());
  pscs->IncrementStatefulBounceCount();
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
  // These resources need to be collected as soon as possible, although some
  // might not be used:
  bool timedout = !client_bounce_detection_timer_.IsRunning();
  client_bounce_detection_timer_.Stop();
  auto now = tick_clock_->NowTicks();

  auto* server_bounce_detection_state = navigation_handle->GetServerState();

  // A user gesture indicates no client-redirect. And, we won't consider a
  // client-redirect to be a bounce if we timedout on the
  // `client_bounce_detection_timer_ `.
  if (navigation_handle->HasUserGesture() || timedout ||
      !client_detection_state_.has_value()) {
    server_bounce_detection_state->navigation_start =
        delegate_->GetLastCommittedURL();
    return;
  }

  base::TimeDelta client_bounce_delay =
      now - client_detection_state_->page_load_time;
  // The delay between the previous navigation commit and the current
  // client-redirect is only tracked for stateful bounces.
  if (client_detection_state_->site_data_access_type >
      SiteDataAccessType::kNone) {
    UmaHistogramTimeToBounce(client_bounce_delay);
  }

  // We cannot append this client-redirect to |committed_redirect_context_|
  // immediately, because we don't know if the navigation will commit. We must
  // wait until DidFinishNavigation() is triggered.
  server_bounce_detection_state->navigation_start =
      std::make_unique<DIPSRedirectInfo>(
          /*url=*/delegate_->GetLastCommittedURL(),
          /*redirect_type=*/DIPSRedirectType::kClient,
          /*access_type=*/client_detection_state_->site_data_access_type,
          /*source_id=*/
          delegate_->GetPageUkmSourceId(),
          /*time=*/clock_->Now(),
          /*client_bounce_delay=*/client_bounce_delay,
          /*has_sticky_activation=*/
          client_detection_state_->last_activation_time.has_value());
}

void DIPSWebContentsObserver::OnSiteDataAccessed(
    const content_settings::AccessDetails& access_details) {
  // NOTE: The current implementation is only acting on all site data types
  // collapsed under `content_settings::SiteDataType::kStorage` with the
  // exception of WebLocks (not monitored by the
  // `content_settings::PageSpecificContentSettings`) as it's not persistent.
  if (access_details.site_data_type !=
      content_settings::SiteDataType::kStorage) {
    return;
  }

  DCHECK(access_details.render_frame_host);

  // TODO(crbug.com/1434764): handle same-site iframes.
  if (!access_details.render_frame_host->IsInPrimaryMainFrame() ||
      access_details.blocked_by_policy) {
    return;
  }

  detector_.OnClientSiteDataAccessed(
      access_details.url, ToCookieOperation(access_details.access_type));
}

void DIPSWebContentsObserver::OnStatefulBounceDetected() {}

void DIPSBounceDetector::OnClientSiteDataAccessed(const GURL& url,
                                                  CookieOperation op) {
  auto now = clock_->Now();

  if (client_detection_state_ &&
      GetSiteForDIPS(url) == client_detection_state_->current_site) {
    client_detection_state_->site_data_access_type =
        client_detection_state_->site_data_access_type |
        ToSiteDataAccessType(op);

    // To decrease the number of writes made to the database, after a site
    // storage event (cookie write) on the page, new storage events will not
    // be recorded for the next |kTimestampUpdateInterval|.
    if (op == CookieOperation::kChange &&
        ShouldUpdateTimestamp(client_detection_state_->last_storage_time,
                              now)) {
      client_detection_state_->last_storage_time = now;
      delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, now);
    }
  } else if (op == CookieOperation::kChange) {
    delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, now);
  }
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!IsInPrimaryPage(render_frame_host) || details.blocked_by_policy ||
      !IsSameSiteForDIPS(details.first_party_url, details.url)) {
    return;
  }

  detector_.OnClientCookiesAccessed(details.url, details.type);
}

void DIPSBounceDetector::OnClientCookiesAccessed(const GURL& url,
                                                 CookieOperation op) {
  base::Time now = clock_->Now();

  // We might be called for "late" server cookie accesses, not just client
  // cookies. Before completing other checks, attempt to attribute the cookie
  // access to the current redirect chain to handle that case.
  //
  // TODO(rtarpine): Is it possible for cookie accesses to be reported late for
  // uncommitted navigations?
  if (committed_redirect_context_.AddLateCookieAccess(url, op)) {
    if (op == CookieOperation::kChange) {
      delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, now);
    }
    return;
  }

  OnClientSiteDataAccessed(url, op);
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  // Discard all notifications that are:
  // - From other page types like FencedFrames, and Prerendered.
  // - Blocked by policies.
  // - That are not same site (wrt GetSiteForDIPS()) with the first party URL.
  // TODO(crbug.com/1445739): Treat partitioned 3P cookies as 1P cookies.
  if (!IsInPrimaryPage(navigation_handle) || details.blocked_by_policy ||
      !IsSameSiteForDIPS(details.first_party_url, details.url)) {
    return;
  }

  // All access within the primary page iframes are attributed to the URL of the
  // main frame (ie the first party URL).
  if (IsInPrimaryPageIFrame(navigation_handle)) {
    detector_.OnClientSiteDataAccessed(details.url, details.type);
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

  // Starts the timer.
  client_bounce_detection_timer_.Reset();

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
  std::vector<SiteDataAccessType> access_types;
  server_state->filter.Filter(navigation_handle->GetRedirectChain(),
                              &access_types);

  for (size_t i = 0; i < access_types.size() - 1; i++) {
    redirects.push_back(std::make_unique<DIPSRedirectInfo>(
        /*url=*/navigation_handle->GetRedirectChain()[i],
        /*redirect_type=*/DIPSRedirectType::kServer,
        /*access_type=*/access_types[i],
        /*source_id=*/navigation_handle->GetRedirectSourceId(i),
        /*time=*/clock_->Now()));
  }

  if (navigation_handle->HasCommitted()) {
    committed_redirect_context_.AppendCommitted(
        std::move(server_state->navigation_start), std::move(redirects),
        navigation_handle->GetURL());
  } else {
    committed_redirect_context_.HandleUncommitted(
        std::move(server_state->navigation_start), std::move(redirects),
        navigation_handle->GetURL());
  }

  if (navigation_handle->HasCommitted()) {
    // The last entry in navigation_handle->GetRedirectChain() is actually the
    // page being committed (i.e., not a redirect). If its HTTP request or
    // response accessed cookies, record this in our client detection state.
    client_detection_state_->site_data_access_type = access_types.back();
  }
}

// TODO(kaklilu): Follow up on how this interacts with Fenced Frames.
void DIPSWebContentsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  // Ignore iframe activations since we only care for its associated main-frame
  // interactions on the top-level site.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  detector_.OnUserActivation();
}

void DIPSBounceDetector::OnUserActivation() {
  GURL url = delegate_->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  base::Time now = clock_->Now();
  if (client_detection_state_.has_value()) {
    // To decrease the number of writes made to the database, after a user
    // activation event on the page, new activation events will not be recorded
    // for the next |kTimestampUpdateInterval|.
    if (!ShouldUpdateTimestamp(client_detection_state_->last_activation_time,
                               now)) {
      return;
    }

    client_detection_state_->last_activation_time = now;
  }

  delegate_->RecordEvent(DIPSRecordedEvent::kInteraction, url, now);
}

bool DIPSBounceDetector::ShouldUpdateTimestamp(
    base::optional_ref<const base::Time> last_time,
    base::Time now) {
  return (!last_time.has_value() ||
          (now - last_time.value()) >= kTimestampUpdateInterval);
}

void DIPSWebContentsObserver::WebContentsDestroyed() {
  detector_.BeforeDestruction();
}

void DIPSBounceDetector::BeforeDestruction() {
  committed_redirect_context_.EndChain(delegate_->GetLastCommittedURL());
}

void DIPSBounceDetector::OnClientBounceDetectionTimeout() {
  committed_redirect_context_.EndChain(delegate_->GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSWebContentsObserver);

ukm::SourceId GetInitialRedirectSourceId(
    content::NavigationHandle* navigation_handle) {
  DIPSNavigationHandleImpl handle(navigation_handle);
  return handle.GetRedirectSourceId(0);
}
