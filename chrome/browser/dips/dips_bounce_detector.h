// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include <string>
#include <variant>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}

namespace site_engagement {
class SiteEngagementService;
}

class DIPSService;

// ClientBounceDetectionState is owned by the DIPSBounceDetector and stores
// data needed to detect stateful client-side redirects.
class ClientBounceDetectionState {
 public:
  ClientBounceDetectionState(GURL url,
                             std::string site,
                             base::TimeTicks load_time) {
    this->previous_url = std::move(url);
    this->current_site = std::move(site);
    this->page_load_time = load_time;
  }

  // The NavigationHandle's previously committed URL at the time the navigation
  // finishes and commits.
  GURL previous_url;
  std::string current_site;
  base::TimeTicks page_load_time;
  bool received_user_activation = false;
  CookieAccessType cookie_access_type = CookieAccessType::kUnknown;
};

// Properties of a redirect chain common to all the URLs within the chain.
struct DIPSRedirectChainInfo {
 public:
  DIPSRedirectChainInfo(const GURL& initial_url,
                        const GURL& final_url,
                        int length);
  ~DIPSRedirectChainInfo();

  const GURL initial_url;
  // The eTLD+1 of initial_url, cached.
  const std::string initial_site;
  const GURL final_url;
  // The eTLD+1 of final_url, cached.
  const std::string final_site;
  // initial_site == final_site, cached.
  const bool initial_and_final_sites_same;
  const int length;
};

// Properties of one URL within a redirect chain.
struct DIPSRedirectInfo {
 public:
  // Constructor for server-side redirects.
  DIPSRedirectInfo(const GURL& url,
                   DIPSRedirectType redirect_type,
                   CookieAccessType access_type,
                   int index,
                   ukm::SourceId source_id);
  // Constructor for client-side redirects.
  DIPSRedirectInfo(const GURL& url,
                   DIPSRedirectType redirect_type,
                   CookieAccessType access_type,
                   int index,
                   ukm::SourceId source_id,
                   base::TimeDelta client_bounce_delay,
                   bool has_sticky_activation);
  ~DIPSRedirectInfo();

  // These properties are required for all redirects:

  const GURL url;
  const DIPSRedirectType redirect_type;
  const CookieAccessType access_type;
  // Index of this URL within the overall chain.
  const int index;
  const ukm::SourceId source_id;

  // The following properties are only applicable for client-side redirects:

  // For client redirects, the time between the previous page committing
  // and the redirect navigation starting. (For server redirects, zero)
  const base::TimeDelta client_bounce_delay;
  // For client redirects, whether the user ever interacted with the page.
  const bool has_sticky_activation;
};

using DIPSRedirectHandler =
    base::RepeatingCallback<void(const DIPSRedirectInfo&,
                                 const DIPSRedirectChainInfo&)>;

// a movable DIPSRedirectInfo, essentially
using DIPSRedirectInfoPtr = std::unique_ptr<DIPSRedirectInfo>;

// Either the URL navigated away from (starting a new chain), or the client-side
// redirect connecting the navigation to the currently-committed chain.
using DIPSNavigationStart = absl::variant<GURL, DIPSRedirectInfoPtr>;

// A redirect-chain-in-progress. It grows by calls to Append() and restarts by
// calls to EndChain().
class DIPSRedirectContext {
 public:
  DIPSRedirectContext(DIPSRedirectHandler handler, const GURL& initial_url);
  ~DIPSRedirectContext();

  // If committed=true, appends the client and server redirects to the current
  // chain. Otherwise, creates a temporary DIPSRedirectContext, appends the
  // redirects, and immediately calls EndChain() on it.
  void Append(bool committed,
              DIPSNavigationStart navigation_start,
              std::vector<DIPSRedirectInfoPtr>&& server_redirects,
              GURL final_url);
  // Terminates the current redirect chain and calls the DIPSRedirectHandler for
  // each entry. Starts a new chain for later calls to Append() to add to.
  void EndChain(GURL url);

  size_t size() const { return redirects_.size(); }

  void SetRedirectHandlerForTesting(DIPSRedirectHandler handler) {
    handler_ = handler;
  }

 private:
  // Appends the client and server redirects to the current chain.
  void Append(DIPSNavigationStart navigation_start,
              std::vector<DIPSRedirectInfoPtr>&& server_redirects);

  DIPSRedirectHandler handler_;
  GURL initial_url_;
  std::vector<DIPSRedirectInfoPtr> redirects_;
};

class DIPSBounceDetector
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSBounceDetector> {
 public:
  ~DIPSBounceDetector() override;
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  void SetRedirectHandlerForTesting(DIPSRedirectHandler handler);

  // This must be called prior to the DIPSBounceDetector being constructed.
  static base::TickClock* SetTickClockForTesting(base::TickClock* clock);

 private:
  explicit DIPSBounceDetector(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSBounceDetector>;

  DIPSCookieMode GetCookieMode() const;
  ukm::SourceId GetRedirectSourceId(
      content::NavigationHandle* navigation_handle,
      int index);

  // Called when any redirect is detected.
  void HandleRedirect(const DIPSRedirectInfo& redirect,
                      const DIPSRedirectChainInfo& chain);

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

  // raw_ptr<> is safe here DIPSService is a KeyedService, associated with the
  // BrowserContext/Profile which will outlive the WebContents that
  // DIPSBounceDetector is observing.
  raw_ptr<DIPSService> dips_service_;
  // raw_ptr<> is safe here for the same reasons as above.
  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;
  absl::optional<ClientBounceDetectionState> client_detection_state_;
  raw_ptr<const base::TickClock> clock_;
  DIPSRedirectContext redirect_context_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// RedirectCategory is basically the cross-product of CookieAccessType and a
// boolean value indicating site engagement. It's used in UMA enum histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RedirectCategory {
  kNoCookies_NoEngagement = 0,
  kReadCookies_NoEngagement = 1,
  kWriteCookies_NoEngagement = 2,
  kReadWriteCookies_NoEngagement = 3,
  kNoCookies_HasEngagement = 4,
  kReadCookies_HasEngagement = 5,
  kWriteCookies_HasEngagement = 6,
  kReadWriteCookies_HasEngagement = 7,
  kUnknownCookies_NoEngagement = 8,
  kUnknownCookies_HasEngagement = 9,
  kMaxValue = kUnknownCookies_HasEngagement,
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
