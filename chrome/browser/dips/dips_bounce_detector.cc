// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

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

// The amount of time since finishing navigation to a page that a client-side
// redirect must happen within to count as a bounce (provided that all other
// criteria are met as well).
const int kBounceThresholdSeconds = 10;

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
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSWebContentsObserver>(*web_contents),
      dips_service_(dips_service),
      detector_(this,
                base::DefaultTickClock::GetInstance(),
                base::DefaultClock::GetInstance()) {}

DIPSWebContentsObserver::~DIPSWebContentsObserver() = default;

const base::TimeDelta DIPSBounceDetector::kInteractionUpdateInterval =
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
      redirect_context_(
          base::BindRepeating(&DIPSBounceDetectorDelegate::HandleRedirectChain,
                              base::Unretained(delegate)),
          /*initial_url=*/GURL::EmptyGURL()) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

DIPSBounceDetectorDelegate::~DIPSBounceDetectorDelegate() = default;

DIPSNavigationHandle::~DIPSNavigationHandle() = default;

ukm::SourceId DIPSNavigationHandle::GetRedirectSourceId(int index) const {
  return ukm::UkmRecorder::GetSourceIdForRedirectUrl(
      base::PassKey<DIPSNavigationHandle>(), GetRedirectChain()[index]);
}

DIPSRedirectContext::DIPSRedirectContext(DIPSRedirectChainHandler handler,
                                         const GURL& initial_url)
    : handler_(handler), initial_url_(initial_url) {}

DIPSRedirectContext::~DIPSRedirectContext() = default;

void DIPSRedirectContext::AppendClientRedirect(
    DIPSRedirectInfoPtr client_redirect) {
  DCHECK_EQ(client_redirect->redirect_type, DIPSRedirectType::kClient);
  redirects_.push_back(std::move(client_redirect));
}

void DIPSRedirectContext::AppendServerRedirects(
    std::vector<DIPSRedirectInfoPtr> server_redirects) {
  for (auto& redirect : server_redirects) {
    DCHECK_EQ(redirect->redirect_type, DIPSRedirectType::kServer);
    redirects_.push_back(std::move(redirect));
  }
}

void DIPSRedirectContext::HandleUncommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects,
    GURL final_url) {
  absl::visit(  //
      base::Overloaded{
          [&](DIPSRedirectInfoPtr client_redirect) {
            // The uncommitted navigation began with a client redirect, so its
            // chain is considered an extension of the in-progress chain
            // (without modifying it).
            DIPSRedirectContext temp_context(handler_, initial_url_);
            temp_context.AppendClientRedirect(std::move(client_redirect));
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.EndChain(std::move(final_url));
          },
          [&](GURL client_url) {
            // The uncommitted navigation began *without* a client redirect, so
            // it started a new chain (the in-progress chain is irrelevant.)
            DIPSRedirectContext temp_context(handler_, client_url);
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.EndChain(std::move(final_url));
          },
      },
      std::move(navigation_start));
}

void DIPSRedirectContext::AppendCommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects) {
  // If there was a client-side redirect, grow the chain. Otherwise, end it.
  absl::visit(  //
      base::Overloaded{
          [this](DIPSRedirectInfoPtr client_redirect) {
            // The committed navigation began with a client redirect, so extend
            // the in-progress chain.
            AppendClientRedirect(std::move(client_redirect));
          },
          [this](GURL client_url) {
            // The committed navigation began *without* a client redirect, so
            // end the old chain and start a new one.
            EndChain(std::move(client_url));
          },
      },
      std::move(navigation_start));

  // Server-side redirects always grow the chain.
  AppendServerRedirects(std::move(server_redirects));
}

void DIPSRedirectContext::EndChain(GURL url) {
  if (!initial_url_.is_empty()) {
    // Uncommitted chains may omit earlier (committed) redirects in the chain,
    // so |redirects_.size()| may not tell us the correct chain length. Instead,
    // use the index of the last item in the chain (since it was generated based
    // on the committed chain length).
    int length = redirects_.empty() ? 0 : redirects_.back()->index + 1;
    auto chain =
        std::make_unique<DIPSRedirectChainInfo>(initial_url_, url, length);
    handler_.Run(std::move(redirects_), std::move(chain));
  }

  initial_url_ = std::move(url);
  redirects_.clear();
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
  return web_contents()->GetLastCommittedURL();
}

ukm::SourceId DIPSWebContentsObserver::GetPageUkmSourceId() const {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

void DIPSWebContentsObserver::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  dips_service_->HandleRedirectChain(std::move(redirects), std::move(chain));
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
      if (client_detection_state_->cookie_access_type >
          CookieAccessType::kNone) {
        UmaHistogramTimeToBounce(bounce_time);
      }

      client_redirect = std::make_unique<DIPSRedirectInfo>(
          /*url=*/delegate_->GetLastCommittedURL(),
          /*redirect_type=*/DIPSRedirectType::kClient,
          /*access_type=*/client_detection_state_->cookie_access_type,
          /*index=*/redirect_context_.size(),
          /*source_id=*/
          delegate_->GetPageUkmSourceId(),
          /*time=*/clock_->Now(),
          /*client_bounce_delay=*/bounce_time,
          /*has_sticky_activation=*/
          client_detection_state_->last_activation_time.has_value());
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
  if (!render_frame_host->IsInPrimaryMainFrame() || details.blocked_by_policy ||
      !net::SiteForCookies::FromUrl(details.first_party_url)
           .IsFirstParty(details.url)) {
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
  if (!navigation_handle->IsInPrimaryMainFrame() || details.blocked_by_policy ||
      !net::SiteForCookies::FromUrl(details.first_party_url)
           .IsFirstParty(details.url)) {
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
  server_state->filter.Filter(navigation_handle->GetRedirectChain(),
                              &access_types);

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
        /*source_id=*/navigation_handle->GetRedirectSourceId(i),
        /*time=*/clock_->Now()));
  }

  if (navigation_handle->HasCommitted()) {
    redirect_context_.AppendCommitted(std::move(server_state->navigation_start),
                                      std::move(redirects));
  } else {
    redirect_context_.HandleUncommitted(
        std::move(server_state->navigation_start), std::move(redirects),
        navigation_handle->GetURL());
  }

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
    // for the next |kInteractionUpdateInterval|.
    if (client_detection_state_->last_activation_time.has_value() &&
        (now - client_detection_state_->last_activation_time.value()) <
            kInteractionUpdateInterval) {
      return;
    }

    client_detection_state_->last_activation_time = now;
  }

  delegate_->RecordEvent(DIPSRecordedEvent::kInteraction, url, now);
}

void DIPSWebContentsObserver::WebContentsDestroyed() {
  detector_.BeforeDestruction();
}

void DIPSBounceDetector::BeforeDestruction() {
  // Handle the current chain before the tab closes and the state is lost.
  redirect_context_.EndChain(delegate_->GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSWebContentsObserver);
