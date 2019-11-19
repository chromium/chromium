// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "components/cast_channel/enum_table.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessagePtr;
using blink::mojom::PresentationConnectionState;

namespace media_router {

namespace {

void ReportClientMessageParseError(const MediaRoute::Id& route_id,
                                   const std::string& error) {
  // TODO(crbug.com/905002): Record UMA metric for parse result.
  DLOG(ERROR) << "Failed to parse Cast client message for " << route_id << ": "
              << error;
}

}  // namespace

CastSessionClient::CastSessionClient(const std::string& client_id,
                                     const url::Origin& origin,
                                     int tab_id)
    : client_id_(client_id), origin_(origin), tab_id_(tab_id) {}

CastSessionClient::~CastSessionClient() = default;

CastSessionClientImpl::CastSessionClientImpl(const std::string& client_id,
                                             const url::Origin& origin,
                                             int tab_id,
                                             AutoJoinPolicy auto_join_policy,
                                             ActivityRecord* activity)
    : CastSessionClient(client_id, origin, tab_id),
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

void CastSessionClientImpl::SendMediaStatusToClient(
    const base::Value& media_status,
    base::Optional<int> request_id) {
  // Look up if there is a pending request from this client associated with this
  // message. If so, send the media status message as a response by setting the
  // sequence number.
  base::Optional<int> sequence_number;
  if (request_id) {
    auto it = pending_media_requests_.find(*request_id);
    if (it != pending_media_requests_.end()) {
      DVLOG(2) << "Found matching request id: " << *request_id << " -> "
               << it->second;
      sequence_number = it->second;
      pending_media_requests_.erase(it);
    }
  }

  SendMessageToClient(
      CreateV2Message(client_id(), media_status, sequence_number));
}

bool CastSessionClientImpl::MatchesAutoJoinPolicy(url::Origin other_origin,
                                                  int other_tab_id) const {
  switch (auto_join_policy_) {
    case AutoJoinPolicy::kPageScoped:
      return false;
    case AutoJoinPolicy::kTabAndOriginScoped:
      return other_origin == origin() && other_tab_id == tab_id();
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
      base::BindRepeating(&CastSessionClientImpl::HandleParsedClientMessage,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CastSessionClientImpl::DidClose(PresentationConnectionCloseReason reason) {
  // TODO(https://crbug.com/809249): Implement close connection with this
  // method once we make sure Blink calls this on navigation and on
  // PresentationConnection::close().
}

void CastSessionClientImpl::SendErrorCodeToClient(
    int sequence_number,
    CastInternalMessage::ErrorCode error_code,
    base::Optional<std::string> description) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("code", base::Value(*cast_util::EnumToString(error_code)));
  message.SetKey("description",
                 description ? base::Value(*description) : base::Value());
  message.SetKey("details", base::Value());
  SendErrorToClient(sequence_number, std::move(message));
}

void CastSessionClientImpl::SendErrorToClient(int sequence_number,
                                              base::Value error) {
  SendMessageToClient(
      CreateErrorMessage(client_id(), std::move(error), sequence_number));
}

void CastSessionClientImpl::HandleParsedClientMessage(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    ReportClientMessageParseError(activity_->route().media_route_id(),
                                  *result.error);
    return;
  }

  std::unique_ptr<CastInternalMessage> cast_message =
      CastInternalMessage::From(std::move(*result.value));
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

    default:
      // TODO(jrw): Log string value of type instead of int value.
      DLOG(ERROR) << "Unhandled message type: "
                  << static_cast<int>(cast_message->type());
  }
}

void CastSessionClientImpl::HandleV2ProtocolMessage(
    const CastInternalMessage& cast_message) {
  const std::string& type_str = cast_message.v2_message_type();
  cast_channel::V2MessageType type =
      cast_channel::V2MessageTypeFromString(type_str);
  if (cast_channel::IsMediaRequestMessageType(type)) {
    DVLOG(2) << "Got media command from client: " << type_str;
    base::Optional<int> request_id =
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
    activity_->SendSetVolumeRequestToReceiver(
        cast_message, base::BindOnce(&CastSessionClientImpl::SendResultResponse,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     *cast_message.sequence_number()));
  } else if (type == cast_channel::V2MessageType::kStop) {
    // TODO(jrw): implement STOP_SESSION.
    DVLOG(2) << "Ignoring stop-session (" << type_str << ") message";
  } else {
    DLOG(ERROR) << "Unknown v2 message type: " << type_str;
  }
}

void CastSessionClientImpl::SendResultResponse(int sequence_number,
                                               cast_channel::Result result) {
  if (result == cast_channel::Result::kOk) {
    // Send an empty message to let the client know the request succeeded.
    SendMessageToClient(
        CreateV2Message(client_id(), base::Value(), sequence_number));
  } else {
    // TODO(crbug.com/951089): Send correct error codes.  The original
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

}  // namespace media_router
