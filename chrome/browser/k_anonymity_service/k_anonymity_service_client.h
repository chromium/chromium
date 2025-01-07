// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_storage.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_trust_token_getter.h"
#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/isolation_info.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/oblivious_http_request.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

// This class implements the KAnonymityServiceDelegate by sending requests
// to the Chrome k-anonymity Service. This class will eventually send requests
// over OHTTP in order to anonymize the source of the requests. The requests are
// internally queued and performed serially.
//
// In the case of JoinSet this is necessary because we need
// to ensure that there is a trust token available to attach to the call, and
// the network service does not expose a method to get the count.
//
// In the case of QuerySets, requests are performed serially in order to
// simplify implementation. With only one request out at a time 1) it is clear
// which request the responses are associated with and 2) the limit on the
// number of outstanding requests can be handled by the caller.
//
// IDs passed into the KAnonymityServiceClient will be hashed with SHA256
// to convert them to a fixed size before being sent to the KAnonymity service.
class KAnonymityServiceClient : public content::KAnonymityServiceDelegate,
                                public KeyedService {
 public:
  // The profile must outlive the KAnonymityServiceClient.
  explicit KAnonymityServiceClient(Profile* profile);
  ~KAnonymityServiceClient() override;

  // Implementation of content::KAnonymityServiceDelegate.

  // JoinSet corresponds directly to the JoinSet endpoint provided by the
  // K-anonymity Service endpoint. The endpoint requires us to provide a trust
  // token for rate limiting for each call, so we need to check that before
  // performing the OHTTP request (assuming we have the key).
  //
  // The trust token interface (and network isolation configuration) assume
  // there exists both a "top frame" and a "current frame" for any requests.
  // The KAnonymityServiceClient makes requests in the background so those
  // concepts don't really apply here, but for the purposes of the interface we
  // choose to use the kKAnonymityAuthServer's origin as the origin for both
  // frames. Not only does this ensure that requests that can be cached are
  // cached with the appropriate isolation applied, but also avoids the less
  // common (and less tested) configuration where trust tokens are issued in a
  // third-party frame.
  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override;
  // QuerySets is implemented as multiple calls to the QuerySet endpoint of
  // the K-anonymity Service.
  void QuerySets(std::vector<std::string> set_ids,
                 base::OnceCallback<void(std::vector<bool>)> callback) override;

  base::TimeDelta GetJoinInterval() override;
  base::TimeDelta GetQueryInterval() override;

  // Returns true if the profile is allowed to use the k-anonymity service. This
  // currently checks if the primary profile CanRunChromePrivacySandboxTrials.
  // This is partially to prevent exposing minors' data to the k-anonymity
  // service.
  static bool CanUseKAnonymityService(Profile* profile);

 private:
  struct PendingJoinRequest {
    PendingJoinRequest(std::string set_id,
                       base::OnceCallback<void(bool)> callback);
    ~PendingJoinRequest();
    std::string id;
    base::TimeTicks request_start;
    int retries = 0;
    base::OnceCallback<void(bool)> callback;
  };

  struct PendingQueryRequest {
    PendingQueryRequest(std::vector<std::string> set_id,
                        base::OnceCallback<void(std::vector<bool>)> callback);
    ~PendingQueryRequest();
    std::vector<std::string> ids;
    base::TimeTicks request_start;
    base::OnceCallback<void(std::vector<bool>)> callback;
  };

  // Called when storage is ready to for us to make requests.
  void JoinSetOnStorageReady(KAnonymityServiceStorage::InitStatus status);
  // Starts processing items in the queue by calling JoinSetCheckTrustTokens()
  void JoinSetStartNextQueued();
  // Checks that the cached OHTTP Key is still valid and if not calls
  // RequestJoinSetOHTTPKey to refresh it.
  void JoinSetCheckOHTTPKey();
  // Starts the HTTP fetch to obtain the OHTTP key from the JoinSet endpoint.
  void RequestJoinSetOHTTPKey();
  // Asynchronously called by the join_url_loader_ with a response containing
  // the OHTTP key for the JoinSet endpoint. The key is cached and then
  // JoinSetCheckTrustTokens is called to continue processing the current
  // request.
  void OnGotJoinSetOHTTPKey(std::unique_ptr<std::string> response);

  // Calls the token_getter_ to ensure there is a trust token for the request.
  void JoinSetCheckTrustTokens(OHTTPKeyAndExpiration ohttp_key);
  // Asynchronous callback from token_getter_ which is passed the key commitment
  // and non-unique client ID that are needed to complete JoinSet. If the
  // provided optional is not empty this triggers the JoinSet request.
  void OnMaybeHasTrustTokens(
      OHTTPKeyAndExpiration ohttp_key,
      std::optional<KeyAndNonUniqueUserId> maybe_key_and_id);
  // Starts the OHTTP JoinSet request for the join_queue_.front() request.
  void JoinSetSendRequest(OHTTPKeyAndExpiration ohttp_key,
                          KeyAndNonUniqueUserId key_and_id);
  // Handle the response to the JoinSet request and call CompleteJoinSetRequest
  // if successful.
  void JoinSetOnGotResponse(const std::optional<std::string>& response,
                            int error_code);
  // Calls DoJoinSetCallback indicating the current request completed
  // successfully. If there are other items in the queue calls
  // JoinSetStartNextQueued to start processing them.
  void CompleteJoinSetRequest();
  // Calls DoJoinSetCallback for each of the requests in the join_queue_
  // indicating the requests were unsuccessful.
  void FailJoinSetRequests();
  // Asynchronously calls the callback for the join_queue_.front() request with
  // the provided status and removes it from the queue.
  void DoJoinSetCallback(bool status);

  // Called when storage is ready to for us to make requests.
  void QuerySetsOnStorageReady(KAnonymityServiceStorage::InitStatus status);
  // Checks that the cached OHTTP Key is still valid and if not calls
  // RequestQuerySetOHTTPKey to refresh it.
  void QuerySetsCheckOHTTPKey();
  // Starts the HTTP fetch to obtain the OHTTP key from the QuerySet endpoint.
  void RequestQuerySetOHTTPKey();
  // Asynchronous response containing the OHTTP key for the QuerySet endpoint.
  // The key is cached and then QuerySetsSendRequests is called to continue
  // processing the current request.
  void OnGotQuerySetOHTTPKey(std::unique_ptr<std::string> response);
  // Handles failures in fetching the OHTTP key. Schedules callbacks indicating
  // failure for all requests on the query_queue_.
  void FailFetchingQueryOHTTPKey();
  // Starts the OHTTP QuerySet request for the current QueryRequest.
  void QuerySetsSendRequest(OHTTPKeyAndExpiration ohttp_key);
  // Called as an asynchronous response to the OHTTP request started by
  // QuerySetsSendRequest. Passes the JSON response received to be decoded and
  // handled in QuerySetsOnParsedResponse.
  void QuerySetsOnGotResponse(const std::optional<std::string>& response,
                              int error_code);
  // Called asynchronously when the QuerySet response from
  // QuerySetsOnGotResponse has been decoded.
  void QuerySetsOnParsedResponse(
      data_decoder::DataDecoder::ValueOrError result);
  // Calls DoQuerySetsCallback indicating the current request completed
  // successfully. If there are other items in the queue calls
  // QuerySetsCheckOHTTPKey to start processing them.
  void CompleteQuerySetsRequest(std::vector<bool> result);
  // Called when the request started by QuerySetsSendRequest fails
  // either because of network errors are parse failure. Calls
  // DoQuerySetsCallback for each of the requests in the query_queue_ indicating
  // the requests were unsuccessful.
  void FailQuerySetsRequests();
  // Called by QuerySetsOnParsedResponse and FailQuerySetRequest. Schedules
  // a callback containing the result and starts work on the next request in the
  // query_queue_.
  void DoQuerySetsCallback(std::vector<bool> result);

  // queues
  base::circular_deque<std::unique_ptr<PendingJoinRequest>> join_queue_;
  base::circular_deque<std::unique_ptr<PendingQueryRequest>> query_queue_;

  mojo::UniqueReceiverSet<network::mojom::ObliviousHttpClient>
      ohttp_client_receivers_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> join_url_loader_;
  std::unique_ptr<network::SimpleURLLoader> query_url_loader_;
  bool enable_ohttp_requests_;
  net::IsolationInfo isolation_info_;

  std::unique_ptr<KAnonymityServiceStorage> storage_;
  RemoteTrustTokenQueryAnswerer trust_token_answerer_;
  KAnonymityTrustTokenGetter token_getter_;

  url::Origin join_origin_;
  url::Origin query_origin_;

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<KAnonymityServiceClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_
