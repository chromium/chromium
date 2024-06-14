// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/throttled_gaia_auth_fetcher.h"

#include <vector>

#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/renderer_configuration.mojom-shared.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

ThrottledGaiaAuthFetcher::ThrottledGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
        bound_session_throttler_params,
    std::unique_ptr<BoundSessionRequestThrottledHandler>
        bound_session_request_throttled_handler)
    : GaiaAuthFetcher(consumer, source, std::move(url_loader_factory)),
      bound_session_throttler_params_(
          std::move(bound_session_throttler_params)),
      bound_session_request_throttled_handler_(
          std::move(bound_session_request_throttled_handler)) {
  CHECK(bound_session_request_throttled_handler_);
}

ThrottledGaiaAuthFetcher::~ThrottledGaiaAuthFetcher() = default;

void ThrottledGaiaAuthFetcher::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& body_content_type,
    const std::string& headers,
    const GURL& gaia_gurl,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if ((IsListAccountsUrl(gaia_gurl) || IsMultiloginUrl(gaia_gurl)) &&
      credentials_mode == network::mojom::CredentialsMode::kInclude &&
      GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
          gaia_gurl, bound_session_throttler_params_) ==
          GoogleURLLoaderThrottle::RequestBoundSessionStatus::
              kCoveredWithMissingCookie) {
    bound_session_request_throttled_handler_->HandleRequestBlockedOnCookie(
        gaia_gurl,
        base::BindOnce(
            &ThrottledGaiaAuthFetcher::OnGaiaFetcherResumedOrCancelled,
            weak_ptr_factory_.GetWeakPtr(), body, body_content_type, headers,
            gaia_gurl, credentials_mode, traffic_annotation));
    return;
  }

  GaiaAuthFetcher::CreateAndStartGaiaFetcher(body, body_content_type, headers,
                                             gaia_gurl, credentials_mode,
                                             traffic_annotation);
}

void ThrottledGaiaAuthFetcher::OnGaiaFetcherResumedOrCancelled(
    const std::string& body,
    const std::string& body_content_type,
    const std::string& headers,
    const GURL& gaia_gurl,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    BoundSessionRequestThrottledHandler::UnblockAction unblock_action,
    chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger) {
  switch (unblock_action) {
    case BoundSessionRequestThrottledHandler::UnblockAction::kResume:
      GaiaAuthFetcher::CreateAndStartGaiaFetcher(
          body, body_content_type, headers, gaia_gurl, credentials_mode,
          traffic_annotation);
      break;
    case BoundSessionRequestThrottledHandler::UnblockAction::kCancel:
      DispatchFetchedRequest(gaia_gurl, /*data=*/"", net::ERR_ABORTED,
                             /*response_code=*/0);
      break;
  }
}
