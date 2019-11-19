// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"

#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/enum_table.h"
#include "net/base/escape.h"

namespace cast_util {

using media_router::CastInternalMessage;

template <>
const EnumTable<CastInternalMessage::Type>
    EnumTable<CastInternalMessage::Type>::instance(
        {
            {CastInternalMessage::Type::kClientConnect, "client_connect"},
            {CastInternalMessage::Type::kAppMessage, "app_message"},
            {CastInternalMessage::Type::kV2Message, "v2_message"},
            {CastInternalMessage::Type::kLeaveSession, "leave_session"},
            {CastInternalMessage::Type::kReceiverAction, "receiver_action"},
            {CastInternalMessage::Type::kNewSession, "new_session"},
            {CastInternalMessage::Type::kUpdateSession, "update_session"},
            {CastInternalMessage::Type::kError, "error"},
            {CastInternalMessage::Type::kOther},
        },
        CastInternalMessage::Type::kMaxValue);

template <>
const EnumTable<CastInternalMessage::ErrorCode>
    EnumTable<CastInternalMessage::ErrorCode>::instance(
        {
            {CastInternalMessage::ErrorCode::kInternalError, "internal_error"},
            {CastInternalMessage::ErrorCode::kCancel, "cancel"},
            {CastInternalMessage::ErrorCode::kTimeout, "timeout"},
            {CastInternalMessage::ErrorCode::kApiNotInitialized,
             "api_not_initialized"},
            {CastInternalMessage::ErrorCode::kInvalidParameter,
             "invalid_parameter"},
            {CastInternalMessage::ErrorCode::kExtensionNotCompatible,
             "extension_not_compatible"},
            {CastInternalMessage::ErrorCode::kReceiverUnavailable,
             "receiver_unavailable"},
            {CastInternalMessage::ErrorCode::kSessionError, "session_error"},
            {CastInternalMessage::ErrorCode::kChannelError, "channel_error"},
            {CastInternalMessage::ErrorCode::kLoadMediaFailed,
             "load_media_failed"},
        },
        CastInternalMessage::ErrorCode::kMaxValue);

}  // namespace cast_util

namespace media_router {

namespace {

// The ID for the backdrop app. Cast devices running the backdrop app is
// considered idle, and an active session should not be reported.
constexpr char kBackdropAppId[] = "E8C28D3C";

bool GetString(const base::Value& value,
               const std::string& key,
               std::string* out) {
  const base::Value* string_value =
      value.FindKeyOfType(key, base::Value::Type::STRING);
  if (!string_value)
    return false;

  *out = string_value->GetString();
  return !out->empty();
}

void CopyValueWithDefault(const base::Value& from,
                          const std::string& key,
                          base::Value default_value,
                          base::Value* to) {
  const base::Value* value = from.FindKey(key);
  to->SetKey(key, value ? value->Clone() : std::move(default_value));
}

void CopyValue(const base::Value& from,
               const std::string& key,
               base::Value* to) {
  const base::Value* value = from.FindKey(key);
  if (value)
    to->SetKey(key, value->Clone());
}

CastInternalMessage::Type CastInternalMessageTypeFromString(
    const std::string& type) {
  return cast_util::StringToEnum<CastInternalMessage::Type>(type).value_or(
      CastInternalMessage::Type::kOther);
}

std::string CastInternalMessageTypeToString(CastInternalMessage::Type type) {
  auto found = cast_util::EnumToString(type);
  DCHECK(found);
  return found.value_or(base::StringPiece()).as_string();
}

// Possible types in a receiver_action message.
constexpr char kReceiverActionTypeCast[] = "cast";
constexpr char kReceiverActionTypeStop[] = "stop";

base::ListValue CapabilitiesToListValue(uint8_t capabilities) {
  base::ListValue value;
  auto& storage = value.GetList();
  if (capabilities & cast_channel::VIDEO_OUT)
    storage.emplace_back("video_out");
  if (capabilities & cast_channel::VIDEO_IN)
    storage.emplace_back("video_in");
  if (capabilities & cast_channel::AUDIO_OUT)
    storage.emplace_back("audio_out");
  if (capabilities & cast_channel::AUDIO_IN)
    storage.emplace_back("audio_in");
  if (capabilities & cast_channel::MULTIZONE_GROUP)
    storage.emplace_back("multizone_group");
  return value;
}

std::string GetReceiverLabel(const MediaSinkInternal& sink,
                             const std::string& hash_token) {
  std::string label = base::SHA1HashString(sink.sink().id() + hash_token);
  base::Base64UrlEncode(label, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &label);
  return label;
}

base::Value CreateReceiver(const MediaSinkInternal& sink,
                           const std::string& hash_token) {
  base::Value receiver(base::Value::Type::DICTIONARY);

  if (!hash_token.empty()) {
    receiver.SetKey("label", base::Value(GetReceiverLabel(sink, hash_token)));
  }

  receiver.SetKey("friendlyName",
                  base::Value(net::EscapeForHTML(sink.sink().name())));
  receiver.SetKey("capabilities",
                  CapabilitiesToListValue(sink.cast_data().capabilities));
  receiver.SetKey("volume", base::Value());
  receiver.SetKey("isActiveInput", base::Value());
  receiver.SetKey("displayStatus", base::Value());

  receiver.SetKey("receiverType", base::Value("cast"));
  return receiver;
}

blink::mojom::PresentationConnectionMessagePtr CreateMessageCommon(
    CastInternalMessage::Type type,
    base::Value payload,
    const std::string& client_id,
    base::Optional<int> sequence_number = base::nullopt) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("type", base::Value(CastInternalMessageTypeToString(type)));
  message.SetKey("message", std::move(payload));
  if (sequence_number) {
    message.SetKey("sequenceNumber", base::Value(*sequence_number));
  }
  message.SetKey("timeoutMillis", base::Value(0));
  message.SetKey("clientId", base::Value(client_id));

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return blink::mojom::PresentationConnectionMessage::NewMessage(str);
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token,
    const char* action_type) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("receiver", CreateReceiver(sink, hash_token));
  message.SetKey("action", base::Value(action_type));

  return CreateMessageCommon(CastInternalMessage::Type::kReceiverAction,
                             std::move(message), client_id);
}

base::Value CreateAppMessageBody(
    const std::string& session_id,
    const cast_channel::CastMessage& cast_message) {
  // TODO(https://crbug.com/862532): Investigate whether it is possible to move
  // instead of copying the contents of |cast_message|. Right now copying is
  // done because the message is passed as a const ref at the
  // CastSocket::Observer level.
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("sessionId", base::Value(session_id));
  message.SetKey("namespaceName", base::Value(cast_message.namespace_()));
  switch (cast_message.payload_type()) {
    case cast_channel::CastMessage_PayloadType_STRING:
      message.SetKey("message", base::Value(cast_message.payload_utf8()));
      break;
    case cast_channel::CastMessage_PayloadType_BINARY: {
      const auto& payload = cast_message.payload_binary();
      message.SetKey("message",
                     base::Value(base::Value::BlobStorage(
                         payload.front(), payload.front() + payload.size())));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return message;
}

// Creates a message with a session value in the "message" field. |type| must
// be either kNewSession or kUpdateSession.
blink::mojom::PresentationConnectionMessagePtr CreateSessionMessage(
    const CastSession& session,
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token,
    CastInternalMessage::Type type) {
  DCHECK(type == CastInternalMessage::Type::kNewSession ||
         type == CastInternalMessage::Type::kUpdateSession);
  base::Value session_with_receiver_label = session.value().Clone();
  DCHECK(!session_with_receiver_label.FindPath({"receiver", "label"}));
  session_with_receiver_label.SetPath(
      {"receiver", "label"}, base::Value(GetReceiverLabel(sink, hash_token)));
  return CreateMessageCommon(type, std::move(session_with_receiver_label),
                             client_id);
}

}  // namespace

// static
std::unique_ptr<CastInternalMessage> CastInternalMessage::From(
    base::Value message) {
  if (!message.is_dict()) {
    DVLOG(2) << "Failed to read JSON message: " << message;
    return nullptr;
  }

  std::string str_type;
  if (!GetString(message, "type", &str_type)) {
    DVLOG(2) << "Missing type value, message: " << message;
    return nullptr;
  }

  CastInternalMessage::Type message_type =
      CastInternalMessageTypeFromString(str_type);
  if (message_type == CastInternalMessage::Type::kOther) {
    DVLOG(2) << __func__ << ": Unsupported message type: " << str_type
             << ", message: " << message;
    return nullptr;
  }

  std::string client_id;

  if (!GetString(message, "clientId", &client_id)) {
    DVLOG(2) << "Missing clientId, message: " << message;
    return nullptr;
  }

  base::Value* message_body_value = message.FindKey("message");
  if (!message_body_value ||
      (!message_body_value->is_dict() && !message_body_value->is_string())) {
    DVLOG(2) << "Missing message body, message: " << message;
    return nullptr;
  }

  base::Value* sequence_number_value =
      message.FindKeyOfType("sequenceNumber", base::Value::Type::INTEGER);

  std::string session_id;
  std::string namespace_or_v2_type;
  base::Value new_message_body;

  if (message_type == Type::kAppMessage || message_type == Type::kV2Message) {
    if (!GetString(*message_body_value, "sessionId", &session_id)) {
      DVLOG(2) << "Missing sessionId, message: " << message;
      return nullptr;
    }

    if (!message_body_value->is_dict()) {
      DVLOG(2) << "Message body is not a dict: " << message;
      return nullptr;
    }

    if (message_type == Type::kAppMessage) {
      if (!GetString(*message_body_value, "namespaceName",
                     &namespace_or_v2_type)) {
        DVLOG(2) << "Missing namespace, message: " << message;
        return nullptr;
      }

      base::Value* app_message_value = message_body_value->FindKey("message");
      if (!app_message_value ||
          (!app_message_value->is_dict() && !app_message_value->is_string())) {
        DVLOG(2) << "Missing app message, message: " << message;
        return nullptr;
      }
      new_message_body = std::move(*app_message_value);
    } else if (message_type == CastInternalMessage::Type::kV2Message) {
      if (!GetString(*message_body_value, "type", &namespace_or_v2_type)) {
        DVLOG(2) << "Missing v2 type, message: " << message;
        return nullptr;
      }
      new_message_body = std::move(*message_body_value);
    }
  }

  return base::WrapUnique(new CastInternalMessage(
      message_type, client_id,
      sequence_number_value ? sequence_number_value->GetInt()
                            : base::Optional<int>(),
      session_id, namespace_or_v2_type, std::move(new_message_body)));
}

CastInternalMessage::~CastInternalMessage() = default;

CastInternalMessage::CastInternalMessage(
    Type type,
    const std::string& client_id,
    base::Optional<int> sequence_number,
    const std::string& session_id,
    const std::string& namespace_or_v2_type,
    base::Value message_body)
    : type_(type),
      client_id_(client_id),
      sequence_number_(sequence_number),
      session_id_(session_id),
      namespace_or_v2_type_(namespace_or_v2_type),
      message_body_(std::move(message_body)) {}

// static
std::unique_ptr<CastSession> CastSession::From(
    const MediaSinkInternal& sink,
    const base::Value& receiver_status) {
  // There should be only 1 app on |receiver_status|.
  const base::Value* app_list_value =
      receiver_status.FindKeyOfType("applications", base::Value::Type::LIST);
  if (!app_list_value || app_list_value->GetList().size() != 1) {
    DVLOG(2) << "receiver_status does not contain exactly one app: "
             << receiver_status;
    return nullptr;
  }

  auto session = std::make_unique<CastSession>();

  // Fill in mandatory Session fields.
  const base::Value& app_value = app_list_value->GetList()[0];
  if (!GetString(app_value, "sessionId", &session->session_id_) ||
      !GetString(app_value, "appId", &session->app_id_) ||
      !GetString(app_value, "transportId", &session->transport_id_) ||
      !GetString(app_value, "displayName", &session->display_name_)) {
    DVLOG(2) << "app_value missing mandatory fields: " << app_value;
    return nullptr;
  }

  if (session->app_id_ == kBackdropAppId) {
    DVLOG(2) << sink.sink().id() << " is running the backdrop app";
    return nullptr;
  }

  // Optional Session fields.
  GetString(app_value, "statusText", &session->status_);

  // The receiver label will be populated by each profile using
  // |session->value|.
  base::Value receiver_value = CreateReceiver(sink, std::string());
  CopyValue(receiver_status, "volume", &receiver_value);
  CopyValue(receiver_status, "isActiveInput", &receiver_value);

  // Create |session->value|.
  session->value_ = base::Value(base::Value::Type::DICTIONARY);
  auto& session_value = session->value_;
  session_value.SetKey("sessionId", base::Value(session->session_id()));
  session_value.SetKey("appId", base::Value(session->app_id()));
  session_value.SetKey("transportId", base::Value(session->transport_id()));
  session_value.SetKey("receiver", std::move(receiver_value));

  CopyValueWithDefault(app_value, "displayName", base::Value(""),
                       &session_value);
  CopyValueWithDefault(app_value, "senderApps", base::ListValue(),
                       &session_value);
  CopyValueWithDefault(app_value, "statusText", base::Value(), &session_value);
  CopyValueWithDefault(app_value, "appImages", base::ListValue(),
                       &session_value);

  const base::Value* namespaces_value =
      app_value.FindKeyOfType("namespaces", base::Value::Type::LIST);
  if (!namespaces_value || namespaces_value->GetList().empty()) {
    // A session without namespaces is invalid, except for a multizone leader.
    if (session->app_id() != kMultizoneLeaderAppId) {
      DVLOG(2) << "Message is missing namespaces.";
      return nullptr;
    }
  } else {
    for (const auto& namespace_value : namespaces_value->GetList()) {
      std::string message_namespace;
      if (!namespace_value.is_dict() ||
          !GetString(namespace_value, "name", &message_namespace)) {
        DVLOG(2) << "Missing namespace name.";
        return nullptr;
      }

      session->message_namespaces_.insert(std::move(message_namespace));
    }
  }
  session_value.SetKey("namespaces",
                       namespaces_value ? namespaces_value->Clone()
                                        : base::Value(base::Value::Type::LIST));
  return session;
}

CastSession::CastSession() = default;
CastSession::~CastSession() = default;

std::string CastSession::GetRouteDescription() const {
  return !status_.empty() ? status_ : display_name_;
}

void CastSession::UpdateSession(std::unique_ptr<CastSession> from) {
  status_ = std::move(from->status_);
  message_namespaces_ = std::move(from->message_namespaces_);

  auto* status_text_value = from->value_.FindKey("statusText");
  DCHECK(status_text_value);
  value_.SetKey("statusText", std::move(*status_text_value));
  auto* namespaces_value = from->value_.FindKey("namespaces");
  DCHECK(namespaces_value);
  value_.SetKey("namespaces", std::move(*namespaces_value));
  auto* receiver_volume_value = from->value_.FindPath({"receiver", "volume"});
  DCHECK(receiver_volume_value);
  value_.SetPath({"receiver", "volume"}, std::move(*receiver_volume_value));
}

void CastSession::UpdateMedia(const base::Value& media) {
  value_.SetKey("media", media.Clone());
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionCastMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreateReceiverActionMessage(client_id, sink, hash_token,
                                     kReceiverActionTypeCast);
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionStopMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreateReceiverActionMessage(client_id, sink, hash_token,
                                     kReceiverActionTypeStop);
}

blink::mojom::PresentationConnectionMessagePtr CreateNewSessionMessage(
    const CastSession& session,
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreateSessionMessage(session, client_id, sink, hash_token,
                              CastInternalMessage::Type::kNewSession);
}

blink::mojom::PresentationConnectionMessagePtr CreateUpdateSessionMessage(
    const CastSession& session,
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreateSessionMessage(session, client_id, sink, hash_token,
                              CastInternalMessage::Type::kUpdateSession);
}

blink::mojom::PresentationConnectionMessagePtr CreateAppMessageAck(
    const std::string& client_id,
    int sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kAppMessage,
                             base::Value(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateAppMessage(
    const std::string& session_id,
    const std::string& client_id,
    const cast_channel::CastMessage& cast_message) {
  return CreateMessageCommon(CastInternalMessage::Type::kAppMessage,
                             CreateAppMessageBody(session_id, cast_message),
                             client_id);
}

blink::mojom::PresentationConnectionMessagePtr CreateV2Message(
    const std::string& client_id,
    const base::Value& payload,
    base::Optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kV2Message,
                             payload.Clone(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateLeaveSessionAckMessage(
    const std::string& client_id,
    base::Optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kLeaveSession,
                             base::Value(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateErrorMessage(
    const std::string& client_id,
    base::Value error,
    base::Optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kError,
                             std::move(error), client_id, sequence_number);
}

base::Value SupportedMediaRequestsToListValue(int media_requests) {
  base::Value value(base::Value::Type::LIST);
  auto& storage = value.GetList();
  if (media_requests & 1)
    storage.emplace_back("pause");
  if (media_requests & 2)
    storage.emplace_back("seek");
  if (media_requests & 4)
    storage.emplace_back("stream_volume");
  if (media_requests & 8)
    storage.emplace_back("stream_mute");
  return value;
}

}  // namespace media_router
