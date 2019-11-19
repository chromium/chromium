// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"

#include <array>

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"
#include "chrome/browser/media/router/route_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "net/base/escape.h"
#include "url/url_util.h"

namespace media_router {

namespace {

constexpr int kSequenceNumberWrap = 2 << 30;

int GetNextDialLaunchSequenceNumber() {
  static int next_seq_number = 0;
  next_seq_number = (next_seq_number + 1) % kSequenceNumberWrap;
  return next_seq_number;
}

std::string GetNextSessionId() {
  static int session_id = 0;
  session_id = (session_id + 1) % kSequenceNumberWrap;
  return base::NumberToString(session_id);
}

std::string DialInternalMessageTypeToString(DialInternalMessageType type) {
  switch (type) {
    case DialInternalMessageType::kClientConnect:
      return "client_connect";
    case DialInternalMessageType::kV2Message:
      return "v2_message";
    case DialInternalMessageType::kReceiverAction:
      return "receiver_action";
    case DialInternalMessageType::kNewSession:
      return "new_session";
    case DialInternalMessageType::kCustomDialLaunch:
      return "custom_dial_launch";
    case DialInternalMessageType::kOther:
      break;
  }
  NOTREACHED() << "Unknown message type: " << static_cast<int>(type);
  return "unknown";
}

DialInternalMessageType StringToDialInternalMessageType(
    const std::string& str_type) {
  if (str_type == "client_connect")
    return DialInternalMessageType::kClientConnect;

  if (str_type == "v2_message")
    return DialInternalMessageType::kV2Message;

  if (str_type == "receiver_action")
    return DialInternalMessageType::kReceiverAction;

  if (str_type == "new_session")
    return DialInternalMessageType::kNewSession;

  if (str_type == "custom_dial_launch")
    return DialInternalMessageType::kCustomDialLaunch;

  return DialInternalMessageType::kOther;
}

std::string DialReceiverActionToString(DialReceiverAction action) {
  switch (action) {
    case DialReceiverAction::kCast:
      return "cast";
    case DialReceiverAction::kStop:
      return "stop";
  }
  NOTREACHED() << "Unknown DialReceiverAction: " << static_cast<int>(action);
  return "";
}

}  // namespace

// static
std::unique_ptr<DialInternalMessage> DialInternalMessage::From(
    base::Value message,
    std::string* error) {
  DCHECK(error);
  if (!message.is_dict()) {
    *error = "Input message was not a dictionary";
    return nullptr;
  }

  base::Value* type_value =
      message.FindKeyOfType("type", base::Value::Type::STRING);
  if (!type_value) {
    *error = "Missing type value";
    return nullptr;
  }

  DialInternalMessageType message_type =
      StringToDialInternalMessageType(type_value->GetString());
  if (message_type == DialInternalMessageType::kOther) {
    *error = "Unsupported message type";
    return nullptr;
  }

  base::Value* client_id_value =
      message.FindKeyOfType("clientId", base::Value::Type::STRING);
  if (!client_id_value) {
    *error = "Missing clientId";
    return nullptr;
  }

  base::Optional<base::Value> message_body;
  base::Value* message_body_value = message.FindKey("message");
  if (message_body_value)
    message_body = std::move(*message_body_value);

  int sequence_number = -1;
  base::Value* sequence_number_value =
      message.FindKeyOfType("sequenceNumber", base::Value::Type::INTEGER);
  if (sequence_number_value)
    sequence_number = sequence_number_value->GetInt();

  return std::make_unique<DialInternalMessage>(
      message_type, std::move(message_body), client_id_value->GetString(),
      sequence_number);
}

DialInternalMessage::DialInternalMessage(DialInternalMessageType type,
                                         base::Optional<base::Value> body,
                                         const std::string& client_id,
                                         int sequence_number)
    : type(type),
      body(std::move(body)),
      client_id(client_id),
      sequence_number(sequence_number) {}
DialInternalMessage::~DialInternalMessage() = default;

// static
CustomDialLaunchMessageBody CustomDialLaunchMessageBody::From(
    const DialInternalMessage& message) {
  DCHECK(message.type == DialInternalMessageType::kCustomDialLaunch);

  const base::Optional<base::Value>& body = message.body;
  if (!body || !body->is_dict())
    return CustomDialLaunchMessageBody();

  const base::Value* do_launch_value =
      body->FindKeyOfType("doLaunch", base::Value::Type::BOOLEAN);
  if (!do_launch_value)
    return CustomDialLaunchMessageBody();

  bool do_launch = do_launch_value->GetBool();

  base::Optional<std::string> launch_parameter;
  const base::Value* launch_parameter_value =
      body->FindKeyOfType("launchParameter", base::Value::Type::STRING);
  if (launch_parameter_value)
    launch_parameter = launch_parameter_value->GetString();

  return CustomDialLaunchMessageBody(do_launch, launch_parameter);
}

CustomDialLaunchMessageBody::CustomDialLaunchMessageBody() = default;
CustomDialLaunchMessageBody::CustomDialLaunchMessageBody(
    bool do_launch,
    const base::Optional<std::string>& launch_parameter)
    : do_launch(do_launch), launch_parameter(launch_parameter) {}
CustomDialLaunchMessageBody::CustomDialLaunchMessageBody(
    const CustomDialLaunchMessageBody& other) = default;
CustomDialLaunchMessageBody::~CustomDialLaunchMessageBody() = default;

DialInternalMessageUtil::DialInternalMessageUtil(const std::string& hash_token)
    : hash_token_(hash_token) {}
DialInternalMessageUtil::~DialInternalMessageUtil() = default;

// static
bool DialInternalMessageUtil::IsStopSessionMessage(
    const DialInternalMessage& message) {
  if (message.type != DialInternalMessageType::kV2Message)
    return false;

  const base::Optional<base::Value>& body = message.body;
  if (!body || !body->is_dict())
    return false;

  const base::Value* request_type =
      body->FindKeyOfType("type", base::Value::Type::STRING);
  return request_type && request_type->GetString() == "STOP";
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateNewSessionMessage(
    const DialLaunchInfo& launch_info,
    const MediaSinkInternal& sink) const {
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kNewSession,
      CreateNewSessionBody(launch_info, sink), launch_info.client_id);

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return message_util::RouteMessageFromString(std::move(str));
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateReceiverActionCastMessage(
    const DialLaunchInfo& launch_info,
    const MediaSinkInternal& sink) const {
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kReceiverAction,
      CreateReceiverActionBody(sink, DialReceiverAction::kCast),
      launch_info.client_id);

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return message_util::RouteMessageFromString(std::move(str));
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateReceiverActionStopMessage(
    const DialLaunchInfo& launch_info,
    const MediaSinkInternal& sink) const {
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kReceiverAction,
      CreateReceiverActionBody(sink, DialReceiverAction::kStop),
      launch_info.client_id);

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return message_util::RouteMessageFromString(std::move(str));
}

std::pair<mojom::RouteMessagePtr, int>
DialInternalMessageUtil::CreateCustomDialLaunchMessage(
    const DialLaunchInfo& launch_info,
    const MediaSinkInternal& sink,
    const ParsedDialAppInfo& app_info) const {
  int seq_number = GetNextDialLaunchSequenceNumber();
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kCustomDialLaunch,
      CreateCustomDialLaunchBody(sink, app_info), launch_info.client_id);
  message.SetKey("sequenceNumber", base::Value(seq_number));

  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return {message_util::RouteMessageFromString(std::move(str)), seq_number};
}

base::Value DialInternalMessageUtil::CreateReceiver(
    const MediaSinkInternal& sink) const {
  base::Value receiver(base::Value::Type::DICTIONARY);

  std::string label = base::SHA1HashString(sink.sink().id() + hash_token_);
  base::Base64UrlEncode(label, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &label);
  receiver.SetKey("label", base::Value(label));

  receiver.SetKey("friendlyName",
                  base::Value(net::EscapeForHTML(sink.sink().name())));
  receiver.SetKey("capabilities", base::ListValue());
  receiver.SetKey("volume", base::Value(base::Value::Type::NONE));
  receiver.SetKey("isActiveInput", base::Value(base::Value::Type::NONE));
  receiver.SetKey("displayStatus", base::Value(base::Value::Type::NONE));

  receiver.SetKey("receiverType", base::Value("dial"));
  receiver.SetKey("ipAddress",
                  base::Value(sink.dial_data().ip_address.ToString()));
  return receiver;
}

base::Value DialInternalMessageUtil::CreateReceiverActionBody(
    const MediaSinkInternal& sink,
    DialReceiverAction action) const {
  base::Value message_body(base::Value::Type::DICTIONARY);
  message_body.SetKey("receiver", CreateReceiver(sink));
  message_body.SetKey("action",
                      base::Value(DialReceiverActionToString(action)));
  return message_body;
}

base::Value DialInternalMessageUtil::CreateNewSessionBody(
    const DialLaunchInfo& launch_info,
    const MediaSinkInternal& sink) const {
  base::Value message_body(base::Value::Type::DICTIONARY);
  message_body.SetKey("sessionId", base::Value(GetNextSessionId()));
  message_body.SetKey("appId", base::Value(""));
  message_body.SetKey("displayName", base::Value(launch_info.app_name));
  message_body.SetKey("statusText", base::Value(""));
  message_body.SetKey("appImages", base::ListValue());
  message_body.SetKey("receiver", CreateReceiver(sink));
  message_body.SetKey("senderApps", base::ListValue());
  message_body.SetKey("namespaces", base::ListValue());
  message_body.SetKey("media", base::ListValue());
  message_body.SetKey("status", base::Value("connected"));
  message_body.SetKey("transportId", base::Value(""));
  return message_body;
}

base::Value DialInternalMessageUtil::CreateCustomDialLaunchBody(
    const MediaSinkInternal& sink,
    const ParsedDialAppInfo& app_info) const {
  base::Value message_body(base::Value::Type::DICTIONARY);
  message_body.SetKey("receiver", CreateReceiver(sink));
  message_body.SetKey("appState",
                      base::Value(DialAppStateToString(app_info.state)));
  if (!app_info.extra_data.empty()) {
    base::Value extra_data(base::Value::Type::DICTIONARY);
    for (const auto& key_value : app_info.extra_data)
      message_body.SetKey(key_value.first, base::Value(key_value.second));

    message_body.SetKey("extraData", std::move(extra_data));
  }
  return message_body;
}

base::Value DialInternalMessageUtil::CreateDialMessageCommon(
    DialInternalMessageType type,
    base::Value body,
    const std::string& client_id) const {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("type", base::Value(DialInternalMessageTypeToString(type)));
  message.SetKey("message", std::move(body));
  message.SetKey("clientId", base::Value(client_id));
  message.SetKey("sequenceNumber", base::Value(-1));
  message.SetKey("timeoutMillis", base::Value(0));
  return message;
}

}  // namespace media_router
