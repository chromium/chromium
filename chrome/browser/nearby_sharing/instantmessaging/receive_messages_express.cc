// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
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
  if (!success_callback_.is_null()) {
    // A pending callback was found, this should be marked as false since the
    // previous receiver has not yet started listening for messages and is now
    // going to be replaced by the new listener.
    std::move(success_callback_).Run(false);
  }
  success_callback_ = std::move(callback);
  url_loader_.reset();
  stream_parser_ = std::make_unique<StreamParser>(listener);

  token_fetcher_->GetAccessToken(
      base::BindOnce(&ReceiveMessagesExpress::DoStartReceivingMessages,
                     weak_ptr_factory_.GetWeakPtr(), request));
}

void ReceiveMessagesExpress::DoStartReceivingMessages(
    const chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesExpressRequest& request,
    const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (oauth_token.empty()) {
    std::move(success_callback_).Run(false);
    return;
  }

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
  if (!success_callback_.is_null())
    std::move(success_callback_).Run(false);

  url_loader_.reset();
  stream_parser_.reset();
}

void ReceiveMessagesExpress::OnDataReceived(base::StringPiece data,
                                            base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success_callback_.is_null())
    std::move(success_callback_).Run(true);

  stream_parser_->Append(data);
  std::move(resume).Run();
}

void ReceiveMessagesExpress::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success_callback_.is_null())
    std::move(success_callback_).Run(success);

  url_loader_.reset();
  stream_parser_.reset();
}

void ReceiveMessagesExpress::OnRetry(base::OnceClosure start_retry) {
  NOTIMPLEMENTED();
}
