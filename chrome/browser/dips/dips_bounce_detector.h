// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

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
  CookieAccessType cookie_access_type = CookieAccessType::kUnknown;
};

// Either the URL navigated away from (starting a new chain), or the client-side
// redirect connecting the navigation to the currently-committed chain.
using DIPSNavigationStart = absl::variant<GURL, DIPSRedirectInfoPtr>;

// A redirect-chain-in-progress. It grows by calls to Append() and restarts by
// calls to EndChain().
class DIPSRedirectContext {
 public:
  DIPSRedirectContext(DIPSRedirectChainHandler handler,
                      const GURL& initial_url);
  ~DIPSRedirectContext();

  // Immediately calls the DIPSRedirectChainHandler for the uncommitted
  // navigation. It will take into account the length and initial URL of the
  // current chain (without modifying it).
  void HandleUncommitted(DIPSNavigationStart navigation_start,
                         std::vector<DIPSRedirectInfoPtr> server_redirects,
                         GURL final_url);
  // Either terminates the current redirect chain (and starts a new one) or
  // extends it, according to the value of `navigation_start`.
  void AppendCommitted(DIPSNavigationStart navigation_start,
                       std::vector<DIPSRedirectInfoPtr> server_redirects);
  // Terminates the current redirect chain, ending it with the given URL.
  void EndChain(GURL url);

  size_t size() const { return redirects_.size(); }

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    handler_ = handler;
  }

 private:
  void AppendClientRedirect(DIPSRedirectInfoPtr client_redirect);
  void AppendServerRedirects(std::vector<DIPSRedirectInfoPtr> server_redirects);

  DIPSRedirectChainHandler handler_;
  GURL initial_url_;
  std::vector<DIPSRedirectInfoPtr> redirects_;
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
  virtual void RecordEvent(DIPSRecordedEvent event,
                           const GURL& url,
                           const base::Time& time) = 0;
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
  static const base::TimeDelta kInteractionUpdateInterval;

  explicit DIPSBounceDetector(DIPSBounceDetectorDelegate* delegate,
                              const base::TickClock* tick_clock,
                              const base::Clock* clock);
  ~DIPSBounceDetector();
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  // The following methods are based on WebContentsObserver, simplified.
  void DidStartNavigation(DIPSNavigationHandle* navigation_handle);
  void OnClientCookiesAccessed(const GURL& url, CookieOperation op);
  void OnServerCookiesAccessed(DIPSNavigationHandle* navigation_handle,
                               const GURL& url,
                               CookieOperation op);
  void DidFinishNavigation(DIPSNavigationHandle* navigation_handle);
  // Only records a new user activation event once per
  // |kInteractionUpdateInterval| for a given page.
  void OnUserActivation();
  void BeforeDestruction();

  // Use the passed handler instead of
  // DIPSBounceDetectorDelegate::HandleRedirect().
  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    redirect_context_.SetRedirectChainHandlerForTesting(handler);
  }

 private:
  raw_ptr<const base::TickClock> tick_clock_;
  raw_ptr<const base::Clock> clock_;
  raw_ptr<DIPSBounceDetectorDelegate> delegate_;
  absl::optional<ClientBounceDetectionState> client_detection_state_;
  DIPSRedirectContext redirect_context_;
};

// A thin wrapper around DIPSBounceDetector to use it as a WebContentsObserver.
class DIPSWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSWebContentsObserver>,
      public DIPSBounceDetectorDelegate {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~DIPSWebContentsObserver() override;

  void SetRedirectChainHandlerForTesting(DIPSRedirectChainHandler handler) {
    detector_.SetRedirectChainHandlerForTesting(handler);
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

  // DIPSBounceDetectorDelegate overrides:
  const GURL& GetLastCommittedURL() const override;
  ukm::SourceId GetPageUkmSourceId() const override;
  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain) override;
  void RecordEvent(DIPSRecordedEvent event,
                   const GURL& url,
                   const base::Time& time) override;

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

  // raw_ptr<> is safe here because DIPSService is a KeyedService, associated
  // with the BrowserContext/Profile which will outlive the WebContents that
  // DIPSWebContentsObserver is observing.
  raw_ptr<DIPSService> dips_service_;
  DIPSBounceDetector detector_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
