// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_trust_token_getter.h"

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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
        "TBD"
      chrome_policy {
      }
    }
    comments:
      ""
    )");

}  // namespace

KAnonymityTrustTokenGetter::KAnonymityTrustTokenGetter(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::TrustTokenQueryAnswerer* answerer)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      trust_token_query_answerer_(answerer) {
  url::Origin auth_origin = url::Origin::Create(GURL(kKAnonymityAuthServer));
  isolation_info_ = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, auth_origin, auth_origin,
      net::SiteForCookies());
}

KAnonymityTrustTokenGetter::~KAnonymityTrustTokenGetter() = default;

void KAnonymityTrustTokenGetter::TryGetTrustTokenAndKey(
    TryGetTrustTokenAndKeyCallback callback) {
  bool currently_fetching = pending_callbacks_.size() > 0;
  pending_callbacks_.push_back(std::move(callback));
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

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    FailAllCallbacks();
    return;
  }

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
    FailAllCallbacks();
    return;
  }

  if (access_token_info.expiration_time <= base::Time::Now()) {
    // Token we got has already expired.
    FailAllCallbacks();
  }

  access_token_ = access_token_info;
  CheckTrustTokenKeyCommitment();
}

void KAnonymityTrustTokenGetter::CheckTrustTokenKeyCommitment() {
  if (key_and_non_unique_user_id_with_expiration_.expiration <=
      base::Time::Now() + kRequestMargin) {
    FetchNonUniqueUserId();
    return;
  }
  CheckTrustTokens();
}

void KAnonymityTrustTokenGetter::FetchNonUniqueUserId() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(base::StrCat({kKAnonymityAuthServer, kGenNonUniqueUserIdPath}));
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
    FailAllCallbacks();
    return;
  }

  base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    FailAllCallbacks();
    return;
  }

  absl::optional<int> maybe_non_unique_user_id =
      response_dict->FindInt("shortClientIdentifier");
  if (!maybe_non_unique_user_id) {
    FailAllCallbacks();
    return;
  }

  FetchTrustTokenKeyCommitment(*maybe_non_unique_user_id);
}

void KAnonymityTrustTokenGetter::FetchTrustTokenKeyCommitment(
    int non_unique_user_id) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(
      base::StrCat({kKAnonymityAuthServer,
                    base::StringPrintf(kFetchKeysPathFmt, non_unique_user_id,
                                       google_apis::GetAPIKey().c_str())}));
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
      /*max_body_size=*/1024);
}

void KAnonymityTrustTokenGetter::OnFetchedTrustTokenKeyCommitment(
    int non_unique_user_id,
    std::unique_ptr<std::string> response) {
  if (url_loader_->NetError() != net::OK) {
    url_loader_.reset();
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
    FailAllCallbacks();
    return;
  }

  base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    FailAllCallbacks();
    return;
  }

  std::string* maybe_version = response_dict->FindString("protocolVersion");
  if (!maybe_version) {
    FailAllCallbacks();
    return;
  }

  const absl::optional<int> maybe_id = response_dict->FindInt("id");
  if (!maybe_id) {
    FailAllCallbacks();
    return;
  }

  const absl::optional<int> maybe_batchsize =
      response_dict->FindInt("batchSize");
  if (!maybe_batchsize) {
    FailAllCallbacks();
    return;
  }

  base::Value::List* maybe_keys = response_dict->FindList("keys");
  if (!maybe_keys) {
    FailAllCallbacks();
    return;
  }

  if (maybe_keys->empty()) {
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
      FailAllCallbacks();
      return;
    }
    const std::string* maybe_key = key_commit_dict->FindString("keyMaterial");
    absl::optional<int> maybe_key_id =
        key_commit_dict->FindInt("keyIdentifier");
    const std::string* maybe_expiry =
        key_commit_dict->FindString("expirationTimestampUsec");
    int64_t expiry;
    if (!maybe_key || !maybe_key_id || !maybe_expiry) {
      DLOG(ERROR) << "Key commitment missing required field: "
                  << key_commitment.DebugString();
      FailAllCallbacks();
      return;
    }
    if (!base::StringToInt64(*maybe_expiry, &expiry)) {
      DLOG(ERROR) << "Key commitment expiry has invalid format: "
                  << *maybe_expiry;
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

  key_and_non_unique_user_id_with_expiration_ =
      KeyAndNonUniqueUserIdWithExpiration{
          KeyAndNonUniqueUserId{key_commitment_str, non_unique_user_id},
          base::Time::UnixEpoch() + base::Microseconds(max_expiry)};

  CheckTrustTokens();
}

void KAnonymityTrustTokenGetter::CheckTrustTokens() {
  url::Origin issuer = url::Origin::Create(GURL(kKAnonymityAuthServer));
  trust_token_query_answerer_->HasTrustTokens(
      issuer,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&KAnonymityTrustTokenGetter::OnHasTrustTokensComplete,
                         weak_ptr_factory_.GetWeakPtr()),
          nullptr));
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
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(base::StrCat(
      {kKAnonymityAuthServer,
       base::StringPrintf(kIssueTrustTokenPathFmt,
                          key_and_non_unique_user_id_with_expiration_.key_and_id
                              .non_unique_user_id)}));
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
  params->type = network::mojom::TrustTokenOperationType::kIssuance;
  params->custom_key_commitment =
      key_and_non_unique_user_id_with_expiration_.key_and_id.key_commitment;
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
  DoCallback(true);
  if (!pending_callbacks_.empty())
    TryGetTrustTokenAndKeyInternal();
}

void KAnonymityTrustTokenGetter::DoCallback(bool status) {
  DCHECK(!pending_callbacks_.empty());

  absl::optional<KeyAndNonUniqueUserId> result;
  if (status) {
    result = key_and_non_unique_user_id_with_expiration_.key_and_id;
  }

  TryGetTrustTokenAndKeyCallback callback(
      std::move(pending_callbacks_.front()));
  pending_callbacks_.pop_front();
  std::move(callback).Run(result);
}
