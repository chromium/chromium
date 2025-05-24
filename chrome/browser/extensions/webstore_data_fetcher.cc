// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/extension_urls.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

constexpr net::NetworkTrafficAnnotationTag
    kWebstoreDataFetcherItemSnippetApiTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("cws_fetch_item_snippet", R"(
        semantics {
          sender: "Webstore Data Fetcher"
          description:
            "Fetches metadata about an extension from the Chrome Web Store "
            "using the item snippet API and returns the metadata as a protobuf "
            "object."
          trigger:
            "The user or another program triggers some action where Chrome "
            "will show metadata about an extension. This includes extension "
            "installation flows, triggering an install for a disabled "
            "extension, an extension being added to Chrome through third-party "
            "sideloading and re-intallation of an extension that was detected "
            "as corrupted. It also happens when a kiosk app account whose "
            "metadata (app icon, name, required platform version) is not "
            "cached locally is detected in device local accounts list. The "
            "account can be set either by device policy or through extensions "
            "web UI, by the device owner (user that was initially added to the "
            "device; implies non managed device). The latter case is "
            "deprecated and not supported on newer Chrome OS boards."
          data:
            "The extension id."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. It will only be "
            "triggered if the user uses extensions."
          policy_exception_justification: "Not implemented."
        })");

bool g_log_response_code_for_testing_ = false;

extensions::FetchItemSnippetResponse* g_mock_item_snippet_response_ = nullptr;

}  // namespace

namespace extensions {

WebstoreDataFetcher::WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                                         const GURL& referrer_url,
                                         const std::string& webstore_item_id)
    : delegate_(delegate), referrer_url_(referrer_url), id_(webstore_item_id) {}

WebstoreDataFetcher::~WebstoreDataFetcher() = default;

// static
void WebstoreDataFetcher::SetLogResponseCodeForTesting(bool enabled) {
  g_log_response_code_for_testing_ = enabled;
}

// static
void WebstoreDataFetcher::SetMockItemSnippetReponseForTesting(
    FetchItemSnippetResponse* mock_response) {
  g_mock_item_snippet_response_ = mock_response;
}

void WebstoreDataFetcher::Start(
    network::mojom::URLLoaderFactory* url_loader_factory) {
  if (g_mock_item_snippet_response_) {
    g_mock_item_snippet_response_->set_item_id(id_);
    delegate_->OnFetchItemSnippetParseSuccess(id_,
                                              *g_mock_item_snippet_response_);
    return;
  }

  GURL webstore_data_url(extension_urls::GetWebstoreItemSnippetURL(id_));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->url = webstore_data_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");

// The endpoint does not require an API key, but one will be provided if it's
// called from a branded build (i.e. Chrome) so the API can distinguish if it's
// called from Chrome or another Chromium browser.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  google_apis::AddAPIKeyToRequest(*resource_request, google_apis::GetAPIKey());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  InitializeSimpleLoaderForRequest(
      std::move(resource_request),
      kWebstoreDataFetcherItemSnippetApiTrafficAnnotation);

  FetchItemSnippetRequest request;
  request.set_name(id_);
  simple_url_loader_->AttachStringForUpload(request.SerializeAsString(),
                                            "application/x-protobuf");

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebstoreDataFetcher::OnFetchItemSnippetResponseReceived,
                     base::Unretained(this)));
}

void WebstoreDataFetcher::InitializeSimpleLoaderForRequest(
    std::unique_ptr<network::ResourceRequest> request,
    const net::NetworkTrafficAnnotationTag& annotation) {
  simple_url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), annotation);

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

void WebstoreDataFetcher::OnFetchItemSnippetResponseReceived(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    delegate_->OnWebstoreRequestFailure(id_);
    return;
  }

  FetchItemSnippetResponse response_proto;
  // It's safe to parse the API data into a protobuf here in the browser process
  // since the endpoint is HTTPS and the FetchItemSnippetResponse schema only
  // contains simple data fields.
  if (response_proto.ParseFromString(*response_body)) {
    delegate_->OnFetchItemSnippetParseSuccess(id_, std::move(response_proto));
  } else {
    delegate_->OnWebstoreResponseParseFailure(id_,
                                              kInvalidWebstoreResponseError);
  }
}

}  // namespace extensions
