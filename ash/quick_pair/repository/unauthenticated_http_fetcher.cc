// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/unauthenticated_http_fetcher.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "components/cross_device/logging/logging.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Max size set to 2MB. This is well over the expected maximum for our
// expected responses, however it can be increased if needed in the future.
constexpr int kMaxDownloadBytes = 2 * 1024 * 1024;

}  // namespace

namespace ash {
namespace quick_pair {

UnauthenticatedHttpFetcher::UnauthenticatedHttpFetcher(
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : traffic_annotation_(traffic_annotation) {}

UnauthenticatedHttpFetcher::~UnauthenticatedHttpFetcher() = default;

void UnauthenticatedHttpFetcher::ExecuteGetRequest(
    const GURL& url,
    FetchCompleteCallback callback) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": executing request to: " << url;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);

  // Enable an immediate retry for client-side transient failures:
  // DNS resolution errors and network configuration changes.
  // Server HTTP 5xx errors are not retried.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  loader->SetRetryOptions(/*max_retries=*/1, retry_mode);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      QuickPairBrowserDelegate::Get()->GetURLLoaderFactory();
  if (!url_loader_factory) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No SharedURLLoaderFactory is available.";
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&UnauthenticatedHttpFetcher::OnComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     std::move(callback)),
      kMaxDownloadBytes);
}

void UnauthenticatedHttpFetcher::OnComplete(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    FetchCompleteCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<FastPairHttpResult> http_result =
      std::make_unique<FastPairHttpResult>(
          /*net_error=*/simple_loader->NetError(),
          /*head=*/simple_loader->ResponseInfo());

  if (http_result->IsSuccess()) {
    CD_LOG(VERBOSE, Feature::FP)
        << "Successfully fetched " << simple_loader->GetContentSize()
        << " bytes from " << simple_loader->GetFinalURL();
    std::move(callback).Run(std::move(response_body), std::move(http_result));
    return;
  }

  CD_LOG(WARNING, Feature::FP)
      << "Downloading to string from " << simple_loader->GetFinalURL()
      << " failed: " << http_result->ToString();

  // TODO(jonmann): Implement retries with back-off.
  std::move(callback).Run(nullptr, std::move(http_result));
}

}  // namespace quick_pair
}  // namespace ash
