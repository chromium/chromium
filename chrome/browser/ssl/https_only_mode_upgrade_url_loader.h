// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_URL_LOADER_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_URL_LOADER_H_

#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using HandleRequest = base::OnceCallback<void(
    content::URLLoaderRequestInterceptor::RequestHandler handler)>;

// A URLLoader that serves an artificial redirect to the HTTPS version of an
// HTTP URL to upgrade requests to HTTPS.
class HttpsOnlyModeUpgradeURLLoader : public network::mojom::URLLoader {
 public:
  HttpsOnlyModeUpgradeURLLoader(
      const network::ResourceRequest& tentative_resource_request,
      HandleRequest callback);
  ~HttpsOnlyModeUpgradeURLLoader() override;

  HttpsOnlyModeUpgradeURLLoader(const HttpsOnlyModeUpgradeURLLoader&) = delete;
  HttpsOnlyModeUpgradeURLLoader& operator=(
      const HttpsOnlyModeUpgradeURLLoader&) = delete;

  void StartRedirectToHttps(int frame_tree_node_id);

  void StartRedirectToOriginalURL(const GURL& original_url);

 private:
  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Serves a redirect to the HTTPS version of the URL.
  void StartHandlingRedirectToModifiedRequest(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Helper method for setting up and serving `redirect_info` to `client`.
  void StartHandlingRedirect(
      const network::ResourceRequest& /* resource_request */,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Helper method to create redirect information to `redirect_url` and modify
  // `redirect_info_` and `modified_resource_request_`.
  void CreateRedirectInformation(const GURL& redirect_url);

  // Mojo error handling. Deletes `this`.
  void OnConnectionClosed();

  // A copy of the initial resource request that has been modified to fetch
  // the HTTPS page.
  network::ResourceRequest modified_resource_request_;

  // Stores the response details for the artificial redirect.
  network::mojom::URLResponseHeadPtr response_head_ =
      network::mojom::URLResponseHead::New();

  // Information about the redirect to the upgraded HTTPS URL.
  net::RedirectInfo redirect_info_;

  // Called upon success or failure to let content/ know whether this class
  // intends to intercept the request. Must be passed a handler if this class
  // intends to intercept the request.
  HandleRequest callback_;

  // Receiver for the URLLoader interface.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  // The owning client. Used for serving redirects.
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpsOnlyModeUpgradeURLLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_URL_LOADER_H_
