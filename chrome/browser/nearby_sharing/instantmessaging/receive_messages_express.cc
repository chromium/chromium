// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"

#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

// TODO(crbug.com/1123164) - Add nearby sharing policy when available.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("receive_messages_express", R"(
        semantics {
          sender: "ReceiveMessagesExpress"
          description:
            "Receives messages sent from another device via a Gaia "
            "authenticated Google messaging backend."
          trigger:
            "Peer uses any Chrome cross-device sharing feature and selects "
            "this devices to send the data to."
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

base::Optional<NearbyShareHttpStatus> HttpStatusFromUrlLoader(
    const network::SimpleURLLoader* loader) {
  if (!loader)
    return base::nullopt;

  return NearbyShareHttpStatus(loader->NetError(), loader->ResponseInfo());
}

void LogReceiveResult(
    bool success,
    const base::Optional<NearbyShareHttpStatus>& http_status) {
  std::stringstream ss;
  ss << "Instant messaging receive express "
     << (success ? "succeeded." : "failed.");
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.ReceiveExpress.Result", success);
  if (http_status) {
    ss << " HTTP status: " << *http_status;
    if (!success) {
      base::UmaHistogramSparse(
          "Nearby.Connections.InstantMessaging.ReceiveExpress.Result."
          "FailureReason",
          http_status->GetResultCodeForMetrics());
    }
  }

  if (success) {
    NS_LOG(VERBOSE) << ss.str();
  } else {
    NS_LOG(ERROR) << ss.str();
  }
}

}  // namespace

ReceiveMessagesExpress::ReceiveMessagesExpress(
    TokenFetcher* token_fetcher,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_fetcher_(token_fetcher),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(token_fetcher_);
}

ReceiveMessagesExpress::~ReceiveMessagesExpress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success_callback_.is_null())
    std::move(success_callback_).Run(false);
}

void ReceiveMessagesExpress::StartReceivingMessages(
    const chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesExpressRequest& request,
    base::RepeatingCallback<void(const std::string& message)> listener,
    SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();
  stream_parser_.reset();

  if (!success_callback_.is_null()) {
    // A pending callback was found. This should be marked as false since the
    // previous receiver has not yet started listening for messages and is now
    // going to be replaced by the new listener.
    NS_LOG(WARNING)
        << __func__
        << ": Found pending request to start receiving messages. "
           "Failing the previous request before handling the new request.";
    std::move(success_callback_).Run(false);
  }
  success_callback_ = std::move(callback);

  token_fetcher_->GetAccessToken(
      base::BindOnce(&ReceiveMessagesExpress::DoStartReceivingMessages,
                     weak_ptr_factory_.GetWeakPtr(), request, listener));
}

void ReceiveMessagesExpress::DoStartReceivingMessages(
    const chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesExpressRequest& request,
    base::RepeatingCallback<void(const std::string& message)> listener,
    const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.ReceiveExpress."
      "OAuthTokenFetchResult",
      !oauth_token.empty());
  if (oauth_token.empty()) {
    NS_LOG(ERROR) << __func__ << ": Failed to fetch OAuth token.";
    std::move(success_callback_).Run(false);
    return;
  }

  NS_LOG(VERBOSE) << __func__
                  << ": OAuth token fetched; starting stream download";

  stream_parser_ = std::make_unique<StreamParser>(
      listener, base::BindOnce(&ReceiveMessagesExpress::OnFastPathReady,
                               base::Unretained(this)));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kInstantMessagingReceiveMessageAPI);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kAuthorizationHeaderFormat, oauth_token.c_str()));

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kNetworkTimeout);
  url_loader_->AttachStringForUpload(request.SerializeAsString(),
                                     "application/x-protobuf");
  url_loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void ReceiveMessagesExpress::StopReceivingMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();
  stream_parser_.reset();

  NS_LOG(VERBOSE) << __func__ << ": callback already invoked? "
                  << (success_callback_ ? "no" : "yes");

  if (!success_callback_.is_null())
    std::move(success_callback_).Run(false);
}

void ReceiveMessagesExpress::OnDataReceived(base::StringPiece data,
                                            base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1162157): Change to DCHECK when bug is resolved.
  CHECK(stream_parser_) << __func__ << ": Stream parser unexpectedly null";

  stream_parser_->Append(data);
  std::move(resume).Run();
}

void ReceiveMessagesExpress::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Optional<NearbyShareHttpStatus> http_status =
      HttpStatusFromUrlLoader(url_loader_.get());
  url_loader_.reset();
  stream_parser_.reset();

  NS_LOG(VERBOSE) << __func__ << ": " << (success ? "success" : "failure")
                  << ", callback already invoked? "
                  << (success_callback_ ? "no" : "yes");

  if (!success_callback_.is_null()) {
    LogReceiveResult(success, http_status);
    std::move(success_callback_).Run(success);
  }
}

void ReceiveMessagesExpress::OnRetry(base::OnceClosure start_retry) {
  NOTIMPLEMENTED();
}

void ReceiveMessagesExpress::OnFastPathReady() {
  if (!success_callback_.is_null()) {
    LogReceiveResult(/*success=*/true, /*http_status=*/base::nullopt);
    std::move(success_callback_).Run(true);
  }
}
