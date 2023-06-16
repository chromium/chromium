// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/blind_sign_http_impl.h"

#include <stdio.h>
#include <functional>
#include <string>

#include "base/strings/strcat.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

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

int kIpProtectionRequestMaxBodySize = 1024;
char kIpProtectionContentType[] = "application/x-protobuf";

BlindSignHttpImpl::BlindSignHttpImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(url_loader_factory_);
}

BlindSignHttpImpl::~BlindSignHttpImpl() = default;

void BlindSignHttpImpl::DoRequest(
    const std::string& path_and_query,
    const std::string& authorization_header,
    const std::string& body,
    std::function<void(absl::StatusOr<quiche::BlindSignHttpResponse>)>
        callback) {
  callback_ = std::move(callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(path_and_query);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", authorization_header}));
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kIpProtectionContentType);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kIpProtectionContentType);

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kIpProtectionTrafficAnnotation);
  url_loader_->AttachStringForUpload(body);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&BlindSignHttpImpl::OnRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      kIpProtectionRequestMaxBodySize);
}

void BlindSignHttpImpl::OnRequestCompleted(
    std::unique_ptr<std::string> response) {
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  url_loader_.reset();
  if (!response) {
    // TODO (crbug.com/1446863): Indicate why the request to Phosphor failed so
    // we can consider not requesting more tokens.
    callback_(absl::InternalError("Failed Request to Authentication Server"));
    return;
  }

  quiche::BlindSignHttpResponse bsa_response(response_code,
                                             std::move(*response));

  callback_(std::move(bsa_response));
}
