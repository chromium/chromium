// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/blind_sign_http_impl.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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
      ip_protection_server_url_(BlindSignHttpImpl::kIpProtectionServerUrl) {
  CHECK(url_loader_factory_);
}

BlindSignHttpImpl::~BlindSignHttpImpl() = default;

void BlindSignHttpImpl::DoRequest(const std::string& path_and_query,
                                  const std::string& authorization_header,
                                  const std::string& body,
                                  quiche::BlindSignHttpCallback callback) {
  callback_ = std::move(callback);

  // Note that the `path_and_query` we parse here comes from the BlindSignAuth
  // library, which is maintained by Google. Thus, this can be considered
  // trustworthy input.
  std::vector<base::StringPiece> split_path_and_query = base::SplitStringPiece(
      path_and_query, "?", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // We assume there will always be a non-empty path component.
  CHECK(split_path_and_query.size() >= 1);

  GURL::Replacements replacements;
  replacements.SetPathStr(split_path_and_query.front());
  // Define `new_query` here so that its value stays alive for the lifetime of
  // `replacements` (if needed).
  std::string new_query;
  if (split_path_and_query.size() > 1) {
    std::vector<base::StringPiece> split_query(split_path_and_query.begin() + 1,
                                               split_path_and_query.end());
    new_query = base::JoinString(split_query, "?");
    replacements.SetQueryStr(new_query);
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      ip_protection_server_url_.ReplaceComponents(replacements);
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

  // Short-circuit non-200 HTTP responses to an OK response with that code.
  if (response_code != 200 && response_code != 0) {
    std::move(callback_)(quiche::BlindSignHttpResponse(response_code, ""));
    return;
  }

  if (!response) {
    std::move(callback_)(
        absl::InternalError("Failed Request to Authentication Server"));
    return;
  }

  quiche::BlindSignHttpResponse bsa_response(response_code,
                                             std::move(*response));

  std::move(callback_)(std::move(bsa_response));
}
