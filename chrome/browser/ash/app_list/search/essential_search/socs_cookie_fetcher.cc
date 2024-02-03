// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "components/version_info/version_info.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace app_list {
namespace {

const char kChromeVersionKey[] = "chromeos_version";
const char kEssentialSearchURL[] =
    "https://chromeoscompliance-pa.googleapis.com/v1/essentialsearch/"
    "socscookieheader";
const char kApiKeyParameter[] = "key";
const char kContentTypeJSON[] = "application/json";
const char kAcceptValue[] =
    "Accept=text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
constexpr int kGetAuthCodeNetworkRetry = 1;
constexpr int kMaxResponseSize = 5 * 1024;
const char kCookieHeaderEntry[] = "cookieHeader";

}  // namespace

SocsCookieFetcher::SocsCookieFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Consumer* consumer)
    : consumer_(consumer), url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(consumer_);
}

SocsCookieFetcher::~SocsCookieFetcher() = default;

SocsCookieFetcher::Consumer::Consumer() = default;

SocsCookieFetcher::Consumer::~Consumer() = default;

void SocsCookieFetcher::StartFetching() {
  auto request_data = base::Value::Dict().Set(kChromeVersionKey,
                                              version_info::GetVersionNumber());
  std::string request_string;
  if (!base::JSONWriter::Write(request_data, &request_string)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    consumer_->OnApiCallFailed(Status::kRequestBodyNotSerialized);
    return;
  }

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("essential_search_manager", R"(
      semantics {
        sender: "Chrome OS essential search manager"
        description:
          "Call ChromeOS Compliance API to fetch SOCS cookie"
        trigger:
          "When the user login or unlock the device and start the session, "
          "ChromeOS devices would call chrome devices and fetch SOCS cookie "
          "to ensure that cookies and data are used in search for essential "
          "purposes only."
        data: "chromeos_version."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting : "Only Admins can enable/disable this feature from the admin"
                  "dashboard."
        chrome_policy {
          EssentialSearchEnabled {
            EssentialSearchEnabled : false
          }
        }
      })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = net::AppendQueryParameter(
      GURL(kEssentialSearchURL), kApiKeyParameter, google_apis::GetAPIKey());
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = net::HttpRequestHeaders::kGetMethod;

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kContentTypeJSON);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kAcceptValue);
  DCHECK(!simple_url_loader_);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_url_loader_->SetRetryOptions(
      kGetAuthCodeNetworkRetry,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SocsCookieFetcher::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void SocsCookieFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> json_response) {
  const int net_error = simple_url_loader_->NetError();
  if (net_error != net::OK) {
    consumer_->OnApiCallFailed(Status::kServerError);
    return;
  } else if (!json_response || json_response->empty()) {
    consumer_->OnApiCallFailed(Status::kEmptyResponse);
    return;
  }
  simple_url_loader_.reset();

  // Parse the JSON response.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_response, base::BindOnce(&SocsCookieFetcher::OnJsonParsed,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SocsCookieFetcher::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    consumer_->OnApiCallFailed(Status::kJsonParseFailure);
    return;
  }

  if (!result->is_dict()) {
    LOG(WARNING) << "Response is not a JSON dictionary.";
    consumer_->OnApiCallFailed(Status::kNotJsonDict);
    return;
  }

  ProcessValidTokenResponse(std::move(result->GetDict()));
}

void SocsCookieFetcher::ProcessValidTokenResponse(
    base::Value::Dict json_response) {
  const std::string* cookie_header =
      json_response.FindString(kCookieHeaderEntry);
  if (!cookie_header) {
    LOG(WARNING) << "Response does not contain cookie header.";
    consumer_->OnApiCallFailed(Status::kFetchNoCookie);
    return;
  }
  consumer_->OnCookieFetched(*cookie_header);
}

}  // namespace app_list
