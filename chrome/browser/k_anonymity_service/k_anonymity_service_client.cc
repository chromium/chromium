// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

KAnonymityServiceClient::PendingJoinRequest::PendingJoinRequest(
    std::string set_id,
    base::OnceCallback<void(bool)> callback)
    : id(std::move(set_id)),
      request_start(base::Time::Now()),
      callback(std::move(callback)) {}

KAnonymityServiceClient::PendingJoinRequest::~PendingJoinRequest() = default;

KAnonymityServiceClient::KAnonymityServiceClient(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      enable_ohttp_requests_(base::FeatureList::IsEnabled(
          features::kKAnonymityServiceOHTTPRequests)),
      // Pass the auth server origin as if it is our "top frame".
      trust_token_answerer_(url::Origin::Create(GURL(kKAnonymityAuthServer)),
                            profile),
      token_getter_(IdentityManagerFactory::GetForProfile(profile),
                    url_loader_factory_,
                    &trust_token_answerer_) {
  // We are currently relying on callers of this service to limit which users
  // are allowed to use this service. No children should use this service
  // since we are not approved to process their data.
  DCHECK(!profile->IsChild());
}

KAnonymityServiceClient::~KAnonymityServiceClient() = default;

void KAnonymityServiceClient::JoinSet(std::string id,
                                      base::OnceCallback<void(bool)> callback) {
  RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSet);
  // Add to the queue. If this is the only request in the queue, start it.
  join_queue_.push_back(
      std::make_unique<PendingJoinRequest>(std::move(id), std::move(callback)));
  if (join_queue_.size() > 1)
    return;
  JoinSetStartNextQueued();
}

void KAnonymityServiceClient::JoinSetStartNextQueued() {
  DCHECK(!join_queue_.empty());
  // TODO(behamilton): Instead of requesting the trust tokens here, we should
  // check the OHTTP Key first.
  JoinSetCheckTrustTokens();
}

void KAnonymityServiceClient::JoinSetCheckTrustTokens() {
  token_getter_.TryGetTrustTokenAndKey(
      base::BindOnce(&KAnonymityServiceClient::OnMaybeHasTrustTokens,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KAnonymityServiceClient::OnMaybeHasTrustTokens(
    absl::optional<KAnonymityTrustTokenGetter::KeyAndNonUniqueUserId>
        maybe_key_and_id) {
  if (!maybe_key_and_id) {
    FailJoinSetRequests();
    return;
  }

  if (!enable_ohttp_requests_) {
    CompleteJoinSetRequest();
    return;
  }
  // Once we know we have a trust token and have the OHTTP key we can send the
  // request.
  JoinSetSendRequest(std::move(*maybe_key_and_id));
}

void KAnonymityServiceClient::JoinSetSendRequest(
    KAnonymityTrustTokenGetter::KeyAndNonUniqueUserId key_and_id) {
  NOTIMPLEMENTED();
  FailJoinSetRequests();
}

void KAnonymityServiceClient::FailJoinSetRequests() {
  while (!join_queue_.empty()) {
    RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSetRequestFailed);
    DoJoinSetCallback(false);
  }
}

void KAnonymityServiceClient::CompleteJoinSetRequest() {
  RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSetSuccess);
  DoJoinSetCallback(true);
  // If we have a request queued, process that one.
  if (!join_queue_.empty())
    JoinSetStartNextQueued();
}

void KAnonymityServiceClient::DoJoinSetCallback(bool status) {
  DCHECK(!join_queue_.empty());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(join_queue_.front()->callback), status));
  join_queue_.pop_front();
}

void KAnonymityServiceClient::QuerySets(
    std::vector<std::string> set_ids,
    base::OnceCallback<void(std::vector<bool>)> callback) {
  RecordQuerySetAction(KAnonymityServiceQuerySetAction::kQuerySet);
  RecordQuerySetSize(set_ids.size());
  if (!enable_ohttp_requests_) {
    // Trigger a "successful" callback.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<bool>(set_ids.size(), false)));
    return;
  }

  NOTIMPLEMENTED();
  // An empty vector passed to the callback signifies failure.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<bool>()));
}
