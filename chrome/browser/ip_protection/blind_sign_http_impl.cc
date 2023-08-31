// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/blind_sign_http_impl.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
constexpr net::NetworkTrafficAnnotationTag kIpProtectionTrafficAnnotation =
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

}  // namespace

// The maximum size of the IpProtectionRequests - 256 KB (in practice these
// should be much smaller than this).
const int kIpProtectionRequestMaxBodySize = 256 * 1024;
const char kIpProtectionContentType[] = "application/x-protobuf";

BlindSignHttpImpl::BlindSignHttpImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      ip_protection_server_url_(net::features::kIpPrivacyTokenServer.Get()),
      ip_protection_server_get_initial_data_path_(
          net::features::kIpPrivacyTokenServerGetInitialDataPath.Get()),
      ip_protection_server_get_tokens_path_(
          net::features::kIpPrivacyTokenServerGetTokensPath.Get()) {
  CHECK(url_loader_factory_);
}

BlindSignHttpImpl::~BlindSignHttpImpl() = default;

void BlindSignHttpImpl::DoRequest(quiche::BlindSignHttpRequestType request_type,
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
                                      kIpProtectionContentType);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kIpProtectionContentType);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kIpProtectionTrafficAnnotation);
  url_loader->AttachStringForUpload(body);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&BlindSignHttpImpl::OnRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void BlindSignHttpImpl::OnRequestCompleted(
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
