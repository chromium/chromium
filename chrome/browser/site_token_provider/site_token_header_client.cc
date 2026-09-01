// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_header_client.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_constants.h"
#include "components/site_token_provider/site_token_provider.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"

namespace site_token_provider {

namespace {

// Resolves and returns the site token if the request URL is eligible and
// allowlisted.
std::optional<std::string> MaybeGetTokenForRequest(
    SiteTokenProviderService* service,
    const GURL& request_url) {
  if (!base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled)) {
    return std::nullopt;
  }

  if (!service) {
    return std::nullopt;
  }

  if (!request_url.has_host() || (!request_url.SchemeIsCryptographic() &&
                                  !net::IsLocalhost(request_url))) {
    return std::nullopt;
  }

  if (!service->IsDomainAllowlisted(request_url.host())) {
    return std::nullopt;
  }

  std::string token = service->GetTokenForDomain(request_url.host());
  if (token.empty()) {
    return std::nullopt;
  }

  // TODO(crbug.com/552904752): Add UMA metrics for invalid header values.
  if (!net::HttpUtil::IsValidHeaderValue(token)) {
    DLOG(WARNING) << "Malformed site token for domain: " << request_url.host();
    return std::nullopt;
  }

  return token;
}

}  // namespace

// static
void SiteTokenHeaderClient::Create(
    base::WeakPtr<SiteTokenProviderService> service,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SiteTokenHeaderClient(std::move(service),
                                                 std::move(target_client))),
      std::move(receiver));
}

SiteTokenHeaderClient::SiteTokenHeaderClient(
    base::WeakPtr<SiteTokenProviderService> service,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client)
    : service_(std::move(service)) {
  if (target_client) {
    target_client_.Bind(std::move(target_client));
    target_client_.set_disconnect_handler(base::BindOnce(
        &SiteTokenHeaderClient::OnTargetDisconnect, base::Unretained(this)));
  }
}

SiteTokenHeaderClient::~SiteTokenHeaderClient() = default;

void SiteTokenHeaderClient::OnBeforeSendHeaders(
    const GURL& request_url,
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  if (target_client_.is_bound()) {
    target_client_->OnBeforeSendHeaders(
        request_url, headers,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                &SiteTokenHeaderClient::OnTargetBeforeSendHeadersComplete,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                request_url, headers),
            net::ERR_FAILED, std::nullopt, std::nullopt));
  } else {
    OnTargetBeforeSendHeadersComplete(std::move(callback), request_url, headers,
                                      net::OK, std::nullopt, std::nullopt);
  }
}

void SiteTokenHeaderClient::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    const std::optional<net::SSLInfo>& ssl_info,
    OnHeadersReceivedCallback callback) {
  if (target_client_.is_bound()) {
    target_client_->OnHeadersReceived(
        headers, remote_endpoint, ssl_info,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback), net::ERR_FAILED, std::nullopt, std::nullopt));
  } else {
    std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
  }
}

void SiteTokenHeaderClient::OnTargetDisconnect() {
  target_client_.reset();
}

void SiteTokenHeaderClient::OnTargetBeforeSendHeadersComplete(
    OnBeforeSendHeadersCallback callback,
    const GURL& request_url,
    const net::HttpRequestHeaders& original_headers,
    int32_t result,
    const std::optional<net::HttpRequestHeaders>& headers,
    std::optional<base::DictValue> extended_net_log_events) {
  if (result != net::OK) {
    std::move(callback).Run(result, std::nullopt, std::nullopt);
    return;
  }

  std::optional<net::HttpRequestHeaders> final_headers = headers;
  if (std::optional<std::string> token =
          MaybeGetTokenForRequest(service_.get(), request_url)) {
    if (!final_headers) {
      final_headers = original_headers;
    }
    final_headers->SetHeader(kChromeSiteTokenHeader, *token);
  }

  std::move(callback).Run(net::OK, final_headers,
                          std::move(extended_net_log_events));
}

}  // namespace site_token_provider
