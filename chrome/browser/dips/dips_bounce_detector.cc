// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <cstddef>
#include <ctime>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/cookies/canonical_cookie.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

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

std::vector<DIPSRedirectInfoPtr> CloneRedirects(
    const std::vector<DIPSRedirectInfoPtr>& redirects) {
  std::vector<DIPSRedirectInfoPtr> clones;
  clones.reserve(redirects.size());
  for (const auto& ptr : redirects) {
    clones.push_back(std::make_unique<DIPSRedirectInfo>(*ptr));
  }
  return clones;
}

// If a PrimaryPageMarker is attached to a page, then it is or was the primary
// page of its WebContents. We use it to determine whether late cookie access
// notifications were from the primary page and so should be attributed to the
// RedirectChainDetector's committed redirect context.
class PrimaryPageMarker : public content::PageUserData<PrimaryPageMarker> {
 private:
  friend class content::PageUserData<PrimaryPageMarker>;
  explicit PrimaryPageMarker(content::Page& page)
      : content::PageUserData<PrimaryPageMarker>(page) {}
  PAGE_USER_DATA_KEY_DECL();
};

PAGE_USER_DATA_KEY_IMPL(PrimaryPageMarker);

}  // namespace

/* static */
void DIPSWebContentsObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  auto* dips_service = DIPSServiceImpl::Get(web_contents->GetBrowserContext());
  if (!dips_service) {
    return;
  }

  DIPSWebContentsObserver::CreateForWebContents(web_contents, dips_service);
}

DIPSWebContentsObserver::DIPSWebContentsObserver(
    content::WebContents* web_contents,
    DIPSServiceImpl* dips_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSWebContentsObserver>(*web_contents),
      dips_service_(dips_service) {
  detector_ = RedirectChainDetector::FromWebContents(web_contents);
  CHECK(detector_);
  detector_->AddObserver(this);
  issue_reporting_callback_ = base::BindRepeating(
      &DIPSWebContentsObserver::EmitDIPSIssue, weak_factory_.GetWeakPtr());
}

DIPSWebContentsObserver::~DIPSWebContentsObserver() = default;

RedirectChainDetector::RedirectChainDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<RedirectChainDetector>(*web_contents),
      detector_(this,
                base::DefaultTickClock::GetInstance(),
                base::DefaultClock::GetInstance()),
      // Unretained() is safe because delayed_handler_ is owned by this.
      delayed_handler_(base::BindRepeating(
          &RedirectChainDetector::NotifyOnRedirectChainEnded,
          base::Unretained(this))) {
  PrimaryPageMarker::CreateForPage(web_contents->GetPrimaryPage());
}

RedirectChainDetector::~RedirectChainDetector() = default;

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
          base::BindRepeating(&DIPSBounceDetectorDelegate::ReportRedirectors,
                              base::Unretained(delegate)),
          /*initial_url=*/UrlAndSourceId(),
          /*redirect_prefix_count=*/0u),
      client_bounce_detection_timer_(
          FROM_HERE,
          features::kDIPSClientBounceDetectionTimeout.Get(),
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
                                         const UrlAndSourceId& initial_url,
                                         size_t redirect_prefix_count)
    : handler_(handler),
      issue_handler_(issue_handler),
      initial_url_(initial_url),
      redirect_prefix_count_(redirect_prefix_count) {}

DIPSRedirectContext::~DIPSRedirectContext() = default;

void DIPSRedirectContext::AppendClientRedirect(
    DIPSRedirectInfoPtr client_redirect) {
  DCHECK_EQ(client_redirect->redirect_type, DIPSRedirectType::kClient);
  if (client_redirect->access_type > SiteDataAccessType::kRead) {
    redirectors_.insert(client_redirect->site);
  }
  redirects_.push_back(std::move(client_redirect));
  TrimRedirectsFromFront();
}

void DIPSRedirectContext::AppendServerRedirects(
    std::vector<DIPSRedirectInfoPtr> server_redirects) {
  for (auto& redirect : server_redirects) {
    DCHECK_EQ(redirect->redirect_type, DIPSRedirectType::kServer);
    if (redirect->access_type > SiteDataAccessType::kRead) {
      redirectors_.insert(redirect->site);
    }
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
}

void DIPSRedirectContext::ReportIssue(const GURL& final_url) {
  // Since redirectors that are the same as the start or final page won't be
  // acted on, we don't report on them.
  //
  // NOTE: This is not exactly right since the end of this navigation may not
  // necessarily be the end of the chain, if a client redirect happens. However,
  // this is better for developer experience than waiting until then, since
  // notifications come faster.
  redirectors_.erase(GetSiteForDIPS(initial_url_.url));
  redirectors_.erase(GetSiteForDIPS(final_url));

  issue_handler_.Run(std::move(redirectors_));

  redirectors_.clear();
}

std::optional<std::pair<size_t, DIPSRedirectInfo*>>
DIPSRedirectContext::GetRedirectInfoFromChain(const std::string& site) const {
  // Iterate in reverse order to obtain the most recent occurrence of the site.
  for (int ind = redirects_.size() - 1; ind >= 0; ind--) {
    if (redirects_.at(ind)->site == site) {
      return std::make_pair(static_cast<size_t>(ind), redirects_.at(ind).get());
    }
  }
  return std::nullopt;
}

bool DIPSRedirectContext::SiteHadUserActivation(const std::string& site) const {
  if (initial_url_had_user_activation_ &&
      site == GetSiteForDIPS(initial_url_.url)) {
    return true;
  }

  for (const auto& redirect : redirects_) {
    if (redirect->has_sticky_activation && redirect->site == site) {
      return true;
    }
  }

  return false;
}

std::set<std::string> DIPSRedirectContext::AllSitesWithUserActivation() const {
  std::set<std::string> sites;

  if (initial_url_had_user_activation_) {
    sites.insert(GetSiteForDIPS(initial_url_.url));
  }

  for (const auto& redirect : redirects_) {
    if (redirect->has_sticky_activation) {
      sites.insert(redirect->site);
    }
  }

  return sites;
}

std::map<std::string, std::pair<GURL, bool>>
DIPSRedirectContext::GetRedirectHeuristicURLs(
    const GURL& first_party_url,
    base::optional_ref<std::set<std::string>> allowed_sites,
    bool require_current_interaction) const {
  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction;

  std::set<std::string> sites_with_user_activation =
      AllSitesWithUserActivation();

  const std::string& first_party_site = GetSiteForDIPS(first_party_url);
  for (const auto& redirect : redirects_) {
    const GURL& url = redirect->url.url;
    const std::string& site = redirect->site;

    // The redirect heuristic does not apply for first-party cookie access.
    if (site == first_party_site) {
      continue;
    }

    // Check the list of allowed sites, if provided.
    if (allowed_sites.has_value() && !allowed_sites->contains(site)) {
      continue;
    }

    // Check for a current interaction, if the flag requires it.
    if (require_current_interaction &&
        !sites_with_user_activation.contains(site)) {
      continue;
    }

    // Add the url to the map, but do not override a previous current
    // interaction.
    auto& [prev_url, had_current_interaction] =
        sites_to_url_and_current_interaction[site];
    if (prev_url.is_empty() || !had_current_interaction) {
      prev_url = url;
      had_current_interaction = sites_with_user_activation.contains(site);
    }
  }

  return sites_to_url_and_current_interaction;
}

void DIPSRedirectContext::HandleUncommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects) {
  // Uncommitted navigations leave the user on the last-committed page; use that
  // for `final_url`.
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
            // Copy the URL of `client_redirect` before moving it.
            UrlAndSourceId final_url = client_redirect->url;
            temp_context.AppendClientRedirect(std::move(client_redirect));
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.ReportIssue(final_url.url);
            temp_context.EndChain(std::move(final_url),
                                  /*current_page_has_sticky_activation=*/false);
          },
          [&](UrlAndSourceId previous_nav_last_committed_url) {
            // The uncommitted navigation began *without* a client redirect, so
            // a new redirect chain within a new `DIPSRedirectContext` and
            // process it immediately (the in-progress chain in *this*
            // `DIPSRedirectContext` is irrelevant).
            DIPSRedirectContext temp_context(handler_, issue_handler_,
                                             previous_nav_last_committed_url,
                                             /*redirect_prefix_count=*/0);
            temp_context.AppendServerRedirects(std::move(server_redirects));
            temp_context.ReportIssue(
                /*final_url=*/previous_nav_last_committed_url.url);
            temp_context.EndChain(
                /*final_url=*/std::move(previous_nav_last_committed_url),
                /*current_page_has_sticky_activation=*/false);
          },
      },
      std::move(navigation_start));
}

void DIPSRedirectContext::AppendCommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects,
    const UrlAndSourceId& final_url,
    bool current_page_has_sticky_activation) {
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
          [this, current_page_has_sticky_activation](
              UrlAndSourceId previous_nav_last_committed_url) {
            // The committed navigation began *without* a client redirect, so
            // end the in-progress redirect chain and start a new one.
            EndChain(previous_nav_last_committed_url,
                     current_page_has_sticky_activation);
          },
      },
      std::move(navigation_start));

  // Server-side redirects always grow the chain.
  AppendServerRedirects(std::move(server_redirects));
  ReportIssue(final_url.url);
}

void DIPSRedirectContext::TrimAndHandleRedirects(size_t trim_count) {
  DCHECK_GE(redirects_.size(), trim_count);

  // Use an empty final_URL. This processes the redirect as different from the
  // final URL, which allows recording in the DIPS database.
  auto chain = std::make_unique<DIPSRedirectChainInfo>(
      initial_url_,
      /*final_url=*/UrlAndSourceId(), GetRedirectChainLength(),
      /*is_partial_chain=*/true);

  std::vector<DIPSRedirectInfoPtr> redirect_subchain;
  for (size_t ind = 0; ind < trim_count; ind++) {
    redirect_subchain.push_back(std::move(redirects_.at(ind)));
  }

  redirects_.erase(redirects_.begin(), redirects_.begin() + trim_count);
  redirect_prefix_count_ += trim_count;

  handler_.Run(std::move(redirect_subchain), std::move(chain));
}

void DIPSRedirectContext::EndChain(UrlAndSourceId final_url,
                                   bool current_page_has_sticky_activation) {
  if (!initial_url_.url.is_empty()) {
    auto chain = std::make_unique<DIPSRedirectChainInfo>(
        initial_url_, final_url, GetRedirectChainLength(),
        /*is_partial_chain=*/false);
    handler_.Run(std::move(redirects_), std::move(chain));
  }

  initial_url_had_user_activation_ = current_page_has_sticky_activation;
  initial_url_ = std::move(final_url);
  redirects_.clear();
}

namespace {
bool AddLateCookieAccess(const GURL& url,
                         CookieOperation op,
                         std::vector<DIPSRedirectInfoPtr>& redirects) {
  const size_t kMaxLookback = 5;
  const size_t lookback = std::min(kMaxLookback, redirects.size());
  for (size_t i = 1; i <= lookback; i++) {
    const size_t offset = redirects.size() - i;
    if (redirects[offset]->url.url == url) {
      redirects[offset]->access_type =
          redirects[offset]->access_type | ToSiteDataAccessType(op);

      // This cookie access might indicate a stateful bounce and ideally we'd
      // report an issue to notify the user, but the navigation already
      // committed and any relevant notifications were already emitted, so it's
      // too late.
      return true;
    }
  }

  return false;
}
}  // namespace

bool DIPSRedirectContext::AddLateCookieAccess(const GURL& url,
                                              CookieOperation op) {
  return ::AddLateCookieAccess(url, op, redirects_);
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

  web_contents()->GetPrimaryMainFrame()->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kBounceTrackingIssue,
          std::move(details)));
}

void RedirectChainDetector::ReportRedirectors(std::set<std::string> sites) {
  if (sites.size() == 0) {
    return;
  }

  for (auto& observer : observers_) {
    observer.ReportRedirectors(sites);
  }
}

void DIPSWebContentsObserver::ReportRedirectors(
    const std::set<std::string>& sites) {
  dips_service_->storage()
      ->AsyncCall(&DIPSStorage::FilterSitesWithoutProtectiveEvent)
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
    case DIPSRecordedEvent::kWebAuthnAssertion: {
      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::RecordWebAuthnAssertion)
          .WithArgs(url, time, dips_service_->GetCookieMode());
      return;
    }
  }
}

UrlAndSourceId RedirectChainDetector::GetLastCommittedURL() const {
  // We can't use RenderFrameHost::GetLastCommittedURL() because that returns an
  // empty URL while the tab is closing (i.e. within
  // WebContentsObserver::WebContentsDestroyed)
  return UrlAndSourceId(
      WebContentsObserver::web_contents()->GetLastCommittedURL(),
      WebContentsObserver::web_contents()
          ->GetPrimaryMainFrame()
          ->GetPageUkmSourceId());
}

namespace dips {
void Populate3PcExceptions(content::BrowserContext* browser_context,
                           content::WebContents* web_contents,
                           const GURL& initial_url,
                           const GURL& final_url,
                           base::span<DIPSRedirectInfoPtr> redirects) {
  const blink::StorageKey initial_url_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(initial_url));
  const blink::StorageKey final_url_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(final_url));
  // TODO: crbug.com/40883201 - When we move to //content, we will call
  // IsFullCookieAccessAllowed() via ContentBrowserClient instead of as a
  // standalone function.
  for (DIPSRedirectInfoPtr& redirect : redirects) {
    redirect->has_3pc_exception =
        dips_move::IsFullCookieAccessAllowed(browser_context, web_contents,
                                             redirect->url.url,
                                             initial_url_key) ||
        dips_move::IsFullCookieAccessAllowed(browser_context, web_contents,
                                             redirect->url.url, final_url_key);
  }
}
}  // namespace dips

void RedirectChainDetector::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  // We have to set `has_3pc_exception` on each redirect before passing them to
  // the DIPSService, because calculating it depends on the WebContents.
  dips::Populate3PcExceptions(web_contents()->GetBrowserContext(),
                              web_contents(), chain->initial_url.url,
                              chain->final_url.url, redirects);
  delayed_handler_.HandleRedirectChain(std::move(redirects), std::move(chain));
}

void RedirectChainDetector::NotifyOnRedirectChainEnded(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  for (auto& observer : observers_) {
    observer.OnRedirectChainEnded(redirects, *chain);
  }
}

void DIPSWebContentsObserver::OnRedirectChainEnded(
    const std::vector<DIPSRedirectInfoPtr>& redirects,
    const DIPSRedirectChainInfo& chain) {
  // We need to pass in a WeakPtr to DIPSWebContentsObserver as it's not
  // guaranteed to outlive the call.
  dips_service_->HandleRedirectChain(
      CloneRedirects(redirects), std::make_unique<DIPSRedirectChainInfo>(chain),
      base::BindRepeating(&DIPSWebContentsObserver::OnStatefulBounce,
                          weak_factory_.GetWeakPtr()));
}

void DIPSWebContentsObserver::OnStatefulBounce(const GURL& final_url) {
  // Do nothing if the current URL doesn't match the final URL of the chain.
  // This means that the user has navigated away from the bounce destination, so
  // we don't want to update settings for the wrong site.
  if (web_contents()->GetURL() != final_url) {
    return;
  }

  dips_service_->NotifyStatefulBounce(web_contents());
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

  ukm::SourceId GetNextPageUkmSourceId() override {
    return handle_->GetNextPageUkmSourceId();
  }

  const GURL& GetPreviousPrimaryMainFrameURL() const override {
    return handle_->GetPreviousPrimaryMainFrameURL();
  }

  const GURL GetInitiator() const override {
    return (!handle_->GetInitiatorOrigin().has_value() ||
            handle_->GetInitiatorOrigin().value().opaque())
               ? GURL("about:blank")
               : handle_->GetInitiatorOrigin().value().GetURL();
  }

  const std::vector<GURL>& GetRedirectChain() const override {
    return handle_->GetRedirectChain();
  }

  bool WasResponseCached() override { return handle_->WasResponseCached(); }

  int GetHTTPResponseCode() override {
    const net::HttpResponseHeaders* headers = handle_->GetResponseHeaders();
    if (headers == nullptr) {
      return 0;
    }
    return headers->response_code();
  }

 private:
  raw_ptr<NavigationHandle> handle_;
};

void RedirectChainDetector::DidStartNavigation(
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

  server_bounce_detection_state->last_server_redirect = now;

  // A user gesture indicates no client-redirect. And, we won't consider a
  // client-redirect to be a bounce if we timedout on the
  // `client_bounce_detection_timer_ `.
  if (navigation_handle->HasUserGesture() || timedout ||
      !client_detection_state_.has_value()) {
    server_bounce_detection_state->navigation_start =
        delegate_->GetLastCommittedURL().url.is_empty()
            ? UrlAndSourceId(navigation_handle->GetInitiator(),
                             ukm::kInvalidSourceId)
            : delegate_->GetLastCommittedURL();
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
          /*time=*/clock_->Now(),
          /*client_bounce_delay=*/client_bounce_delay,
          /*has_sticky_activation=*/
          client_detection_state_->last_activation_time.has_value(),
          /*web_authn_assertion_request_succeeded=*/
          client_detection_state_->last_successful_web_authn_assertion_time
              .has_value());
}

void RedirectChainDetector::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  DUMP_WILL_BE_CHECK(!navigation_handle->IsSameDocument());
  DUMP_WILL_BE_CHECK(!navigation_handle->HasCommitted());

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.DidRedirectNavigation(&dips_handle);
}

void DIPSBounceDetector::DidRedirectNavigation(
    DIPSNavigationHandle* navigation_handle) {
  ServerBounceDetectionState* server_state =
      navigation_handle->GetServerState();
  if (!server_state) {
    return;
  }

  auto now = tick_clock_->NowTicks();
  base::TimeDelta bounce_delay = now - server_state->last_server_redirect;
  server_state->last_server_redirect = now;

  server_state->server_redirects.push_back(
      ServerBounceDetectionState::ServerRedirectData({
          .http_response_code = navigation_handle->GetHTTPResponseCode(),
          .bounce_delay = bounce_delay,
          .was_response_cached = navigation_handle->WasResponseCached(),
          .destination_url = GURL(navigation_handle->GetURL()),
      }));
}

void RedirectChainDetector::NotifyStorageAccessed(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::StorageTypeAccessed storage_type,
    bool blocked) {
  if (!render_frame_host->GetPage().IsPrimary() || blocked) {
    return;
  }

  detector_.OnClientSiteDataAccessed(render_frame_host->GetLastCommittedURL(),
                                     CookieOperation::kChange);
}

void RedirectChainDetector::PrimaryPageChanged(content::Page& page) {
  PrimaryPageMarker::CreateForPage(page);
}

namespace dips {

bool IsOrWasInPrimaryPage(content::RenderFrameHost* render_frame_host) {
  return IsInPrimaryPage(render_frame_host) ||
         PrimaryPageMarker::GetForPage(render_frame_host->GetPage());
}

}  // namespace dips

void RedirectChainDetector::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Discard all notifications that are:
  // - From other page types like FencedFrames and Prerendered.
  // - Blocked by policies.
  if (details.blocked_by_policy ||
      !dips::IsOrWasInPrimaryPage(render_frame_host)) {
    return;
  }

  // We might be called for "late" server cookie accesses, not just client
  // cookies. Before completing other checks, attempt to attribute the
  // cookie access to the current redirect chain to handle that case.
  //
  // TODO(rtarpine): Is it possible for cookie accesses to be reported late
  // for uncommitted navigations?
  if (delayed_handler_.AddLateCookieAccess(details.url, details.type) ||
      detector_.AddLateCookieAccess(details.url, details.type)) {
    return;
  }

  // Otherwise, attribute the client cookie access to the first party site of
  // the RFH.
  const std::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (!fpu.has_value()) {
    return;
  }
  if (!HasCHIPS(details.cookie_access_result_list) &&
      !IsSameSiteForDIPS(fpu.value(), details.url)) {
    return;
  }

  detector_.OnClientSiteDataAccessed(fpu.value(), details.type);
}

void DIPSWebContentsObserver::OnSiteStorageAccessed(const GURL& first_party_url,
                                                    CookieOperation op,
                                                    bool http_cookie) {
  base::Time now = clock_->Now();

  if (!http_cookie) {
    // Throttle client-side storage timestamp updates.
    if (!UpdateTimestamp(last_storage_timestamp_, now)) {
      return;
    }
  }

  RecordEvent(DIPSRecordedEvent::kStorage, first_party_url, now);
}

void RedirectChainDetector::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  // Discard all notifications that are:
  // - From other page types like FencedFrames and Prerendered.
  // - Blocked by policies.
  if (!IsInPrimaryPage(navigation_handle) || details.blocked_by_policy) {
    return;
  }

  // All accesses within the primary page iframes are attributed to the URL of
  // the main frame (ie the first party URL).
  if (IsInPrimaryPageIFrame(navigation_handle)) {
    const std::optional<GURL> fpu = GetFirstPartyURL(navigation_handle);
    if (!fpu.has_value()) {
      return;
    }

    if (!HasCHIPS(details.cookie_access_result_list) &&
        !IsSameSiteForDIPS(fpu.value(), details.url)) {
      return;
    }

    detector_.OnClientSiteDataAccessed(fpu.value(), details.type);
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.OnServerCookiesAccessed(&dips_handle, details.url, details.type);
}

void DIPSBounceDetector::OnClientSiteDataAccessed(const GURL& url,
                                                  CookieOperation op) {
  auto now = clock_->Now();

  if (client_detection_state_ &&
      GetSiteForDIPS(url) == client_detection_state_->current_site) {
    client_detection_state_->site_data_access_type =
        client_detection_state_->site_data_access_type |
        ToSiteDataAccessType(op);

    if (op == CookieOperation::kChange) {
      client_detection_state_->last_storage_time = now;
    }
  }

  if (op == CookieOperation::kChange) {
    delegate_->OnSiteStorageAccessed(url, op, /*http_cookie=*/false);
  }
}

void DIPSBounceDetector::OnServerCookiesAccessed(
    DIPSNavigationHandle* navigation_handle,
    const GURL& url,
    CookieOperation op) {
  if (op == CookieOperation::kChange) {
    delegate_->OnSiteStorageAccessed(url, op, /*http_cookie=*/true);
  }

  if (navigation_handle) {
    ServerBounceDetectionState* state = navigation_handle->GetServerState();
    if (state) {
      state->filter.AddAccess(url, op);
    }
  }
}

void RedirectChainDetector::OnSiteStorageAccessed(const GURL& first_party_url,
                                                  CookieOperation op,
                                                  bool http_cookie) {
  for (auto& observer : observers_) {
    observer.OnSiteStorageAccessed(first_party_url, op, http_cookie);
  }
}

void DIPSWebContentsObserver::OnServiceWorkerAccessed(
    content::RenderFrameHost* render_frame_host,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  if (!IsInPrimaryPage(render_frame_host) || !allowed) {
    return;
  }

  const std::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    // TODO: crbug.com/324585403 - This is not observed by RedirectChainDetector
    // and so doesn't influence whether a bounce is stateful or not. Should it?
    RecordEvent(DIPSRecordedEvent::kStorage, fpu.value(), clock_->Now());
  }
}

void DIPSWebContentsObserver::OnServiceWorkerAccessed(
    content::NavigationHandle* navigation_handle,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  if (!IsInPrimaryPage(navigation_handle) || !allowed) {
    return;
  }

  const std::optional<GURL> fpu = GetFirstPartyURL(navigation_handle);
  if (!fpu.has_value()) {
    return;
  }

  // TODO: crbug.com/324585403 - This is not observed by RedirectChainDetector
  // and so doesn't influence whether a bounce is stateful or not. Should it?
  RecordEvent(DIPSRecordedEvent::kStorage, fpu.value(), clock_->Now());
}

void DIPSWebContentsObserver::OnClientAdded(
    const blink::SharedWorkerToken& token,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);

  if (!IsInPrimaryPage(render_frame_host)) {
    return;
  }

  const std::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    // TODO: crbug.com/324585403 - This is not observed by RedirectChainDetector
    // and so doesn't influence whether a bounce is stateful or not. Should it?
    RecordEvent(DIPSRecordedEvent::kStorage, fpu.value(), clock_->Now());
  }
}

void DIPSWebContentsObserver::OnWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    content::DedicatedWorkerCreator creator) {
  const content::GlobalRenderFrameHostId* const render_frame_host_id =
      absl::get_if<content::GlobalRenderFrameHostId>(&creator);
  if (!render_frame_host_id) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(*render_frame_host_id);

  if (!IsInPrimaryPage(render_frame_host)) {
    return;
  }

  const std::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    RecordEvent(DIPSRecordedEvent::kStorage, fpu.value(), clock_->Now());
  }
}

void DIPSWebContentsObserver::PrimaryPageChanged(content::Page& page) {
  if (last_committed_site_.has_value()) {
    dips_service_->RemoveOpenSite(last_committed_site_.value());
  }
  last_committed_site_ = GetSiteForDIPS(web_contents()->GetLastCommittedURL());
  dips_service_->AddOpenSite(last_committed_site_.value());

  last_storage_timestamp_.reset();
  last_interaction_timestamp_.reset();
}

void RedirectChainDetector::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.DidFinishNavigation(&dips_handle);

  if (navigation_handle->HasCommitted()) {
    for (auto& observer : observers_) {
      observer.OnNavigationCommitted();
    }
  }
}

void DIPSBounceDetector::DidFinishNavigation(
    DIPSNavigationHandle* navigation_handle) {
  base::TimeTicks now = tick_clock_->NowTicks();

  bool current_page_has_sticky_activation =
      client_detection_state_ &&
      client_detection_state_->last_activation_time.has_value();

  // Starts the timer to detect further client redirects.
  client_bounce_detection_timer_.Reset();

  // Before we replace the ClientBounceDetectionState, keep a copy of whether
  // the previous page accessed storage or not.
  std::optional<SiteDataAccessType> prev_page_access_type;
  if (client_detection_state_) {
    prev_page_access_type = client_detection_state_->site_data_access_type;
  }

  if (navigation_handle->HasCommitted()) {
    // Iff the primary page changed, reset the client detection state while
    // storing the page load time and previous_url. A primary page change is
    // verified by checking IsInPrimaryMainFrame, !IsSameDocument, and
    // HasCommitted. HasCommitted is the only one not previously checked here.
    client_detection_state_ = ClientBounceDetectionState(
        navigation_handle->GetPreviousPrimaryMainFrameURL(),
        GetSiteForDIPS(navigation_handle->GetURL()), now);
  }

  ServerBounceDetectionState* server_state =
      navigation_handle->GetServerState();

  if (!server_state) {
    return;
  }

  if (DIPSRedirectInfoPtr* client_redirect =
          absl::get_if<DIPSRedirectInfoPtr>(&server_state->navigation_start)) {
    if (prev_page_access_type.has_value()) {
      // In case there were any late storage notifications, update the client
      // redirect info.
      (*client_redirect)->access_type = prev_page_access_type.value();
    }
  }

  std::vector<DIPSRedirectInfoPtr> redirects;
  std::vector<SiteDataAccessType> access_types;
  server_state->filter.Filter(navigation_handle->GetRedirectChain(),
                              &access_types);

  // The length of the redirect chain should be equal to the number of server
  // redirects observed by the `DidRedirectNavigation` handler (plus one
  // additional element for the destination URL). The exception to this is in
  // the case a URL client-redirects to itself. Then the redirect chain will
  // have a duplicate entry prepended to itself (but there will be no server
  // redirect).
  //
  // See http://crbug.com/371004127 for more context.
  CHECK_GT(access_types.size(), server_state->server_redirects.size());
  // Use the length difference between the redirect chain and number of server
  // redirects to calculate an offset. When creating instances of
  // `DIPSRedirectInfo`, we avoid earlier entries within the redirect chain.
  size_t offset =
      access_types.size() - (server_state->server_redirects.size() + 1);
  for (size_t i = 0; i < server_state->server_redirects.size(); i++) {
    // The next item in the redirect chain should be equal to the destination
    // URL recorded by the corresponding redirect navigation.
    DCHECK_EQ(
        navigation_handle->GetRedirectChain()[i + offset + 1].host_piece(),
        server_state->server_redirects[i].destination_url.host_piece());
    DCHECK_EQ(
        navigation_handle->GetRedirectChain()[i + offset + 1].path_piece(),
        server_state->server_redirects[i].destination_url.path_piece());
    redirects.push_back(std::make_unique<DIPSRedirectInfo>(
        /*url=*/UrlAndSourceId(
            navigation_handle->GetRedirectChain()[i + offset],
            navigation_handle->GetRedirectSourceId(i + offset)),
        /*redirect_type=*/DIPSRedirectType::kServer,
        /*access_type=*/access_types[i + offset],
        /*time=*/clock_->Now(),
        /*was_response_cached=*/
        server_state->server_redirects[i].was_response_cached,
        /*response_code=*/server_state->server_redirects[i].http_response_code,
        /*server_bounce_delay=*/
        server_state->server_redirects[i].bounce_delay));
  }

  if (navigation_handle->HasCommitted()) {
    committed_redirect_context_.AppendCommitted(
        std::move(server_state->navigation_start), std::move(redirects),
        UrlAndSourceId(navigation_handle->GetURL(),
                       navigation_handle->GetNextPageUkmSourceId()),
        current_page_has_sticky_activation);
  } else {
    // For uncommitted navigations, treat the last URL visited as a server
    // redirect, so it is considered a potential tracker.
    const size_t i = access_types.size() - 1;
    redirects.push_back(std::make_unique<DIPSRedirectInfo>(
        /*url=*/UrlAndSourceId(navigation_handle->GetRedirectChain()[i],
                               navigation_handle->GetRedirectSourceId(i)),
        /*redirect_type=*/DIPSRedirectType::kServer,
        /*access_type=*/access_types[i],
        /*time=*/clock_->Now(),
        /*was_response_cached=*/navigation_handle->WasResponseCached(),
        /*response_code=*/navigation_handle->GetHTTPResponseCode(),
        /*server_bounce_delay=*/now - server_state->last_server_redirect));
    committed_redirect_context_.HandleUncommitted(
        std::move(server_state->navigation_start), std::move(redirects));
  }

  if (navigation_handle->HasCommitted()) {
    // The last entry in navigation_handle->GetRedirectChain() is actually the
    // page being committed (i.e., not a redirect). If its HTTP request or
    // response accessed cookies, record this in our client detection state.
    client_detection_state_->site_data_access_type = access_types.back();
  }
}

// TODO(kaklilu): Follow up on how this interacts with Fenced Frames.
void RedirectChainDetector::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  // Ignore iframe activations since we only care for its associated main-frame
  // interactions on the top-level site.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  detector_.OnUserActivation();
}

void DIPSBounceDetector::OnUserActivation() {
  GURL url = delegate_->GetLastCommittedURL().url;
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (client_detection_state_.has_value()) {
    client_detection_state_->last_activation_time = clock_->Now();
  }
}

void DIPSWebContentsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  GURL url = render_frame_host->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  base::Time now = clock_->Now();
  // To decrease the number of writes made to the database, after a user
  // activation event on the page, new activation events will not be recorded
  // for the next |kTimestampUpdateInterval|.
  if (!UpdateTimestamp(last_interaction_timestamp_, now)) {
    return;
  }
  RecordEvent(DIPSRecordedEvent::kInteraction, url, now);
}

void DIPSBounceDetector::WebAuthnAssertionRequestSucceeded() {
  if (client_detection_state_.has_value()) {
    client_detection_state_->last_successful_web_authn_assertion_time =
        clock_->Now();
  }
}

void RedirectChainDetector::WebAuthnAssertionRequestSucceeded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  detector_.WebAuthnAssertionRequestSucceeded();
}

void DIPSWebContentsObserver::WebAuthnAssertionRequestSucceeded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  RecordEvent(DIPSRecordedEvent::kWebAuthnAssertion,
              render_frame_host->GetLastCommittedURL(), clock_->Now());
}

void DIPSWebContentsObserver::WebContentsDestroyed() {
  if (last_committed_site_.has_value()) {
    dips_service_->RemoveOpenSite(last_committed_site_.value());
  }
  detector_ = nullptr;  // was observing the same WebContents.
}

void RedirectChainDetector::WebContentsDestroyed() {
  detector_.BeforeDestruction();
  delayed_handler_.HandlePreviousChainNow();
}

void DIPSBounceDetector::BeforeDestruction() {
  committed_redirect_context_.EndChain(
      delegate_->GetLastCommittedURL(),
      /*current_page_has_sticky_activation=*/false);
}

void DIPSBounceDetector::OnClientBounceDetectionTimeout() {
  bool current_page_has_sticky_activation =
      client_detection_state_ &&
      client_detection_state_->last_activation_time.has_value();
  committed_redirect_context_.EndChain(delegate_->GetLastCommittedURL(),
                                       current_page_has_sticky_activation);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RedirectChainDetector);
WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSWebContentsObserver);

namespace dips {

ukm::SourceId GetInitialRedirectSourceId(
    content::NavigationHandle* navigation_handle) {
  DIPSNavigationHandleImpl handle(navigation_handle);
  return handle.GetRedirectSourceId(0);
}

}  // namespace dips

void RedirectChainDetector::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RedirectChainDetector::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

DelayedChainHandler::DelayedChainHandler(DIPSRedirectChainHandler handler)
    : handler_(handler),
      timer_(
          FROM_HERE,
          base::Seconds(1),
          base::BindRepeating(&DelayedChainHandler::HandlePreviousChainNowImpl,
                              base::Unretained(this),
                              /*timer_fired=*/true)) {
  CHECK(!timer_.IsRunning());
  CHECK(!prev_chain_pair_.has_value());
}

DelayedChainHandler::~DelayedChainHandler() = default;

void DelayedChainHandler::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  HandlePreviousChainNow();

  prev_chain_pair_ = std::make_pair(std::move(redirects), std::move(chain));
  timer_.Reset();
}

bool DelayedChainHandler::AddLateCookieAccess(const GURL& url,
                                              CookieOperation op) {
  if (!prev_chain_pair_.has_value()) {
    return false;
  }

  return ::AddLateCookieAccess(url, op, prev_chain_pair_->first);
}

void DelayedChainHandler::HandlePreviousChainNowImpl(bool timer_fired) {
  if (timer_fired) {
    CHECK(!timer_.IsRunning());
  }
  // If `prev_chain_pair_` has a value, then either the timer is currently
  // running or it just fired. If `prev_chain_pair_` doesn't have a value,
  // then the timer is not running nor did it just fire.
  CHECK_EQ(prev_chain_pair_.has_value(), timer_.IsRunning() ^ timer_fired);

  if (!prev_chain_pair_.has_value()) {
    return;
  }

  timer_.Stop();
  auto [prev_redirects, prev_chain] = std::move(prev_chain_pair_.value());
  prev_chain_pair_.reset();
  handler_.Run(std::move(prev_redirects), std::move(prev_chain));
}
