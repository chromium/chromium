// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_TRUST_TOKEN_GETTER_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_TRUST_TOKEN_GETTER_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_storage.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/isolation_info.h"
#include "net/http/http_response_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

// This class performs the series of requests necessary to get a trust token
// from the Google K-anonymity server using hard-coded URLs. The trust token is
// stored by the network service, while the non-unique user ID and trust token
// key commitment are provided to the class's caller. The steps of this are:
// 1. Get an OAuth token with the K-anonymity service scope.
// 2. Request a non-unique user ID (short ID) from the k-anonymity auth server.
//    This requires the OAuth token for authentication.
// 3. Request the trust token key commitment corresponding to the short bucket
//    ID. Note that the receiver responds in a non-standard format that the
//    browser converts internally.
// 4. Request the trust token. This requires the OAuth token for authentication.
//
// The short ID and the trust token key commitment expire once every 24 hours.
// The trust token will be good until it is used or the key that created
// expires.
class KAnonymityTrustTokenGetter {
 public:
  // Callback where argument tells if the client has the trust token or not.
  using TryGetTrustTokenAndKeyCallback =
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>;

  // `identity_manager` and `answerer` must outlive the current instance
  KAnonymityTrustTokenGetter(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::TrustTokenQueryAnswerer* answerer,
      KAnonymityServiceStorage* storage);

  ~KAnonymityTrustTokenGetter();

  // Check if we have a trust token and key commitment ready for use. If not,
  // tries to request the trust token and key commitment. The trust token will
  // be stored in the network service, while the key commitment and non-unique
  // user ID are cached. Calls the callback with the key commitment and
  // non-uniform user ID if successful and std::nullopt if not successful.
  // Calls to this function are serialized through the `pending_callbacks_`
  // queue to avoid making redundant requests -- since most of the request
  // results can be cached and reused. Since this function may need to get an
  // OAuth token, it needs to only be called from the browser UI thread. To
  // minimize the risk of a racing thread using the trust token the callback
  // should immediately initiate the request that uses the trust token.
  void TryGetTrustTokenAndKey(TryGetTrustTokenAndKeyCallback callback);

 private:
  struct PendingRequest {
    explicit PendingRequest(TryGetTrustTokenAndKeyCallback callback);
    ~PendingRequest();
    PendingRequest(PendingRequest&&) noexcept;
    PendingRequest& operator=(PendingRequest&&) noexcept;
    base::TimeTicks request_start;
    TryGetTrustTokenAndKeyCallback callback;
  };
  // Entry point for processing the request on the front of the queue.
  void TryGetTrustTokenAndKeyInternal();

  // Checks if `this` already has a cached non-expired token, if not try to get
  // the access token.
  void CheckAccessToken();
  // Calls the IdentityManager asynchronously to request the access token.
  void RequestAccessToken();
  // Gets the access token and caches the result.
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  // Checks if `this` already has a cached non-expired key commitment, if not
  // triggers getting the non-unique user ID and key commitment.
  void CheckTrustTokenKeyCommitment();
  // Starts the HTTP request for the non-unique user ID.
  void FetchNonUniqueUserId();
  // Passes the non-unique user ID response body to the JSON parser.
  void OnFetchedNonUniqueUserId(std::unique_ptr<std::string> response);
  // Extracts the non-unique user ID from the decoded JSON and triggers fetching
  // the key commitment.
  void OnParsedNonUniqueUserId(data_decoder::DataDecoder::ValueOrError result);
  // Starts the HTTP request for the trust token key commitment.
  void FetchTrustTokenKeyCommitment(int non_unique_user_id);
  // Passes the trust token key commitment response body to the JSON parser.
  void OnFetchedTrustTokenKeyCommitment(int non_unique_user_id,
                                        std::unique_ptr<std::string> response);
  // Extracts the trust token key commitment from the custom response structure
  // provided by the Google k-anonymity server and reformats it into the V3
  // trust token key commitment format expected by the network service.
  void OnParsedTrustTokenKeyCommitment(
      int non_unique_user_id,
      data_decoder::DataDecoder::ValueOrError result);

  // Asynchronously queries the network service to see if there is at least one
  // trust token.
  void CheckTrustTokens();
  // Triggers fetching a trust token if we don't have one.
  void OnHasTrustTokensComplete(network::mojom::HasTrustTokensResultPtr result);
  // Starts the HTTP request to fetch the trust token.
  void FetchTrustToken();
  // Completes the request if the trust token was fetched successfully.
  void OnFetchedTrustToken(scoped_refptr<net::HttpResponseHeaders> headers);

  // Calls the callbacks for all queued requests indicating failure.
  void FailAllCallbacks();
  // Calls the callback for the front request in the queue indicating success.
  void CompleteOneRequest();
  // Calls the callback for the request indicating the provided status.
  void DoCallback(bool status);

  signin::AccessTokenInfo access_token_;
  base::circular_deque<PendingRequest> pending_callbacks_;

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  raw_ptr<network::mojom::TrustTokenQueryAnswerer> trust_token_query_answerer_;
  raw_ptr<KAnonymityServiceStorage> storage_;
  net::IsolationInfo isolation_info_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  url::Origin auth_origin_;

  base::WeakPtrFactory<KAnonymityTrustTokenGetter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_TRUST_TOKEN_GETTER_H_
