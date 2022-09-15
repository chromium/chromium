// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_trust_token_getter.h"
#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

// This class will implement the KAnonymityServiceDelegate by sending requests
// to the Chrome k-anonymity Service. For now this class only requests the
// trust token. In the future this class will send requests over OHTTP in order
// to anonymize the source of the requests. The requests are internally queued
// and performed serially.
//
// In the case of JoinSet this is necessary because we need
// to ensure that there is a trust token available to attach to the call, and
// the network service does not expose a method to get the count.
//
// In the case of QuerySets, requests are performed serially in order to
// simplify implementation. With only one request out at a time 1) it is clear
// which request the responses are associated with and 2) the limit on the
// number of outstanding requests can be handled by the caller.
class KAnonymityServiceClient : public content::KAnonymityServiceDelegate {
 public:
  // The profile must outlive the KAnonymityServiceClient.
  explicit KAnonymityServiceClient(Profile* profile);
  ~KAnonymityServiceClient() override;

  // Implementation of content::KAnonymityServiceDelegate.

  // JoinSet corresponds directly to the JoinSet endpoint provided by the
  // K-anonymity Service endpoint. The endpoint requires us to provide a trust
  // token for rate limiting for each call, so we need to check that before
  // performing the OHTTP request (assuming we have the key). For now this
  // function only fetches the trust token without performing the request.
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
  // QuerySets will be implemented as multiple calls to the QuerySet endpoint of
  // the K-anonymity Service. For now no requests are made and this function
  // just marks all of the requested sets as not k-anonymous.
  void QuerySets(std::vector<std::string> set_ids,
                 base::OnceCallback<void(std::vector<bool>)> callback) override;

 private:
  struct PendingJoinRequest {
    PendingJoinRequest(std::string set_id,
                       base::OnceCallback<void(bool)> callback);
    ~PendingJoinRequest();
    std::string id;
    base::Time request_start;
    base::OnceCallback<void(bool)> callback;
  };

  // Starts processing items in the queue by calling JoinSetCheckTrustTokens()
  void JoinSetStartNextQueued();
  // Calls the token_getter_ to ensure there is a trust token for the request.
  void JoinSetCheckTrustTokens();
  // Asynchronous callback from token_getter_ which is passed the key commitment
  // and non-unique client ID that are needed to complete JoinSet. If the
  // provided optional is not empty this triggers the JoinSet request.
  void OnMaybeHasTrustTokens(
      absl::optional<KAnonymityTrustTokenGetter::KeyAndNonUniqueUserId>
          maybe_key_and_id);
  // In the future, this will start the OHTTP JoinSet request for the
  // join_queue_.front() request, but for now just calls CompleteJoinSetRequest.
  void JoinSetSendRequest(
      KAnonymityTrustTokenGetter::KeyAndNonUniqueUserId key_and_id);
  // Calls DoJoinSetCallback indicating the current request completed
  // successfully. If there are other items in the queue calls
  // JoinSetStartNextQueued to start processing them.
  void FailJoinSetRequests();
  // Calls DoJoinSetCallback for each of the requests in the join_queue_
  // indicating the requests were unsuccessful.
  void CompleteJoinSetRequest();
  // Asynchronously calls the callback for the join_queue_.front() request with
  // the provided status and removes it from the queue.
  void DoJoinSetCallback(bool status);

  base::circular_deque<std::unique_ptr<PendingJoinRequest>> join_queue_;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const bool enable_ohttp_requests_;
  RemoteTrustTokenQueryAnswerer trust_token_answerer_;
  KAnonymityTrustTokenGetter token_getter_;

  base::WeakPtrFactory<KAnonymityServiceClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_CLIENT_H_
