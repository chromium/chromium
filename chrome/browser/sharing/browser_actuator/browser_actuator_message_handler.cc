// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/browser_actuator/public/features.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"

namespace {

bool IsStartSessionCommand(stream_transport::PayloadType payload_type,
                           std::string_view serialized_payload) {
  if (payload_type != stream_transport::PayloadType::kControl) {
    return false;
  }
  browser_actuator::ControlCommand command;
  return command.ParseFromString(serialized_payload) &&
         command.has_start_session();
}

bool IsDeviceOptInRequest(stream_transport::PayloadType payload_type,
                          std::string_view serialized_payload) {
  if (payload_type != stream_transport::PayloadType::kExperimentalTriggering) {
    return false;
  }
  components_sharing_message::GlicExperimentalTriggering triggering;
  return triggering.ParseFromString(serialized_payload) &&
         triggering.request().has_device_opt_in_request();
}

}  // namespace

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(
    stream_transport::TransportService* transport_service)
    : transport_service_(raw_ref<stream_transport::TransportService>::from_ptr(
          transport_service)) {}

BrowserActuatorMessageHandler::~BrowserActuatorMessageHandler() = default;

void BrowserActuatorMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator));

  switch (message.payload_case()) {
    case components_sharing_message::SharingMessage::
        kActuatorDownstreamMessage: {
      const browser_actuator::ActuatorDownstreamMessage& bundled_message =
          message.actuator_downstream_message();
      const std::string& session_id = bundled_message.session_id();
      if (session_id.empty()) {
        // TODO(crbug.com/538161953): Handle invalid/empty session_id (e.g. via
        // UMA metrics).
        break;
      }
      for (const auto& typed_payload : bundled_message.typed_payloads()) {
        std::optional<stream_transport::PayloadType> payload_type =
            stream_transport::FromDownstreamProtoPayloadType(
                typed_payload.payload_type());
        if (!payload_type.has_value()) {
          continue;
        }
        if (*payload_type ==
                stream_transport::PayloadType::kExperimentalTriggering &&
            !base::FeatureList::IsEnabled(
                browser_actuator::
                    kEnableBrowserActuatorForGlicExperimentalTriggering)) {
          continue;
        }
        const std::string& payload_bytes =
            typed_payload.proto_payload().value();
        const bool is_start_session =
            IsStartSessionCommand(*payload_type, payload_bytes);
        // FCM only carries `StartSession` and optional `DeviceOptInRequest`
        // payloads. Actuation prompts (`TriggerActuationRequest`) and other
        // payloads are delivered over the One Platform `WatchSessions`
        // connection.
        if (!is_start_session &&
            !IsDeviceOptInRequest(*payload_type, payload_bytes)) {
          continue;
        }
        stream_transport::TransportSession* session =
            is_start_session
                ? transport_service_->CreateSession(
                      session_id, stream_transport::InitIngressSource::kFcmPush)
                : transport_service_->GetSession(session_id);
        if (!session) {
          // If a session with `session_id` is already active (e.g. a duplicate
          // `StartSession` FCM arrives when the server retries or switches
          // instances), `CreateSession()` returns `nullptr` and the request is
          // dropped so bundled payloads are not re-dispatched.
          // TODO(b/584389152): Consider replying to the server (e.g. with
          // `StartSessionAck`) when `session_id` is already active to confirm
          // that Chrome is alive.
          break;
        }
        session->OnMessage(*payload_type, payload_bytes);
      }
      break;
    }
    default:
      // Other payloads are not handled by this handler.
      break;
  }

  std::move(done_callback).Run(nullptr);
}
