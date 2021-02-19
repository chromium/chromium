// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"

#include "base/callback_helpers.h"
#include "base/token.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      token_fetcher_(identity_manager),
      send_message_express_(&token_fetcher_, url_loader_factory),
      url_loader_factory_(url_loader_factory) {}

WebRtcSignalingMessenger::~WebRtcSignalingMessenger() = default;

void WebRtcSignalingMessenger::SendMessage(
    const std::string& self_id,
    const std::string& peer_id,
    sharing::mojom::LocationHintPtr location_hint,
    const std::string& message,
    SendMessageCallback callback) {
  NS_LOG(VERBOSE) << __func__ << ": self_id=" << self_id
                  << ", peer_id=" << peer_id
                  << ", location hint=" << location_hint->location
                  << ", location format=" << location_hint->format
                  << ", message size=" << message.size();

  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request = BuildSendRequest(self_id, peer_id, std::move(location_hint));

  chrome_browser_nearby_sharing_instantmessaging::InboxMessage* inbox_message =
      request.mutable_message();
  inbox_message->set_message_id(base::Token::CreateRandom().ToString());
  inbox_message->set_message(message);
  inbox_message->set_message_class(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::EPHEMERAL);
  inbox_message->set_message_type(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::BASIC);

  send_message_express_.SendMessage(request, std::move(callback));
}

void WebRtcSignalingMessenger::StartReceivingMessages(
    const std::string& self_id,
    sharing::mojom::LocationHintPtr location_hint,
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    StartReceivingMessagesCallback callback) {
  // Starts a self owned mojo pipe for the receive session that can be stopped
  // with the remote returned in the start callback. Resources will be cleaned
  // up when the mojo pipe goes down.
  ReceiveMessagesExpress::StartReceiveSession(
      self_id, std::move(location_hint), std::move(incoming_messages_listener),
      std::move(callback), identity_manager_, url_loader_factory_);
}
