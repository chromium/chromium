// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <cmath>
#include <cstddef>
#include <ctime>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
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
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
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
    redirectors_.insert(client_redirect->site);
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
      redirectors_.insert(redirect->site);
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

absl::optional<std::pair<size_t, DIPSRedirectInfo*>>
DIPSRedirectContext::GetRedirectInfoFromChain(const std::string& site) const {
  // Iterate in reverse order to obtain the most recent occurrence of the site.
  for (int ind = redirects_.size() - 1; ind >= 0; ind--) {
    if (redirects_.at(ind)->site == site) {
      return std::make_pair(static_cast<size_t>(ind), redirects_.at(ind).get());
    }
  }
  return absl::nullopt;
}

bool DIPSRedirectContext::SiteHadUserActivation(const std::string& site) const {
  if (initial_url_had_user_activation_ &&
      site == GetSiteForDIPS(initial_url_)) {
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
    sites.insert(GetSiteForDIPS(initial_url_));
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
    absl::optional<std::set<std::string>> allowed_sites) const {
  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction;

  std::set<std::string> sites_with_user_activation =
      AllSitesWithUserActivation();

  const std::string& first_party_site = GetSiteForDIPS(first_party_url);
  for (const auto& redirect : redirects_) {
    const GURL& url = redirect->url;
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
    if (tpcd::experiment::kTpcdRedirectHeuristicRequireCurrentInteraction
            .Get() &&
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
            temp_context.EndChain(std::move(final_url),
                                  /*current_page_has_sticky_activation=*/false);
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
            temp_context.EndChain(std::move(final_url),
                                  /*current_page_has_sticky_activation=*/false);
          },
      },
      std::move(navigation_start));
}

void DIPSRedirectContext::AppendCommitted(
    DIPSNavigationStart navigation_start,
    std::vector<DIPSRedirectInfoPtr> server_redirects,
    const GURL& final_url,
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
              GURL previous_nav_last_committed_url) {
            // The committed navigation began *without* a client redirect, so
            // end the in-progress redirect chain and start a new one.
            EndChain(previous_nav_last_committed_url,
                     current_page_has_sticky_activation);
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

void DIPSRedirectContext::EndChain(GURL final_url,
                                   bool current_page_has_sticky_activation) {
  if (!initial_url_.is_empty()) {
    auto chain = std::make_unique<DIPSRedirectChainInfo>(
        initial_url_, final_url, GetRedirectChainLength(),
        /*is_partial_chain=*/false);
    handler_.Run(std::move(redirects_), std::move(chain));
  }

  initial_url_had_user_activation_ = current_page_has_sticky_activation;
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
      ->AsyncCall(&DIPSStorage::FilterSitesWithoutInteractionOrWaa)
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
  // We need to pass in a WeakPtr to DIPSWebContentsObserver as it's not
  // guaranteed to outlive the call.
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

  const GURL GetInitiator() const override {
    return (!handle_->GetInitiatorOrigin().has_value() ||
            handle_->GetInitiatorOrigin().value().opaque())
               ? GURL("about:blank")
               : handle_->GetInitiatorOrigin().value().GetURL();
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
        delegate_->GetLastCommittedURL().is_empty()
            ? navigation_handle->GetInitiator()
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
          /*source_id=*/
          delegate_->GetPageUkmSourceId(),
          /*time=*/clock_->Now(),
          /*client_bounce_delay=*/client_bounce_delay,
          /*has_sticky_activation=*/
          client_detection_state_->last_activation_time.has_value(),
          /*web_authn_assertion_request_succeeded=*/
          client_detection_state_->last_successful_web_authn_assertion_time
              .has_value());
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

  if (!access_details.is_from_primary_page ||
      access_details.blocked_by_policy) {
    return;
  }

  detector_.OnClientSiteDataAccessed(
      access_details.url, ToCookieOperation(access_details.access_type));
}

void DIPSWebContentsObserver::OnStatefulBounceDetected() {}

bool HasCHIPS(const net::CookieList& cookie_list) {
  for (const auto& cookie : cookie_list) {
    if (cookie.IsPartitioned()) {
      return true;
    }
  }
  return false;
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // Record a RedirectHeuristic UKM event if applicable. We cannot record it
    // while prerendering due to our data collection policy.
    MaybeRecordRedirectHeuristic(render_frame_host->GetPageUkmSourceId(),
                                 details);
  }

  // Discard all notifications that are:
  // - From other page types like FencedFrames and Prerendered.
  // - Blocked by policies.
  if (!IsInPrimaryPage(render_frame_host) || details.blocked_by_policy) {
    return;
  }

  // We might be called for "late" server cookie accesses, not just client
  // cookies. Before completing other checks, attempt to attribute the cookie
  // access to the current redirect chain to handle that case.
  //
  // TODO(rtarpine): Is it possible for cookie accesses to be reported late for
  // uncommitted navigations?
  if (detector_.CommittedRedirectContext().AddLateCookieAccess(details.url,
                                                               details.type)) {
    detector_.OnServerCookiesAccessed(/*navigation_handle=*/nullptr,
                                      details.url, details.type);
    return;
  }

  // Otherwise, attribute the client cookie access to the first party site of
  // the RFH.
  const absl::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (!fpu.has_value()) {
    return;
  }
  if (!HasCHIPS(details.cookie_list) &&
      !IsSameSiteForDIPS(fpu.value(), details.url)) {
    return;
  }

  detector_.OnClientSiteDataAccessed(fpu.value(), details.type);
}

void DIPSWebContentsObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  // Record a RedirectHeuristic UKM event if applicable.
  MaybeRecordRedirectHeuristic(navigation_handle->GetNextPageUkmSourceId(),
                               details);

  // Discard all notifications that are:
  // - From other page types like FencedFrames and Prerendered.
  // - Blocked by policies.
  if (!IsInPrimaryPage(navigation_handle) || details.blocked_by_policy) {
    return;
  }

  // All accesses within the primary page iframes are attributed to the URL of
  // the main frame (ie the first party URL).
  if (IsInPrimaryPageIFrame(navigation_handle)) {
    const absl::optional<GURL> fpu = GetFirstPartyURL(navigation_handle);
    if (!fpu.has_value()) {
      return;
    }

    if (!HasCHIPS(details.cookie_list) &&
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

void DIPSBounceDetector::OnServerCookiesAccessed(
    DIPSNavigationHandle* navigation_handle,
    const GURL& url,
    CookieOperation op) {
  if (op == CookieOperation::kChange) {
    delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, clock_->Now());
  }
  if (navigation_handle) {
    ServerBounceDetectionState* state = navigation_handle->GetServerState();
    if (state) {
      state->filter.AddAccess(url, op);
    }
  }
}

void DIPSWebContentsObserver::MaybeRecordRedirectHeuristic(
    const ukm::SourceId& first_party_source_id,
    const content::CookieAccessDetails& details) {
  const std::string first_party_site = GetSiteForDIPS(details.first_party_url);
  const std::string third_party_site = GetSiteForDIPS(details.url);
  if (first_party_site == third_party_site) {
    // The redirect heuristic does not apply for first-party cookie access.
    return;
  }

  auto third_party_site_info =
      detector_.CommittedRedirectContext().GetRedirectInfoFromChain(
          third_party_site);
  if (!third_party_site_info.has_value()) {
    // The redirect heuristic doesn't apply if the third party is not in the
    // current redirect chain.
    return;
  }
  size_t third_party_site_index = third_party_site_info->first;
  ukm::SourceId third_party_source_id =
      third_party_site_info->second->source_id;
  bool is_current_interaction =
      detector_.CommittedRedirectContext().SiteHadUserActivation(
          third_party_site);

  auto first_party_site_info =
      detector_.CommittedRedirectContext().GetRedirectInfoFromChain(
          first_party_site);
  size_t first_party_site_index;
  if (!first_party_site_info.has_value() ||
      first_party_site_info->first < third_party_site_index) {
    // If the first-party site does not appear in the list of committed
    // redirects after the third-party site, that must mean it's in an ongoing
    // navigation.
    first_party_site_index = detector_.CommittedRedirectContext().size();
  } else {
    first_party_site_index = first_party_site_info->first;
  }
  const size_t sites_passed_count =
      first_party_site_index - third_party_site_index;

  dips_service_->storage()
      ->AsyncCall(&DIPSStorage::LastInteractionTime)
      .WithArgs(details.url)
      .Then(base::BindOnce(&DIPSWebContentsObserver::RecordRedirectHeuristic,
                           weak_factory_.GetWeakPtr(), first_party_source_id,
                           third_party_source_id, details, sites_passed_count,
                           is_current_interaction));
}

void DIPSWebContentsObserver::RecordRedirectHeuristic(
    const ukm::SourceId& first_party_source_id,
    const ukm::SourceId& third_party_source_id,
    const content::CookieAccessDetails& details,
    const size_t sites_passed_count,
    bool is_current_interaction,
    absl::optional<base::Time> last_user_interaction_time) {
  // This function can only be reached if the redirect heuristic is satisfied
  // for the previous recorded redirect.
  DCHECK(last_commit_timestamp_.has_value());
  int milliseconds_since_redirect = Bucketize3PCDHeuristicTimeDelta(
      detector_.GetClock()->Now() - last_commit_timestamp_.value(),
      base::Minutes(15), base::BindRepeating(&base::TimeDelta::InMilliseconds));

  int hours_since_last_interaction = -1;
  if (last_user_interaction_time.has_value()) {
    hours_since_last_interaction = Bucketize3PCDHeuristicTimeDelta(
        detector_.GetClock()->Now() - last_user_interaction_time.value(),
        base::Days(60),
        base::BindRepeating(&base::TimeDelta::InHours)
            .Then(base::BindRepeating([](int64_t t) { return t; })));
  }

  OptionalBool has_same_site_iframe = ToOptionalBool(
      HasSameSiteIframe(WebContentsObserver::web_contents(), details.url));

  const bool first_party_precedes_third_party =
      AllSitesFollowingFirstParty(details.first_party_url)
          .contains(GetSiteForDIPS(details.url));

  int32_t access_id = base::RandUint64();

  ukm::builders::RedirectHeuristic_CookieAccess(first_party_source_id)
      .SetAccessId(access_id)
      .SetAccessAllowed(!details.blocked_by_policy)
      .SetHoursSinceLastInteraction(hours_since_last_interaction)
      .SetMillisecondsSinceRedirect(milliseconds_since_redirect)
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_same_site_iframe))
      .SetSitesPassedCount(sites_passed_count)
      .SetDoesFirstPartyPrecedeThirdParty(first_party_precedes_third_party)
      .SetIsCurrentInteraction(is_current_interaction)
      .Record(ukm::UkmRecorder::Get());

  ukm::builders::RedirectHeuristic_CookieAccessThirdParty(third_party_source_id)
      .SetAccessId(access_id)
      .Record(ukm::UkmRecorder::Get());
}

void DIPSWebContentsObserver::CreateAllRedirectHeuristicGrants(
    const GURL& first_party_url) {
  base::TimeDelta grant_duration =
      tpcd::experiment::kTpcdWriteRedirectHeuristicGrants.Get();
  if (!grant_duration.is_positive()) {
    return;
  }

  absl::optional<std::set<std::string>> sites_with_aba_flow = absl::nullopt;
  if (tpcd::experiment::kTpcdRedirectHeuristicRequireABAFlow.Get()) {
    sites_with_aba_flow = AllSitesFollowingFirstParty(first_party_url);
  }

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction =
          detector_.CommittedRedirectContext().GetRedirectHeuristicURLs(
              first_party_url, sites_with_aba_flow);

  for (const auto& kv : sites_to_url_and_current_interaction) {
    auto [url, is_current_interaction] = kv.second;
    // If there was a current interaction, there is no need to call DIPSStorage
    // to check the db for a past interaction.
    if (is_current_interaction) {
      CreateRedirectHeuristicGrant(url, first_party_url, grant_duration,
                                   /*has_interaction=*/true);
    } else {
      // If `kTpcdRedirectHeuristicRequireCurrentInteraction` is true, then
      // `GetRedirectHeuristicURLs` should not return any entries without a
      // current interaction.
      DCHECK(!tpcd::experiment::kTpcdRedirectHeuristicRequireCurrentInteraction
                  .Get());
      base::OnceCallback<void(absl::optional<base::Time>)> create_grant =
          base::BindOnce(
              [](absl::optional<base::Time> last_user_interaction_time) {
                return last_user_interaction_time.has_value();
              })
              .Then(base::BindOnce(
                  &DIPSWebContentsObserver::CreateRedirectHeuristicGrant,
                  weak_factory_.GetWeakPtr(), url, first_party_url,
                  grant_duration));

      dips_service_->storage()
          ->AsyncCall(&DIPSStorage::LastInteractionTime)
          .WithArgs(url)
          .Then(std::move(create_grant));
    }
  }
}

void DIPSWebContentsObserver::CreateRedirectHeuristicGrant(
    const GURL& url,
    const GURL& first_party_url,
    base::TimeDelta grant_duration,
    bool has_interaction) {
  if (has_interaction) {
    // TODO(crbug.com/1484324): Make these grants lossy to avoid spamming
    // profile prefs with blocking requests.
    // TODO(crbug.com/1484324): Add bounds to these grants to avoid overflow.
    // TODO(crbug.com/1484324): Consider applying these grants only to rSA
    // calls.
    cookie_settings_->SetTemporaryCookieGrantForHeuristic(url, first_party_url,
                                                          grant_duration);
  }
}

void DIPSWebContentsObserver::OnServiceWorkerAccessed(
    content::RenderFrameHost* render_frame_host,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  if (!IsInPrimaryPage(render_frame_host) || !allowed) {
    return;
  }

  const absl::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    detector_.OnWorkerInitialized(fpu.value());
  }
}

void DIPSWebContentsObserver::OnServiceWorkerAccessed(
    content::NavigationHandle* navigation_handle,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  if (!IsInPrimaryPage(navigation_handle) || !allowed) {
    return;
  }

  const absl::optional<GURL> fpu = GetFirstPartyURL(navigation_handle);
  if (!fpu.has_value()) {
    return;
  }

  detector_.OnWorkerInitialized(fpu.value());
}

void DIPSWebContentsObserver::OnClientAdded(
    const blink::SharedWorkerToken& token,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);

  if (!IsInPrimaryPage(render_frame_host)) {
    return;
  }

  const absl::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    detector_.OnWorkerInitialized(fpu.value());
  }
}

void DIPSWebContentsObserver::OnWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
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

  const absl::optional<GURL> fpu = GetFirstPartyURL(render_frame_host);
  if (fpu.has_value()) {
    detector_.OnWorkerInitialized(fpu.value());
  }
}

void DIPSBounceDetector::OnWorkerInitialized(const GURL& url) {
  delegate_->RecordEvent(DIPSRecordedEvent::kStorage, url, clock_->Now());
}

void DIPSWebContentsObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_handle->HasCommitted()) {
    if (last_committed_site_.has_value()) {
      dips_service_->RemoveOpenSite(last_committed_site_.value());
    }
    last_committed_site_ = GetSiteForDIPS(navigation_handle->GetURL());
    last_commit_timestamp_ = detector_.GetClock()->Now();
    dips_service_->AddOpenSite(last_committed_site_.value());
  }

  DIPSNavigationHandleImpl dips_handle(navigation_handle);
  detector_.DidFinishNavigation(&dips_handle);

  if (navigation_handle->HasCommitted()) {
    // Use the redirects just added to the DIPSRedirectContext in order to
    // create new storage access grants when the Redirect heuristic applies.
    CreateAllRedirectHeuristicGrants(navigation_handle->GetURL());
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
        navigation_handle->GetURL(), current_page_has_sticky_activation);
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

void DIPSBounceDetector::WebAuthnAssertionRequestSucceeded() {
  GURL url = delegate_->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  base::Time now = clock_->Now();
  if (client_detection_state_.has_value()) {
    // To decrease the number of writes made to the database, after a user web
    // authn assertion happens, subsequent events will not be recorded for the
    // next `kTimestampUpdateInterval`.
    if (!ShouldUpdateTimestamp(
            client_detection_state_->last_successful_web_authn_assertion_time,
            now)) {
      return;
    }
    client_detection_state_->last_successful_web_authn_assertion_time = now;
  }

  delegate_->RecordEvent(DIPSRecordedEvent::kWebAuthnAssertion, url, now);
}

void DIPSWebContentsObserver::WebAuthnAssertionRequestSucceeded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  detector_.WebAuthnAssertionRequestSucceeded();
}

bool DIPSBounceDetector::ShouldUpdateTimestamp(
    base::optional_ref<const base::Time> last_time,
    base::Time now) {
  return (!last_time.has_value() ||
          (now - last_time.value()) >= kTimestampUpdateInterval);
}

std::set<std::string> DIPSWebContentsObserver::AllSitesFollowingFirstParty(
    const GURL& first_party_url) {
  std::set<std::string> sites;

  content::NavigationController& nav_controller =
      WebContentsObserver::web_contents()->GetController();
  const std::string first_party_site = GetSiteForDIPS(first_party_url);

  int min_index = std::max(0, nav_controller.GetCurrentEntryIndex() -
                                  kAllSitesFollowingFirstPartyLookbackLength);
  absl::optional<std::string> prev_site = absl::nullopt;
  for (int ind = nav_controller.GetCurrentEntryIndex(); ind >= min_index;
       ind--) {
    std::string cur_site =
        GetSiteForDIPS(nav_controller.GetEntryAtIndex(ind)->GetURL());

    if (cur_site == first_party_site) {
      if (prev_site.has_value() && *prev_site != first_party_site) {
        sites.insert(*prev_site);
      }
    }

    prev_site = cur_site;
  }

  return sites;
}

void DIPSWebContentsObserver::WebContentsDestroyed() {
  if (last_committed_site_.has_value()) {
    dips_service_->RemoveOpenSite(last_committed_site_.value());
  }

  detector_.BeforeDestruction();
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSWebContentsObserver);

ukm::SourceId GetInitialRedirectSourceId(
    content::NavigationHandle* navigation_handle) {
  DIPSNavigationHandleImpl handle(navigation_handle);
  return handle.GetRedirectSourceId(0);
}
