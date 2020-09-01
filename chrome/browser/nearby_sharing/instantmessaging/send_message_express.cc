// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/send_message_express.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {
// 256 KB as max response size.
constexpr int kMaxSendResponseSize = 256;

// TODO(crbug.com/1123164) - Add nearby sharing policy when available.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("send_message_express", R"(
        semantics {
          sender: "SendMessageExpress"
          description:
            "Sends a message to another device via a Gaia authenticated Google"
            " messaging backend."
          trigger:
            "User uses any Chrome cross-device sharing feature and selects a"
            " peer device to send the data to."
          data: "WebRTC session description protocol messages are exchanged "
            "between devices to set up a peer to peer connection as documented "
            "in https://tools.ietf.org/html/rfc4566 and "
            "https://www.w3.org/TR/webrtc/#session-description-model. No user "
            "data is sent in the request."
          destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is only enabled for signed-in users who enable "
              "Nearby sharing"
            chrome_policy {
              BrowserSignin {
                policy_options {mode: MANDATORY}
                BrowserSignin: 0
              }
            }
          })");

bool IsLoaderSuccessful(const network::SimpleURLLoader* loader) {
  if (!loader || loader->NetError() != net::OK)
    return false;

  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers)
    return false;

  return network::cors::IsOkStatus(
      loader->ResponseInfo()->headers->response_code());
}
}  // namespace

SendMessageExpress::SendMessageExpress(
    TokenFetcher* token_fetcher,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_fetcher_(token_fetcher),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(token_fetcher_);
}

SendMessageExpress::~SendMessageExpress() = default;

void SendMessageExpress::SendMessage(
    const chrome_browser_nearby_sharing_instantmessaging::
        SendMessageExpressRequest& request,
    SuccessCallback callback) {
  token_fetcher_->GetAccessToken(base::BindOnce(
      &SendMessageExpress::DoSendMessage, weak_ptr_factory_.GetWeakPtr(),
      request, std::move(callback)));
}

void SendMessageExpress::DoSendMessage(
    const chrome_browser_nearby_sharing_instantmessaging::
        SendMessageExpressRequest& request,
    SuccessCallback callback,
    const std::string& oauth_token) {
  if (oauth_token.empty()) {
    std::move(callback).Run(false);
    return;
  }

  std::string message_id = request.header().requester_id().id();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kInstantMessagingSendMessageAPI);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kAuthorizationHeaderFormat, oauth_token.c_str()));

  std::unique_ptr<network::SimpleURLLoader> send_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  auto* const send_url_loader_ptr = send_url_loader.get();
  send_url_loader->SetTimeoutDuration(kNetworkTimeout);
  send_url_loader->AttachStringForUpload(request.SerializeAsString(),
                                         "application/x-protobuf");
  send_url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SendMessageExpress::OnSendMessageResponse,
                     weak_ptr_factory_.GetWeakPtr(), message_id,
                     std::move(send_url_loader), std::move(callback)),
      kMaxSendResponseSize);
}

void SendMessageExpress::OnSendMessageResponse(
    const std::string& message_id,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    SuccessCallback callback,
    std::unique_ptr<std::string> response_body) {
  // TODO(crbug.com/1123172) - Add metrics for success and failures, with error
  // codes for failures.
  bool success = response_body && !response_body->empty();
  success &= IsLoaderSuccessful(url_loader.get());
  std::move(callback).Run(success);
}
