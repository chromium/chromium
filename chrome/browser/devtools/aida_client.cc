// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"
#include <string>
#include "base/json/json_string_value_serializer.h"
#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !defined(AIDA_ENDPOINT)
#define AIDA_ENDPOINT ""
#endif

#if !defined(AIDA_SCOPE)
#define AIDA_SCOPE ""
#endif

AidaClient::AidaClient(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(*profile),
      url_loader_factory_(url_loader_factory),
      aida_endpoint_(AIDA_ENDPOINT),
      aida_scope_(AIDA_SCOPE) {}

AidaClient::~AidaClient() = default;

void AidaClient::OverrideAidaEndpointAndScopeForTesting(
    const std::string& aida_endpoint,
    const std::string& aida_scope) {
  aida_endpoint_ = aida_endpoint;
  aida_scope_ = aida_scope;
}

void AidaClient::DoConversation(
    std::string request,
    base::OnceCallback<void(const std::string&)> callback) {
  if (aida_scope_.empty()) {
    std::move(callback).Run(R"([{"error": "AIDA scope is not configured"}])");
    return;
  }
  if (!access_token_.empty()) {
    SendAidaRequest(std::move(request), std::move(callback));
    return;
  }
  auto* identity_manager = IdentityManagerFactory::GetForProfile(&*profile_);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "AIDA client", signin::ScopeSet{aida_scope_},
      base::BindOnce(&AidaClient::AccessTokenFetchFinished,
                     base::Unretained(this), std::move(request),
                     std::move(callback)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void AidaClient::AccessTokenFetchFinished(
    std::string request,
    base::OnceCallback<void(const std::string&)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(base::ReplaceStringPlaceholders(
        R"([{"error": "Cannot get OAuth credentials", "detail": $1}])",
        {base::GetQuotedJSONString(error.ToString())}, nullptr));
    return;
  }

  access_token_ = access_token_info.token;
  SendAidaRequest(std::move(request), std::move(callback));
}

void AidaClient::SendAidaRequest(
    std::string request,
    base::OnceCallback<void(const std::string&)> callback) {
  CHECK(!access_token_.empty());

  if (aida_endpoint_.empty()) {
    std::move(callback).Run(
        R"([{"error": "AIDA endpoint is not configured"}])");
    return;
  }

  auto aida_request = std::make_unique<network::ResourceRequest>();
  aida_request->url = GURL(aida_endpoint_);
  aida_request->load_flags = net::LOAD_DISABLE_CACHE;
  aida_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  aida_request->method = "POST";
  aida_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                  std::string("Bearer ") + access_token_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_cdp_network_resource", R"(
        semantics {
          sender: "Developer Tools via CDP"
          description:
            "When user opens Developer Tools, the browser may fetch additional "
            "resources from the network to enrich the debugging experience "
            "(e.g. source map resources)."
          trigger: "User opens Developer Tools to debug a web page."
          data: "Any resources requested by Developer Tools."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "It's not possible to disable this feature from settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(aida_request), traffic_annotation);
  simple_url_loader->SetAllowHttpErrorResults(true);
  simple_url_loader->AttachStringForUpload(request);

  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AidaClient::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(request),
                     std::move(callback), std::move(simple_url_loader)));
}

void AidaClient::OnSimpleLoaderComplete(
    std::string request,
    base::OnceCallback<void(const std::string&)> callback,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }
  if (!response_body) {
    std::move(callback).Run(R"([{"error": "Got empty response from AIDA"}])");
    return;
  }
  if (response_code == net::HTTP_UNAUTHORIZED) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(&*profile_);
    CoreAccountId account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
    identity_manager->RemoveAccessTokenFromCache(
        account_id, signin::ScopeSet{aida_scope_}, access_token_);
    access_token_.clear();
    DoConversation(request, std::move(callback));
    return;
  }
  if (response_code != net::HTTP_OK) {
    std::move(callback).Run(base::ReplaceStringPlaceholders(
        R"([{"error": "Got error response from AIDA", "detail": $1}])",
        {std::move(*response_body)}, nullptr));
    return;
  }
  std::move(callback).Run(std::move(*response_body));
}
