// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/network_traversal_ice_config_fetcher.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kNetworkTraversalIceConfigApiUrl[] =
    "https://networktraversal.googleapis.com/v1alpha/iceconfig?key=";

// Response with 2 ice server configs takes ~1KB. A loose upper bound of 16KB is
// chosen to avoid breaking the flow in case the response has longer URLs in ice
// configs.
constexpr int kMaxBodySize = 16 * 1024;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("network_traversal_ice_config_fetcher",
                                        R"(
        semantics {
          sender: "NetworkTraversalIceConfigFetcher"
          description:
            "Fetches ice server configurations for p2p webrtc connection as "
            "described in "
            "https://www.w3.org/TR/webrtc/#rtciceserver-dictionary."
          trigger:
            "User uses any Chrome cross-device sharing feature and selects one"
            " of their devices to send the data to."
          data: "No data is sent in the request."
          destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can disable this behavior by signing out of Chrome."
            chrome_policy {
              BrowserSignin {
                policy_options {mode: MANDATORY}
                BrowserSignin: 0
              }
            }
          })");

bool IsLoaderSuccessful(const network::SimpleURLLoader* loader) {
  DCHECK(loader);

  if (loader->NetError() != net::OK) {
    LOG(ERROR) << "NetworkTraversalIceConfigFetcher url loader network error: "
               << loader->NetError();
    return false;
  }

  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    LOG(ERROR) << "NetworkTraversalIceConfigFetcher invalid response or "
                  "missing headers";
    return false;
  }

  // Success response codes are 2xx.
  bool is_successful_response_code =
      (loader->ResponseInfo()->headers->response_code() / 100) == 2;
  if (!is_successful_response_code) {
    LOG(ERROR)
        << "NetworkTraversalIceConfigFetcher non-successful response code: "
        << loader->ResponseInfo()->headers->response_code();
  }
  return is_successful_response_code;
}

std::vector<::sharing::mojom::IceServerPtr> GetDefaultIceServers() {
  ::sharing::mojom::IceServerPtr ice_server(::sharing::mojom::IceServer::New());
  ice_server->urls.emplace_back("stun:stun.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun1.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun2.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun3.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun4.l.google.com:19302");

  std::vector<::sharing::mojom::IceServerPtr> default_servers;
  default_servers.push_back(std::move(ice_server));
  return default_servers;
}

std::vector<::sharing::mojom::IceServerPtr> ParseIceConfigJson(
    std::string json) {
  std::vector<::sharing::mojom::IceServerPtr> ice_servers;
  std::optional<base::Value> response = base::JSONReader::Read(json);
  if (!response)
    return ice_servers;

  base::Value::List* ice_servers_json =
      response->GetDict().FindList("iceServers");
  if (!ice_servers_json)
    return ice_servers;

  for (base::Value& server : *ice_servers_json) {
    base::Value::Dict& server_dict = server.GetDict();
    const base::Value::List* urls_json = server_dict.FindList("urls");
    if (!urls_json)
      continue;

    std::vector<GURL> urls;
    for (const base::Value& url_json : *urls_json) {
      const std::string* url = url_json.GetIfString();
      if (!url)
        continue;

      urls.emplace_back(*url);
    }

    if (urls.empty())
      continue;

    ::sharing::mojom::IceServerPtr ice_server(
        ::sharing::mojom::IceServer::New());
    ice_server->urls = std::move(urls);

    std::string* retrieved_username = server_dict.FindString("username");
    if (retrieved_username)
      ice_server->username.emplace(std::move(*retrieved_username));

    std::string* retrieved_credential = server_dict.FindString("credential");
    if (retrieved_credential)
      ice_server->credential.emplace(std::move(*retrieved_credential));

    ice_servers.push_back(std::move(ice_server));
  }

  return ice_servers;
}

void OnIceServersResponse(
    ::sharing::mojom::IceConfigFetcher::GetIceServersCallback callback,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  std::vector<::sharing::mojom::IceServerPtr> ice_servers;

  if (IsLoaderSuccessful(url_loader.get()) && response_body)
    ice_servers = ParseIceConfigJson(*response_body);

  sharing::LogWebRtcIceConfigFetched(ice_servers.size());

  if (ice_servers.empty()) {
    VLOG(1) << "NetworkTraversalIceConfigFetcher returning default ice servers";
    ice_servers = GetDefaultIceServers();
  }

  std::move(callback).Run(std::move(ice_servers));
}

}  // namespace

NetworkTraversalIceConfigFetcher::NetworkTraversalIceConfigFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

NetworkTraversalIceConfigFetcher::~NetworkTraversalIceConfigFetcher() = default;

void NetworkTraversalIceConfigFetcher::GetIceServers(
    GetIceServersCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(base::StrCat(
      {kNetworkTraversalIceConfigApiUrl, google_apis::GetSharingAPIKey()}));
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OnIceServersResponse, std::move(callback),
                     std::move(url_loader)),
      kMaxBodySize);
}
