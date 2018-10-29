// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/extension_urls.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace {

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

}  // namespace

namespace extensions {

WebstoreDataFetcher::WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                                         const GURL& referrer_url,
                                         const std::string webstore_item_id)
    : delegate_(delegate),
      referrer_url_(referrer_url),
      id_(webstore_item_id),
      max_auto_retries_(0) {}

WebstoreDataFetcher::~WebstoreDataFetcher() {}

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
  resource_request->load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DISABLE_CACHE;
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
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebstoreDataFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void WebstoreDataFetcher::OnJsonParseSuccess(
    std::unique_ptr<base::Value> parsed_json) {
  if (!parsed_json->is_dict()) {
    OnJsonParseFailure(kInvalidWebstoreResponseError);
    return;
  }

  delegate_->OnWebstoreResponseParseSuccess(
      std::unique_ptr<base::DictionaryValue>(
          static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void WebstoreDataFetcher::OnJsonParseFailure(
    const std::string& error) {
  delegate_->OnWebstoreResponseParseFailure(error);
}

void WebstoreDataFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    delegate_->OnWebstoreRequestFailure();
    return;
  }

  // The parser will call us back via one of the callbacks.
  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      *response_body,
      base::Bind(&WebstoreDataFetcher::OnJsonParseSuccess, AsWeakPtr()),
      base::Bind(&WebstoreDataFetcher::OnJsonParseFailure, AsWeakPtr()));
}

}  // namespace extensions
