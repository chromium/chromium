// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_trust_token_getter.h"

#include <string_view>

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/numerics/checked_math.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
constexpr base::TimeDelta kRequestMargin = base::Minutes(5);
constexpr base::TimeDelta kRequestTimeout = base::Minutes(1);

constexpr net::NetworkTrafficAnnotationTag
    kKAnonymityServiceGetTokenTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("k_anonymity_service_get_token",
                                            R"(
    semantics {
      sender: "Chrome k-Anonymity Service Client"
      description:
        "Request to the Chrome k-Anonymity Auth server to obtain a trust token"
      trigger:
        "The Chrome k-Anonymity Service Client is out of trust tokens"
      data:
        "Chrome sign-in OAuth Token"
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

// Pull a number that may be an unsigned 32 bit integer from Dict and return it
// as an int. If the number is less than int32_t max then the JSON parser will
// store it as an int, otherwise it will be stored as a double. Check that the
// double would fit in a 32 bit integer exactly before returning.
std::optional<uint32_t> FindUnsignedInt(base::Value::Dict& dict,
                                        std::string_view field) {
  const base::Value* found = dict.Find(field);
  if (!found) {
    return std::nullopt;
  }
  switch (found->type()) {
    case base::Value::Type::INTEGER: {
      // Convert to uint32_t. This is an Id, not a number so this is okay.
      return static_cast<uint32_t>(found->GetInt());
    }
    case base::Value::Type::DOUBLE: {
      double double_value = found->GetDouble();
      if (std::floor(double_value) != double_value) {
        return std::nullopt;
      }
      // If it's a floating point number we still require it to fit in a
      // uint32_t.
      if (!base::IsValueInRangeForNumericType<uint32_t>(double_value)) {
        return std::nullopt;
      }
      return double_value;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace

KAnonymityTrustTokenGetter::PendingRequest::PendingRequest(
    KAnonymityTrustTokenGetter::TryGetTrustTokenAndKeyCallback callback)
    : request_start(base::TimeTicks::Now()), callback(std::move(callback)) {}

KAnonymityTrustTokenGetter::PendingRequest::~PendingRequest() = default;

KAnonymityTrustTokenGetter::PendingRequest::PendingRequest(
    KAnonymityTrustTokenGetter::PendingRequest&&) noexcept = default;
KAnonymityTrustTokenGetter::PendingRequest&
KAnonymityTrustTokenGetter::PendingRequest::operator=(
    KAnonymityTrustTokenGetter::PendingRequest&&) noexcept = default;

KAnonymityTrustTokenGetter::KAnonymityTrustTokenGetter(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::TrustTokenQueryAnswerer* answerer,
    KAnonymityServiceStorage* storage)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      trust_token_query_answerer_(answerer),
      storage_(storage) {
  auth_origin_ =
      url::Origin::Create(GURL(features::kKAnonymityServiceAuthServer.Get()));
  isolation_info_ = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, auth_origin_, auth_origin_,
      net::SiteForCookies());
}

KAnonymityTrustTokenGetter::~KAnonymityTrustTokenGetter() = default;

void KAnonymityTrustTokenGetter::TryGetTrustTokenAndKey(
    TryGetTrustTokenAndKeyCallback callback) {
  if ((!base::FeatureList::IsEnabled(network::features::kPrivateStateTokens) &&
       !base::FeatureList::IsEnabled(network::features::kFledgePst)) ||
      !identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey);
  bool currently_fetching = pending_callbacks_.size() > 0;
  pending_callbacks_.emplace_back(std::move(callback));
  if (currently_fetching)
    return;
  TryGetTrustTokenAndKeyInternal();
}

// This function is where we start for each queued request.
void KAnonymityTrustTokenGetter::TryGetTrustTokenAndKeyInternal() {
  CheckAccessToken();
}

void KAnonymityTrustTokenGetter::CheckAccessToken() {
  if (access_token_.expiration_time <= base::Time::Now() + kRequestMargin) {
    RequestAccessToken();
    return;
  }
  CheckTrustTokenKeyCommitment();
}

void KAnonymityTrustTokenGetter::RequestAccessToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kRequestAccessToken);

  // Choose scopes to obtain for the access token.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kKAnonymityServiceOAuth2Scope);

  // Choose the mode in which to fetch the access token:
  // see AccessTokenFetcher::Mode below for definitions.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the fetcher.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"KAnonymityService", identity_manager_, scopes,
          base::BindOnce(
              &KAnonymityTrustTokenGetter::OnAccessTokenRequestCompleted,
              weak_ptr_factory_.GetWeakPtr()),
          mode, signin::ConsentLevel::kSignin);
}

void KAnonymityTrustTokenGetter::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kAccessTokenRequestFailed);
    FailAllCallbacks();
    return;
  }

  access_token_ = access_token_info;
  CheckTrustTokenKeyCommitment();
}

void KAnonymityTrustTokenGetter::CheckTrustTokenKeyCommitment() {
  std::optional<KeyAndNonUniqueUserIdWithExpiration> key_commitment =
      storage_->GetKeyAndNonUniqueUserId();
  if (!key_commitment ||
      key_commitment->expiration <= base::Time::Now() + kRequestMargin) {
    FetchNonUniqueUserId();
    return;
  }
  CheckTrustTokens();
}

void KAnonymityTrustTokenGetter::FetchNonUniqueUserId() {
  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      auth_origin_.GetURL().Resolve(kGenNonUniqueUserIdPath);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_.token}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = isolation_info_;
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kKAnonymityServiceGetTokenTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kRequestTimeout);
  // The response should just contain a JSON message containing a uint32.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KAnonymityTrustTokenGetter::OnFetchedNonUniqueUserId,
                     weak_ptr_factory_.GetWeakPtr()),
      /*max_body_size=*/1024);
}

void KAnonymityTrustTokenGetter::OnFetchedNonUniqueUserId(
    std::unique_ptr<std::string> response) {
  url_loader_.reset();
  if (!response) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDFailed);
    FailAllCallbacks();
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&KAnonymityTrustTokenGetter::OnParsedNonUniqueUserId,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KAnonymityTrustTokenGetter::OnParsedNonUniqueUserId(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDParseError);
    FailAllCallbacks();
    return;
  }

  base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDParseError);
    FailAllCallbacks();
    return;
  }

  std::optional<int> maybe_non_unique_user_id =
      response_dict->FindInt("shortClientIdentifier");
  if (!maybe_non_unique_user_id || *maybe_non_unique_user_id < 0) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDParseError);
    FailAllCallbacks();
    return;
  }

  FetchTrustTokenKeyCommitment(*maybe_non_unique_user_id);
}

void KAnonymityTrustTokenGetter::FetchTrustTokenKeyCommitment(
    int non_unique_user_id) {
  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = auth_origin_.GetURL().Resolve(base::StringPrintf(
      kFetchKeysPathFmt, non_unique_user_id, google_apis::GetAPIKey().c_str()));
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kOmit;  // No credentials required for
                                               // key fetch.
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = isolation_info_;
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kKAnonymityServiceGetTokenTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &KAnonymityTrustTokenGetter::OnFetchedTrustTokenKeyCommitment,
          weak_ptr_factory_.GetWeakPtr(), non_unique_user_id),
      /*max_body_size=*/4096);
}

void KAnonymityTrustTokenGetter::OnFetchedTrustTokenKeyCommitment(
    int non_unique_user_id,
    std::unique_ptr<std::string> response) {
  if (url_loader_->NetError() != net::OK) {
    url_loader_.reset();
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyFailed);
    FailAllCallbacks();
    return;
  }
  url_loader_.reset();

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(
          &KAnonymityTrustTokenGetter::OnParsedTrustTokenKeyCommitment,
          weak_ptr_factory_.GetWeakPtr(), non_unique_user_id));
}

// The server sends the key commitment in a custom message format. We have to
// reformat the response from the server into a format the browser understands
// (V3 trust token key commitment). See the explainer here:
// https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md
void KAnonymityTrustTokenGetter::OnParsedTrustTokenKeyCommitment(
    int non_unique_user_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  std::string* maybe_version = response_dict->FindString("protocolVersion");
  if (!maybe_version) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  const std::optional<int> maybe_id = response_dict->FindInt("id");
  if (!maybe_id || *maybe_id < 0) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  const std::optional<int> maybe_batchsize =
      response_dict->FindInt("batchSize");
  if (!maybe_batchsize || *maybe_batchsize < 0) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  base::Value::List* maybe_keys = response_dict->FindList("keys");
  if (!maybe_keys) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  if (maybe_keys->empty()) {
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
    FailAllCallbacks();
    return;
  }

  int64_t max_expiry = 0;
  base::Value::Dict keys_out;
  for (base::Value& key_commitment : *maybe_keys) {
    base::Value::Dict* key_commit_dict = key_commitment.GetIfDict();
    if (!key_commit_dict) {
      DLOG(ERROR) << "Key commitment not a dict: "
                  << key_commitment.DebugString();
      RecordTrustTokenGetterAction(
          KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
      FailAllCallbacks();
      return;
    }
    const std::string* maybe_key = key_commit_dict->FindString("keyMaterial");
    if (!maybe_key) {
      DLOG(ERROR) << "Key commitment missing required field \"" << "keyMaterial"
                  << "\":" << key_commitment.DebugString();
      RecordTrustTokenGetterAction(
          KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
      FailAllCallbacks();
      return;
    }
    std::optional<uint32_t> maybe_key_id =
        FindUnsignedInt(*key_commit_dict, "keyIdentifier");
    if (!maybe_key_id) {
      DLOG(ERROR) << "Key commitment missing required field \""
                  << "keyIdentifier" << "\":" << key_commitment.DebugString();
      RecordTrustTokenGetterAction(
          KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
      FailAllCallbacks();
      return;
    }

    const std::string* maybe_expiry =
        key_commit_dict->FindString("expirationTimestampUsec");
    if (!maybe_expiry) {
      DLOG(ERROR) << "Key commitment missing required field \""
                  << "expirationTimestampUsec"
                  << "\":" << key_commitment.DebugString();
      RecordTrustTokenGetterAction(
          KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
      FailAllCallbacks();
      return;
    }

    int64_t expiry;
    if (!base::StringToInt64(*maybe_expiry, &expiry)) {
      DLOG(ERROR) << "Key commitment expiry has invalid format: "
                  << *maybe_expiry;
      RecordTrustTokenGetterAction(
          KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError);
      FailAllCallbacks();
      return;
    }

    max_expiry = std::max(max_expiry, expiry);

    base::Value::Dict key_out;
    key_out.Set("Y", *maybe_key);
    key_out.Set("expiry", *maybe_expiry);
    keys_out.Set(base::NumberToString(*maybe_key_id), std::move(key_out));
  }

  base::Value::Dict key_commitment_value;
  key_commitment_value.Set("protocol_version", *maybe_version);
  key_commitment_value.Set("id", *maybe_id);
  key_commitment_value.Set("batchsize", *maybe_batchsize);
  key_commitment_value.Set("keys", std::move(keys_out));

  base::Value::Dict outer_commitment;
  outer_commitment.Set(*maybe_version, std::move(key_commitment_value));

  std::string key_commitment_str;
  base::JSONWriter::Write(outer_commitment, &key_commitment_str);

  KeyAndNonUniqueUserIdWithExpiration key_commitment{
      KeyAndNonUniqueUserId{key_commitment_str, non_unique_user_id},
      base::Time::UnixEpoch() + base::Microseconds(max_expiry)};
  storage_->UpdateKeyAndNonUniqueUserId(key_commitment);

  CheckTrustTokens();
}

void KAnonymityTrustTokenGetter::CheckTrustTokens() {
  trust_token_query_answerer_->HasTrustTokens(
      auth_origin_,
      base::BindOnce(&KAnonymityTrustTokenGetter::OnHasTrustTokensComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KAnonymityTrustTokenGetter::OnHasTrustTokensComplete(
    network::mojom::HasTrustTokensResultPtr result) {
  if (!result ||
      result->status != network::mojom::TrustTokenOperationStatus::kOk) {
    DLOG(ERROR) << "Failed checking trust tokens " << result->status;
    FailAllCallbacks();
    return;
  }

  if (!result->has_trust_tokens) {
    FetchTrustToken();
    return;
  }
  CompleteOneRequest();
}

void KAnonymityTrustTokenGetter::FetchTrustToken() {
  auto key_commitment = storage_->GetKeyAndNonUniqueUserId();
  DCHECK(key_commitment);

  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kFetchTrustToken);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = auth_origin_.GetURL().Resolve(base::StringPrintf(
      kIssueTrustTokenPathFmt, key_commitment->key_and_id.non_unique_user_id));
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_.token}));
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kOmit;  // No cache read, always download
                                               // from the network.
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = isolation_info_;

  network::mojom::TrustTokenParamsPtr params =
      network::mojom::TrustTokenParams::New();
  params->operation = network::mojom::TrustTokenOperationType::kIssuance;
  params->custom_key_commitment = key_commitment->key_and_id.key_commitment;
  resource_request->trust_token_params = *params;
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kKAnonymityServiceGetTokenTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kRequestTimeout);
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&KAnonymityTrustTokenGetter::OnFetchedTrustToken,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KAnonymityTrustTokenGetter::OnFetchedTrustToken(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  if (url_loader_->NetError() != net::OK) {
    DLOG(ERROR) << "Couldn't get trust token: " << url_loader_->NetError();
    url_loader_.reset();
    RecordTrustTokenGetterAction(
        KAnonymityTrustTokenGetterAction::kFetchTrustTokenFailed);
    FailAllCallbacks();
    return;
  }
  url_loader_.reset();
  // If the fetch succeeded, that means that the response included the trust
  // tokens we requested. They are stored in the network service so we can
  // redeem them later.

  CompleteOneRequest();
}

void KAnonymityTrustTokenGetter::FailAllCallbacks() {
  while (!pending_callbacks_.empty())
    DoCallback(false);
}

void KAnonymityTrustTokenGetter::CompleteOneRequest() {
  RecordTrustTokenGetterAction(
      KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess);
  // Only record timing UMA when we actually fetched a token.
  RecordTrustTokenGet(pending_callbacks_.front().request_start,
                      base::TimeTicks::Now());
  DoCallback(true);
  if (!pending_callbacks_.empty())
    TryGetTrustTokenAndKeyInternal();
}

void KAnonymityTrustTokenGetter::DoCallback(bool status) {
  DCHECK(!pending_callbacks_.empty());

  std::optional<KeyAndNonUniqueUserId> result;
  if (status) {
    auto key_commitment = storage_->GetKeyAndNonUniqueUserId();
    DCHECK(key_commitment);
    result = key_commitment->key_and_id;
  }

  // We call the callback *before* removing the current request from the list.
  // It is possible that the callback may synchronously enqueue another request.
  // If we remove the current request first then enqueuing the request would
  // start another thread of execution since there was an empty queue.
  std::move(pending_callbacks_.front().callback).Run(result);
  pending_callbacks_.pop_front();
}
