// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"

#include <array>

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "components/media_router/browser/route_message_util.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
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
    case DialInternalMessageType::kDialAppInfo:
      return "dial_app_info";
    case DialInternalMessageType::kError:
      return "error";
    case DialInternalMessageType::kOther:
      break;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown message type: " << static_cast<int>(type);
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

  if (str_type == "dial_app_info")
    return DialInternalMessageType::kDialAppInfo;

  if (str_type == "error")
    return DialInternalMessageType::kError;

  return DialInternalMessageType::kOther;
}

std::string DialReceiverActionToString(DialReceiverAction action) {
  switch (action) {
    case DialReceiverAction::kCast:
      return "cast";
    case DialReceiverAction::kStop:
      return "stop";
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown DialReceiverAction: " << static_cast<int>(action);
  return "";
}

std::string DialAppInfoErrorToString(DialAppInfoResultCode error) {
  switch (error) {
    case DialAppInfoResultCode::kNetworkError:
      return "network_error";
    case DialAppInfoResultCode::kParsingError:
      return "parsing_error";
    case DialAppInfoResultCode::kHttpError:
      return "http_error";
    case DialAppInfoResultCode::kOk:
    case DialAppInfoResultCode::kCount:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected DialAppInfoResultCode: " << static_cast<int>(error);
      return "";
  }
}

}  // namespace

// static
std::unique_ptr<DialInternalMessage> DialInternalMessage::From(
    base::Value::Dict message,
    std::string* error) {
  DCHECK(error);

  std::string* type_value = message.FindString("type");
  if (!type_value) {
    *error = "Missing type value";
    return nullptr;
  }

  DialInternalMessageType message_type =
      StringToDialInternalMessageType(*type_value);
  if (message_type == DialInternalMessageType::kOther) {
    *error = "Unsupported message type";
    return nullptr;
  }

  std::string* client_id = message.FindString("clientId");
  if (!client_id) {
    *error = "Missing clientId";
    return nullptr;
  }

  std::optional<base::Value> message_body;
  base::Value* message_body_value = message.Find("message");
  if (message_body_value)
    message_body = std::move(*message_body_value);

  int sequence_number = message.FindInt("sequenceNumber").value_or(-1);

  return std::make_unique<DialInternalMessage>(
      message_type, std::move(message_body), *client_id, sequence_number);
}

DialInternalMessage::DialInternalMessage(DialInternalMessageType type,
                                         std::optional<base::Value> body,
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

  const std::optional<base::Value>& body = message.body;
  if (!body || !body->is_dict())
    return CustomDialLaunchMessageBody();

  const std::optional<bool> do_launch = body->GetDict().FindBool("doLaunch");
  if (!do_launch) {
    return CustomDialLaunchMessageBody();
  }

  std::optional<std::string> launch_parameter;
  const std::string* launch_parameter_value =
      body->GetDict().FindString("launchParameter");
  if (launch_parameter_value)
    launch_parameter = *launch_parameter_value;

  return CustomDialLaunchMessageBody(*do_launch, launch_parameter);
}

CustomDialLaunchMessageBody::CustomDialLaunchMessageBody() = default;
CustomDialLaunchMessageBody::CustomDialLaunchMessageBody(
    bool do_launch,
    const std::optional<std::string>& launch_parameter)
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

  const std::optional<base::Value>& body = message.body;
  if (!body || !body->is_dict())
    return false;

  const std::string* request_type = body->GetDict().FindString("type");
  return request_type && *request_type == "STOP";
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateNewSessionMessage(
    const std::string& app_name,
    const std::string& client_id,
    const MediaSinkInternal& sink) const {
  base::Value message =
      CreateDialMessageCommon(DialInternalMessageType::kNewSession,
                              CreateNewSessionBody(app_name, sink), client_id);
  return message_util::RouteMessageFromValue(std::move(message));
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateReceiverActionCastMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink) const {
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kReceiverAction,
      CreateReceiverActionBody(sink, DialReceiverAction::kCast), client_id);
  return message_util::RouteMessageFromValue(std::move(message));
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateReceiverActionStopMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink) const {
  base::Value message = CreateDialMessageCommon(
      DialInternalMessageType::kReceiverAction,
      CreateReceiverActionBody(sink, DialReceiverAction::kStop), client_id);
  return message_util::RouteMessageFromValue(std::move(message));
}

std::pair<mojom::RouteMessagePtr, int>
DialInternalMessageUtil::CreateCustomDialLaunchMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const ParsedDialAppInfo& app_info) const {
  int seq_number = GetNextDialLaunchSequenceNumber();
  // A CUSTOM_DIAL_LAUNCH message is the same as A DIAL_APP_INFO response except
  // for the message type.
  return {CreateDialAppInfoMessage(client_id, sink, app_info, seq_number,
                                   DialInternalMessageType::kCustomDialLaunch),
          seq_number};
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateDialAppInfoMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const ParsedDialAppInfo& app_info,
    int sequence_number,
    DialInternalMessageType type) const {
  base::Value message = CreateDialMessageCommon(
      type, CreateDialAppInfoBody(sink, app_info), client_id, sequence_number);
  return message_util::RouteMessageFromValue(std::move(message));
}

mojom::RouteMessagePtr DialInternalMessageUtil::CreateDialAppInfoErrorMessage(
    DialAppInfoResultCode result_code,
    const std::string& client_id,
    int sequence_number,
    const std::string& error_message,
    std::optional<int> http_error_code) const {
  // The structure of an error message body is defined as chrome.cast.Error in
  // the Cast SDK.
  base::Value::Dict body;
  body.Set("code", DialAppInfoErrorToString(result_code));
  body.Set("description", error_message);
  if (result_code == DialAppInfoResultCode::kHttpError) {
    DCHECK(http_error_code);
    base::Value::Dict details;
    details.Set("http_error_code", *http_error_code);
    body.Set("details", std::move(details));
  }
  base::Value message =
      CreateDialMessageCommon(DialInternalMessageType::kError, std::move(body),
                              client_id, sequence_number);
  return message_util::RouteMessageFromValue(std::move(message));
}

base::Value::Dict DialInternalMessageUtil::CreateReceiver(
    const MediaSinkInternal& sink) const {
  base::Value::Dict receiver;

  std::string label = base::SHA1HashString(sink.sink().id() + hash_token_);
  base::Base64UrlEncode(label, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &label);
  receiver.Set("label", base::Value(label));

  receiver.Set("friendlyName",
               base::Value(base::EscapeForHTML(sink.sink().name())));
  receiver.Set("capabilities", base::Value::List());

  receiver.Set("volume", base::Value());
  receiver.Set("isActiveInput", base::Value());
  receiver.Set("displayStatus", base::Value());

  receiver.Set("receiverType", base::Value("dial"));
  receiver.Set("ipAddress",
               base::Value(sink.dial_data().ip_address.ToString()));
  return receiver;
}

base::Value::Dict DialInternalMessageUtil::CreateReceiverActionBody(
    const MediaSinkInternal& sink,
    DialReceiverAction action) const {
  base::Value::Dict message_body;
  message_body.Set("receiver", CreateReceiver(sink));
  message_body.Set("action", base::Value(DialReceiverActionToString(action)));
  return message_body;
}

base::Value::Dict DialInternalMessageUtil::CreateNewSessionBody(
    const std::string& app_name,
    const MediaSinkInternal& sink) const {
  base::Value::Dict message_body;
  message_body.Set("sessionId", base::Value(GetNextSessionId()));
  message_body.Set("appId", base::Value(""));
  message_body.Set("displayName", base::Value(app_name));
  message_body.Set("statusText", base::Value(""));
  message_body.Set("appImages", base::Value::List());
  message_body.Set("receiver", CreateReceiver(sink));
  message_body.Set("senderApps", base::Value::List());
  message_body.Set("namespaces", base::Value::List());
  message_body.Set("media", base::Value::List());
  message_body.Set("status", base::Value("connected"));
  message_body.Set("transportId", base::Value(""));
  return message_body;
}

base::Value::Dict DialInternalMessageUtil::CreateDialAppInfoBody(
    const MediaSinkInternal& sink,
    const ParsedDialAppInfo& app_info) const {
  base::Value::Dict message_body;
  message_body.Set("receiver", CreateReceiver(sink));
  message_body.Set("appState",
                   base::Value(DialAppStateToString(app_info.state)));

  base::Value::Dict extra_data;
  for (const auto& key_value : app_info.extra_data) {
    extra_data.Set(key_value.first, base::Value(key_value.second));
  }
  message_body.Set("extraData", std::move(extra_data));
  return message_body;
}

base::Value DialInternalMessageUtil::CreateDialMessageCommon(
    DialInternalMessageType type,
    base::Value::Dict body,
    const std::string& client_id,
    int sequence_number) const {
  base::Value::Dict message;
  message.Set("type", base::Value(DialInternalMessageTypeToString(type)));
  message.Set("message", std::move(body));
  message.Set("clientId", base::Value(client_id));
  message.Set("sequenceNumber", base::Value(sequence_number));
  message.Set("timeoutMillis", base::Value(0));
  return base::Value(std::move(message));
}

}  // namespace media_router
