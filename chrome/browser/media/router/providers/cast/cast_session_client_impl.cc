// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_client_impl.h"

#include <vector>

#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessagePtr;
using blink::mojom::PresentationConnectionState;

namespace media_router {

namespace {

void ReportClientMessageParseError(const MediaRoute::Id& route_id,
                                   const std::string& error) {
  // TODO(crbug.com/41426190): Record UMA metric for parse result.
  DLOG(ERROR) << "Failed to parse Cast client message for " << route_id << ": "
              << error;
}

// Traverses a JSON value, recursively removing any dict entries whose value is
// null.
void RemoveNullFields(base::Value& value) {
  if (auto* list = value.GetIfList()) {
    for (auto& item : *list) {
      RemoveNullFields(item);
    }
  } else if (auto* dict = value.GetIfDict()) {
    std::vector<std::string> to_remove;
    for (auto [key, val] : *dict) {
      if (val.is_none()) {
        to_remove.push_back(key);
      } else {
        RemoveNullFields(val);
      }
    }
    for (const auto& key : to_remove) {
      dict->Remove(key);
    }
  }
}

}  // namespace

CastSessionClientImpl::CastSessionClientImpl(
    const std::string& client_id,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id,
    AutoJoinPolicy auto_join_policy,
    CastActivity* activity)
    : CastSessionClient(client_id, origin, frame_tree_node_id),
      auto_join_policy_(auto_join_policy),
      activity_(activity) {}

CastSessionClientImpl::~CastSessionClientImpl() = default;

mojom::RoutePresentationConnectionPtr CastSessionClientImpl::Init() {
  auto renderer_connection = connection_receiver_.BindNewPipeAndPassRemote();
  mojo::PendingRemote<blink::mojom::PresentationConnection>
      pending_connection_remote;
  auto connection_receiver =
      pending_connection_remote.InitWithNewPipeAndPassReceiver();
  connection_remote_.Bind(std::move(pending_connection_remote));
  connection_remote_->DidChangeState(PresentationConnectionState::CONNECTED);
  return mojom::RoutePresentationConnection::New(
      std::move(renderer_connection), std::move(connection_receiver));
}

void CastSessionClientImpl::SendMessageToClient(
    PresentationConnectionMessagePtr message) {
  connection_remote_->OnMessage(std::move(message));
}

void CastSessionClientImpl::SendMediaMessageToClient(
    const base::Value::Dict& payload,
    std::optional<int> request_id) {
  // Look up if there is a pending request from this client associated with this
  // message. If so, send the media status message as a response by setting the
  // sequence number.
  std::optional<int> sequence_number;
  if (request_id) {
    auto it = pending_media_requests_.find(*request_id);
    if (it != pending_media_requests_.end()) {
      DVLOG(2) << "Found matching request id: " << *request_id << " -> "
               << it->second;
      sequence_number = it->second;
      pending_media_requests_.erase(it);
    }
  }
  SendMessageToClient(CreateV2Message(client_id(), payload, sequence_number));
}

bool CastSessionClientImpl::MatchesAutoJoinPolicy(
    url::Origin other_origin,
    content::FrameTreeNodeId other_frame_tree_node_id) const {
  switch (auto_join_policy_) {
    case AutoJoinPolicy::kPageScoped:
      return false;
    case AutoJoinPolicy::kTabAndOriginScoped:
      return other_origin == origin() &&
             other_frame_tree_node_id == frame_tree_node_id();
    case AutoJoinPolicy::kOriginScoped:
      return other_origin == origin();
  }
}

void CastSessionClientImpl::OnMessage(
    PresentationConnectionMessagePtr message) {
  if (!message->is_message())
    return;

  GetDataDecoder().ParseJson(
      message->get_message(),
      base::BindOnce(&CastSessionClientImpl::HandleParsedClientMessage,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CastSessionClientImpl::DidClose(PresentationConnectionCloseReason reason) {
  activity_->CloseConnectionOnReceiver(client_id(), reason);
}

void CastSessionClientImpl::SendErrorCodeToClient(
    int sequence_number,
    CastInternalMessage::ErrorCode error_code,
    std::optional<std::string> description) {
  base::Value::Dict message;
  message.Set("code", base::Value(*cast_util::EnumToString(error_code)));
  message.Set("description",
              description ? base::Value(*description) : base::Value());
  message.Set("details", base::Value());
  SendErrorToClient(sequence_number, std::move(message));
}

void CastSessionClientImpl::SendErrorToClient(int sequence_number,
                                              base::Value::Dict error) {
  SendMessageToClient(
      CreateErrorMessage(client_id(), std::move(error), sequence_number));
}

void CastSessionClientImpl::HandleParsedClientMessage(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result.value().is_dict()) {
    ReportClientMessageParseError(activity_->route().media_route_id(),
                                  result.error());
    return;
  }

  // NOTE(jrw): This step isn't part of the Cast protocol per se, but it's
  // required for backward compatibility.  There is one known case
  // (crbug.com/1129217) where not doing it breaks the Cast SDK.
  RemoveNullFields(*result);

  std::unique_ptr<CastInternalMessage> cast_message =
      CastInternalMessage::From(std::move(result.value().GetDict()));
  if (!cast_message) {
    ReportClientMessageParseError(activity_->route().media_route_id(),
                                  "Not a Cast message");
    return;
  }

  if (cast_message->client_id() != client_id()) {
    DLOG(ERROR) << "Client ID mismatch: expected: " << client_id()
                << ", got: " << cast_message->client_id();
    return;
  }

  if (cast_message->has_session_id() &&
      cast_message->session_id() != activity_->session_id()) {
    DLOG(ERROR) << "Session ID mismatch: expected: "
                << activity_->session_id().value_or("<missing>")
                << ", got: " << cast_message->session_id();
    return;
  }

  switch (cast_message->type()) {
    case CastInternalMessage::Type::kAppMessage:
      // Send an ACK message back to SDK client to indicate it is handled.
      if (activity_->SendAppMessageToReceiver(*cast_message) ==
          cast_channel::Result::kOk) {
        DCHECK(cast_message->sequence_number());
        SendMessageToClient(CreateAppMessageAck(
            cast_message->client_id(), *cast_message->sequence_number()));
      }
      break;

    case CastInternalMessage::Type::kV2Message:
      HandleV2ProtocolMessage(*cast_message);
      break;

    case CastInternalMessage::Type::kLeaveSession:
      SendMessageToClient(CreateLeaveSessionAckMessage(
          client_id(), cast_message->sequence_number()));
      activity_->HandleLeaveSession(client_id());
      break;

    case CastInternalMessage::Type::kClientConnect:
      // This message type is obsolete and can be safely ignored (see b/34104690
      // and the corresponding TODO in the Cast API implementation).
      break;

    default:
      auto opt_string = cast_util::EnumToString(cast_message->type());
      if (opt_string) {
        DLOG(ERROR) << "Unhandled message type: " << *opt_string;
      } else {
        DLOG(ERROR) << "Invalid message type: "
                    << static_cast<int>(cast_message->type());
      }
  }
}

void CastSessionClientImpl::HandleV2ProtocolMessage(
    const CastInternalMessage& cast_message) {
  const std::string& type_str = cast_message.v2_message_type();
  cast_channel::V2MessageType type =
      cast_channel::V2MessageTypeFromString(type_str);
  if (cast_channel::IsMediaRequestMessageType(type)) {
    DVLOG(2) << "Got media command from client: " << type_str;
    std::optional<int> request_id =
        activity_->SendMediaRequestToReceiver(cast_message);
    if (request_id) {
      DCHECK(cast_message.sequence_number());
      if (pending_media_requests_.size() >= kMaxPendingMediaRequests) {
        // Delete old pending requests.  Request IDs are generated sequentially,
        // so this should always delete the oldest requests.  Deleting requests
        // is O(n) in the size of the table, so we delete half the outstanding
        // requests at once so the amortized deletion cost is O(1).
        pending_media_requests_.erase(pending_media_requests_.begin(),
                                      pending_media_requests_.begin() +
                                          pending_media_requests_.size() / 2);
      }
      pending_media_requests_.emplace(*request_id,
                                      *cast_message.sequence_number());
    }
  } else if (type == cast_channel::V2MessageType::kSetVolume) {
    DVLOG(2) << "Got volume command from client";
    DCHECK(cast_message.sequence_number());
    activity_->SendSetVolumeRequestToReceiver(cast_message,
                                              MakeResultCallback(cast_message));
  } else if (type == cast_channel::V2MessageType::kStop) {
    activity_->StopSessionOnReceiver(cast_message.client_id(),
                                     MakeResultCallback(cast_message));
  } else {
    DLOG(ERROR) << "Unknown v2 message type: " << type_str;
  }
}

void CastSessionClientImpl::SendResultResponse(int sequence_number,
                                               cast_channel::Result result) {
  if (result == cast_channel::Result::kOk) {
    // Send an empty message to let the client know the request succeeded.
    SendMessageToClient(
        CreateV2Message(client_id(), base::Value::Dict(), sequence_number));
  } else {
    // TODO(crbug.com/41452006): Send correct error codes.  The original
    // implementation isn't much help here because it sends incorrectly
    // formatted error messages without a valid error code in a lot of cases.
    SendErrorCodeToClient(sequence_number,
                          CastInternalMessage::ErrorCode::kInternalError,
                          "unknown error");
  }
}

void CastSessionClientImpl::CloseConnection(
    PresentationConnectionCloseReason close_reason) {
  if (connection_remote_)
    connection_remote_->DidClose(close_reason);
  TearDownPresentationConnection();
  activity_->CloseConnectionOnReceiver(client_id(), close_reason);
}

void CastSessionClientImpl::TerminateConnection() {
  if (connection_remote_)
    connection_remote_->DidChangeState(PresentationConnectionState::TERMINATED);

  TearDownPresentationConnection();
}

void CastSessionClientImpl::TearDownPresentationConnection() {
  connection_remote_.reset();
  connection_receiver_.reset();
}

cast_channel::ResultCallback CastSessionClientImpl::MakeResultCallback(
    const CastInternalMessage& cast_message) {
  DCHECK(cast_message.sequence_number());
  return base::BindOnce(&CastSessionClientImpl::SendResultResponse,
                        weak_ptr_factory_.GetWeakPtr(),
                        *cast_message.sequence_number());
}
}  // namespace media_router
