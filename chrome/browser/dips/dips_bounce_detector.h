// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include <memory>
#include <string>
#include <variant>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

using DIPSIssueHandler =
    base::RepeatingCallback<void(std::set<std::string> sites)>;
using DIPSIssueReportingCallback =
    base::RepeatingCallback<void(const std::set<std::string>& sites)>;

// ClientBounceDetectionState is owned by the DIPSBounceDetector and stores
// data needed to detect stateful client-side redirects.
class ClientBounceDetectionState {
 public:
  ClientBounceDetectionState(GURL url,
                             std::string site,
                             base::TimeTicks load_time);
  ClientBounceDetectionState(const ClientBounceDetectionState& other);
  ~ClientBounceDetectionState();

  // The NavigationHandle's previously committed URL at the time the navigation
  // finishes and commits.
  GURL previous_url;
  std::string current_site;
  base::TimeTicks page_load_time;
  std::optional<base::Time> last_activation_time;
  std::optional<base::Time> last_storage_time;
  std::optional<base::Time> last_successful_web_authn_assertion_time;
  SiteDataAccessType site_data_access_type = SiteDataAccessType::kUnknown;
};

// Either the URL navigated away from (starting a new chain), or the client-side
// redirect connecting the navigation to the currently-committed chain.
// TODO: crbug.com/324573484 - rename to remove association with DIPS.
using DIPSNavigationStart = absl::variant<GURL, DIPSRedirectInfoPtr>;

// In case of a client-side redirect loop, we need to impose a limit on the
// stored redirect chain to avoid boundless memory use. Past this limit,
// redirects are trimmed from the front of the list.
constexpr size_t kDIPSRedirectChainMax = 1000;

// When checking the history of the current tab for sites following the
// first-party site, this is the maximum number of navigation entries to check.
inline constexpr int kAllSitesFollowingFirstPartyLookbackLength = 10;

// A redirect-chain-in-progress. It grows by calls to Append() and restarts by
// calls to EndChain().
// TODO: crbug.com/324573484 - rename to remove association with DIPS.
class DIPSRedirectContext {
 public:
  DIPSRedirectContext(DIPSRedirectChainHandler handler,
                      DIPSIssueHandler issue_handler,
                      const GURL& initial_url,
                      size_t redirect_prefix_count);
  ~DIPSRedirectContext();

  // Immediately calls the `DIPSRedirectChainHandler` for the uncommitted
  // navigation. It will take into account the length and initial URL of the
  // current chain (without modifying it).
  void HandleUncommitted(DIPSNavigationStart navigation_start,
                         std::vector<DIPSRedirectInfoPtr> server_redirects,
                         GURL final_url);

  // Either calls for termination of the in-progress redirect chain, with a
  // start of a new one, or extends it, according to the value of
  // `navigation_start`.
  void AppendCommitted(DIPSNavigationStart navigation_start,
                       std::vector<DIPSRedirectInfoPtr> server_redirects,
                       const GURL& final_url,
                       bool current_page_has_sticky_activation);

  // Trims |trim_count| redirect from the front of the in-progress redirect
  // chain. Passes the redirects as partial chains to the
  // `DIPSRedirectChainHandler`.
  void TrimAndHandleRedirects(size_t trim_count);

  // Terminates the in-progress redirect chain, ending it with `final_url`, and
  // parsing it to the `DIPSRedirectChainHandler` iff the chain is valid. It
  // also starts a fresh redirect chain with `final_url` whilst clearing the
  // state of the terminated chain.
  // NOTE: A chain is valid if it has a non-empty `initial_url_`.
  void EndChain(GURL final_url, bool current_page_has_sticky_activation);

  void ReportIssue(const GURL& final_url);

  [[nodiscard]] bool AddLateCookieAccess(GURL url, CookieOperation op);

  size_t size() const { return redirects_.size(); }

  GURL GetInitialURLForTesting() const { return initial_url_; }

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    handler_ = handler;
  }

  size_t GetRedirectChainLength() const {
    return redirects_.size() + redirect_prefix_count_;
  }

  std::optional<std::pair<size_t, DIPSRedirectInfo*>> GetRedirectInfoFromChain(
      const std::string& site) const;

  // Return whether `site` had an interaction in the current redirect context.
  bool SiteHadUserActivation(const std::string& site) const;

  // Return all sites that had an interaction in the current redirect context.
  std::set<std::string> AllSitesWithUserActivation() const;

  // Returns a map of (site, (url, has_current_interaction)) for all URLs in the
  // current redirect chain that satisfy the redirect heuristic. This performs
  // all checks except for the presence of a past interaction, which should be
  // checked by the caller using the DIPS db. If `allowed_sites` is present,
  // only sites in `allowed_sites` should be included.
  std::map<std::string, std::pair<GURL, bool>> GetRedirectHeuristicURLs(
      const GURL& first_party_url,
      std::optional<std::set<std::string>> allowed_sites) const;

 private:
  void AppendClientRedirect(DIPSRedirectInfoPtr client_redirect);
  void AppendServerRedirects(std::vector<DIPSRedirectInfoPtr> server_redirects);
  void TrimRedirectsFromFront();

  DIPSRedirectChainHandler handler_;
  DIPSIssueHandler issue_handler_;
  // Represents the start of a chain and also indicates the presence of a valid
  // chain.
  GURL initial_url_;
  // Whether the initial_url_ had an interaction while loaded.
  bool initial_url_had_user_activation_;
  // TODO(amaliev): Make redirects_ a circular queue to handle the memory bound
  // more gracefully.
  std::vector<DIPSRedirectInfoPtr> redirects_;
  std::set<std::string> redirectors_;
  // The index of the last redirect to have a known cookie access. When adding
  // late cookie accesses, we only consider redirects from this offset onwards.
  size_t update_offset_ = 0;
  // The number of redirects preceding this chain, that should be counted toward
  // this chain's total length. Includes both committed redirects (for an
  // uncommitted chain) and trimmed redirects.
  size_t redirect_prefix_count_ = 0;
};

// A simplified interface to WebContents and DIPSService that can be faked in
// tests. Needed to allow unit testing DIPSBounceDetector.
// TODO: crbug.com/324573484 - rename to remove association with DIPS.
class DIPSBounceDetectorDelegate {
 public:
  virtual ~DIPSBounceDetectorDelegate();
  virtual const GURL& GetLastCommittedURL() const = 0;
  virtual ukm::SourceId GetPageUkmSourceId() const = 0;
  virtual void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                                   DIPSRedirectChainInfoPtr chain) = 0;
  virtual void ReportRedirectors(std::set<std::string> sites) = 0;
  virtual void OnSiteStorageAccessed(const GURL& first_party_url,
                                     CookieOperation op,
                                     bool http_cookie) = 0;
};

// ServerBounceDetectionState gets attached to NavigationHandle (which is a
// SupportsUserData subclass) to store data needed to detect stateful
// server-side redirects.
class ServerBounceDetectionState
    : public content::NavigationHandleUserData<ServerBounceDetectionState> {
 public:
  ServerBounceDetectionState();
  ~ServerBounceDetectionState() override;

  DIPSNavigationStart navigation_start;
  CookieAccessFilter filter;

 private:
  explicit ServerBounceDetectionState(
      content::NavigationHandle& navigation_handle);

  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// A simplified interface to content::NavigationHandle that can be faked in
// tests. Needed to allow unit testing DIPSBounceDetector.
// TODO: crbug.com/324573484 - rename to remove association with DIPS.
class DIPSNavigationHandle {
 public:
  virtual ~DIPSNavigationHandle();

  // See content::NavigationHandle for an explanation of these methods:
  const GURL& GetURL() const { return GetRedirectChain().back(); }
  virtual const GURL& GetPreviousPrimaryMainFrameURL() const = 0;
  virtual bool HasCommitted() const = 0;
  virtual const std::vector<GURL>& GetRedirectChain() const = 0;
  // This method has one important (simplifying) change from
  // content::NavigationHandle::HasUserGesture(): it returns true if the
  // navigation was not renderer-initiated.
  virtual bool HasUserGesture() const = 0;
  //  This method doesn't have a direct equivalent in content::NavigationHandle,
  //  as it relies on GetInitiatorOrigin(), but returns what is effectively a
  //  base URL. Also, this returns `about:blank` if the initiator origin is
  //  unspecified or opaque.
  virtual const GURL GetInitiator() const = 0;

  // Get a SourceId of type REDIRECT_ID for the index'th URL in the redirect
  // chain.
  ukm::SourceId GetRedirectSourceId(int index) const;
  // Calls ServerBounceDetectionState::GetOrCreateForNavigationHandle(). We
  // declare this instead of making DIPSNavigationHandle a subclass of
  // SupportsUserData, because ServerBounceDetectionState inherits from
  // NavigationHandleUserData, whose helper functions only work with actual
  // content::NavigationHandle, not any SupportsUserData.
  virtual ServerBounceDetectionState* GetServerState() = 0;
};

// Detects client/server-side bounces and handles them (currently by collecting
// metrics and storing them in the DIPSDatabase).
// TODO: crbug.com/324573484 - rename this to avoid confusion with
// RedirectChainDetector and remove its association with DIPS.
class DIPSBounceDetector {
 public:
  explicit DIPSBounceDetector(DIPSBounceDetectorDelegate* delegate,
                              const base::TickClock* tick_clock,
                              const base::Clock* clock);
  ~DIPSBounceDetector();
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  const base::Clock* GetClock() { return clock_.get(); }
  // The following methods are based on WebContentsObserver, simplified.
  void DidStartNavigation(DIPSNavigationHandle* navigation_handle);
  void OnClientSiteDataAccessed(const GURL& url, CookieOperation op);
  // Note: `navigation_handle` may be null if this server cookie access is
  // associated with a document rather than a navigation.
  void OnServerCookiesAccessed(DIPSNavigationHandle* navigation_handle,
                               const GURL& url,
                               CookieOperation op);
  void DidFinishNavigation(DIPSNavigationHandle* navigation_handle);
  // Only records a new user activation event once per
  // |kTimestampUpdateInterval| for a given page.
  void OnUserActivation();
  // Only records a new Web authn assertion event once per
  // |kTimestampUpdateInterval| for a given page.
  void WebAuthnAssertionRequestSucceeded();
  // Makes a call to process the current chain before its state is destroyed by
  // the tab closure.
  void BeforeDestruction();
  // Use the passed handler instead of
  // DIPSBounceDetectorDelegate::HandleRedirect().
  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    committed_redirect_context_.SetRedirectChainHandlerForTesting(handler);
  }
  const DIPSRedirectContext& CommittedRedirectContext() const {
    return committed_redirect_context_;
  }

  [[nodiscard]] bool AddLateCookieAccess(GURL url, CookieOperation op) {
    bool was_late = committed_redirect_context_.AddLateCookieAccess(url, op);
    if (was_late) {
      OnServerCookiesAccessed(/*navigation_handle=*/nullptr, url, op);
    }
    return was_late;
  }

  // Makes a call to process the current chain on
  // `client_bounce_detection_timer_`'s timeout.
  void OnClientBounceDetectionTimeout();

 private:
  // Whether or not the `last_time` timestamp should be updated yet. This is
  // used to enforce throttling of timestamp updates, reducing the number of
  // writes to the DIPS db.
  bool ShouldUpdateTimestamp(base::optional_ref<const base::Time> last_time,
                             base::Time now);

  raw_ptr<const base::TickClock> tick_clock_;
  raw_ptr<const base::Clock> clock_;
  raw_ptr<DIPSBounceDetectorDelegate> delegate_;
  std::optional<ClientBounceDetectionState> client_detection_state_;
  DIPSRedirectContext committed_redirect_context_;
  base::RetainingOneShotTimer client_bounce_detection_timer_;
};

// Detects chains of server- and client redirects, and notifies observers.
// TODO: crbug.com/324573485 - move to separate file.
class RedirectChainDetector
    : public content_settings::PageSpecificContentSettings::SiteDataObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<RedirectChainDetector>,
      public DIPSBounceDetectorDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a navigation has committed and the redirect context has been
    // updated. (If you override WebContentsObserver::DidFinishNavigation()
    // directly, you could be called before the context has been updated.)
    virtual void OnNavigationCommitted() {}
    // Called when any redirect chain ends, including ones that end with an
    // uncommitted navigation.
    virtual void OnRedirectChainEnded(const std::vector<DIPSRedirectInfoPtr>&,
                                      const DIPSRedirectChainInfo&) {}
    // Called before OnRedirectChainEnded() with set of redirector sites in the
    // chain, omitting the initial and final sites.
    // TODO(rtarpine) - replace with more general purpose method
    virtual void ReportRedirectors(const std::set<std::string>& sites) {}
    // Called when most types of storage are accessed (including cookies,
    // the Web Storage API, IndexedDB, etc). Does not report 3PCs, and
    // attributes partitioned storage to the top-level URL.
    virtual void OnSiteStorageAccessed(const GURL& first_party_url,
                                       CookieOperation op,
                                       bool http_cookie) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

  ~RedirectChainDetector() override;

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    detector_.SetRedirectChainHandlerForTesting(handler);
  }

  const DIPSRedirectContext& CommittedRedirectContext() const {
    return detector_.CommittedRedirectContext();
  }

  void SetClockForTesting(base::Clock* clock) {
    detector_.SetClockForTesting(clock);
  }

 private:
  explicit RedirectChainDetector(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<RedirectChainDetector>;

  // DIPSBounceDetectorDelegate overrides:
  const GURL& GetLastCommittedURL() const override;
  ukm::SourceId GetPageUkmSourceId() const override;
  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain) override;
  void ReportRedirectors(std::set<std::string> sites) override;
  void OnSiteStorageAccessed(const GURL& first_party_url,
                             CookieOperation op,
                             bool http_cookie) override;
  // End DIPSBounceDetectorDelegate overrides.

  // Start WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  // End WebContentsObserver overrides:

  // Start SiteDataObserver overrides:
  void OnSiteDataAccessed(
      const content_settings::AccessDetails& access_details) override;
  void OnStatefulBounceDetected() override;
  // End SiteDataObserver overrides.

  DIPSBounceDetector detector_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<RedirectChainDetector> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Populates the DIPS Database with site metadata, for the DIPS Service to
// recognize and delete the storage of sites that perform bounce tracking.
class DIPSWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSWebContentsObserver>,
      public content::SharedWorkerService::Observer,
      public content::DedicatedWorkerService::Observer,
      public RedirectChainDetector::Observer {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~DIPSWebContentsObserver() override;

  // Use the passed handler instead of DIPSWebContentsObserver::EmitDIPSIssue().
  void SetIssueReportingCallbackForTesting(
      DIPSIssueReportingCallback callback) {
    issue_reporting_callback_ = callback;
  }

  // TODO(rtarpine): make this take a Clock&.
  void SetClockForTesting(base::Clock* clock) {
    DCHECK(dips_service_);
    dips_service_->storage()
        ->AsyncCall(&DIPSStorage::SetClockForTesting)
        .WithArgs(clock);
    RedirectChainDetector::FromWebContents(web_contents())
        ->SetClockForTesting(clock);
    clock_ = *clock;
  }

  std::set<std::string> AllSitesFollowingFirstPartyForTesting(
      const GURL& first_party_url) {
    return AllSitesFollowingFirstParty(first_party_url);
  }

 private:
  DIPSWebContentsObserver(content::WebContents* web_contents,
                          DIPSService* dips_service);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSWebContentsObserver>;

  void EmitDIPSIssue(const std::set<std::string>& sites);

  // Record a RedirectHeuristic event for a cookie access, if eligible. This
  // applies when the tracking site has appeared previously in the current
  // redirect context.
  void MaybeRecordRedirectHeuristic(
      const ukm::SourceId& first_party_source_id,
      const content::CookieAccessDetails& details);
  void RecordRedirectHeuristic(
      const ukm::SourceId& first_party_source_id,
      const ukm::SourceId& third_party_source_id,
      const content::CookieAccessDetails& details,
      const size_t sites_passed_count,
      bool is_current_interaction,
      std::optional<base::Time> last_user_interaction_time);

  // Create all eligible RedirectHeuristic grants for the current redirect
  // chain. This may create a storage access grant for any site in the redirect
  // chain on the last committed site, if it meets the criteria.
  void CreateAllRedirectHeuristicGrants(const GURL& first_party_url);
  void CreateRedirectHeuristicGrant(const GURL& url,
                                    const GURL& first_party_url,
                                    base::TimeDelta grant_duration,
                                    bool has_interaction);

  void RecordEvent(DIPSRecordedEvent event,
                   const GURL& url,
                   const base::Time& time);
  void IncrementPageSpecificBounceCount(const GURL& final_url);
  std::set<std::string> AllSitesFollowingFirstParty(
      const GURL& first_party_url);

  // Start RedirectChainDetector::Observer overrides:
  void OnNavigationCommitted() override;
  void ReportRedirectors(const std::set<std::string>& sites) override;
  void OnRedirectChainEnded(const std::vector<DIPSRedirectInfoPtr>& redirects,
                            const DIPSRedirectChainInfo& chain) override;
  void OnSiteStorageAccessed(const GURL& first_party_url,
                             CookieOperation op,
                             bool http_cookie) override;
  // End RedirectChainDetector::Observer overrides.

  // Start WebContentsObserver overrides:
  void PrimaryPageChanged(content::Page& page) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnServiceWorkerAccessed(
      content::RenderFrameHost* render_frame_host,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void OnServiceWorkerAccessed(
      content::NavigationHandle* navigation_handle,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  // End WebContentsObserver overrides:

  // Start SharedWorkerService.Observer overrides:
  void OnClientAdded(
      const blink::SharedWorkerToken& token,
      content::GlobalRenderFrameHostId render_frame_host_id) override;
  void OnWorkerCreated(const blink::SharedWorkerToken& token,
                       int worker_process_id,
                       const base::UnguessableToken& dev_tools_token) override {
  }
  void OnBeforeWorkerDestroyed(const blink::SharedWorkerToken& token) override {
  }
  void OnClientRemoved(
      const blink::SharedWorkerToken& token,
      content::GlobalRenderFrameHostId render_frame_host_id) override {}
  using content::SharedWorkerService::Observer::OnFinalResponseURLDetermined;
  // End SharedWorkerService.Observer overrides.

  // Start DedicatedWorkerService.Observer overrides:
  void OnWorkerCreated(const blink::DedicatedWorkerToken& worker_token,
                       int worker_process_id,
                       content::DedicatedWorkerCreator creator) override;
  void OnBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& worker_token,
      content::DedicatedWorkerCreator creator) override {}
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& worker_token,
      const GURL& url) override {}
  // End DedicatedWorkerService.Observer overrides.

  raw_ptr<RedirectChainDetector> detector_;
  // raw_ptr<> is safe here because DIPSService is a KeyedService, associated
  // with the BrowserContext/Profile which will outlive the WebContents that
  // DIPSWebContentsObserver is observing.
  raw_ptr<DIPSService> dips_service_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};
  DIPSIssueReportingCallback issue_reporting_callback_;

  std::optional<std::string> last_committed_site_;
  std::optional<base::Time> last_commit_timestamp_;
  std::optional<base::Time> last_storage_timestamp_;
  std::optional<base::Time> last_interaction_timestamp_;

  base::WeakPtrFactory<DIPSWebContentsObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

ukm::SourceId GetInitialRedirectSourceId(
    content::NavigationHandle* navigation_handle);

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
