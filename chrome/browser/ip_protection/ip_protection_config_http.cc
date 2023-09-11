// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_config_http.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ip_protection/get_proxy_config.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
constexpr net::NetworkTrafficAnnotationTag kGetTokenTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ip_protection_service_get_token",
                                        R"(
    semantics {
      sender: "Chrome IP Protection Service Client"
      description:
        "Request to a Google auth server to obtain an authentication token "
        "for Chrome's IP Protection privacy proxies."
      trigger:
        "The Chrome IP Protection Service is out of proxy authentication "
        "tokens."
      data:
        "Chrome sign-in OAuth Token"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "ip-protection-team@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2023-05-23"
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

constexpr net::NetworkTrafficAnnotationTag kGetProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "ip_protection_service_get_proxy_config",
        R"(
    semantics {
      sender: "Chrome IP Protection Service Client"
      description:
        "Request to a Google auth server to obtain proxy server hostnames "
        "for Chrome's IP Protection privacy proxies."
      trigger:
        "On startup, periodically, and on failure to connect to a proxy."
      data:
        "None"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "ip-protection-team@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-08-30"
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

const char kGoogApiKeyHeader[] = "X-Goog-Api-Key";
}  // namespace

// The maximum size of the IpProtectionRequests - 256 KB (in practice these
// should be much smaller than this).
const int kIpProtectionRequestMaxBodySize = 256 * 1024;
const char kProtobufContentType[] = "application/x-protobuf";

IpProtectionConfigHttp::IpProtectionConfigHttp(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      ip_protection_server_url_(net::features::kIpPrivacyTokenServer.Get()),
      ip_protection_server_get_initial_data_path_(
          net::features::kIpPrivacyTokenServerGetInitialDataPath.Get()),
      ip_protection_server_get_tokens_path_(
          net::features::kIpPrivacyTokenServerGetTokensPath.Get()),
      ip_protection_server_get_proxy_config_path_(
          net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()) {
  CHECK(url_loader_factory_);
}

IpProtectionConfigHttp::~IpProtectionConfigHttp() = default;

void IpProtectionConfigHttp::DoRequest(
    quiche::BlindSignHttpRequestType request_type,
    const std::string& authorization_header,
    const std::string& body,
    quiche::BlindSignHttpCallback callback) {
  GURL::Replacements replacements;
  switch (request_type) {
    case quiche::BlindSignHttpRequestType::kGetInitialData:
      replacements.SetPathStr(ip_protection_server_get_initial_data_path_);
      break;
    case quiche::BlindSignHttpRequestType::kAuthAndSign:
      replacements.SetPathStr(ip_protection_server_get_tokens_path_);
      break;
    case quiche::BlindSignHttpRequestType::kUnknown:
      NOTREACHED_NORETURN();
  }

  GURL request_url = ip_protection_server_url_.ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback)(absl::InternalError("Invalid IP Protection Token URL"));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", authorization_header}));
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kProtobufContentType);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetTokenTrafficAnnotation);
  url_loader->AttachStringForUpload(body);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IpProtectionConfigHttp::OnDoRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void IpProtectionConfigHttp::OnDoRequestCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    quiche::BlindSignHttpCallback callback,
    std::unique_ptr<std::string> response) {
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  // Short-circuit non-200 HTTP responses to an OK response with that code.
  if (response_code != 200 && response_code != 0) {
    std::move(callback)(quiche::BlindSignHttpResponse(response_code, ""));
    return;
  }

  if (!response) {
    std::move(callback)(
        absl::InternalError("Failed Request to Authentication Server"));
    return;
  }

  quiche::BlindSignHttpResponse bsa_response(response_code,
                                             std::move(*response));

  std::move(callback)(std::move(bsa_response));
}

void IpProtectionConfigHttp::GetProxyConfig(GetProxyConfigCallback callback) {
  GURL::Replacements replacements;
  replacements.SetPathStr(ip_protection_server_get_proxy_config_path_);
  GURL request_url = ip_protection_server_url_.ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback).Run(
        absl::InternalError("Invalid IP Protection GetProxyConfig URL"));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(kGoogApiKeyHeader,
                                      google_apis::GetAPIKey());
  // Although this request has an empty request-body, it must still have the
  // protobuf content-type, or else the API server will ignore the `Accept`
  // header and respond with JSON.
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kProtobufContentType);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetProxyConfigTrafficAnnotation);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IpProtectionConfigHttp::OnGetProxyConfigCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Include the URLLoader in the callback so that it stays
                     // alive until the download is complete.
                     std::move(url_loader), std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void IpProtectionConfigHttp::OnGetProxyConfigCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    GetProxyConfigCallback callback,
    std::unique_ptr<std::string> response) {
  if (!response) {
    std::move(callback).Run(
        absl::InternalError("Failed GetProxyConfig request"));
    return;
  }

  ip_protection::GetProxyConfigResponse response_proto;
  if (!response_proto.ParseFromString(*response)) {
    std::move(callback).Run(
        absl::InternalError("Failed to parse GetProxyConfig response"));
    return;
  }

  std::move(callback).Run(std::move(response_proto));
}
