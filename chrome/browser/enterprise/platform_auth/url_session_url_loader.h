// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_

#include <Foundation/Foundation.h>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_version.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace enterprise_auth {

class URLSessionURLLoaderTest;

// URLLoader implementation for making requests with Apple's URLSession api.
// It is only meant for simple requests, received data over 1MiB will result in
// an error sent to the client.
// This class is self-owned, it will destroy itself if:
//  - the request is complete
//  - the client disconnects
//  - an error occurs with mojo or URLSession
// WARNING! This is meant to be used only for the Okta SSO flow, this class
// comes with multiple restrications and behaviour specific for this use case.
class URLSessionURLLoader : public network::mojom::URLLoader {
 public:
  URLSessionURLLoader(const URLSessionURLLoader&) = delete;
  URLSessionURLLoader& operator=(const URLSessionURLLoader&) = delete;

  static void CreateAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info);

  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const ::net::HttpRequestHeaders& modified_headers,
      const ::net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<::GURL>& new_url) override;

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

 private:
  URLSessionURLLoader();
  ~URLSessionURLLoader() override;

  void Start(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info_remote);

  void OnRequestComplete(NSURLResponse* response, NSData* data);

  void OnRequestFailed();

  void OnClientDisconnect();

  void OnReceiverDisconnect();

  void DisconnectAndDelete();

  inline void OverrideSessionForTesting(NSURLSession* session) {
    session_override_ = session;
  }

 private:
  friend URLSessionURLLoaderTest;

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  NSURLSessionTask* task_ = nil;
  base::TimeTicks request_start_;

  NSURLSession* session_override_ = nil;

  base::WeakPtrFactory<URLSessionURLLoader> weak_ptr_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_
