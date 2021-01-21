// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_INTERNAL_MESSAGE_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_INTERNAL_MESSAGE_UTIL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_app_info.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace media_router {

class MediaSinkInternal;

// Types of internal messages that are used in a custom DIAL launch workflow.
enum class DialInternalMessageType {
  // Cast SDK -> MR
  kClientConnect,
  kV2Message,

  // MR -> Cast SDK
  kNewSession,
  kReceiverAction,
  kError,

  // MR <-> Cast SDK
  kCustomDialLaunch,
  kDialAppInfo,

  kOther
};

// Possible types of ReceiverAction taken by the user on a receiver.
enum class DialReceiverAction {
  // The user selected a receiver with the intent of casting to it with the
  // sender application.
  kCast,

  // The user requested to stop the session running on a receiver.
  kStop
};

// Parsed custom DIAL launch internal message coming from a Cast SDK client.
struct DialInternalMessage {
  // Returns a DialInternalMessage for |message|. If |message| is not a valid
  // custom DIAL launch internal message, returns nullptr and sets |error| with
  // an error reason.
  static std::unique_ptr<DialInternalMessage> From(base::Value message,
                                                   std::string* error);

  DialInternalMessage(DialInternalMessageType type,
                      base::Optional<base::Value> body,
                      const std::string& client_id,
                      int sequence_number);
  ~DialInternalMessage();

  DialInternalMessageType type;
  base::Optional<base::Value> body;
  std::string client_id;
  int sequence_number;

  DISALLOW_COPY_AND_ASSIGN(DialInternalMessage);
};

// Parsed CUSTOM_DIAL_LAUNCH response from the Cast SDK client.
struct CustomDialLaunchMessageBody {
  // Returns a CustomDialLaunchMessageBody for |message|.
  // This method is only valid to call if |message.type| == |kCustomDialLaunch|.
  static CustomDialLaunchMessageBody From(const DialInternalMessage& message);

  CustomDialLaunchMessageBody();
  CustomDialLaunchMessageBody(
      bool do_launch,
      const base::Optional<std::string>& launch_parameter);
  CustomDialLaunchMessageBody(const CustomDialLaunchMessageBody& other);
  ~CustomDialLaunchMessageBody();

  // If |true|, the DialMediaRouteProvider should handle the app launch.
  bool do_launch = true;

  // If |do_launch| is |true|, optional launch parameter to include with the
  // launch (POST) request. This overrides the launch parameter that was
  // specified in the MediaSource (if any).
  base::Optional<std::string> launch_parameter;
};

class DialInternalMessageUtil final {
 public:
  // |hash_token|: A per-profile value used to hash sink IDs.
  explicit DialInternalMessageUtil(const std::string& hash_token);
  ~DialInternalMessageUtil();

  // Returns |true| if |message| is a valid STOP_SESSION message.
  static bool IsStopSessionMessage(const DialInternalMessage& message);

  // Returns a NEW_SESSION message to be sent to the page when the user requests
  // an app launch.
  mojom::RouteMessagePtr CreateNewSessionMessage(
      const std::string& app_name,
      const std::string& client_id,
      const MediaSinkInternal& sink) const;

  // Returns a RECEIVER_ACTION / CAST message to be sent to the page when the
  // user requests an app launch.
  mojom::RouteMessagePtr CreateReceiverActionCastMessage(
      const std::string& client_id,
      const MediaSinkInternal& sink) const;

  // Returns a RECEIVER_ACTION / STOP message to be sent to the page when an app
  // is stopped by DialMediaRouteProvider.
  mojom::RouteMessagePtr CreateReceiverActionStopMessage(
      const std::string& client_id,
      const MediaSinkInternal& sink) const;

  // Returns a CUSTOM_DIAL_LAUNCH request message to be sent to the page.
  // Generates and returns the next number to associate a DIAL launch sequence
  // with.
  std::pair<mojom::RouteMessagePtr, int> CreateCustomDialLaunchMessage(
      const std::string& client_id,
      const MediaSinkInternal& sink,
      const ParsedDialAppInfo& app_info) const;

  // Creates an app info message used in a DIAL_APP_INFO response or a
  // CUSTOM_DIAL_LAUNCH (called via CreateCustomDialLaunchMessage() above)
  // message.
  mojom::RouteMessagePtr CreateDialAppInfoMessage(
      const std::string& client_id,
      const MediaSinkInternal& sink,
      const ParsedDialAppInfo& app_info,
      int sequence_number,
      DialInternalMessageType type) const;

  mojom::RouteMessagePtr CreateDialAppInfoErrorMessage(
      DialAppInfoResultCode result_code,
      const std::string& client_id,
      int sequence_number,
      const std::string& error_message,
      base::Optional<int> http_error_code = base::nullopt) const;

 private:
  base::Value CreateReceiver(const MediaSinkInternal& sink) const;
  base::Value CreateReceiverActionBody(const MediaSinkInternal& sink,
                                       DialReceiverAction action) const;
  base::Value CreateNewSessionBody(const std::string& app_name,
                                   const MediaSinkInternal& sink) const;
  base::Value CreateDialAppInfoBody(const MediaSinkInternal& sink,
                                    const ParsedDialAppInfo& app_info) const;

  // |sequence_number| is used by the Cast SDK to match up requests from the SDK
  // to Chrome with responses from Chrome. If a message from Chrome has no
  // corresponding request, then its |sequence_number| is an invalid value of
  // -1.
  base::Value CreateDialMessageCommon(DialInternalMessageType type,
                                      base::Value body,
                                      const std::string& client_id,
                                      int sequence_number = -1) const;

  std::string hash_token_;
  DISALLOW_COPY_AND_ASSIGN(DialInternalMessageUtil);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_INTERNAL_MESSAGE_UTIL_H_
