// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_THROTTLED_GAIA_AUTH_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_THROTTLED_GAIA_AUTH_FETCHER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class GaiaAuthConsumer;

namespace network {
class SharedURLLoaderFactory;
}

// A `GaiaAuthFetcher` subclass that applies bound session throttling to
// specific requests.
//
// Currently, only ListAccounts requests are throttled this way.
class ThrottledGaiaAuthFetcher : public GaiaAuthFetcher {
 public:
  ThrottledGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
          bound_session_throttler_params,
      std::unique_ptr<BoundSessionRequestThrottledHandler>
          bound_session_request_throttled_handler);
  ~ThrottledGaiaAuthFetcher() override;

 protected:
  void CreateAndStartGaiaFetcher(
      const std::string& body,
      const std::string& body_content_type,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  void OnGaiaFetcherResumedOrCancelled(
      const std::string& body,
      const std::string& body_content_type,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      BoundSessionRequestThrottledHandler::UnblockAction unblock_action,
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger);

  const std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
      bound_session_throttler_params_;
  const std::unique_ptr<BoundSessionRequestThrottledHandler>
      bound_session_request_throttled_handler_;
  base::WeakPtrFactory<ThrottledGaiaAuthFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_THROTTLED_GAIA_AUTH_FETCHER_H_
