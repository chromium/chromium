// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/actor_log.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_id.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace glic {
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
    case TaskUpdate::RESUMED:
      return "RESUMED";
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
    case Request::kGetScreenshotRequest:
      return "GetScreenshotRequest";
    case Request::kContinueActuationRequest:
      return "ContinueActuationRequest";
    case Request::kExecuteActionsRequest:
      return "ExecuteActionsRequest";
    case Request::kSubmitConfirmation:
      return "SubmitConfirmation";
    case Request::PAYLOAD_NOT_SET:
      return "PAYLOAD_NOT_SET";
  }
  return "PAYLOAD_NOT_SET";
}

std::string FormatOptionalSequenceNumber(std::optional<int64_t> seq) {
  return seq.has_value() ? base::ToString(*seq) : std::string("none");
}

std::string_view ExperimentalTriggeringRequestPayloadToString(
    const ExperimentalTriggeringRequest::Payload& payload) {
  return std::visit(
      absl::Overload{
          [](std::monostate) -> std::string_view { return "PAYLOAD_NOT_SET"; },
          [](const RequestPayloadNotSet&) -> std::string_view {
            return "PAYLOAD_NOT_SET";
          },
          [](const TriggerActuationRequest&) -> std::string_view {
            return "TriggerActuationRequest";
          },
          [](const ContinueActuationRequest&) -> std::string_view {
            return "ContinueActuationRequest";
          },
          [](const StopActuationRequest&) -> std::string_view {
            return "StopActuationRequest";
          },
          [](const DeviceOptInRequest&) -> std::string_view {
            return "DeviceOptInRequest";
          },
          [](const TaskMetadataUpdated&) -> std::string_view {
            return "TaskMetadataUpdated";
          },
          [](const GetScreenshotRequest&) -> std::string_view {
            return "GetScreenshotRequest";
          },
          [](const ExecuteActionsRequest&) -> std::string_view {
            return "ExecuteActionsRequest";
          },
          [](const SubmitConfirmation&) -> std::string_view {
            return "SubmitConfirmation";
          },
      },
      payload);
}

void LogExperimentalTriggeringJournalEvent(
    actor::AggregatedJournal* journal,
    std::string_view context_id,
    std::string_view event_name,
    actor::JournalDetailsBuilder details) {
  if (!journal) {
    return;
  }
  details.Add("context_id", context_id);
  journal->Log(GURL(), actor::TaskId(),
               actor::MakeGlicExperimentalTriggeringTrackUUID(context_id),
               event_name, std::move(details).Build());
}

}  // namespace

std::string_view ExperimentalTriggeringHandlerDestroyReasonToString(
    GlicExperimentalTriggeringHandlerDestroyReason reason) {
  switch (reason) {
    case GlicExperimentalTriggeringHandlerDestroyReason::kIdle:
      return "Idle";
    case GlicExperimentalTriggeringHandlerDestroyReason::kSessionComplete:
      return "SessionComplete";
    case GlicExperimentalTriggeringHandlerDestroyReason::kSessionStopped:
      return "SessionStopped";
    case GlicExperimentalTriggeringHandlerDestroyReason::kSessionFailed:
      return "SessionFailed";
    case GlicExperimentalTriggeringHandlerDestroyReason::kRejectedSynchronously:
      return "RejectedSynchronously";
    case GlicExperimentalTriggeringHandlerDestroyReason::kInvokeError:
      return "InvokeError";
    case GlicExperimentalTriggeringHandlerDestroyReason::kSuperseded:
      return "Superseded";
    case GlicExperimentalTriggeringHandlerDestroyReason::kTabClosed:
      return "TabClosed";
    case GlicExperimentalTriggeringHandlerDestroyReason::kInstanceDestroyed:
      return "InstanceDestroyed";
    case GlicExperimentalTriggeringHandlerDestroyReason::kClientDisconnected:
      return "ClientDisconnected";
    case GlicExperimentalTriggeringHandlerDestroyReason::kRegistrationFailed:
      return "RegistrationFailed";
    case GlicExperimentalTriggeringHandlerDestroyReason::kCoordinatorShutdown:
      return "CoordinatorShutdown";
    case GlicExperimentalTriggeringHandlerDestroyReason::
        kUnexpectedLiveDestruction:
      return "UnexpectedLiveDestruction";
  }
  NOTREACHED();
}

std::string_view ExperimentalTriggeringSequenceAnomalyToString(
    GlicExperimentalTriggeringSequenceAnomaly anomaly) {
  switch (anomaly) {
    case GlicExperimentalTriggeringSequenceAnomaly::kStreamRestarted:
      return "StreamRestarted";
    case GlicExperimentalTriggeringSequenceAnomaly::kMissingHistory:
      return "MissingHistory";
    case GlicExperimentalTriggeringSequenceAnomaly::kLastSeenAheadOfSent:
      return "LastSeenAheadOfSent";
    case GlicExperimentalTriggeringSequenceAnomaly::kRequestGap:
      return "RequestGap";
    case GlicExperimentalTriggeringSequenceAnomaly::kDuplicateOrReplayedRequest:
      return "DuplicateOrReplayedRequest";
  }
  NOTREACHED();
}
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

  if (proto.has_response()) {
    if (proto.response().has_task_update()) {
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
    } else if (proto.response().has_execute_actions_response()) {
      const auto& exec_resp = proto.response().execute_actions_response();
      if (exec_resp.has_actions_result()) {
        builder.Add("actions_result_code",
                    exec_resp.actions_result().action_result());
        builder.Add("script_tools_results_count",
                    exec_resp.actions_result().script_tool_results_size());
      }
    } else if (proto.response().has_confirmation_result()) {
      const auto& conf_resp = proto.response().confirmation_result();
      if (conf_resp.has_status()) {
        builder.Add("confirmation_status", conf_resp.status());
      }
    }
  }

  actor_service->GetJournal().LogProto(
      GURL(), actor::TaskId(),
      actor::MakeGlicExperimentalTriggeringTrackUUID(context_id), event_name,
      std::move(builder).Build(), proto);
}

void LogGlicExperimentalTriggeringHandlerDestroyed(
    actor::AggregatedJournal* journal,
    std::string_view context_id,
    GlicExperimentalTriggeringHandlerDestroyReason reason,
    const UpdatesHandlerDestroyedJournalSnapshot& snapshot) {
  actor::JournalDetailsBuilder builder;
  builder.Add("reason",
              ExperimentalTriggeringHandlerDestroyReasonToString(reason));
  builder.Add("has_instance", snapshot.has_instance);
  builder.Add("has_actions_runner", snapshot.has_actions_runner);
  builder.Add("awaiting_opt_in", snapshot.awaiting_opt_in);
  builder.Add("pending_invoke_count", snapshot.pending_invoke_count);
  builder.Add("is_actuation_started", snapshot.is_actuation_started);
  builder.Add("terminal_update_sent", snapshot.terminal_update_sent);
  LogExperimentalTriggeringJournalEvent(
      journal, context_id, "UpdatesHandlerDestroyed", std::move(builder));
}

void LogGlicExperimentalTriggeringSequenceAnomaly(
    actor::AggregatedJournal* journal,
    std::string_view context_id,
    GlicExperimentalTriggeringSequenceAnomaly anomaly,
    const ExperimentalTriggeringRequest& request,
    std::optional<int64_t> handler_max_seen_seq,
    int64_t handler_sent_count) {
  const std::optional<int64_t> request_seq =
      request.task_metadata ? request.task_metadata->sender_sequence_number
                            : std::nullopt;
  const std::optional<int64_t> request_last_seen =
      request.task_metadata ? request.task_metadata->last_seen_sequence_number
                            : std::nullopt;
  actor::JournalDetailsBuilder builder;
  builder.Add("anomaly",
              ExperimentalTriggeringSequenceAnomalyToString(anomaly));
  builder.Add("request_type",
              ExperimentalTriggeringRequestPayloadToString(request.payload));
  builder.Add("request_seq", FormatOptionalSequenceNumber(request_seq));
  builder.Add("request_last_seen",
              FormatOptionalSequenceNumber(request_last_seen));
  builder.Add("handler_max_seen_request_seq",
              FormatOptionalSequenceNumber(handler_max_seen_seq));
  builder.Add("handler_sent_count", handler_sent_count);
  LogExperimentalTriggeringJournalEvent(journal, context_id, "SequenceAnomaly",
                                        std::move(builder));
}
}  // namespace glic
