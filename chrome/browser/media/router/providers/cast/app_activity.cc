// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/app_activity.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessagePtr;

namespace media_router {

namespace {

bool IsMediaStatusMessage(const cast_channel::InternalMessage& message) {
  return message.message_namespace == cast_channel::kMediaNamespace &&
         message.type == cast_channel::CastMessageType::kMediaStatus;
}

}  // namespace

AppActivity::AppActivity(const MediaRoute& route,
                         const std::string& app_id,
                         cast_channel::CastMessageHandler* message_handler,
                         CastSessionTracker* session_tracker)
    : CastActivity(route, app_id, message_handler, session_tracker) {}

AppActivity::~AppActivity() = default;

void AppActivity::OnSessionSet(const CastSession& session) {
  if (media_controller_)
    media_controller_->SetSession(session);
}

void AppActivity::OnSessionUpdated(const CastSession& session,
                                   const std::string& hash_token) {
  for (auto& client : connected_clients_) {
    client.second->SendMessageToClient(
        CreateUpdateSessionMessage(session, client.first, sink_, hash_token));
  }
  if (media_controller_)
    media_controller_->SetSession(session);
}

cast_channel::Result AppActivity::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  CastSessionClient* client = GetClient(cast_message.client_id());
  const CastSession* session = GetSession();
  if (!session) {
    if (client && cast_message.sequence_number()) {
      client->SendErrorCodeToClient(
          *cast_message.sequence_number(),
          CastInternalMessage::ErrorCode::kSessionError,
          "Invalid session ID: " + session_id_.value_or("<missing>"));
    }

    return cast_channel::Result::kFailed;
  }
  const std::string& message_namespace = cast_message.app_message_namespace();
  if (!base::Contains(session->message_namespaces(), message_namespace)) {
    DLOG(ERROR) << "Disallowed message namespace: " << message_namespace;
    if (client && cast_message.sequence_number()) {
      client->SendErrorCodeToClient(
          *cast_message.sequence_number(),
          CastInternalMessage::ErrorCode::kInvalidParameter,
          "Invalid namespace: " + message_namespace);
    }
    return cast_channel::Result::kFailed;
  }
  return message_handler_->SendAppMessage(
      cast_channel_id(),
      cast_channel::CreateCastMessage(
          message_namespace, cast_message.app_message_body(),
          cast_message.client_id(), session->destination_id()));
}

std::optional<int> AppActivity::SendMediaRequestToReceiver(
    const CastInternalMessage& cast_message) {
  CastSession* session = GetSession();
  if (!session)
    return std::nullopt;
  return message_handler_->SendMediaRequest(
      cast_channel_id(), cast_message.v2_message_body(),
      cast_message.client_id(), session->destination_id());
}

void AppActivity::SendSetVolumeRequestToReceiver(
    const CastInternalMessage& cast_message,
    cast_channel::ResultCallback callback) {
  message_handler_->SendSetVolumeRequest(
      cast_channel_id(), cast_message.v2_message_body(),
      cast_message.client_id(), std::move(callback));
}

void AppActivity::SendMediaStatusToClients(
    const base::Value::Dict& media_status,
    std::optional<int> request_id) {
  CastActivity::SendMediaStatusToClients(media_status, request_id);
  if (media_controller_)
    media_controller_->SetMediaStatus(media_status);
}

void AppActivity::BindMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  if (!media_controller_) {
    media_controller_ = std::make_unique<CastMediaController>(this);
  }
  media_controller_->AddMediaController(std::move(media_controller),
                                        std::move(observer));

  if (session_id_) {
    CastSession* session = GetSession();
    if (session) {
      media_controller_->SetSession(*session);
      base::Value::Dict status_request;
      status_request.Set("type",
                         cast_util::EnumToString<
                             cast_channel::V2MessageType,
                             cast_channel::V2MessageType::kMediaGetStatus>());
      message_handler_->SendMediaRequest(cast_channel_id(), status_request,
                                         media_controller_->sender_id(),
                                         session->destination_id());
    }
  }
}

void AppActivity::OnAppMessage(
    const openscreen::cast::proto::CastMessage& message) {
  if (!session_id_) {
    DVLOG(2) << "No session associated with activity!";
    return;
  }

  const std::string& client_id = message.destination_id();
  if (client_id == "*") {
    for (const auto& client : connected_clients_) {
      SendMessageToClient(
          client.first, CreateAppMessage(*session_id_, client.first, message));
    }
  } else {
    SendMessageToClient(client_id,
                        CreateAppMessage(*session_id_, client_id, message));
  }
}

void AppActivity::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  // Forward messages in the media namespace other than media statuses to the
  // client. Media status messages are handled by SendMediaStatusToClients().
  if (message.message_namespace == cast_channel::kMediaNamespace &&
      !IsMediaStatusMessage(message)) {
    std::optional<int> request_id =
        cast_channel::GetRequestIdFromResponse(message.message);
    auto client_it = connected_clients_.find(message.destination_id);
    // Okay to drop messages for clients that have gone away.
    if (client_it != connected_clients_.end()) {
      client_it->second->SendMediaMessageToClient(message.message, request_id);
    }
  }
}

bool AppActivity::CanJoinSession(const CastMediaSource& cast_source) const {
  if (!cast_source.ContainsApp(app_id()))
    return false;

  if (base::Contains(connected_clients_, cast_source.client_id()))
    return false;

  return true;
}

bool AppActivity::HasJoinableClient(
    AutoJoinPolicy policy,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id) const {
  return base::ranges::any_of(
      connected_clients_,
      [policy, &origin, frame_tree_node_id](const auto& client) {
        return IsAutoJoinAllowed(policy, origin, frame_tree_node_id.value(),
                                 client.second->origin(),
                                 client.second->frame_tree_node_id().value());
      });
}

}  // namespace media_router
