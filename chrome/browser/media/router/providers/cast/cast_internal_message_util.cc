// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/escape.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"

using cast_channel::CastDeviceCapability;
using cast_channel::CastDeviceCapabilitySet;

namespace cast_util {

using media_router::CastInternalMessage;

template <>
const EnumTable<CastInternalMessage::Type>&
EnumTable<CastInternalMessage::Type>::GetInstance() {
  static const EnumTable<CastInternalMessage::Type> kInstance(
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
  return kInstance;
}

template <>
const EnumTable<CastInternalMessage::ErrorCode>&
EnumTable<CastInternalMessage::ErrorCode>::GetInstance() {
  static const EnumTable<CastInternalMessage::ErrorCode> kInstance(
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
  return kInstance;
}

}  // namespace cast_util

namespace media_router {

namespace {

// The ID for the backdrop app. Cast devices running the backdrop app is
// considered idle, and an active session should not be reported.
constexpr char kBackdropAppId[] = "E8C28D3C";

bool GetString(const base::Value::Dict& value,
               const std::string& key,
               std::string* out) {
  const std::string* string = value.FindString(key);
  if (!string)
    return false;

  *out = *string;
  return !out->empty();
}

void CopyValueWithDefault(const base::Value::Dict& from,
                          const std::string& key,
                          base::Value default_value,
                          base::Value::Dict& to) {
  const base::Value* value = from.Find(key);
  to.Set(key, value ? value->Clone() : std::move(default_value));
}

void CopyValue(const base::Value::Dict& from,
               const std::string& key,
               base::Value::Dict& to) {
  const base::Value* value = from.Find(key);
  if (value)
    to.Set(key, value->Clone());
}

CastInternalMessage::Type CastInternalMessageTypeFromString(
    const std::string& type) {
  return cast_util::StringToEnum<CastInternalMessage::Type>(type).value_or(
      CastInternalMessage::Type::kOther);
}

std::string CastInternalMessageTypeToString(CastInternalMessage::Type type) {
  auto found = cast_util::EnumToString(type);
  DCHECK(found);
  return std::string(found.value_or(std::string_view()));
}

// Possible types in a receiver_action message.
constexpr char kReceiverActionTypeCast[] = "cast";
constexpr char kReceiverActionTypeStop[] = "stop";

base::Value::List CapabilitiesToListValue(
    CastDeviceCapabilitySet capabilities) {
  base::Value::List value;
  if (capabilities.Has(CastDeviceCapability::kVideoOut)) {
    value.Append("video_out");
  }
  if (capabilities.Has(CastDeviceCapability::kVideoIn)) {
    value.Append("video_in");
  }
  if (capabilities.Has(CastDeviceCapability::kAudioOut)) {
    value.Append("audio_out");
  }
  if (capabilities.Has(CastDeviceCapability::kAudioIn)) {
    value.Append("audio_in");
  }
  if (capabilities.Has(CastDeviceCapability::kMultizoneGroup)) {
    value.Append("multizone_group");
  }
  return value;
}

std::string GetReceiverLabel(const MediaSinkInternal& sink,
                             const std::string& hash_token) {
  std::string label = base::SHA1HashString(sink.sink().id() + hash_token);
  base::Base64UrlEncode(label, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &label);
  return label;
}

base::Value::Dict CreateReceiver(const MediaSinkInternal& sink,
                                 const std::string& hash_token) {
  base::Value::Dict receiver;

  if (!hash_token.empty()) {
    receiver.Set("label", GetReceiverLabel(sink, hash_token));
  }

  receiver.Set("friendlyName", base::EscapeForHTML(sink.sink().name()));
  receiver.Set("capabilities",
               CapabilitiesToListValue(sink.cast_data().capabilities));
  receiver.Set("volume", base::Value());
  receiver.Set("isActiveInput", base::Value());
  receiver.Set("displayStatus", base::Value());

  receiver.Set("receiverType", "cast");
  return receiver;
}

blink::mojom::PresentationConnectionMessagePtr CreateMessageCommon(
    CastInternalMessage::Type type,
    base::Value::Dict payload,
    const std::string& client_id,
    std::optional<int> sequence_number = std::nullopt) {
  base::Value::Dict message;

  message.Set("type", base::Value(CastInternalMessageTypeToString(type)));

  // When `payload` is empty, we want to set `message` to null instead of {} in
  // the JSON that is generated.
  if (payload.empty())
    message.Set("message", base::Value());
  else
    message.Set("message", std::move(payload));

  if (sequence_number) {
    message.Set("sequenceNumber", base::Value(*sequence_number));
  }
  message.Set("timeoutMillis", base::Value(0));
  message.Set("clientId", base::Value(client_id));

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return blink::mojom::PresentationConnectionMessage::NewMessage(str);
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token,
    const char* action_type) {
  base::Value::Dict message;
  message.Set("receiver", CreateReceiver(sink, hash_token));
  message.Set("action", action_type);

  return CreateMessageCommon(CastInternalMessage::Type::kReceiverAction,
                             std::move(message), client_id);
}

base::Value::Dict CreateAppMessageBody(
    const std::string& session_id,
    const openscreen::cast::proto::CastMessage& cast_message) {
  // TODO(crbug.com/41400942): Investigate whether it is possible to move
  // instead of copying the contents of |cast_message|. Right now copying is
  // done because the message is passed as a const ref at the
  // CastSocket::Observer level.
  base::Value::Dict message;
  message.Set("sessionId", base::Value(session_id));
  message.Set("namespaceName", base::Value(cast_message.namespace_()));
  switch (cast_message.payload_type()) {
    case openscreen::cast::proto::CastMessage_PayloadType_STRING:
      message.Set("message", base::Value(cast_message.payload_utf8()));
      break;
    case openscreen::cast::proto::CastMessage_PayloadType_BINARY: {
      const auto& payload = cast_message.payload_binary();
      message.Set("message",
                  base::Value(base::Value::BlobStorage(
                      payload.front(), payload.front() + payload.size())));
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
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
  base::Value::Dict session_with_receiver_label = session.value().Clone();
  DCHECK(!session_with_receiver_label.FindByDottedPath("receiver.label"));
  session_with_receiver_label.SetByDottedPath(
      "receiver.label", base::Value(GetReceiverLabel(sink, hash_token)));
  return CreateMessageCommon(type, std::move(session_with_receiver_label),
                             client_id);
}

}  // namespace

// static
std::unique_ptr<CastInternalMessage> CastInternalMessage::From(
    base::Value::Dict message) {
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

  base::Value* message_body_value = message.Find("message");
  if (!message_body_value ||
      (!message_body_value->is_dict() && !message_body_value->is_string())) {
    DVLOG(2) << "Missing message body, message: " << message;
    return nullptr;
  }

  std::optional<int> sequence_number = message.FindInt("sequenceNumber");

  std::string session_id;
  std::string namespace_or_v2_type;
  base::Value new_message_body;

  if (message_type == Type::kAppMessage || message_type == Type::kV2Message) {
    if (!message_body_value->is_dict()) {
      DVLOG(2) << "Message body is not a dict: " << message;
      return nullptr;
    }

    if (!GetString(message_body_value->GetDict(), "sessionId", &session_id)) {
      DVLOG(2) << "Missing sessionId, message: " << message;
      return nullptr;
    }

    if (message_type == Type::kAppMessage) {
      if (!GetString(message_body_value->GetDict(), "namespaceName",
                     &namespace_or_v2_type)) {
        DVLOG(2) << "Missing namespace, message: " << message;
        return nullptr;
      }

      base::Value* app_message_value =
          message_body_value->GetDict().Find("message");
      if (!app_message_value ||
          (!app_message_value->is_dict() && !app_message_value->is_string())) {
        DVLOG(2) << "Missing app message, message: " << message;
        return nullptr;
      }
      new_message_body = std::move(*app_message_value);
    } else if (message_type == CastInternalMessage::Type::kV2Message) {
      if (!GetString(message_body_value->GetDict(), "type",
                     &namespace_or_v2_type)) {
        DVLOG(2) << "Missing v2 type, message: " << message;
        return nullptr;
      }
      new_message_body = std::move(*message_body_value);
    }
  }

  return base::WrapUnique(new CastInternalMessage(
      message_type, client_id, sequence_number, session_id,
      namespace_or_v2_type, std::move(new_message_body)));
}

CastInternalMessage::~CastInternalMessage() = default;

CastInternalMessage::CastInternalMessage(
    Type type,
    const std::string& client_id,
    std::optional<int> sequence_number,
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
    const base::Value::Dict& receiver_status) {
  // There should be only 1 app on |receiver_status|.
  const base::Value::List* app_list_value =
      receiver_status.FindList("applications");
  if (!app_list_value || app_list_value->size() != 1) {
    DVLOG(2) << "receiver_status does not contain exactly one app: "
             << receiver_status;
    return nullptr;
  }

  auto session = std::make_unique<CastSession>();

  // Fill in mandatory Session fields.
  const base::Value::Dict* app_dict = (*app_list_value)[0].GetIfDict();
  if (!app_dict || !GetString(*app_dict, "sessionId", &session->session_id_) ||
      !GetString(*app_dict, "appId", &session->app_id_) ||
      !GetString(*app_dict, "transportId", &session->destination_id_) ||
      !GetString(*app_dict, "displayName", &session->display_name_)) {
    DVLOG(2) << "app_value missing mandatory fields: " << (*app_list_value)[0];
    return nullptr;
  }

  if (session->app_id_ == kBackdropAppId) {
    DVLOG(2) << sink.sink().id() << " is running the backdrop app";
    return nullptr;
  }

  // Optional Session fields.
  GetString(*app_dict, "statusText", &session->status_);

  // The receiver label will be populated by each profile using
  // |session->value|.
  base::Value::Dict receiver_value = CreateReceiver(sink, std::string());
  CopyValue(receiver_status, "volume", receiver_value);
  CopyValue(receiver_status, "isActiveInput", receiver_value);

  // Create value for |session->value|.
  base::Value::Dict session_dict;
  session_dict.Set("sessionId", session->session_id());
  session_dict.Set("appId", session->app_id());
  session_dict.Set("transportId", session->destination_id());
  session_dict.Set("receiver", std::move(receiver_value));

  CopyValueWithDefault(*app_dict, "displayName", base::Value(""), session_dict);
  CopyValueWithDefault(*app_dict, "senderApps",
                       base::Value(base::Value::List()), session_dict);
  CopyValueWithDefault(*app_dict, "statusText", base::Value(), session_dict);
  CopyValueWithDefault(*app_dict, "appImages", base::Value(base::Value::List()),
                       session_dict);
  // Optional fields
  CopyValue(*app_dict, "appType", session_dict);
  CopyValue(*app_dict, "universalAppId", session_dict);

  session->value_ = std::move(session_dict);

  const base::Value::List* namespaces_value = app_dict->FindList("namespaces");
  if (!namespaces_value || namespaces_value->empty()) {
    // A session without namespaces is invalid, except for a multizone leader.
    if (session->app_id() != kMultizoneLeaderAppId) {
      DVLOG(2) << "Message is missing namespaces.";
      return nullptr;
    }
  } else {
    for (const auto& namespace_value : *namespaces_value) {
      std::string message_namespace;
      if (!namespace_value.is_dict() ||
          !GetString(namespace_value.GetDict(), "name", &message_namespace)) {
        DVLOG(2) << "Missing namespace name.";
        return nullptr;
      }

      session->message_namespaces_.insert(std::move(message_namespace));
    }
  }
  session->value_.Set("namespaces", namespaces_value ? namespaces_value->Clone()
                                                     : base::Value::List());
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

  auto* status_text_value = from->value_.Find("statusText");
  DCHECK(status_text_value);
  value_.Set("statusText", std::move(*status_text_value));
  auto* namespaces_value = from->value_.Find("namespaces");
  DCHECK(namespaces_value);
  value_.Set("namespaces", std::move(*namespaces_value));
  auto* receiver_volume_value =
      from->value_.FindByDottedPath("receiver.volume");
  DCHECK(receiver_volume_value);
  value_.SetByDottedPath("receiver.volume", std::move(*receiver_volume_value));
}

void CastSession::UpdateMedia(const base::Value::List& media) {
  value_.Set("media", base::Value(media.Clone()));
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
                             base::Value::Dict(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateAppMessage(
    const std::string& session_id,
    const std::string& client_id,
    const openscreen::cast::proto::CastMessage& cast_message) {
  return CreateMessageCommon(CastInternalMessage::Type::kAppMessage,
                             CreateAppMessageBody(session_id, cast_message),
                             client_id);
}

blink::mojom::PresentationConnectionMessagePtr CreateV2Message(
    const std::string& client_id,
    const base::Value::Dict& payload,
    std::optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kV2Message,
                             payload.Clone(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateLeaveSessionAckMessage(
    const std::string& client_id,
    std::optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kLeaveSession,
                             base::Value::Dict(), client_id, sequence_number);
}

blink::mojom::PresentationConnectionMessagePtr CreateErrorMessage(
    const std::string& client_id,
    base::Value::Dict error,
    std::optional<int> sequence_number) {
  return CreateMessageCommon(CastInternalMessage::Type::kError,
                             std::move(error), client_id, sequence_number);
}

base::Value::List SupportedMediaCommandsToListValue(int media_commands) {
  base::Value::List value;
  if (media_commands & static_cast<int>(MediaCommand::kPause))
    value.Append(kMediaCommandPause);
  if (media_commands & static_cast<int>(MediaCommand::kSeek))
    value.Append(kMediaCommandSeek);
  if (media_commands & static_cast<int>(MediaCommand::kStreamVolume))
    value.Append(kMediaCommandStreamVolume);
  if (media_commands & static_cast<int>(MediaCommand::kStreamMute))
    value.Append(kMediaCommandStreamMute);
  if (media_commands & static_cast<int>(MediaCommand::kQueueNext))
    value.Append(kMediaCommandQueueNext);
  if (media_commands & static_cast<int>(MediaCommand::kQueuePrev))
    value.Append(kMediaCommandQueuePrev);
  return value;
}

}  // namespace media_router
