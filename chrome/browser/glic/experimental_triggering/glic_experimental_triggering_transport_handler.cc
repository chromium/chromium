// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "content/public/browser/web_contents.h"

namespace glic {

using GlicExperimentalTriggering =
    components_sharing_message::GlicExperimentalTriggering;
using DeviceOptInResult = GlicExperimentalTriggering::
    ExperimentalTriggeringResponse::DeviceOptInResult;

GlicExperimentalTriggeringTransportHandler::
    GlicExperimentalTriggeringTransportHandler(
        GlicExperimentalOptInController* opt_in_controller,
        browser_actuator::TransportSession* session)
    : opt_in_controller_(opt_in_controller), session_(session) {
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
  if (triggering.has_request() &&
      triggering.request().has_device_opt_in_request()) {
    HandleOptInRequest(triggering);
  }
}

void GlicExperimentalTriggeringTransportHandler::HandleOptInRequest(
    const GlicExperimentalTriggering& triggering) {
  if (!opt_in_controller_) {
    SendOptInResponse(
        triggering,
        GlicExperimentalTriggering::ExperimentalTriggeringResponse::FAILED);
    return;
  }

  content::WebContents* web_contents =
      opt_in_controller_->GetOrCreateSuitableWebContents();
  if (!web_contents) {
    SendOptInResponse(
        triggering,
        GlicExperimentalTriggering::ExperimentalTriggeringResponse::FAILED);
    return;
  }

  auto callback = base::BindOnce(
      &GlicExperimentalTriggeringTransportHandler::OnOptInCompleted,
      weak_ptr_factory_.GetWeakPtr(), triggering);
  opt_in_controller_->ShowDialog(web_contents, std::move(callback));
}

void GlicExperimentalTriggeringTransportHandler::OnOptInCompleted(
    const GlicExperimentalTriggering& request,
    bool accepted) {
  auto result =
      accepted
          ? GlicExperimentalTriggering::ExperimentalTriggeringResponse::ACCEPTED
          : GlicExperimentalTriggering::ExperimentalTriggeringResponse::
                DECLINED;
  SendOptInResponse(request, result);
}

void GlicExperimentalTriggeringTransportHandler::SendOptInResponse(
    const GlicExperimentalTriggering& request,
    DeviceOptInResult result) {
  GlicExperimentalTriggering response;
  if (request.has_context_id()) {
    response.set_context_id(request.context_id());
  }
  if (request.has_task_metadata()) {
    auto* meta = response.mutable_task_metadata();
    if (request.task_metadata().has_conversation_id()) {
      meta->set_conversation_id(request.task_metadata().conversation_id());
    }
    if (request.task_metadata().has_task_id()) {
      meta->set_task_id(request.task_metadata().task_id());
    }
    if (request.task_metadata().has_sender_sequence_number()) {
      meta->set_last_seen_sequence_number(
          request.task_metadata().sender_sequence_number());
    }
  }

  response.mutable_response()->set_device_opt_in_result(result);
  auto send_result = session_->SendMessage(
      browser_actuator::PayloadType::kExperimentalTriggering, response);
  if (!send_result.has_value()) {
    DLOG(ERROR) << "Failed to send opt-in response: "
                << static_cast<int>(send_result.error());
  }
}

GlicExperimentalTriggeringTransportHandlerFactory::
    GlicExperimentalTriggeringTransportHandlerFactory(
        GlicExperimentalOptInController* opt_in_controller)
    : opt_in_controller_(opt_in_controller) {}

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
      opt_in_controller_.get(), session);
}

}  // namespace glic
