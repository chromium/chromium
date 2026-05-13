// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/actor_log.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"

namespace {

std::string_view TaskUpdateStateToString(
    components_sharing_message::GlicExperimentalTriggering::
        ExperimentalTriggeringResponse::TaskUpdate::State state) {
  using TaskUpdate = components_sharing_message::GlicExperimentalTriggering::
      ExperimentalTriggeringResponse::TaskUpdate;
  switch (state) {
    case TaskUpdate::UNKNOWN_STATE:
      return "UNKNOWN_STATE";
    case TaskUpdate::STARTING:
      return "STARTING";
    case TaskUpdate::RUNNING:
      return "RUNNING";
    case TaskUpdate::PAUSED:
      return "PAUSED";
    case TaskUpdate::YIELD:
      return "YIELD";
    case TaskUpdate::STOPPED:
      return "STOPPED";
    case TaskUpdate::COMPLETE:
      return "COMPLETE";
    case TaskUpdate::FAILED:
      return "FAILED";
  }
  return "UNKNOWN_STATE";
}

std::string_view TaskUpdateDataTypeToString(
    components_sharing_message::GlicExperimentalTriggering::
        ExperimentalTriggeringResponse::TaskUpdate::DataType data_type) {
  using TaskUpdate = components_sharing_message::GlicExperimentalTriggering::
      ExperimentalTriggeringResponse::TaskUpdate;
  switch (data_type) {
    case TaskUpdate::UNKNOWN_DATA_TYPE:
      return "UNKNOWN_DATA_TYPE";
    case TaskUpdate::WORKLOG:
      return "WORKLOG";
    case TaskUpdate::PARTIAL_RESPONSE:
      return "PARTIAL_RESPONSE";
    case TaskUpdate::FINAL_RESPONSE:
      return "FINAL_RESPONSE";
    case TaskUpdate::ERROR_MESSAGE:
      return "ERROR_MESSAGE";
  }
  return "UNKNOWN_DATA_TYPE";
}

std::string_view ExperimentalTriggeringRequestPayloadCaseToString(
    components_sharing_message::GlicExperimentalTriggering::
        ExperimentalTriggeringRequest::PayloadCase payload_case) {
  using Request = components_sharing_message::GlicExperimentalTriggering::
      ExperimentalTriggeringRequest;
  switch (payload_case) {
    case Request::kTriggerActuationRequest:
      return "TriggerActuationRequest";
    case Request::kStopActuationRequest:
      return "StopActuationRequest";
    case Request::kDeviceOptInRequest:
      return "DeviceOptInRequest";
    case Request::PAYLOAD_NOT_SET:
      return "PAYLOAD_NOT_SET";
  }
  return "PAYLOAD_NOT_SET";
}

}  // namespace

void LogGlicExperimentalTriggeringProto(
    actor::ActorKeyedService* actor_service,
    std::string_view event_name,
    std::string_view context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto) {
  if (!actor_service) {
    DLOG(WARNING) << "Unable to log GlicExperimentalTriggering proto to actor "
                  << "journal, since ActorKeyedService is not found.";
    return;
  }

  actor::JournalDetailsBuilder builder;
  if (proto.has_task_metadata()) {
    if (proto.task_metadata().has_conversation_id()) {
      builder.Add("conversation_id", proto.task_metadata().conversation_id());
    }
    if (proto.task_metadata().has_task_id()) {
      builder.Add("task_id", proto.task_metadata().task_id());
    }
    if (proto.task_metadata().has_sender_sequence_number()) {
      builder.Add("sender_sequence_number",
                  proto.task_metadata().sender_sequence_number());
    }
  }

  if (proto.has_request()) {
    builder.Add("request_type",
                ExperimentalTriggeringRequestPayloadCaseToString(
                    proto.request().payload_case()));
  }

  if (proto.has_response() && proto.response().has_task_update()) {
    const auto& task_update = proto.response().task_update();
    if (task_update.has_state()) {
      builder.Add("state", TaskUpdateStateToString(task_update.state()));
    }
    if (task_update.has_data_type()) {
      builder.Add("data_type",
                  TaskUpdateDataTypeToString(task_update.data_type()));
    }
    if (task_update.has_data()) {
      builder.Add("data", task_update.data());
    }
  }

  actor_service->GetJournal().LogProto(
      GURL(), actor::TaskId(),
      actor::MakeGlicExperimentalTriggeringTrackUUID(context_id), event_name,
      std::move(builder).Build(), proto);
}
