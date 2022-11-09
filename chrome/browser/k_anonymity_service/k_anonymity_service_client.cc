// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace {

constexpr base::TimeDelta kRequestTimeout = base::Seconds(5);
constexpr base::TimeDelta kRequestMargin = base::Minutes(5);
constexpr base::TimeDelta kKeyCacheDuration = base::Hours(4);
constexpr int kMaxRetries = 5;
constexpr size_t kMaxQueueSize = 100;

const char kKAnonType[] = "fledge";

// TODO(behamilton): Change description once indirect (OHTTP) requests are
// supported.
constexpr net::NetworkTrafficAnnotationTag
    kKAnonymityServiceJoinSetTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("k_anonymity_service_join_set",
                                            R"(
    semantics {
      sender: "Chrome k-Anonymity Service Client"
      description:
        "Request to the Chrome k-Anonymity JoinSet server to notify it of use "
        "of a k-anonymity protected element."
      trigger:
        "Use of a k-anonymity protected element."
      data:
        "Hash of a feature protected by k-anonymity, such as the URL of a "
        "FLEDGE ad. Also contains a trust token issued by the k-Anonymity Auth "
        "server."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "Disable features using k-anonymity, such as FLEDGE and Attribution "
        "Reporting."
      chrome_policy {
      }
    }
    comments:
      ""
    )");

// TODO(behamilton): Change description once indirect (OHTTP) requests are
// supported.
constexpr net::NetworkTrafficAnnotationTag
    kKAnonymityServiceQuerySetTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("k_anonymity_service_query_set",
                                            R"(
    semantics {
      sender: "Chrome k-Anonymity Service Client"
      description:
        "Request to the Chrome k-Anonymity JoinSet server to query if "
        "k-anonymity protected element is k-anonymous. These results are "
        "typically cached."
      trigger:
        "Expected use of a k-anonymity protected element."
      data:
        "Hash of a feature protected by k-anonymity, such as the URL of a "
        "FLEDGE ad."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "Disable features using k-anonymity, such as FLEDGE and Attribution "
        "Reporting."
      chrome_policy {
      }
    }
    comments:
      ""
    )");

// KAnonObliviousHttpClient accepts OnCompleted calls and forwards them to the
// provided callback. It also calls the callback if it is destroyed before the
// callback is called.
class KAnonObliviousHttpClient : public network::mojom::ObliviousHttpClient {
 public:
  using OnCompletedCallback =
      base::OnceCallback<void(const absl::optional<std::string>&, int)>;

  explicit KAnonObliviousHttpClient(OnCompletedCallback callback)
      : callback_(std::move(callback)) {}

  ~KAnonObliviousHttpClient() override {
    if (!called_) {
      std::move(callback_).Run(absl::nullopt, net::ERR_FAILED);
    }
  }

  void OnCompleted(const absl::optional<std::string>& response,
                   int net_error) override {
    if (called_) {
      mojo::ReportBadMessage("OnCompleted called more than once");
      return;
    }
    called_ = true;
    std::move(callback_).Run(response, net_error);
  }

 private:
  bool called_ = false;
  OnCompletedCallback callback_;
};

}  // namespace

KAnonymityServiceClient::PendingJoinRequest::PendingJoinRequest(
    std::string set_id,
    base::OnceCallback<void(bool)> callback)
    : id(std::move(set_id)),
      request_start(base::TimeTicks::Now()),
      callback(std::move(callback)) {}

KAnonymityServiceClient::PendingJoinRequest::~PendingJoinRequest() = default;

KAnonymityServiceClient::PendingQueryRequest::PendingQueryRequest(
    std::vector<std::string> set_ids,
    base::OnceCallback<void(std::vector<bool>)> callback)
    : ids(std::move(set_ids)),
      request_start(base::TimeTicks::Now()),
      callback(std::move(callback)) {}

KAnonymityServiceClient::PendingQueryRequest::~PendingQueryRequest() = default;

KAnonymityServiceClient::KAnonymityServiceClient(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      enable_ohttp_requests_(base::FeatureList::IsEnabled(
          features::kKAnonymityServiceOHTTPRequests)),
      // Pass the auth server origin as if it is our "top frame".
      trust_token_answerer_(url::Origin::Create(GURL(
                                features::kKAnonymityServiceAuthServer.Get())),
                            profile),
      token_getter_(IdentityManagerFactory::GetForProfile(profile),
                    url_loader_factory_,
                    &trust_token_answerer_),
      profile_(profile) {
  // We are currently relying on callers of this service to limit which users
  // are allowed to use this service. No children should use this service
  // since we are not approved to process their data.
  DCHECK(!profile->IsChild());
  join_origin_ =
      url::Origin::Create(GURL(features::kKAnonymityServiceJoinServer.Get()));
  DCHECK(!join_origin_.opaque());
  query_origin_ =
      url::Origin::Create(GURL(features::kKAnonymityServiceQueryServer.Get()));
  DCHECK(!query_origin_.opaque());
}

KAnonymityServiceClient::~KAnonymityServiceClient() = default;

void KAnonymityServiceClient::JoinSet(std::string id,
                                      base::OnceCallback<void(bool)> callback) {
  RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSet);

  // Fail immediately if the queue is full.
  if (join_queue_.size() >= kMaxQueueSize) {
    RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSetQueueFull);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Add to the queue. If this is the only request in the queue, start it.
  join_queue_.push_back(
      std::make_unique<PendingJoinRequest>(std::move(id), std::move(callback)));
  if (join_queue_.size() > 1)
    return;
  JoinSetStartNextQueued();
}

void KAnonymityServiceClient::JoinSetStartNextQueued() {
  DCHECK(!join_queue_.empty());
  JoinSetCheckOHTTPKey();
}

void KAnonymityServiceClient::JoinSetCheckOHTTPKey() {
  // We need the OHTTP key to send the OHTTP request.
  if (enable_ohttp_requests_ && joinset_ohttp_key_with_expiration_.expiration <=
                                    base::Time::Now() + kRequestMargin) {
    RequestJoinSetOHTTPKey();
    return;
  }
  JoinSetCheckTrustTokens();
}

void KAnonymityServiceClient::RequestJoinSetOHTTPKey() {
  RecordJoinSetAction(KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = join_origin_.GetURL().Resolve(
      base::StrCat({kJoinSetOhttpPath, google_apis::GetAPIKey()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = isolation_info_;
  join_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kKAnonymityServiceJoinSetTrafficAnnotation);
  join_url_loader_->SetTimeoutDuration(kRequestTimeout);
  join_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KAnonymityServiceClient::OnGotJoinSetOHTTPKey,
                     weak_ptr_factory_.GetWeakPtr()),
      /*max_body_size=*/1024);
}

void KAnonymityServiceClient::OnGotJoinSetOHTTPKey(
    std::unique_ptr<std::string> response) {
  join_url_loader_.reset();
  if (!response) {
    RecordJoinSetAction(
        KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKeyFailed);
    FailJoinSetRequests();
    return;
  }

  joinset_ohttp_key_with_expiration_ =
      OHTTPKeyAndExpiration{*response, base::Time::Now() + kKeyCacheDuration};
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
  RecordJoinSetAction(KAnonymityServiceJoinSetAction::kSendJoinSetRequest);
  std::string hashed_id = crypto::SHA256HashString(join_queue_.front()->id);
  std::string encoded_id;
  base::Base64UrlEncode(hashed_id, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_id);

  network::mojom::ObliviousHttpRequestPtr request =
      network::mojom::ObliviousHttpRequest::New();
  request->relay_url = GURL(features::kKAnonymityServiceJoinRelayServer.Get());
  request->traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kKAnonymityServiceJoinSetTrafficAnnotation);
  request->key_config = joinset_ohttp_key_with_expiration_.key;

  request->resource_url = join_origin_.GetURL().Resolve(
      base::StringPrintf(kJoinSetPathFmt, kKAnonType, encoded_id.c_str(),
                         google_apis::GetAPIKey().c_str()));
  request->method = net::HttpRequestHeaders::kPostMethod;

  std::string payload = base::StringPrintf(
      "{name: 'type/%s/sets/%s', shortClientIdentifier: %d}", kKAnonType,
      encoded_id.c_str(), key_and_id.non_unique_user_id);

  request->request_body = network::mojom::ObliviousHttpRequestBody::New(
      payload, /*content_type=*/"application/json");

  // We want to send the redemption request to the join_origin, but the tokens
  // are scoped to auth_origin. That means we need to specify auth_origin as the
  // issuer.
  url::Origin auth_origin =
      url::Origin::Create(GURL(features::kKAnonymityServiceAuthServer.Get()));
  network::mojom::TrustTokenParamsPtr params =
      network::mojom::TrustTokenParams::New();
  params->type = network::mojom::TrustTokenOperationType::kRedemption;
  params->refresh_policy = network::mojom::TrustTokenRefreshPolicy::kRefresh;
  params->custom_key_commitment = key_and_id.key_commitment;
  params->custom_issuer = auth_origin;
  params->issuers.push_back(auth_origin);

  request->trust_token_params = std::move(params);

  mojo::PendingReceiver<network::mojom::ObliviousHttpClient> pending_receiver;
  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetViaObliviousHttp(std::move(request),
                            pending_receiver.InitWithNewPipeAndPassRemote());
  ohttp_client_receivers_.Add(
      std::make_unique<KAnonObliviousHttpClient>(
          base::BindOnce(&KAnonymityServiceClient::JoinSetOnGotResponse,
                         weak_ptr_factory_.GetWeakPtr())),
      std::move(pending_receiver));
}

void KAnonymityServiceClient::JoinSetOnGotResponse(
    const absl::optional<std::string>& response,
    int error_code) {
  if (error_code != net::OK) {
    // If failure was because we didn't have the trust token (it was used before
    // we could get it) then retry. We don't need to back off because getting
    // this error implies that the server is not overloaded.
    if (error_code == net::ERR_TRUST_TOKEN_OPERATION_FAILED &&
        join_queue_.front()->retries++ < kMaxRetries) {
      JoinSetCheckTrustTokens();
      return;
    }
    RecordJoinSetAction(KAnonymityServiceJoinSetAction::kJoinSetRequestFailed);
    FailJoinSetRequests();
    return;
  }

  // Only record latency for successful requests.
  RecordJoinSetLatency(join_queue_.front()->request_start,
                       base::TimeTicks::Now());
  CompleteJoinSetRequest();
}

void KAnonymityServiceClient::FailJoinSetRequests() {
  while (!join_queue_.empty()) {
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(join_queue_.front()->callback), status));
  join_queue_.pop_front();
}

void KAnonymityServiceClient::QuerySets(
    std::vector<std::string> set_ids,
    base::OnceCallback<void(std::vector<bool>)> callback) {
  RecordQuerySetAction(KAnonymityServiceQuerySetAction::kQuerySet);
  RecordQuerySetSize(set_ids.size());

  // Fail immediately if the queue is full.
  if (query_queue_.size() >= kMaxQueueSize || set_ids.empty()) {
    RecordQuerySetAction(KAnonymityServiceQuerySetAction::kQuerySetQueueFull);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<bool>()));
    return;
  }

  if (!enable_ohttp_requests_) {
    // Trigger a "successful" callback.
    RecordQuerySetAction(KAnonymityServiceQuerySetAction::kQuerySetsSuccess);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<bool>(set_ids.size(), false)));
    return;
  }

  query_queue_.push_back(std::make_unique<PendingQueryRequest>(
      std::move(set_ids), std::move(callback)));
  // We only process one query at a time for simplicity.
  if (query_queue_.size() > 1)
    return;
  QuerySetsCheckOHTTPKey();
}

void KAnonymityServiceClient::QuerySetsCheckOHTTPKey() {
  if (queryset_ohttp_key_with_expiration_.expiration <=
      base::Time::Now() + kRequestMargin) {
    RequestQuerySetOHTTPKey();
    return;
  }
  QuerySetsSendRequest();
}

void KAnonymityServiceClient::RequestQuerySetOHTTPKey() {
  DCHECK(!query_url_loader_);
  RecordQuerySetAction(KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = query_origin_.GetURL().Resolve(
      base::StrCat({kQuerySetOhttpPath, google_apis::GetAPIKey()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = isolation_info_;
  query_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kKAnonymityServiceQuerySetTrafficAnnotation);
  query_url_loader_->SetTimeoutDuration(kRequestTimeout);

  query_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KAnonymityServiceClient::OnGotQuerySetOHTTPKey,
                     weak_ptr_factory_.GetWeakPtr()),
      /*max_body_size=*/1024);
}

void KAnonymityServiceClient::OnGotQuerySetOHTTPKey(
    std::unique_ptr<std::string> response) {
  query_url_loader_.reset();
  if (!response) {
    RecordQuerySetAction(
        KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKeyFailed);
    FailQuerySetsRequests();
    return;
  }

  queryset_ohttp_key_with_expiration_ =
      OHTTPKeyAndExpiration{*response, base::Time::Now() + kKeyCacheDuration};
  QuerySetsSendRequest();
}

void KAnonymityServiceClient::QuerySetsSendRequest() {
  DCHECK(!query_url_loader_);
  RecordQuerySetAction(KAnonymityServiceQuerySetAction::kSendQuerySetRequest);

  // Request looks like this:
  // { setsForType: [
  //  { type: "t1", hashes: ["a", "b", "c"]},
  //  { type: "t1", hashes: ["d', "e", "f", "f", "c"]},
  //  { type: "t2"}
  // ]}

  base::Value::List request_hashes;
  for (const auto& id : query_queue_.front()->ids) {
    std::string hashed_id = crypto::SHA256HashString(id);
    std::string encoded_name;
    base::Base64Encode(hashed_id, &encoded_name);
    request_hashes.Append(encoded_name);
  }
  base::Value::Dict sets_for_type;
  sets_for_type.Set("type", kKAnonType);
  sets_for_type.Set("hashes", std::move(request_hashes));

  base::Value::List types;
  types.Append(std::move(sets_for_type));

  base::Value::Dict request_dict;
  request_dict.Set("setsForType", std::move(types));

  std::string request_body;
  base::JSONWriter::Write(request_dict, &request_body);

  network::mojom::ObliviousHttpRequestPtr request =
      network::mojom::ObliviousHttpRequest::New();
  request->relay_url = GURL(features::kKAnonymityServiceQueryRelayServer.Get());
  request->traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kKAnonymityServiceQuerySetTrafficAnnotation);
  request->key_config = queryset_ohttp_key_with_expiration_.key;

  request->resource_url = query_origin_.GetURL().Resolve(
      base::StrCat({kQuerySetsPath, google_apis::GetAPIKey()}));
  request->method = net::HttpRequestHeaders::kPostMethod;

  request->request_body = network::mojom::ObliviousHttpRequestBody::New(
      request_body, /*content_type=*/"application/json");

  mojo::PendingReceiver<network::mojom::ObliviousHttpClient> pending_receiver;
  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetViaObliviousHttp(std::move(request),
                            pending_receiver.InitWithNewPipeAndPassRemote());
  ohttp_client_receivers_.Add(
      std::make_unique<KAnonObliviousHttpClient>(
          base::BindOnce(&KAnonymityServiceClient::QuerySetsOnGotResponse,
                         weak_ptr_factory_.GetWeakPtr())),
      std::move(pending_receiver));
}

void KAnonymityServiceClient::QuerySetsOnGotResponse(
    const absl::optional<std::string>& response,
    int error_code) {
  if (error_code != net::OK) {
    RecordQuerySetAction(
        KAnonymityServiceQuerySetAction::kQuerySetRequestFailed);
    FailQuerySetsRequests();
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&KAnonymityServiceClient::QuerySetsOnParsedResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KAnonymityServiceClient::QuerySetsOnParsedResponse(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    RecordQuerySetAction(
        KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
    FailQuerySetsRequests();
    return;
  }

  // Response has the form:
  // { kAnonymousSets: [
  //  { type: "t1", hashes: ["c", "f"]}
  // ]}

  const base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    RecordQuerySetAction(
        KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
    FailQuerySetsRequests();
    return;
  }

  const base::Value::List* set_types =
      response_dict->FindList("kAnonymousSets");
  if (!set_types) {
    RecordQuerySetAction(
        KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
    FailQuerySetsRequests();
    return;
  }

  std::vector<std::string> returned_hashes;
  for (const auto& set_type_value : *set_types) {
    const base::Value::Dict* set_type_dict = set_type_value.GetIfDict();
    if (!set_type_dict) {
      RecordQuerySetAction(
          KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
      FailQuerySetsRequests();
      return;
    }

    const std::string* type = set_type_dict->FindString("type");
    if (!type || *type != kKAnonType) {
      RecordQuerySetAction(
          KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
      FailQuerySetsRequests();
      return;
    }

    const base::Value::List* hashes = set_type_dict->FindList("hashes");
    if (!hashes) {
      RecordQuerySetAction(
          KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
      FailQuerySetsRequests();
      return;
    }

    for (const base::Value& val : *hashes) {
      const std::string* string_val = val.GetIfString();
      if (!string_val) {
        RecordQuerySetAction(
            KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
        FailQuerySetsRequests();
        return;
      }
      std::string decoded_value;
      if (!base::Base64Decode(*string_val, &decoded_value)) {
        FailQuerySetsRequests();
        RecordQuerySetAction(
            KAnonymityServiceQuerySetAction::kQuerySetRequestParseError);
        return;
      }
      returned_hashes.emplace_back(std::move(decoded_value));
    }
  }

  base::flat_set<std::string> k_anon_set(std::move(returned_hashes));
  std::vector<bool> output;
  output.reserve(query_queue_.front()->ids.size());
  for (const auto& id : query_queue_.front()->ids) {
    std::string hashed_id = crypto::SHA256HashString(id);
    output.push_back(k_anon_set.contains(hashed_id));
  }

  // Only record latency for successful requests.
  RecordQuerySetLatency(query_queue_.front()->request_start,
                        base::TimeTicks::Now());
  CompleteQuerySetsRequest(std::move(output));
}

void KAnonymityServiceClient::CompleteQuerySetsRequest(
    std::vector<bool> result) {
  RecordQuerySetAction(KAnonymityServiceQuerySetAction::kQuerySetsSuccess);
  DoQuerySetsCallback(std::move(result));
  if (!query_queue_.empty()) {
    QuerySetsCheckOHTTPKey();
  }
}

void KAnonymityServiceClient::FailQuerySetsRequests() {
  // Callback with empty result indicating failure.
  while (!query_queue_.empty()) {
    DoQuerySetsCallback(std::vector<bool>());
  }
}

void KAnonymityServiceClient::DoQuerySetsCallback(std::vector<bool> result) {
  DCHECK(!query_queue_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(query_queue_.front()->callback),
                                std::move(result)));
  query_queue_.pop_front();
}

base::TimeDelta KAnonymityServiceClient::GetJoinInterval() {
  return features::kKAnonymityServiceJoinInterval.Get();
}

base::TimeDelta KAnonymityServiceClient::GetQueryInterval() {
  return features::kKAnonymityServiceQueryInterval.Get();
}
