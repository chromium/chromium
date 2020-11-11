// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_url_loader.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

PrefetchProxyURLLoader::PrefetchProxyURLLoader(
    const network::ResourceRequest& unused_request,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    int32_t routing_id,
    int32_t request_id)
    : network_loader_factory_(network_loader_factory),
      routing_id_(routing_id),
      request_id_(request_id) {
  // |unused_request| is not used since it has not been acted on by
  // URLLoaderThrottles yet.
}

PrefetchProxyURLLoader::~PrefetchProxyURLLoader() = default;

PrefetchProxyURLLoader::RequestHandler
PrefetchProxyURLLoader::ServingResponseHandler() {
  return base::BindOnce(&PrefetchProxyURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr());
}

void PrefetchProxyURLLoader::BindAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  // Bind to the underlying URLLoader.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&PrefetchProxyURLLoader::OnConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));

  network::ResourceRequest isolated_request = request;
  // TODO(crbug/1023485): Set NIK, otherwise the loading initiated here will
  // share the network stack with other browser-initiated loading.
  isolated_request.load_flags = isolated_request.load_flags |
                                net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  isolated_request.credentials_mode = network::mojom::CredentialsMode::kOmit;

  network_loader_factory_->CreateLoaderAndStart(
      network_loader_.BindNewPipeAndPassReceiver(), routing_id_, request_id_,
      network::mojom::kURLLoadOptionNone, isolated_request,
      network_loader_client_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(
          net::DefineNetworkTrafficAnnotation("prefetch_proxy_loader", R"(
          semantics {
            sender: "Prefetch Proxy Loader"
            description:
              "Loads a webpage resource during a proxied prefetch. This is a "
              "prefetcher which is done in total isolation from other user "
              "browsing data like cookies or cache state. Such prefetches "
              "will be used to prefetch render-blocking content before being "
              "navigated by the user without impacting privacy."
            trigger:
              "Used for sites off of Google SRPs (Search Result Pages) only "
              "for Lite mode users when the feature is enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "An ephemeral cookie jar used for only 1 page load."
            setting:
              "Users can control Lite mode on Android via the settings menu. "
              "Lite mode is not available on iOS, and on desktop only for "
              "developer testing."
            policy_exception_justification: "Not implemented."
        })")));

  network_loader_client_.set_disconnect_handler(base::BindOnce(
      &PrefetchProxyURLLoader::OnConnectionError, base::Unretained(this)));
}

void PrefetchProxyURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  forwarding_client_->OnReceiveResponse(std::move(head));
}

void PrefetchProxyURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchProxyURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void PrefetchProxyURLLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  forwarding_client_->OnReceiveCachedMetadata(std::move(data));
}

void PrefetchProxyURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchProxyURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void PrefetchProxyURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

void PrefetchProxyURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  network_loader_->FollowRedirect(removed_headers, modified_headers,
                                  modified_cors_exempt_headers, new_url);
}

void PrefetchProxyURLLoader::SetPriority(net::RequestPriority priority,
                                         int32_t intra_priority_value) {
  network_loader_->SetPriority(priority, intra_priority_value);
}

void PrefetchProxyURLLoader::PauseReadingBodyFromNet() {
  network_loader_->PauseReadingBodyFromNet();
}

void PrefetchProxyURLLoader::ResumeReadingBodyFromNet() {
  network_loader_->ResumeReadingBodyFromNet();
}

void PrefetchProxyURLLoader::OnConnectionError() {
  network_loader_.reset();
  network_loader_client_.reset();
  receiver_.reset();
  forwarding_client_.reset();
  delete this;
}
