// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_url_loader_throttle.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/private_verification_tokens/common/private_verification_tokens_metrics.h"
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
    base::WeakPtr<Profile> profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!pvt_service || !url_loader_factory) {
    return nullptr;
  }
  return base::WrapUnique(new PrivateVerificationTokensURLLoaderThrottle(
      pvt_service->GetWeakPtr(), std::move(profile),
      std::move(url_loader_factory)));
}

PrivateVerificationTokensURLLoaderThrottle::
    PrivateVerificationTokensURLLoaderThrottle(
        base::WeakPtr<PrivateVerificationTokensService> pvt_service,
        base::WeakPtr<Profile> profile,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pvt_service_(std::move(pvt_service)),
      profile_(std::move(profile)),
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

  if (!request->request_initiator.has_value()) {
    // PVT feature code is executed only for browser-initiated requests.
    // This metric is intentionally recorded as a raw volume counter rather
    // than a boolean, because the throttle is never instantiated upstream
    // when the feature is disabled. For Origin Trial capacity calculations,
    // this acts as the specific volume numerator. The correct denominator
    // to determine the global experimentation percentage of Chrome traffic
    // is Navigation.MainFrameProfileTypeDifferentPage2, which tracks all
    // cross-document outermost main frame navigations globally.
    base::UmaHistogramExactLinear(
        private_verification_tokens::kFeatureActiveHistogram, 1, 2);
  }

  // Token Issuance: Trigger token fetch if request_initiator is null and not
  // off the record.
  if (!request->request_initiator.has_value() &&
      (profile_ && !profile_->IsOffTheRecord())) {
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
    base::TimeTicks start_time = base::TimeTicks::Now();
    auto token_info = pvt_service_->GetTokenForRedemption(
        *request->trusted_params->isolation_info.top_frame_origin(),
        profile_.get());
    if (token_info.has_value()) {
      base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
      base::UmaHistogramCustomMicrosecondsTimes(
          private_verification_tokens::kTokenAttachTimeHistogram, elapsed,
          base::Microseconds(1), base::Milliseconds(100), 50);

      token_id_ = token_info->first;
      redeemer_origin_ =
          *request->trusted_params->isolation_info.top_frame_origin();
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
      if (profile_ && redeemer_origin_) {
        pvt_service_->TrackerInsert(profile_.get(), *redeemer_origin_);
      }
      pvt_service_->DeleteToken(*token_id_, base::DoNothing());
    }
    token_id_.reset();
    redeemer_origin_.reset();
  }
}

void PrivateVerificationTokensURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (token_id_.has_value()) {
    if (pvt_service_ && response_head &&
        !response_head->pvt_token_removed_due_to_cookies) {
      if (profile_ && redeemer_origin_) {
        pvt_service_->TrackerInsert(profile_.get(), *redeemer_origin_);
      }
      pvt_service_->DeleteToken(*token_id_, base::DoNothing());
    }
    token_id_.reset();
    redeemer_origin_.reset();
  }
}
