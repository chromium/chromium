// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_

#include "chrome/browser/previews/previews_lite_page_navigation_throttle_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
struct OpenURLParams;
class BrowserContext;
}

// If the given URL is a LitePage Preview URL, this returns true but does not
// change the |url|. This will set |update_virtual_url_with_url| on
// NavigationEntry so that |HandlePreviewsLitePageURLRewriteReverse| is called
// when the navigation finishes.
// Note: This means the virtual URL will not be set during the navigation load.
// This is handled separately in UI on Android.
bool HandlePreviewsLitePageURLRewrite(GURL* url,
                                      content::BrowserContext* browser_context);

// Handles translating the given Lite Page URL to the original URL. Returns true
// if the given |url| was a preview, otherwise returns false and does not change
// |url|.
bool HandlePreviewsLitePageURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context);

// This class does the actual decision making about when to serve a Lite Page
// Server Preview, and the legwork to trigger the Preview navigation. When a
// Preview is triggered, it will cancel the incoming navigation and PostTask a
// new one to the Previews Server.
class PreviewsLitePageNavigationThrottle : public content::NavigationThrottle {
 public:
  // Reasons that a navigation is blacklisted from this preview. This enum must
  // remain synchronized with the enum |PreviewsServerLitePageBlacklistReason|
  // in metrics/histograms/enums.xml.
  enum class BlacklistReason {
    kPathSuffixBlacklisted = 0,
    kNavigationToPreviewsDomain = 1,
    kNavigationToPrivateDomain = 2,
    kHostBlacklisted = 3,
    kMaxValue = kHostBlacklisted,
  };

  // Reasons that a navigation is not eligible for this preview. This enum must
  // remain synchronized with the enum |PreviewsServerLitePageIneligibleReason|
  // in metrics/histograms/enums.xml.
  enum class IneligibleReason {
    kNonHttpsScheme = 0,
    kHttpPost = 1,
    kSubframeNavigation = 2,
    kServerUnavailable = 3,
    kInfoBarNotSeen = 4,
    kNetworkNotSlow = 5,
    kLoadOriginalReload = 6,
    kMaxValue = kLoadOriginalReload,
  };

  // The response type from the previews server. This enum must
  // remain synchronized with the enum |PreviewsServerLitePageServerResponse| in
  // metrics/histograms/enums.xml.
  enum class ServerResponse {
    // A preview was served (HTTP 200).
    kOk = 0,

    // The client was redirected to another page (HTTP 307).
    kRedirect = 1,

    // The requested preview was not available (HTTP 307).
    kPreviewUnavailable = 2,

    // The previews server is not available (HTTP 503).
    kServiceUnavailable = 3,

    // The previews server responded with some other HTTP code.
    kOther = 4,

    // There was some network error and we did not get a response from the
    // previews server.
    kFailed = 5,

    // The previews server did not respond after a timeout.
    kTimeout = 6,

    // The previews server rejected our authentication (HTTP 403).
    kAuthFailure = 7,

    kMaxValue = kAuthFailure,
  };

  PreviewsLitePageNavigationThrottle(
      content::NavigationHandle* handle,
      PreviewsLitePageNavigationThrottleManager* manager);

  ~PreviewsLitePageNavigationThrottle() override;

  // Attempts to extract the original URL from the given Previews URL. Returns
  // false if |url| is not a valid Preview URL. It is ok to pass nullptr for
  // |original_url| if you only want the boolean return value.
  static bool GetOriginalURL(const GURL& url, std::string* original_url);

  // Returns the URL for a preview given by the url.
  static GURL GetPreviewsURLForURL(const GURL& original_url);

  // Starts a new navigation with |params| page in the given |web_contents|,
  // adding the params' url as a single bypass to |manager|.
  static void LoadAndBypass(content::WebContents* web_contents,
                            PreviewsLitePageNavigationThrottleManager* manager,
                            const content::OpenURLParams& params,
                            bool use_post_task);

 private:
  // The current effective connection type;
  net::EffectiveConnectionType GetECT() const;

  // Returns true if the current URL is eligible for the preview. Does not check
  // any blacklisting or single bypass.
  bool IsEligibleForPreview() const;

  // Returns the URL for a preview of the current URL given by the navigation
  // handle.
  GURL GetPreviewsURL() const;

  // This method is called on every request and redirect. It checks all
  // eligibility and blacklisting criteria for the current URL and will return
  // CANCEL when the preview is triggered and post a new navigation, or will
  // return PROCEED if the preview is not triggered.
  content::NavigationThrottle::ThrottleCheckResult MaybeNavigateToPreview()
      const;

  // Can be called by any of the content::NavigationThrottle implementation
  // methods to trigger the preview. Returns the
  // |content::NavigationThrottle::ThrottleCheckResult| for the implemented
  // method to return.
  content::NavigationThrottle::ThrottleCheckResult TriggerPreview() const;

  // content::NavigationThrottle implementation:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  // The manager class to report state changes to and get state from. Outlives
  // |this|.
  PreviewsLitePageNavigationThrottleManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageNavigationThrottle);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
