// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/snapper_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_snapper";
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/json; charset=UTF-8";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeoutMs = base::Seconds(90);

}  // namespace

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

SnapperProvider::~SnapperProvider() = default;

void SnapperProvider::Call(const std::string& input,
                           EndpointFetcherCallback done_callback) {
  std::unique_ptr<EndpointFetcher> fetcher =
      CreateEndpointFetcher(GURL{kEndpointUrl}, {kOAuthScope}, input);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SnapperProvider::HandleResponse, weak_ptr_factory_.GetWeakPtr(),
      std::move(done_callback), std::move(fetcher)));
}

void SnapperProvider::HandleResponse(
    EndpointFetcherCallback done_callback,
    std::unique_ptr<EndpointFetcher> /* endpoint_fetcher */,
    std::unique_ptr<EndpointResponse> response) {
  std::move(done_callback).Run(std::move(response));
}

std::unique_ptr<EndpointFetcher> SnapperProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/kOauthConsumerName, /*url=*/url,
      /*http_method=*/kHttpMethod, /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout_ms=*/kTimeoutMs.InMilliseconds(), /*post_data=*/post_data,
      /*annotation_tag=*/MISSING_TRAFFIC_ANNOTATION,
      /*identity_manager=*/identity_manager_,
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

}  // namespace manta
