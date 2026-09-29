// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/fake_trusted_url_loader_header_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace site_token_provider {

namespace {

// Per-request client handed out by FakeTrustedURLLoaderHeaderClient. It is an
// implementation detail of the fake and is never named by tests.
class FakeTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  explicit FakeTrustedHeaderClient(base::RepeatingClosure on_request_observed)
      : on_request_observed_(std::move(on_request_observed)) {}
  ~FakeTrustedHeaderClient() override = default;

  FakeTrustedHeaderClient(const FakeTrustedHeaderClient&) = delete;
  FakeTrustedHeaderClient& operator=(const FakeTrustedHeaderClient&) = delete;

  void OnBeforeSendHeaders(const GURL& request_url,
                           const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override {
    on_request_observed_.Run();
    net::HttpRequestHeaders modified_headers = headers;
    modified_headers.SetHeader(kFakeHeaderClientHeaderName,
                               kFakeHeaderClientHeaderValue);
    std::move(callback).Run(net::OK, modified_headers, std::nullopt);
  }

  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         const std::optional<net::SSLInfo>& ssl_info,
                         OnHeadersReceivedCallback callback) override {
    std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
  }

 private:
  base::RepeatingClosure on_request_observed_;
};

}  // namespace

FakeTrustedURLLoaderHeaderClient::FakeTrustedURLLoaderHeaderClient() = default;

FakeTrustedURLLoaderHeaderClient::~FakeTrustedURLLoaderHeaderClient() = default;

mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
FakeTrustedURLLoaderHeaderClient::AddReceiver() {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeTrustedURLLoaderHeaderClient::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeTrustedHeaderClient>(base::BindRepeating(
          &FakeTrustedURLLoaderHeaderClient::RecordObservedRequest,
          weak_factory_.GetWeakPtr())),
      std::move(receiver));
}

void FakeTrustedURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  // Preflights are not exercised by these tests; dropping the receiver closes
  // the pipe.
}

int FakeTrustedURLLoaderHeaderClient::observed_request_count() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observed_request_count_;
}

void FakeTrustedURLLoaderHeaderClient::RecordObservedRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observed_request_count_++;
}

}  // namespace site_token_provider
