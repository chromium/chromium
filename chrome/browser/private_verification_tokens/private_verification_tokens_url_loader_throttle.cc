// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_url_loader_throttle.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "net/base/features.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// static
std::unique_ptr<PrivateVerificationTokensURLLoaderThrottle>
PrivateVerificationTokensURLLoaderThrottle::Create(
    PrivateVerificationTokensService* pvt_service,
    bool is_off_the_record,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!pvt_service || !url_loader_factory) {
    return nullptr;
  }
  return base::WrapUnique(new PrivateVerificationTokensURLLoaderThrottle(
      pvt_service->GetWeakPtr(), is_off_the_record,
      std::move(url_loader_factory)));
}

PrivateVerificationTokensURLLoaderThrottle::
    PrivateVerificationTokensURLLoaderThrottle(
        base::WeakPtr<PrivateVerificationTokensService> pvt_service,
        bool is_off_the_record,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pvt_service_(std::move(pvt_service)),
      is_off_the_record_(is_off_the_record),
      url_loader_factory_(std::move(url_loader_factory)) {}

PrivateVerificationTokensURLLoaderThrottle::
    ~PrivateVerificationTokensURLLoaderThrottle() = default;

void PrivateVerificationTokensURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens) ||
      !pvt_service_) {
    return;
  }

  // Token header should not already exist, remove it if it does.
  request->headers.RemoveHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken);

  // Token Issuance: Trigger token fetch if request_initiator is null and not
  // off the record.
  if (!request->request_initiator.has_value() && !is_off_the_record_) {
    pvt_service_->MaybeFetchTokens(request->url, url_loader_factory_);
  }

  // Token Redemption: Check redemption conditions.
  const bool top_frame_matches =
      request->trusted_params &&
      request->trusted_params->isolation_info.top_frame_origin().has_value() &&
      *request->trusted_params->isolation_info.top_frame_origin() ==
          url::Origin::Create(request->url);

  if (!request->request_initiator.has_value() &&
      !request->headers.HasHeader(net::HttpRequestHeaders::kCookie) &&
      request->credentials_mode != network::mojom::CredentialsMode::kOmit &&
      request->is_outermost_main_frame && top_frame_matches) {
    auto token_info = pvt_service_->GetTokenForRedemption(
        *request->trusted_params->isolation_info.top_frame_origin());
    if (token_info.has_value()) {
      token_id_ = token_info->first;
      request->headers.SetHeader(
          net::HttpRequestHeaders::kSecPrivateVerificationToken,
          token_info->second);
    }
  }
}

void PrivateVerificationTokensURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  if (headers_update_params) {
    headers_update_params->removed_headers.push_back(
        net::HttpRequestHeaders::kSecPrivateVerificationToken);
  }
  if (token_id_.has_value()) {
    if (pvt_service_ && !response_head.pvt_token_removed_due_to_cookies) {
      pvt_service_->DeleteToken(*token_id_, base::DoNothing());
    }
    token_id_.reset();
  }
}

void PrivateVerificationTokensURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (token_id_.has_value()) {
    if (pvt_service_ && response_head &&
        !response_head->pvt_token_removed_due_to_cookies) {
      pvt_service_->DeleteToken(*token_id_, base::DoNothing());
    }
    token_id_.reset();
  }
}
