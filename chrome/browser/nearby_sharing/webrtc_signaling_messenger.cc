// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"

#include "base/functional/callback_helpers.h"
#include "base/token.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"
#include "components/cross_device/logging/logging.h"

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

WebRtcSignalingMessenger::~WebRtcSignalingMessenger() = default;

void WebRtcSignalingMessenger::SendMessage(
    const std::string& self_id,
    const std::string& peer_id,
    ::sharing::mojom::LocationHintPtr location_hint,
    const std::string& message,
    SendMessageCallback callback) {
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request = BuildSendRequest(self_id, peer_id, std::move(location_hint));

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": self_id=" << self_id << ", peer_id=" << peer_id
      << ", request_id=" << request.header().request_id()
      << ", message size=" << message.size();

  chrome_browser_nearby_sharing_instantmessaging::InboxMessage* inbox_message =
      request.mutable_message();
  inbox_message->set_message_id(base::Token::CreateRandom().ToString());
  inbox_message->set_message(message);
  inbox_message->set_message_class(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::EPHEMERAL);
  inbox_message->set_message_type(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::BASIC);

  // We tie the lifetime of the SendMessageExpress object to the lifetime of the
  // mojo call. Once the call completes, we allow the unique_ptr to go out of
  // scope in the lambda cleaning up all resources.
  auto send_message_express = std::make_unique<SendMessageExpress>(
      identity_manager_, url_loader_factory_);
  // The call to SendMessage is done on the raw pointer so we can std::move the
  // unique_ptr into the bind closure without 'use-after-move' warnings.
  auto* send_message_express_ptr = send_message_express.get();
  send_message_express_ptr->SendMessage(
      request,
      base::BindOnce(
          [](SendMessageCallback cb,
             std::unique_ptr<SendMessageExpress> send_message, bool success) {
            // Complete the original mojo call.
            std::move(cb).Run(success);
            // Intentionally let |send_message| go out of scope and delete the
            // object.
          },
          std::move(callback), std::move(send_message_express)));
}

void WebRtcSignalingMessenger::StartReceivingMessages(
    const std::string& self_id,
    ::sharing::mojom::LocationHintPtr location_hint,
    mojo::PendingRemote<::sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    StartReceivingMessagesCallback callback) {
  // Starts a self owned mojo pipe for the receive session that can be stopped
  // with the remote returned in the start callback. Resources will be cleaned
  // up when the mojo pipe goes down.
  ReceiveMessagesExpress::StartReceiveSession(
      self_id, std::move(location_hint), std::move(incoming_messages_listener),
      std::move(callback), identity_manager_, url_loader_factory_);
}
