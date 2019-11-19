// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_INTERCEPTOR_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/previews/previews_lite_page_redirect_serving_url_loader.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

class PreviewsLitePageRedirectDecider;

namespace previews {

// Reasons that a navigation is blacklisted from a lite page redirect preview.
// This enum must remain synchronized with the enum
// |PreviewsServerLitePageBlacklistReason| in
// tools/metrics/histograms/enums.xml.
enum class LitePageRedirectBlacklistReason {
  // DEPRECATED: kPathSuffixBlacklisted = 0,
  kNavigationToPreviewsDomain = 1,
  kNavigationToPrivateDomain = 2,
  kHostBypassBlacklisted = 3,
  kMaxValue = kHostBypassBlacklisted,
};

// Reasons that a navigation is not eligible for a lite page redirect preview.
// This enum must remain synchronized with the enum
// |PreviewsServerLitePageIneligibleReason| in
// tools/metrics/histograms/enums.xml.
enum class LitePageRedirectIneligibleReason {
  kNonHttpsScheme = 0,
  // DEPRECATED: kHttpPost = 1,
  kSubframeNavigation = 2,
  kServerUnavailable = 3,
  kInfoBarNotSeen = 4,
  // DEPRECATED: kNetworkNotSlow = 5,
  kLoadOriginalReload = 6,
  kCookiesBlocked = 7,
  // DEPRECATED: kECTUnknown = 8,
  kExceededMaxNavigationRestarts = 9,
  // DEPRECATED: kPreviewsState = 10,
  kInvalidProxyHeaders = 11,
  kServiceProbeIncomplete = 12,
  kServiceProbeFailed = 13,
  kAPIPageTransition = 14,
  kForwardBackPageTransition = 15,
  kMaxValue = kForwardBackPageTransition,
};

// The response type from the previews server. This enum must
// remain synchronized with the enum |PreviewsServerLitePageServerResponse| in
// tools/metrics/histograms/enums.xml.
enum class LitePageRedirectServerResponse {
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

  // No response headers were available from the preview server.
  kNoResponseHeaders = 8,

  // The connection was closed without getting a response.
  kOnCompleteBeforeOnResponse = 9,

  // There was an error connecting to the previews server.
  kConnectionError = 10,

  kMaxValue = kConnectionError,
};

// Records an entry in the lite page redirect ineligibility histogram.
void LogLitePageRedirectIneligibleReason(
    LitePageRedirectIneligibleReason reason);

// If the given URL is a LitePage Preview URL, this returns true but does not
// change the |url|. This will set |update_virtual_url_with_url| on
// NavigationEntry so that |HandlePreviewsLitePageRedirectURLRewriteReverse| is
// called when the navigation finishes. Note: This means the virtual URL will
// not be set during the navigation load. This is handled separately in UI on
// Android.
bool HandlePreviewsLitePageRedirectURLRewrite(
    GURL* url,
    content::BrowserContext* browser_context);

// Handles translating the given Lite Page URL to the original URL. Returns true
// if the given |url| was a preview, otherwise returns false and does not change
// |url|.
bool HandlePreviewsLitePageRedirectURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context);

// Returns the URL for a preview given by the url.
GURL GetLitePageRedirectURLForURL(const GURL& original_url);

// A class that attempts to intercept requests and fetch the Lite Page version
// of the request. Its lifetime matches that of the content/ navigation loader
// code.
class PreviewsLitePageRedirectURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  PreviewsLitePageRedirectURLLoaderInterceptor(
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory,
      uint64_t page_id,
      int frame_tree_node_id);
  ~PreviewsLitePageRedirectURLLoaderInterceptor() override;

  // Gets the ServerLitePageInfo struct from an existing attempted lite page
  // navigation, if there is one. If not, returns a new ServerLitePageInfo
  // initialized with metadata from navigation_handle() and |this| that is owned
  // by the PreviewsUserData associated with navigation_handle().
  static PreviewsUserData::ServerLitePageInfo* GetOrCreateServerLitePageInfo(
      content::NavigationHandle* navigation_handle,
      PreviewsLitePageRedirectDecider* manager);

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // Begins an attempt at fetching the lite page version of the URL.
  void CreateRedirectLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // Creates a redirect URL loader that immediately serves a redirect to
  // |original_url|.
  void CreateOriginalURLLoader(
      const network::ResourceRequest& tentative_resource_request,
      const GURL& original_url,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // Runs |callback| with |handler| and stores |serving_url_loader|.
  void HandleRedirectLoader(
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      std::unique_ptr<PreviewsLitePageRedirectServingURLLoader>
          serving_url_loader,
      RequestHandler handler);

  // Checks if the pending navigation is a forward/back nav and should be
  // disallowed according to experiment parameters.
  bool IsDisallowedFwdBackNavigation();

  // All URLs already seen in this navigation. This prevents redirect loops,
  // etc.
  std::set<GURL> urls_processed_;

  // While attempting to fetch a lite page, this object manages communication
  // with the lite page server and serving redirects. Once, a decision has been
  // made regarding serving the preview, this object will be null.
  std::unique_ptr<PreviewsLitePageRedirectURLLoader> redirect_url_loader_;

  // Once a decision to serve the lite page has been made (based on server
  // response), this object will exist until a redirect to the lite page URL has
  // been handed off to the navigation stack and the next request is being
  // handled.
  std::unique_ptr<PreviewsLitePageRedirectServingURLLoader> serving_url_loader_;

  // Factory to create a network service URLLoader.
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // Used in the chrome-proxy header if a preview is attempted.
  uint64_t page_id_;

  // Used to create the network service URLLoader.
  int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectURLLoaderInterceptor);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_INTERCEPTOR_H_
