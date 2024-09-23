// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_UPGRADES_INTERCEPTOR_H_
#define CHROME_BROWSER_SSL_HTTPS_UPGRADES_INTERCEPTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
// #include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include <optional>

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace content {
class BrowserContext;
class NavigationUIData;
class WebContents;
}  // namespace content

// A class that attempts to intercept HTTP navigation requests and redirect them
// to HTTPS, and then if the upgraded requests fail redirect them back to HTTP.
// Its lifetime matches that of the content/ navigation loader code.
//
// (Aside: An alternate implementation of this class would be as a
// NavigationLoaderInterceptor in content/, which could have a slightly simpler
// implementation, but by having this in chrome/ we don't need a Delegate
// interface for the embedder logic. If we move this into content/ in the future
// -- for example, in order to have a default HTTPS Upgrades implementation in
// the content/ platform -- then we can make that switch.)
class HttpsUpgradesInterceptor : public content::URLLoaderRequestInterceptor,
                                 public network::mojom::URLLoader {
 public:
  static std::unique_ptr<HttpsUpgradesInterceptor> MaybeCreateInterceptor(
      content::FrameTreeNodeId frame_tree_node_id,
      content::NavigationUIData* navigation_ui_data_);

  HttpsUpgradesInterceptor(content::FrameTreeNodeId frame_tree_node_id,
                           bool http_interstitial_enabled,
                           content::NavigationUIData* navigation_ui_data_);
  ~HttpsUpgradesInterceptor() override;

  HttpsUpgradesInterceptor(const HttpsUpgradesInterceptor&) = delete;
  HttpsUpgradesInterceptor& operator=(const HttpsUpgradesInterceptor&) = delete;

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;
  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader) override;

  // Continuation of MaybeCreateLoader() after querying the network service for
  // the HSTS status for the hostname in the request.
  void MaybeCreateLoaderOnHstsQueryCompleted(
      const network::ResourceRequest& tentative_resource_request,
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      bool is_hsts_active_for_host);

  // Sets the ports used by the EmbeddedTestServer (which uses random ports)
  // to determine the correct port to upgrade/fallback to in tests.
  static void SetHttpsPortForTesting(int port);
  static void SetHttpPortForTesting(int port);
  static int GetHttpsPortForTesting();
  static int GetHttpPortForTesting();

 private:
  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  // Returns a RequestHandler callback that can be passed to the underlying
  // LoaderCallback to serve an artificial redirect to `new_url`.
  RequestHandler CreateRedirectHandler(const GURL& new_url);

  // Passed to the LoaderCallback as the ResponseHandler with `new_url` bound,
  // this method receives the receiver and client_remote from the
  // NavigationLoader, to bind against. Triggers a redirect to `new_url` using
  // `client`.
  void RedirectHandler(
      const GURL& new_url,
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Mojo error handling. Resets `receiver_` and `client_`.
  void OnConnectionClosed();

  // Used to access the WebContents for the navigation.
  content::FrameTreeNodeId frame_tree_node_id_;

  // Controls whether we are upgrading and falling back with an interstitial
  // before proceeding with the HTTP navigation. This reflects the general
  // UI setting. Only used to set the values of interstitial_state_.
  bool http_interstitial_enabled_by_pref_ = false;

  // Parameters about whether the throttle should trigger the interstitial
  // warning before navigating to the HTTP fallback URL. Can be null if the
  // current load isn't eligible for an upgrade.
  std::unique_ptr<
      security_interstitials::https_only_mode::HttpInterstitialState>
      interstitial_state_;

  // URLs seen by the interceptor, used to detect a redirect loop.
  std::set<GURL> urls_seen_;

  // Receiver for the URLLoader interface.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  // The owning client. Used for serving redirects.
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // Owned by NavigationURLLoaderImpl, which should outlive the interceptor.
  raw_ptr<content::NavigationUIData> navigation_ui_data_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpsUpgradesInterceptor> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SSL_HTTPS_UPGRADES_INTERCEPTOR_H_
