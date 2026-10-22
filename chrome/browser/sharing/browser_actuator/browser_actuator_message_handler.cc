// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/browser_actuator/transport/transport_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

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

      if (!session_id.empty() && profile_) {
        browser_actuator::transport::TransportService* transport_service =
            browser_actuator::transport::TransportServiceFactory::GetForProfile(
                profile_);
        if (transport_service) {
          // TODO(crbug.com/538161953): Handle invalid/empty session_id or null
          // profile (e.g. via UMA metrics).
          if (transport_service->GetOrCreateSession(session_id)) {
            for (const auto& typed_payload : bundled_message.typed_payloads()) {
              // Re-resolve the session on every iteration: a previous payload
              // (e.g. a CloseSession control command) may have synchronously
              // destroyed it, so a pointer cached across iterations could
              // dangle.
              browser_actuator::transport::TransportSession* session =
                  transport_service->GetSession(session_id);
              if (!session) {
                break;
              }
              std::optional<browser_actuator::transport::PayloadType>
                  payload_type = browser_actuator::transport::
                      FromDownstreamProtoPayloadType(
                          typed_payload.payload_type());
              if (!payload_type.has_value()) {
                continue;
              }
              switch (*payload_type) {
                case browser_actuator::transport::PayloadType::kControl: {
                  browser_actuator::ControlCommand command;
                  if (command.ParseFromString(
                          typed_payload.proto_payload().value())) {
                    session->OnMessage(
                        browser_actuator::transport::PayloadType::kControl,
                        command);
                  } else {
                    DLOG(WARNING)
                        << "Failed to parse ControlCommand payload"
                        << " session_id: " << session_id
                        << " payload_type: " << typed_payload.payload_type();
                  }
                  break;
                }
                case browser_actuator::transport::PayloadType::
                    kExperimentalTriggering: {
                  if (!base::FeatureList::IsEnabled(
                          browser_actuator::
                              kEnableBrowserActuatorForGlicExperimentalTriggering)) {
                    break;
                  }
                  components_sharing_message::GlicExperimentalTriggering
                      triggering;
                  if (triggering.ParseFromString(
                          typed_payload.proto_payload().value())) {
                    session->OnMessage(browser_actuator::transport::
                                           PayloadType::kExperimentalTriggering,
                                       triggering);
                  } else {
                    DLOG(WARNING)
                        << "Failed to parse GlicExperimentalTriggering payload"
                        << " session_id: " << session_id
                        << " payload_type: " << typed_payload.payload_type();
                  }
                  break;
                }
                case browser_actuator::transport::PayloadType::kActuation:
                  // Handled by a separate transport handler.
                  break;
                case browser_actuator::transport::PayloadType::kUnspecified:
                  break;
              }
            }
          }
        }
      }
      break;
    }
    default:
      // Other payloads are not handled by this handler.
      break;
  }

  std::move(done_callback).Run(nullptr);
}
