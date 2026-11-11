// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_session.h"

namespace glic {

namespace {

bool IsTerminalResponse(const ExperimentalTriggeringResponse& response) {
  if (response.task_update.has_value()) {
    return response.task_update->IsTerminal();
  }
  if (response.execute_actions_response.has_value()) {
    return true;
  }
  if (response.device_opt_in_result.has_value()) {
    return *response.device_opt_in_result != DeviceOptInResult::kAccepted;
  }
  return false;
}

}  // namespace

using GlicExperimentalTriggering =
    components_sharing_message::GlicExperimentalTriggering;

GlicExperimentalTriggeringTransportHandler::
    GlicExperimentalTriggeringTransportHandler(
        Profile* profile,
        stream_transport::TransportSession* session,
        base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator)
    : TransportHandler(session),
      profile_(profile),
      coordinator_(std::move(coordinator)) {
  CHECK(profile_);
  CHECK(session);
}

GlicExperimentalTriggeringTransportHandler::
    ~GlicExperimentalTriggeringTransportHandler() = default;

void GlicExperimentalTriggeringTransportHandler::OnMessage(
    stream_transport::PayloadType payload_type,
    std::string_view serialized_payload) {
  GlicExperimentalTriggering triggering;
  if (!triggering.ParseFromString(serialized_payload)) {
    DLOG(WARNING) << "Failed to parse GlicExperimentalTriggering payload";
    return;
  }

  GlicMetricReporter reporter(profile_);
  ScopedIncomingMessageResultLogger result_logger(
      reporter,
      ScopedIncomingMessageResultLogger::Channel::kBrowserActuatorTransport);

  std::string context_id = triggering.context_id();
  if (context_id.empty() && session()) {
    context_id = std::string(session()->GetSessionId());
  }
  if (context_id.empty()) {
    context_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
  triggering.set_context_id(context_id);

  if (!coordinator_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(profile_);
    LogGlicExperimentalTriggeringProto(
        actor_service, "GlicExperimentalTriggering", context_id, triggering);

    result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                 kCoordinatorUnavailable);
    ExperimentalTriggeringResponse response;
    response.context_id = context_id;
    auto request_metadata = ProtoToTaskMetadata(triggering);
    if (request_metadata.has_value()) {
      request_metadata->last_seen_sequence_number =
          request_metadata->sender_sequence_number;
      request_metadata->sender_sequence_number = 0;
      response.task_metadata = std::move(request_metadata);
    }
    TaskUpdate task_update;
    task_update.state = TaskUpdate::State::kFailed;
    task_update.data_type = TaskUpdate::DataType::kErrorMessage;
    task_update.data = "Coordinator is not available.";
    response.task_update = std::move(task_update);
    SendResponse(std::move(response));
    return;
  }

  coordinator_->OnProtoMessage(
      context_id, triggering, std::move(result_logger),
      base::BindRepeating(
          &GlicExperimentalTriggeringTransportHandler::SendResponse,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          [](base::WeakPtr<GlicExperimentalTriggeringTransportHandler> self,
             std::optional<ExperimentalTriggeringResponse> response) {
            if (self && response.has_value()) {
              self->SendResponse(std::move(*response));
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void GlicExperimentalTriggeringTransportHandler::SendResponse(
    ExperimentalTriggeringResponse response) {
  const bool is_terminal = IsTerminalResponse(response);
  components_sharing_message::GlicExperimentalTriggering triggering =
      ResponseToTriggeringProto(response);

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  LogGlicExperimentalTriggeringProto(actor_service,
                                     "GlicExperimentalTriggering",
                                     response.context_id, triggering);

  // `SendUpstreamMessage` synchronously serializes the protobuf `triggering`
  // reference, so passing the const reference is safe during this synchronous
  // call.
  auto send_result = SendUpstreamMessage(
      stream_transport::PayloadType::kExperimentalTriggering, triggering);
  if (!send_result.has_value()) {
    DLOG(ERROR) << "Failed to send experimental triggering response: "
                << static_cast<int>(send_result.error());
  }
  if (is_terminal) {
    // WARNING: `CloseSession()` may destroy the owning `TransportSession` and
    // `this` handler. Do not access any member variables after this call.
    CloseSession();
  }
}

GlicExperimentalTriggeringTransportHandlerFactory::
    GlicExperimentalTriggeringTransportHandlerFactory(Profile* profile)
    : GlicExperimentalTriggeringTransportHandlerFactory(
          profile,
          std::make_unique<GlicExperimentalTriggeringCoordinator>(profile)) {}

GlicExperimentalTriggeringTransportHandlerFactory::
    GlicExperimentalTriggeringTransportHandlerFactory(
        Profile* profile,
        std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator)
    : profile_(profile), coordinator_(std::move(coordinator)) {
  CHECK(profile_);
}

GlicExperimentalTriggeringTransportHandlerFactory::
    ~GlicExperimentalTriggeringTransportHandlerFactory() = default;

stream_transport::FactoryId
GlicExperimentalTriggeringTransportHandlerFactory::GetFactoryId() const {
  return stream_transport::FactoryId::kExperimentalTriggering;
}

std::vector<stream_transport::PayloadType>
GlicExperimentalTriggeringTransportHandlerFactory::GetSupportedPayloadTypes()
    const {
  return {stream_transport::PayloadType::kExperimentalTriggering};
}

std::unique_ptr<stream_transport::TransportHandler>
GlicExperimentalTriggeringTransportHandlerFactory::OnNewSession(
    stream_transport::TransportSession* session) {
  return std::make_unique<GlicExperimentalTriggeringTransportHandler>(
      profile_, session, coordinator_ ? coordinator_->GetWeakPtr() : nullptr);
}

}  // namespace glic
