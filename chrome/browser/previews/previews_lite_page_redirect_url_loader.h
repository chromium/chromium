// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/previews/previews_lite_page_redirect_serving_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

class PrefService;

namespace previews {

using HandleRequest = base::OnceCallback<void(
    std::unique_ptr<PreviewsLitePageRedirectServingURLLoader>
        serving_url_loader,
    content::URLLoaderRequestInterceptor::RequestHandler handler)>;

// A URL loader that attempts to fetch an HTTPS server lite page, and if
// successful, redirects to the lite page URL, and hands the underlying
// network URLLoader to a success callback. Currently, it supports serving the
// Preview and falling back to default behavior. If enabled, the origin server
// will be probed in parallel with the request to the lite page server and the
// probe must complete successfully before the success callback is run.
class PreviewsLitePageRedirectURLLoader : public network::mojom::URLLoader,
                                          public AvailabilityProber::Delegate {
 public:
  PreviewsLitePageRedirectURLLoader(
      content::BrowserContext* browser_context,
      const network::ResourceRequest& tentative_resource_request,
      HandleRequest callback);
  ~PreviewsLitePageRedirectURLLoader() override;

  // Creates and starts |serving_url_loader_|. |chrome_proxy_headers| are added
  // to the request, and the other parameters are used to start the network
  // service URLLoader.
  void StartRedirectToPreview(
      const net::HttpRequestHeaders& chrome_proxy_headers,
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory,
      int frame_tree_node_id);

  // Creates a redirect to |original_url|.
  void StartRedirectToOriginalURL(const GURL& original_url);

  // AvailabilityProber::Delegate:
  bool ShouldSendNextProbe() override;
  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override;

 private:
  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Processes |result|. Used as a callback for |serving_url_loader_|.
  // |redirect_info| and |response| should be present if and only if |result| is
  // kRedirect.
  void OnResultDetermined(ServingLoaderResult result,
                          base::Optional<net::RedirectInfo> redirect_info,
                          scoped_refptr<network::ResourceResponse> response);

  // Called when the lite page can be successfully served.
  void OnLitePageSuccess();

  // Called when a non-200, non-307 response is received from the previews
  // server.
  void OnLitePageFallback();

  // Called when a redirect (307) is received from the previews server.
  void OnLitePageRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseHead& response_head);

  // The handler when trying to serve the lite page to the user. Serves a
  // redirect to the lite page server URL.
  void StartHandlingRedirectToModifiedRequest(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Helper method for setting up and serving |redirect_info| to |client|.
  void StartHandlingRedirect(
      const network::ResourceRequest& /* resource_request */,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Helper method to create redirect information to |redirect_url| and modify
  // |redirect_info_| and |modified_resource_request_|.
  void CreateRedirectInformation(const GURL& redirect_url);

  // Mojo error handling. Deletes |this|.
  void OnConnectionClosed();

  // Checks that both the origin probe and the previews request have completed
  // before calling |OnLitePageSuccess|.
  void MaybeCallOnLitePageSuccess();

  // A callback passed to |origin_connectivity_prober_| to notify the completion
  // of a probe.
  void OnOriginProbeComplete(bool success);

  // Starts the origin probe given the original page url.
  void StartOriginProbe(const GURL& original_url,
                        const scoped_refptr<network::SharedURLLoaderFactory>&
                            network_loader_factory);

  // The underlying URLLoader that speculatively tries to fetch the lite page.
  std::unique_ptr<PreviewsLitePageRedirectServingURLLoader> serving_url_loader_;

  // A copy of the initial resource request that has been modified to fetch
  // the lite page.
  network::ResourceRequest modified_resource_request_;

  // Stores the response when a 307 (redirect) is received from the previews
  // server.
  network::ResourceResponseHead response_head_;

  // Information about the redirect to the lite page server.
  net::RedirectInfo redirect_info_;

  // Called upon success or failure to let content/ know whether this class
  // intends to intercept the request. Must be passed a handler if this class
  // intends to intercept the request.
  HandleRequest callback_;

  // Binding to the URLLoader interface.
  mojo::Binding<network::mojom::URLLoader> binding_;

  // The owning client. Used for serving redirects.
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // A reference to the profile's prefs. May be null.
  PrefService* pref_service_;

  // Before a preview can be triggered, we must check that the origin site is
  // accessible on the network. This prober manages that check and is only set
  // while determining if a preview can be served.
  std::unique_ptr<AvailabilityProber> origin_connectivity_prober_;

  // Used to remember if the origin probe completes successfully before the
  // litepage request.
  bool origin_probe_finished_successfully_;

  // Used to remember if the lite page request completes successfully before the
  // origin probe.
  bool litepage_request_finished_successfully_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsLitePageRedirectURLLoader> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectURLLoader);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
