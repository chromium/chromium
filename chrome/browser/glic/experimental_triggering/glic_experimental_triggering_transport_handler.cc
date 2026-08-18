// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

namespace glic {

using GlicExperimentalTriggering =
    components_sharing_message::GlicExperimentalTriggering;

GlicExperimentalTriggeringTransportHandler::
    GlicExperimentalTriggeringTransportHandler(
        Profile* profile,
        browser_actuator::TransportSession* session,
        std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator)
    : profile_(profile),
      session_(session),
      coordinator_(std::move(coordinator)) {
  CHECK(profile_);
  CHECK(session_);
}

GlicExperimentalTriggeringTransportHandler::
    ~GlicExperimentalTriggeringTransportHandler() = default;

void GlicExperimentalTriggeringTransportHandler::OnMessage(
    const google::protobuf::MessageLite& message) {
  if (message.GetTypeName() !=
      GlicExperimentalTriggering::default_instance().GetTypeName()) {
    return;
  }
  const auto& triggering =
      static_cast<const GlicExperimentalTriggering&>(message);

  ScopedIncomingMessageResultLogger result_logger(
      ScopedIncomingMessageResultLogger::Channel::kBrowserActuatorTransport);

  std::string context_id = triggering.context_id();
  if (context_id.empty()) {
    context_id = std::string(session_->GetSessionId());
  }

  if (!coordinator_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(profile_);
    LogGlicExperimentalTriggeringProto(
        actor_service, "GlicExperimentalTriggering", context_id, triggering);

    result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                 kCoordinatorUnavailable);
    ExperimentalTriggeringResponse response;
    response.context_id = context_id;
    if (triggering.has_task_metadata()) {
      TaskMetadata meta;
      const auto& proto_meta = triggering.task_metadata();
      if (proto_meta.has_conversation_id()) {
        meta.conversation_id = proto_meta.conversation_id();
      }
      if (proto_meta.has_task_id()) {
        meta.task_id = proto_meta.task_id();
      }
      if (proto_meta.has_sender_sequence_number()) {
        meta.last_seen_sequence_number = proto_meta.sender_sequence_number();
      }
      meta.sender_sequence_number = 0;
      response.task_metadata = std::move(meta);
    }
    TaskUpdate task_update;
    task_update.state = TaskUpdate::State::kFailed;
    task_update.data_type = TaskUpdate::DataType::kErrorMessage;
    task_update.data = "Coordinator is not available.";
    response.task_update = std::move(task_update);
    SendResponse(std::move(response));
    return;
  }

  std::optional<ExperimentalTriggeringResponse> domain_response =
      coordinator_->OnProtoMessage(
          context_id, triggering, std::move(result_logger),
          base::BindRepeating(
              &GlicExperimentalTriggeringTransportHandler::SendResponse,
              weak_ptr_factory_.GetWeakPtr()),
          /*prepared_tab=*/nullptr);

  if (domain_response.has_value()) {
    SendResponse(std::move(*domain_response));
  }
}

void GlicExperimentalTriggeringTransportHandler::SendResponse(
    ExperimentalTriggeringResponse response) {
  components_sharing_message::SharingMessage outgoing_message =
      ResponseToProto(response);
  const auto& triggering = outgoing_message.glic_experimental_triggering();

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  LogGlicExperimentalTriggeringProto(actor_service,
                                     "GlicExperimentalTriggering",
                                     response.context_id, triggering);

  // `SendMessage` synchronously serializes the protobuf `triggering`
  // reference, so passing the const reference from `outgoing_message` is safe
  // during this synchronous call.
  auto send_result = session_->SendMessage(
      browser_actuator::PayloadType::kExperimentalTriggering, triggering);
  if (!send_result.has_value()) {
    DLOG(ERROR) << "Failed to send experimental triggering response: "
                << static_cast<int>(send_result.error());
  }
}

GlicExperimentalTriggeringTransportHandlerFactory::
    GlicExperimentalTriggeringTransportHandlerFactory(Profile* profile)
    : profile_(profile) {}

GlicExperimentalTriggeringTransportHandlerFactory::
    ~GlicExperimentalTriggeringTransportHandlerFactory() = default;

browser_actuator::FactoryId
GlicExperimentalTriggeringTransportHandlerFactory::GetFactoryId() const {
  return browser_actuator::FactoryId::kExperimentalTriggering;
}

std::vector<browser_actuator::PayloadType>
GlicExperimentalTriggeringTransportHandlerFactory::GetSupportedPayloadTypes()
    const {
  return {browser_actuator::PayloadType::kExperimentalTriggering};
}

std::unique_ptr<browser_actuator::TransportHandler>
GlicExperimentalTriggeringTransportHandlerFactory::OnNewSession(
    browser_actuator::TransportSession* session) {
  return std::make_unique<GlicExperimentalTriggeringTransportHandler>(
      profile_, session,
      std::make_unique<GlicExperimentalTriggeringCoordinator>(profile_));
}

}  // namespace glic
