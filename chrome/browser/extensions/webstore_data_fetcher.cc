// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/extension_urls.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

bool g_log_response_code_for_testing_ = false;

}  // namespace

namespace extensions {

WebstoreDataFetcher::WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                                         const GURL& referrer_url,
                                         const std::string webstore_item_id)
    : delegate_(delegate), referrer_url_(referrer_url), id_(webstore_item_id) {}

WebstoreDataFetcher::~WebstoreDataFetcher() {}

// static
void WebstoreDataFetcher::SetLogResponseCodeForTesting(bool enabled) {
  g_log_response_code_for_testing_ = enabled;
}

void WebstoreDataFetcher::Start(
    network::mojom::URLLoaderFactory* url_loader_factory) {
  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webstore_data_fetcher", R"(
        semantics {
          sender: "Webstore Data Fetcher"
          description:
            "Fetches metadata about an extension from the Chrome Web Store."
          trigger:
            "The user or another program triggers some action where Chrome "
            "will show metadata about an extension. This includes extension "
            "installation flows, triggering an install for a disabled "
            "extension, and an extension being added to Chrome through "
            "third-party sideloading. It also happens when a kiosk app account "
            "whose metadata (app icon, name, required platform version) is not "
            "cached locally is detected in device local accounts list. The "
            "account can be set either by device policy or through extensions "
            "web UI, by the device owner (user that was initially added to the "
            "device; implies non managed device). The latter case is "
            "deprecated and not supported on newer Chrome OS boards."
          data:
            "The extension id and referrer url. The referrer chain is also "
            "included if the user has not opted out of SafeBrowsing."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. It will only be "
            "triggered if the user uses extensions."
          policy_exception_justification: "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = webstore_data_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->referrer = referrer_url_;
  resource_request->method = "GET";
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (max_auto_retries_ > 0) {
    simple_url_loader_->SetRetryOptions(
        max_auto_retries_,
        network::SimpleURLLoader::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  }

  if (g_log_response_code_for_testing_) {
    simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        &WebstoreDataFetcher::OnResponseStarted, base::Unretained(this)));
  }

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebstoreDataFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void WebstoreDataFetcher::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers)
    return;

  int response_code = response_head.headers->response_code();
  if (response_code != 200)
    LOG(ERROR) << "Response_code: " << response_code;
}

void WebstoreDataFetcher::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    delegate_->OnWebstoreResponseParseFailure(id_, result.error());
    return;
  }

  if (!result->is_dict()) {
    delegate_->OnWebstoreResponseParseFailure(id_,
                                              kInvalidWebstoreResponseError);
    return;
  }

  delegate_->OnWebstoreResponseParseSuccess(id_, result->GetDict());
}

void WebstoreDataFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    delegate_->OnWebstoreRequestFailure(id_);
    return;
  }

  // The parser will call us back via one of the callbacks.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body, base::BindOnce(&WebstoreDataFetcher::OnJsonParsed,
                                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace extensions
