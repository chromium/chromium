// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_url_loader_header_client.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_client.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace enterprise_custom_headers {

// static
void HttpHeaderInjectionURLLoaderHeaderClient::Create(
    base::WeakPtr<HttpHeaderInjectionService> service,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        target_client) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new HttpHeaderInjectionURLLoaderHeaderClient(
          std::move(service), std::move(target_client))),
      std::move(receiver));
}

HttpHeaderInjectionURLLoaderHeaderClient::
    HttpHeaderInjectionURLLoaderHeaderClient(
        base::WeakPtr<HttpHeaderInjectionService> service,
        mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
            target_client)
    : service_(std::move(service)) {
  if (target_client) {
    target_client_.Bind(std::move(target_client));
    target_client_.set_disconnect_handler(base::BindOnce(
        &HttpHeaderInjectionURLLoaderHeaderClient::OnTargetDisconnect,
        base::Unretained(this)));
  }
}

HttpHeaderInjectionURLLoaderHeaderClient::
    ~HttpHeaderInjectionURLLoaderHeaderClient() = default;

void HttpHeaderInjectionURLLoaderHeaderClient::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_header_client;
  if (target_client_.is_bound()) {
    target_client_->OnLoaderCreated(
        request_id, target_header_client.InitWithNewPipeAndPassReceiver());
  }

  HttpHeaderInjectionClient::Create(service_, std::move(receiver),
                                    std::move(target_header_client));
}

void HttpHeaderInjectionURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_header_client;
  if (target_client_.is_bound()) {
    target_client_->OnLoaderForCorsPreflightCreated(
        request, target_header_client.InitWithNewPipeAndPassReceiver());
  }

  HttpHeaderInjectionClient::Create(service_, std::move(receiver),
                                    std::move(target_header_client));
}

void HttpHeaderInjectionURLLoaderHeaderClient::OnLoaderFailedWithRemoteEndpoint(
    int32_t request_id,
    const net::IPEndPoint& remote_endpoint) {
  if (target_client_.is_bound()) {
    target_client_->OnLoaderFailedWithRemoteEndpoint(request_id,
                                                     remote_endpoint);
  }
}

void HttpHeaderInjectionURLLoaderHeaderClient::OnTargetDisconnect() {
  target_client_.reset();
}

}  // namespace enterprise_custom_headers
