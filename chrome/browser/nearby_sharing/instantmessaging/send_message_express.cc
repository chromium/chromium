// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/send_message_express.h"

#include <optional>
#include <sstream>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "components/cross_device/logging/logging.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

// 256 KB as max response size.
constexpr int kMaxSendResponseSize = 256;

// Timeout for network calls to instantmessaging servers.
const base::TimeDelta kNetworkTimeout = base::Milliseconds(2500);

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
              "Nearby sharing or Phone Hub."
            chrome_policy {
              NearbyShareAllowed {
                policy_options {mode: MANDATORY}
                NearbyShareAllowed: false
              },
              PhoneHubAllowed {
                policy_options {mode: MANDATORY}
                PhoneHubAllowed: false
              }
            }
          })");

void LogSendResult(bool success,
                   const ash::nearby::NearbyHttpStatus& http_status,
                   const std::string& request_id) {
  std::stringstream ss;
  ss << "Instant messaging send express " << (success ? "succeeded" : "failed")
     << " for request " << request_id << ". HTTP status: " << http_status;
  if (success) {
    CD_LOG(VERBOSE, Feature::NS) << ss.str();
  } else {
    CD_LOG(ERROR, Feature::NS) << ss.str();
  }
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.SendExpress.Result", success);
  if (!success) {
    base::UmaHistogramSparse(
        "Nearby.Connections.InstantMessaging.SendExpress.Result.FailureReason",
        http_status.GetResultCodeForMetrics());
  }
}

}  // namespace

SendMessageExpress::SendMessageExpress(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_fetcher_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

SendMessageExpress::~SendMessageExpress() = default;

void SendMessageExpress::SendMessage(
    const chrome_browser_nearby_sharing_instantmessaging::
        SendMessageExpressRequest& request,
    SuccessCallback callback) {
  token_fetcher_.GetAccessToken(base::BindOnce(
      &SendMessageExpress::DoSendMessage, weak_ptr_factory_.GetWeakPtr(),
      request, std::move(callback)));
}

void SendMessageExpress::DoSendMessage(
    const chrome_browser_nearby_sharing_instantmessaging::
        SendMessageExpressRequest& request,
    SuccessCallback callback,
    const std::string& oauth_token) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.SendExpress.OAuthTokenFetchResult",
      !oauth_token.empty());
  if (oauth_token.empty()) {
    CD_LOG(ERROR, Feature::NS) << __func__ << ": Failed to fetch OAuth token.";
    std::move(callback).Run(false);
    // NOTE: |this| might be destroyed here after running the callback
    return;
  }

  std::string request_id = request.header().request_id();

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
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(send_url_loader), std::move(callback)),
      kMaxSendResponseSize);
}

void SendMessageExpress::OnSendMessageResponse(
    const std::string& request_id,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    SuccessCallback callback,
    std::unique_ptr<std::string> response_body) {
  ash::nearby::NearbyHttpStatus http_status(url_loader->NetError(),
                                            url_loader->ResponseInfo());
  bool success =
      http_status.IsSuccess() && response_body && !response_body->empty();
  LogSendResult(success, http_status, request_id);
  std::move(callback).Run(success);
  // NOTE: |this| might be destroyed here after running the callback
}
