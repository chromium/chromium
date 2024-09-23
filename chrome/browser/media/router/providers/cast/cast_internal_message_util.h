// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace media_router {

using openscreen::cast::proto::CastMessage;

class MediaSinkInternal;

// Values in the "supportedMediaCommands" list in media status messages
// sent to the Cast sender SDK.
constexpr char kMediaCommandPause[] = "pause";
constexpr char kMediaCommandSeek[] = "seek";
constexpr char kMediaCommandStreamVolume[] = "stream_volume";
constexpr char kMediaCommandStreamMute[] = "stream_mute";
constexpr char kMediaCommandQueueNext[] = "queue_next";
constexpr char kMediaCommandQueuePrev[] = "queue_prev";

// Values in the "supportedMediaCommands" bit array in media status messages
// received from Cast receivers. They are converted to string values by
// SupportedMediaCommandsToListValue().
enum class MediaCommand {
  kPause = 1 << 0,
  kSeek = 1 << 1,
  kStreamVolume = 1 << 2,
  kStreamMute = 1 << 3,
  // 1 << 4 and 1 << 5 are not in use.
  kQueueNext = 1 << 6,
  kQueuePrev = 1 << 7,
};

// Represents a message sent or received by the Cast SDK via a
// PresentationConnection.
class CastInternalMessage {
 public:
  // TODO(crbug.com/40561499): Add other types of messages.
  enum class Type {
    kClientConnect,   // Initial message sent by SDK client to connect to MRP.
    kAppMessage,      // App messages to pass through between SDK client and the
                      // receiver.
    kV2Message,       // Cast protocol messages between SDK client and the
                      // receiver.
    kLeaveSession,    // Message sent by SDK client to leave current session.
    kReceiverAction,  // Message sent by MRP to inform SDK client of action.
    kNewSession,      // Message sent by MRP to inform SDK client of new
                      // session.
    kUpdateSession,   // Message sent by MRP to inform SDK client of updated
                      // session.
    kError,
    kOther,  // All other types of messages which are not considered
             // part of communication with Cast SDK.
    kMaxValue = kOther,
  };

  // Errors that may be returned by the SDK.
  enum class ErrorCode {
    kInternalError,           // Internal error.  (Not specified by Cast API.)
    kCancel,                  // The operation was canceled by the user.
    kTimeout,                 // The operation timed out.
    kApiNotInitialized,       // The API is not initialized.
    kInvalidParameter,        // The parameters to the operation were not valid.
    kExtensionNotCompatible,  // The API script is not compatible with
                              // this Cast implementation.
    kReceiverUnavailable,     // No receiver was compatible with the session
                              // request.
    kSessionError,  // A session could not be created, or a session was invalid.
    kChannelError,  // A channel to the receiver is not available.
    kLoadMediaFailed,  // Load media failed.
    kMaxValue = kLoadMediaFailed,
  };

  // Returns a CastInternalMessage for |message|, or nullptr is |message| is not
  // a valid Cast internal message.
  static std::unique_ptr<CastInternalMessage> From(base::Value::Dict message);

  CastInternalMessage(const CastInternalMessage&) = delete;
  CastInternalMessage& operator=(const CastInternalMessage&) = delete;

  ~CastInternalMessage();

  Type type() const { return type_; }
  const std::string& client_id() const { return client_id_; }
  std::optional<int> sequence_number() const { return sequence_number_; }

  bool has_session_id() const {
    return type_ == Type::kAppMessage || type_ == Type::kV2Message;
  }

  const std::string& session_id() const {
    DCHECK(has_session_id());
    return session_id_;
  }

  const std::string& app_message_namespace() const {
    DCHECK(type_ == Type::kAppMessage);
    return namespace_or_v2_type_;
  }

  const std::string& v2_message_type() const {
    DCHECK(type_ == Type::kV2Message);
    return namespace_or_v2_type_;
  }

  const base::Value& app_message_body() const {
    DCHECK(type_ == Type::kAppMessage);
    return message_body_;
  }

  const base::Value::Dict& v2_message_body() const {
    DCHECK(type_ == Type::kV2Message);
    return message_body_.GetDict();
  }

 private:
  CastInternalMessage(Type type,
                      const std::string& client_id,
                      std::optional<int> sequence_number,
                      const std::string& session_id,
                      const std::string& namespace_or_v2_type_,
                      base::Value message_body);

  const Type type_;
  const std::string client_id_;
  const std::optional<int> sequence_number_;

  // Set if |type| is |kAppMessage| or |kV2Message|.
  const std::string session_id_;
  const std::string namespace_or_v2_type_;
  const base::Value message_body_;
};

// Represents a Cast session on a Cast device. Cast sessions are derived from
// RECEIVER_STATUS messages sent by Cast devices.
//
// TODO(crbug.com/1291743): Rename either this class or ::CastSession to avoid
// confusion.
class CastSession {
 public:
  // Returns a CastSession from |receiver_status| message sent by |sink|, or
  // nullptr if |receiver_status| is not a valid RECEIVER_STATUS message.
  static std::unique_ptr<CastSession> From(
      const MediaSinkInternal& sink,
      const base::Value::Dict& receiver_status);

  CastSession();
  ~CastSession();

  // Returns a string that can be used as the description of the MediaRoute
  // associated with this session.
  std::string GetRouteDescription() const;

  // Partially updates the contents of this object using data in |from|.
  void UpdateSession(std::unique_ptr<CastSession> from);

  // Sets the 'media' field of |value_| with a value received from the client.
  void UpdateMedia(const base::Value::List& media);

  // ID of the session.
  const std::string& session_id() const { return session_id_; }

  // ID of the app in the session.
  const std::string& app_id() const { return app_id_; }

  // The ID of the Cast device this session is communicating with.
  const std::string& destination_id() const { return destination_id_; }

  // The set of accepted message namespaces. Must be non-empty, unless the
  // session represents a multizone leader.
  const base::flat_set<std::string>& message_namespaces() const {
    return message_namespaces_;
  }

  // The dictionary representing this session, derived from |receiver_status|.
  // For convenience, this is used for generating messages sent to the SDK that
  // include the session value.
  const base::Value::Dict& value() const { return value_; }

 private:
  std::string session_id_;
  std::string app_id_;
  std::string destination_id_;
  base::flat_set<std::string> message_namespaces_;
  base::Value::Dict value_;

  // The human-readable name of the Cast application, for example, "YouTube".
  // Mandatory.
  std::string display_name_;

  // Descriptive text for the current application content, for example “My
  // Wedding Slideshow”. May be empty.
  std::string status_;
};

// Utility methods for generating messages sent to the SDK.
// |hash_token| is a per-profile value that is used to hash the sink ID.
blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionCastMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionStopMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateNewSessionMessage(
    const CastSession& session,
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateUpdateSessionMessage(
    const CastSession& session,
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateAppMessageAck(
    const std::string& client_id,
    int sequence_number);
blink::mojom::PresentationConnectionMessagePtr CreateAppMessage(
    const std::string& session_id,
    const std::string& client_id,
    const CastMessage& cast_message);
blink::mojom::PresentationConnectionMessagePtr CreateV2Message(
    const std::string& client_id,
    const base::Value::Dict& payload,
    std::optional<int> sequence_number);
blink::mojom::PresentationConnectionMessagePtr CreateErrorMessage(
    const std::string& client_id,
    base::Value::Dict error,
    std::optional<int> sequence_number);
blink::mojom::PresentationConnectionMessagePtr CreateLeaveSessionAckMessage(
    const std::string& client_id,
    std::optional<int> sequence_number);
blink::mojom::PresentationConnectionMessagePtr CreateLeaveSessionAckMessage(
    const std::string& client_id,
    std::optional<int> sequence_number);

base::Value::List SupportedMediaCommandsToListValue(int media_commands);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_
