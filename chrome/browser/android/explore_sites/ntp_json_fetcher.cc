// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/ntp_json_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/url_util_experimental.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/system_connector.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace explore_sites {

namespace {

const int kMaxRetries = 3;
const int kMaxJsonSize = 1000000;  // 1Mb

}  // namespace

NTPJsonFetcher::NTPJsonFetcher(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

NTPJsonFetcher::~NTPJsonFetcher() {}

void NTPJsonFetcher::Start(Callback callback) {
  // Cancels ongoing requests.
  Stop();

  callback_ = std::move(callback);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("explore_sites_catalog_fetcher", R"(
          semantics {
            sender: "Explore Sites NTP Catalog fetcher"
            description:
              "Downloads sites and categories to be shown on the New Tab Page "
              "for the purposes of exploring the Web."
            trigger:
              "When a mobile Android user views the New Tab Page."
            data:
              "JSON data comprising interesting site and category information. "
              "No user information is sent."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            policy_exception_justification:
              "This feature is only enabled explicitly by flag."
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetNtpPrototypeURL();
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  network::mojom::URLLoaderFactory* loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  simple_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                       network::SimpleURLLoader::RETRY_ON_5XX);
  simple_loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&NTPJsonFetcher::OnSimpleLoaderComplete,
                     weak_factory_.GetWeakPtr()),
      kMaxJsonSize);  // 1Mb max
}

void NTPJsonFetcher::Stop() {
  weak_factory_.InvalidateWeakPtrs();
  simple_loader_.reset();
}

void NTPJsonFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    const char kBadResponse[] = "Unable to parse response body.";
    OnJsonParseError(kBadResponse);
    return;
  }

  // The parser will call us back via one of the callbacks.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&NTPJsonFetcher::OnJsonParse, weak_factory_.GetWeakPtr()));
}

void NTPJsonFetcher::OnJsonParse(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    OnJsonParseError(*result.error);
    return;
  }

  if (!result.value->is_dict()) {
    OnJsonParseError("Parsed JSON is not a dictionary.");
    return;
  }

  std::move(callback_).Run(NTPCatalog::create(*result.value));
}

void NTPJsonFetcher::OnJsonParseError(const std::string& error) {
  DVLOG(1) << "Unable to parse NTP JSON from " << GetNtpPrototypeURL()
           << " error: " << error;
  std::move(callback_).Run(nullptr);
}

}  // namespace explore_sites
