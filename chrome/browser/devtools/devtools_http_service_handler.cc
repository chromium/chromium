// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_http_service_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

DevToolsHttpServiceHandler::Result::Result() = default;
DevToolsHttpServiceHandler::Result::~Result() = default;
DevToolsHttpServiceHandler::Result::Result(Result&&) = default;
DevToolsHttpServiceHandler::Result&
DevToolsHttpServiceHandler::Result::operator=(Result&&) = default;

DevToolsHttpServiceHandler::~DevToolsHttpServiceHandler() = default;
DevToolsHttpServiceHandler::DevToolsHttpServiceHandler() = default;

void DevToolsHttpServiceHandler::Request(
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    Callback callback) {
  CanMakeRequest(profile,
                 base::BindOnce(&DevToolsHttpServiceHandler::OnValidationDone,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                profile, params));
}

void DevToolsHttpServiceHandler::CanMakeRequest(
    Profile* profile,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(profile && !profile->IsOffTheRecord());
}

void DevToolsHttpServiceHandler::OnValidationDone(
    Callback callback,
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    bool validation_success) {
  if (!validation_success) {
    auto result = std::make_unique<Result>();
    result->error = Result::Error::kValidationFailed;
    std::move(callback).Run(std::move(result));
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  auto fetcher_id = base::UnguessableToken::Create();
  auto access_token_fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          "DevTools_dispatchHttpRequest", OAuthScopes(),
          base::BindOnce(&DevToolsHttpServiceHandler::OnTokenFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         profile, params, fetcher_id),
          signin::AccessTokenFetcher::Mode::kImmediate);
  access_token_fetchers_.insert({
      fetcher_id,
      std::move(access_token_fetcher),
  });
}

void DevToolsHttpServiceHandler::OnTokenFetched(
    Callback callback,
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    base::UnguessableToken fetcher_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetchers_.erase(fetcher_id);
  if (error.state() != GoogleServiceAuthError::NONE) {
    auto result = std::make_unique<Result>();
    result->error = Result::Error::kTokenFetchFailed;
    result->error_detail = error.ToString();
    std::move(callback).Run(std::move(result));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  GURL url = BaseURL().Resolve(params.path);
  for (const auto& pair : params.query_params) {
    const std::string& key = pair.first;
    for (const std::string& value : pair.second) {
      url = net::AppendQueryParameter(url, key, value);
    }
  }
  resource_request->url = url;
  resource_request->method = params.method;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), NetworkTrafficAnnotationTag());
  simple_url_loader->SetAllowHttpErrorResults(true);

  if (params.body.has_value()) {
    simple_url_loader->AttachStringForUpload(params.body.value(),
                                             "application/json");
  }

  network::SimpleURLLoader* loader_ptr = simple_url_loader.get();
  loader_ptr->DownloadToString(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&DevToolsHttpServiceHandler::OnRequestComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(simple_url_loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void DevToolsHttpServiceHandler::OnRequestComplete(
    Callback callback,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    std::optional<std::string> response_body) {
  auto result = std::make_unique<Result>();
  result->net_error = simple_url_loader->NetError();
  result->response_body = std::move(response_body);
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    result->http_status =
        simple_url_loader->ResponseInfo()->headers->response_code();
  }

  if (result->net_error != net::OK) {
    result->error = Result::Error::kNetworkError;
  } else if (result->http_status < net::HTTP_OK ||
             result->http_status >= net::HTTP_MULTIPLE_CHOICES) {
    result->error = Result::Error::kHttpError;
  }

  std::move(callback).Run(std::move(result));
}
