// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_asset_domain_list_include_handler.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace android_webview {

namespace {
const int kNumNetworkRetries = 1;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("digital_asset_links_include", R"(
      semantics {
        sender: "Digital Asset Links Include Resolver"
        description:
          "Digital Asset Links APIs allows any caller to check pre declared "
          "relationships between two assets which can be either web domains "
          "or native applications. These relationships can be defined "
          "directly within an app's manifest or remotely and downloaded "
          "via an 'include' statement. This request downloads the statements "
          "from the specfied include and returns the web domains "
          "specifically. Fetching these URLS requires no user data."
        trigger:
          "When an application needs to download the domains from an "
          "'include' statement."
        data: "None"
        destination: WEBSITE
        internal {
          contacts {
            owners: "//android_webview/OWNERS"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2024-08-03"
      }
      policy {
        cookies_allowed: NO
        setting: "Not user controlled."
        policy_exception_justification:
          "Not implemented, considered not useful as no content is being "
          "uploaded; this request merely downloads the resources on the web."
      })");
}  // namespace

AssetDomainListIncludeHandler::AssetDomainListIncludeHandler(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

AssetDomainListIncludeHandler::~AssetDomainListIncludeHandler() = default;

void AssetDomainListIncludeHandler::LoadAppDefinedDomainIncludes(
    const GURL& include_url,
    AssetDomainListIncludeHandler::LoadCallback callback) {
  if (!include_url.is_valid()) {
    std::move(callback).Run({});
    return;
  }

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = include_url;
  // Exclude credentials (cookies and client certs) from the request.
  request->credentials_mode =
      network::mojom::CredentialsMode::kOmitBug_775438_Workaround;

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader->SetRetryOptions(
      kNumNetworkRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  // Get a raw pointer so we can move the unique_ptr into the callback and still
  // call a method on the underlying object.
  network::SimpleURLLoader* raw_url_loader = url_loader.get();

  raw_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AssetDomainListIncludeHandler::OnNetworkRequestComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)));
}

namespace {
void LogNetworkLoadResult(bool success) {
  base::UmaHistogramBoolean(
      "Android.WebView.DigitalAssetLinks.AssetIncludes.NetworkLoadResult",
      success);
}
}  // namespace

void AssetDomainListIncludeHandler::OnNetworkRequestComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    AssetDomainListIncludeHandler::LoadCallback callback,
    std::unique_ptr<std::string> response_body) {
  int net_error = url_loader->NetError();
  if (net_error != net::OK) {
    DLOG(WARNING) << "Digital Asset Links fetch from include connection failed "
                     "with error "
                  << net::ErrorToString(net_error);
    LogNetworkLoadResult(false);
    std::move(callback).Run({});
    return;
  }

  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  if (!response_body || response_code != net::HTTP_OK) {
    DLOG(WARNING)
        << "Digital Asset Links include domain endpoint responded with code "
        << response_code;
    LogNetworkLoadResult(false);
    std::move(callback).Run({});
    return;
  }
  LogNetworkLoadResult(true);

  // Log response size up to 1M bytes.
  // Anything beyond that for a JSON list of small objects is a lot.
  base::UmaHistogramCounts1M(
      "Android.WebView.DigitalAssetLinks.AssetIncludes.FileSize",
      response_body->size());

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&AssetDomainListIncludeHandler::OnJsonParseResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

namespace {
// Helper method to find the relevant `target->site` value in an asset link
// include statement.
const std::string* GetSiteFromWebRelationStatement(
    const base::Value& statement) {
  // Statements are expected to be on the form
  //   {
  //     "relation": ["delegate_permission/common.handle_all_urls"],
  //     "target": {
  //       "namespace": "web",
  //       "site": "site_root_url"
  //   }
  const base::Value::Dict* statement_dict = statement.GetIfDict();
  if (!statement_dict) {
    return nullptr;
  }
  const base::Value::Dict* target = statement_dict->FindDict("target");
  if (!target) {
    return nullptr;
  }

  const std::string* ns = target->FindString("namespace");
  if (!ns || *ns != "web") {
    return nullptr;
  }

  return target->FindString("site");
}

void LogStatementListParseResult(bool success) {
  base::UmaHistogramBoolean(
      "Android.WebView.DigitalAssetLinks.AssetIncludes.ParseSuccess", success);
}
}  // namespace

void AssetDomainListIncludeHandler::OnJsonParseResult(
    AssetDomainListIncludeHandler::LoadCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  base::UmaHistogramBoolean(
      "Android.WebView.DigitalAssetLinks.AssetIncludes.ValidJson",
      result.has_value());
  if (!result.has_value()) {
    DLOG(WARNING) << "Digital Asset Links fetch from include response "
                     "parsing failed with message: " +
                         result.error();
    std::move(callback).Run({});
    return;
  }

  base::Value::List* statement_list = result->GetIfList();
  if (!statement_list) {
    DLOG(WARNING) << "Statement List is not a list.";
    LogStatementListParseResult(false);
    std::move(callback).Run({});
    return;
  }

  std::vector<std::string> domains;

  for (const base::Value& statement : *statement_list) {
    const std::string* domain = GetSiteFromWebRelationStatement(statement);
    if (domain && !domain->empty()) {
      GURL parsed_domain = GURL(*domain);
      if (parsed_domain.is_valid() &&
          network::IsUrlPotentiallyTrustworthy(parsed_domain) &&
          !parsed_domain.host().empty()) {
        domains.push_back(parsed_domain.host());
      }
    }
  }

  // Sort and remove duplicates
  std::sort(domains.begin(), domains.end());
  domains.erase(std::unique(domains.begin(), domains.end()), domains.end());

  // If we didn't get at least a single domain then this was a big waste of
  // time.
  LogStatementListParseResult(!domains.empty());
  std::move(callback).Run(domains);
}

}  // namespace android_webview
