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
#include "chrome/browser/browser_actuator/transport/transport_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

namespace {

bool IsStartSessionCommand(
    browser_actuator::transport::PayloadType payload_type,
    std::string_view serialized_payload) {
  if (payload_type != browser_actuator::transport::PayloadType::kControl) {
    return false;
  }
  browser_actuator::ControlCommand command;
  return command.ParseFromString(serialized_payload) &&
         command.has_start_session();
}

}  // namespace

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(Profile* profile)
    : profile_(profile) {}

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
      if (session_id.empty() || !profile_) {
        // TODO(crbug.com/538161953): Handle invalid/empty session_id or null
        // profile (e.g. via UMA metrics).
        break;
      }
      browser_actuator::transport::TransportService* transport_service =
          browser_actuator::transport::TransportServiceFactory::GetForProfile(
              profile_);
      if (!transport_service) {
        // TODO(crbug.com/538161953): Record UMA metrics when service is
        // unavailable.
        break;
      }
      for (const auto& typed_payload : bundled_message.typed_payloads()) {
        std::optional<browser_actuator::transport::PayloadType> payload_type =
            browser_actuator::transport::FromDownstreamProtoPayloadType(
                typed_payload.payload_type());
        if (!payload_type.has_value()) {
          continue;
        }
        if (*payload_type == browser_actuator::transport::PayloadType::
                                 kExperimentalTriggering &&
            !base::FeatureList::IsEnabled(
                browser_actuator::
                    kEnableBrowserActuatorForGlicExperimentalTriggering)) {
          continue;
        }
        const std::string& payload_bytes =
            typed_payload.proto_payload().value();
        // Only a StartSession control command creates a new session; all other
        // payloads resolve an existing active session on every iteration (since
        // a previous payload may have synchronously destroyed it). If a
        // duplicate StartSession wake-up arrives for an already-active session,
        // CreateSession() returns nullptr and we break to avoid re-dispatching
        // bundled initial payloads a second time.
        browser_actuator::transport::TransportSession* session =
            IsStartSessionCommand(*payload_type, payload_bytes)
                ? transport_service->CreateSession(session_id)
                : transport_service->GetSession(session_id);
        if (!session) {
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
