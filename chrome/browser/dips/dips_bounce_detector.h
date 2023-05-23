// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include <memory>
#include <string>
#include <variant>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

using DIPSIssueHandler =
    base::RepeatingCallback<void(const std::set<std::string>& sites)>;
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
  absl::optional<base::Time> last_activation_time;
  absl::optional<base::Time> last_storage_time;
  SiteDataAccessType site_data_access_type = SiteDataAccessType::kUnknown;
};

// Either the URL navigated away from (starting a new chain), or the client-side
// redirect connecting the navigation to the currently-committed chain.
using DIPSNavigationStart = absl::variant<GURL, DIPSRedirectInfoPtr>;

// In case of a client-side redirect loop, we need to impose a limit on the
// stored redirect chain to avoid boundless memory use. Past this limit,
// redirects are trimmed from the front of the list.
constexpr size_t kDIPSRedirectChainMax = 1000;

// A redirect-chain-in-progress. It grows by calls to Append() and restarts by
// calls to EndChain().
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
                       const GURL& final_url);

  // Trims |trim_count| redirect from the front of the in-progress redirect
  // chain. Passes the redirects as partial chains to the
  // `DIPSRedirectChainHandler`.
  void TrimAndHandleRedirects(size_t trim_count);

  // Terminates the in-progress redirect chain, ending it with `final_url`, and
  // parsing it to the `DIPSRedirectChainHandler` iff the chain is valid. It
  // also starts a fresh redirect chain with `final_url` whilst clearing the
  // state of the terminated chain.
  // NOTE: A chain is valid if it has a non-empty `initial_url_`.
  void EndChain(GURL final_url);

  void ReportIssue(const GURL& final_url);

  [[nodiscard]] bool AddLateCookieAccess(GURL url, CookieOperation op);

  size_t size() const { return redirects_.size(); }

  GURL GetInitialURL() { return initial_url_; }

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    handler_ = handler;
  }

  size_t GetRedirectChainLength() {
    return redirects_.size() + redirect_prefix_count_;
  }

 private:
  void AppendClientRedirect(DIPSRedirectInfoPtr client_redirect);
  void AppendServerRedirects(std::vector<DIPSRedirectInfoPtr> server_redirects);
  void TrimRedirectsFromFront();

  DIPSRedirectChainHandler handler_;
  DIPSIssueHandler issue_handler_;
  // Represents the start of a chain and also indicates the presence of a valid
  // chain.
  GURL initial_url_;
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
class DIPSBounceDetectorDelegate {
 public:
  virtual ~DIPSBounceDetectorDelegate();
  virtual const GURL& GetLastCommittedURL() const = 0;
  virtual ukm::SourceId GetPageUkmSourceId() const = 0;
  virtual void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                                   DIPSRedirectChainInfoPtr chain) = 0;
  virtual void ReportRedirectorsWithoutInteraction(
      const std::set<std::string>& sites) = 0;
  virtual void RecordEvent(DIPSRecordedEvent event,
                           const GURL& url,
                           const base::Time& time) = 0;
  virtual void IncrementPageSpecificBounceCount(const GURL& final_url) = 0;
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
class DIPSBounceDetector {
 public:
  // The amount of time since a page last received user interaction before a
  // subsequent user interaction event may be recorded to DIPS Storage for the
  // same page.
  static const base::TimeDelta kTimestampUpdateInterval;

  explicit DIPSBounceDetector(DIPSBounceDetectorDelegate* delegate,
                              const base::TickClock* tick_clock,
                              const base::Clock* clock);
  ~DIPSBounceDetector();
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  // The following methods are based on WebContentsObserver, simplified.
  void DidStartNavigation(DIPSNavigationHandle* navigation_handle);
  void OnClientSiteDataAccessed(const GURL& url, CookieOperation op);
  void OnClientCookiesAccessed(const GURL& url, CookieOperation op);
  void OnServerCookiesAccessed(DIPSNavigationHandle* navigation_handle,
                               const GURL& url,
                               CookieOperation op);
  void DidFinishNavigation(DIPSNavigationHandle* navigation_handle);
  // Only records a new user activation event once per
  // |kTimestampUpdateInterval| for a given page.
  void OnUserActivation();
  // Makes a call to process the current chain before its state is destroyed by
  // the tab closure.
  void BeforeDestruction();
  // Use the passed handler instead of
  // DIPSBounceDetectorDelegate::HandleRedirect().
  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    committed_redirect_context_.SetRedirectChainHandlerForTesting(handler);
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
  absl::optional<ClientBounceDetectionState> client_detection_state_;
  DIPSRedirectContext committed_redirect_context_;
  base::RetainingOneShotTimer client_bounce_detection_timer_;
};

// A thin wrapper around DIPSBounceDetector to use it as a WebContentsObserver.
class DIPSWebContentsObserver
    : public content_settings::PageSpecificContentSettings::SiteDataObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSWebContentsObserver>,
      public DIPSBounceDetectorDelegate {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~DIPSWebContentsObserver() override;

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    detector_.SetRedirectChainHandlerForTesting(handler);
  }

  // Use the passed handler instead of DIPSWebContentsObserver::EmitDIPSIssue().
  void SetIssueReportingCallbackForTesting(
      DIPSIssueReportingCallback callback) {
    issue_reporting_callback_ = callback;
  }

  void SetClockForTesting(base::Clock* clock) {
    detector_.SetClockForTesting(clock);
    DCHECK(dips_service_);
    dips_service_->storage()
        ->AsyncCall(&DIPSStorage::SetClockForTesting)
        .WithArgs(clock);
  }

 private:
  DIPSWebContentsObserver(content::WebContents* web_contents,
                          DIPSService* dips_service);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSWebContentsObserver>;

  void EmitDIPSIssue(const std::set<std::string>& sites);

  // DIPSBounceDetectorDelegate overrides:
  const GURL& GetLastCommittedURL() const override;
  ukm::SourceId GetPageUkmSourceId() const override;
  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain) override;
  void ReportRedirectorsWithoutInteraction(
      const std::set<std::string>& sites) override;
  void RecordEvent(DIPSRecordedEvent event,
                   const GURL& url,
                   const base::Time& time) override;
  void IncrementPageSpecificBounceCount(const GURL& final_url) override;

  // WebContentsObserver overrides:
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
  void WebContentsDestroyed() override;

  // Start SiteDataObserver overrides:
  void OnSiteDataAccessed(
      const content_settings::AccessDetails& access_details) override;
  void OnStatefulBounceDetected() override;
  // End SiteDataObserver overrides.

  // raw_ptr<> is safe here because DIPSService is a KeyedService, associated
  // with the BrowserContext/Profile which will outlive the WebContents that
  // DIPSWebContentsObserver is observing.
  raw_ptr<DIPSService> dips_service_;
  DIPSBounceDetector detector_;
  DIPSIssueReportingCallback issue_reporting_callback_;

  base::WeakPtrFactory<DIPSWebContentsObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

ukm::SourceId GetInitialRedirectSourceId(
    content::NavigationHandle* navigation_handle);

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
