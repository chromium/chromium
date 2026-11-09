// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_actions_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_api_metrics.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"  // nogncheck
#else
#include "base/android/application_status_listener.h"
#endif

namespace glic {

namespace {

const base::FeatureParam<base::TimeDelta>
    kGlicExperimentalTriggeringScriptToolsTimeout{
        &features::kGlicExperimentalTriggeringScriptTools, "timeout",
        base::Minutes(2)};

GlicInvokeOptions CreateInvokeOptions(
    const ExperimentalTriggeringRequest& request,
    BrowserWindowInterface* window,
    tabs::TabInterface* tab) {
  GlicInvokeOptions options{mojom::InvocationSource::kExperimentalTriggering};

  if (tab) {
    options.target.surface = tab->GetHandle();
  } else {
    LastActiveOrNew last_active_or_new;
    last_active_or_new.window = window;
    last_active_or_new.open_in_foreground = false;
    options.target.surface = last_active_or_new;
  }
  options.target.actuation_target = mojom::ActuationTarget::kTargetSurface;

  options.timeout = base::Minutes(5);

  if (std::holds_alternative<TriggerActuationRequest>(request.payload)) {
    const auto& trigger_req =
        std::get<TriggerActuationRequest>(request.payload);
    options.feature_mode = mojom::FeatureMode::kExperimentalTriggering;
    if (!trigger_req.initial_prompt.empty()) {
      options.prompts.push_back(trigger_req.initial_prompt);
    }
    auto et_payload = mojom::ExperimentalTriggeringPayload::New();
    et_payload->payment_metadata = trigger_req.payment_metadata;
    options.source_or_payload =
        mojom::InvocationPayload::NewExperimentalTriggering(
            std::move(et_payload));
  } else if (std::holds_alternative<ContinueActuationRequest>(
                 request.payload)) {
    options.feature_mode = mojom::FeatureMode::kActuation;
    options.supersede_if_in_progress = true;
    const auto& continue_req =
        std::get<ContinueActuationRequest>(request.payload);
    if (!continue_req.continuation_prompt.empty()) {
      options.prompts.push_back(continue_req.continuation_prompt);
    }
  }

  if (request.task_metadata.has_value() &&
      request.task_metadata->parent_conversation_metadata.has_value()) {
    const auto& parent_metadata =
        *request.task_metadata->parent_conversation_metadata;
    auto context = mojom::AdditionalContext::New();
    context->source = mojom::AdditionalContextSource::kExperimentalTriggering;
    auto parent_conversation = mojom::ParentConversationMetadata::New();
    if (!parent_metadata.conversation_id.empty()) {
      parent_conversation->conversation_id = parent_metadata.conversation_id;
    }
    if (!parent_metadata.conversation_title.empty()) {
      parent_conversation->conversation_title =
          parent_metadata.conversation_title;
    }
    context->parts.push_back(
        mojom::AdditionalContextPart::NewParentConversationMetadata(
            std::move(parent_conversation)));
    options.additional_context = AdditionalTabContext(
        std::move(context), content::GlobalRenderFrameHostId(),
        PolicyCheck::kNone);
  }

  if (request.task_metadata.has_value() &&
      !request.task_metadata->conversation_id.empty()) {
    options.target.conversation =
        ConversationId(request.task_metadata->conversation_id);
  } else {
    options.target.conversation = NewConversation();
  }
  return options;
}

// Builds base response metadata for synchronous request replies (using incoming
// request metadata).
ExperimentalTriggeringResponse CreateBaseResponseMessage(
    const std::string& context_id,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number) {
  ExperimentalTriggeringResponse response;
  response.context_id = context_id;
  TaskMetadata metadata;
  if (request_task_metadata) {
    metadata.conversation_id = request_task_metadata->conversation_id;
    metadata.task_id = request_task_metadata->task_id;
    if (request_task_metadata->sender_sequence_number.has_value()) {
      metadata.last_seen_sequence_number =
          request_task_metadata->sender_sequence_number;
    }
  }
  metadata.sender_sequence_number = sender_sequence_number;
  response.task_metadata = std::move(metadata);
  return response;
}

// Builds a complete task update response for synchronous request replies.
ExperimentalTriggeringResponse CreateResponseMessage(
    const std::string& context_id,
    TaskUpdate::State state,
    std::optional<TaskUpdate::DataType> data_type,
    const std::string& message,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number,
    std::map<std::string, std::string> task_update_metadata) {
  if (data_type == TaskUpdate::DataType::kErrorMessage) {
    DLOG(WARNING) << message;
  }
  ExperimentalTriggeringResponse response = CreateBaseResponseMessage(
      context_id, request_task_metadata, sender_sequence_number);
  TaskUpdate task_update;
  task_update.state = state;
  task_update.data_type = data_type;
  task_update.data = message;
  task_update.metadata = std::move(task_update_metadata);
  response.task_update = std::move(task_update);
  return response;
}

ExperimentalTriggeringResponse CreateResponseMessage(
    const std::string& context_id,
    TaskUpdate::State state,
    std::optional<TaskUpdate::DataType> data_type,
    const std::string& message,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number) {
  return CreateResponseMessage(context_id, state, data_type, message,
                               request_task_metadata, sender_sequence_number,
                               /*task_update_metadata=*/{});
}

void RejectProtoMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto,
    ScopedIncomingMessageResultLogger& result_logger,
    GlicExperimentalTriggeringIncomingMessageResult result,
    const std::string& error_message,
    GlicExperimentalTriggeringResponseCallback response_callback,
    std::map<std::string, std::string> task_update_metadata) {
  result_logger.set_result(result);
  std::optional<TaskMetadata> request_metadata = ProtoToTaskMetadata(proto);
  std::move(response_callback)
      .Run(CreateResponseMessage(
          context_id, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage, error_message,
          request_metadata.has_value() ? &*request_metadata : nullptr,
          /*sender_sequence_number=*/0, std::move(task_update_metadata)));
}

void RejectProtoMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto,
    ScopedIncomingMessageResultLogger& result_logger,
    GlicExperimentalTriggeringIncomingMessageResult result,
    const std::string& error_message,
    GlicExperimentalTriggeringResponseCallback response_callback) {
  RejectProtoMessage(context_id, proto, result_logger, result, error_message,
                     std::move(response_callback), /*task_update_metadata=*/{});
}

// Builds a device opt-in response for synchronous request replies.
ExperimentalTriggeringResponse CreateDeviceOptInResponse(
    const std::string& context_id,
    DeviceOptInResult opt_in_result,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number) {
  ExperimentalTriggeringResponse response = CreateBaseResponseMessage(
      context_id, request_task_metadata, sender_sequence_number);
  response.device_opt_in_result = opt_in_result;
  return response;
}

// Builds a screenshot result with the specified status and tokens.
ScreenshotResult CreateScreenshotResult(
    ScreenshotResult::Status status,
    std::string_view file_token = {},
    std::vector<uint8_t> request_token = {}) {
  ScreenshotResult result;
  result.status = status;
  if (!file_token.empty()) {
    result.file_token = std::string(file_token);
  }
  if (!request_token.empty()) {
    result.request_token = std::move(request_token);
  }
  return result;
}

// Builds a screenshot result response for synchronous request replies.
ExperimentalTriggeringResponse CreateScreenshotResultResponse(
    const std::string& context_id,
    ScreenshotResult screenshot_result,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number) {
  ExperimentalTriggeringResponse response = CreateBaseResponseMessage(
      context_id, request_task_metadata, sender_sequence_number);
  response.screenshot_result = std::move(screenshot_result);
  return response;
}

// Builds a confirmation result with the specified status.
ConfirmationResult CreateConfirmationResult(ConfirmationResult::Status status,
                                            std::string error_message = {}) {
  ConfirmationResult result;
  result.status = status;
  result.error_message = std::move(error_message);
  return result;
}

// Builds a confirmation result response for synchronous request replies.
ExperimentalTriggeringResponse CreateConfirmationResultResponse(
    const std::string& context_id,
    ConfirmationResult confirmation_result,
    const TaskMetadata* request_task_metadata,
    int64_t sender_sequence_number) {
  ExperimentalTriggeringResponse response = CreateBaseResponseMessage(
      context_id, request_task_metadata, sender_sequence_number);
  response.confirmation_result = std::move(confirmation_result);
  return response;
}

// Builds base response metadata for asynchronous Mojo updates and callbacks
// (using instance state or stored request metadata).
ExperimentalTriggeringResponse CreateBaseResponse(
    const std::string& context_id,
    int64_t sender_sequence_number,
    std::optional<int64_t> last_seen_sequence_number,
    const GlicInstance* instance,
    std::string_view conversation_id = {},
    std::string_view task_id = {}) {
  ExperimentalTriggeringResponse response;
  response.context_id = context_id;
  TaskMetadata metadata;
  metadata.sender_sequence_number = sender_sequence_number;
  if (last_seen_sequence_number.has_value()) {
    metadata.last_seen_sequence_number = last_seen_sequence_number;
  }
  if (instance && instance->conversation_id()) {
    metadata.conversation_id = *instance->conversation_id();
  } else if (!conversation_id.empty()) {
    metadata.conversation_id = std::string(conversation_id);
  }
  if (!task_id.empty()) {
    metadata.task_id = std::string(task_id);
  }
  response.task_metadata = std::move(metadata);
  return response;
}

GlicExperimentalTriggeringExecutionOutcome GlicInvokeErrorToOutcome(
    GlicInvokeError error,
    bool client_connected) {
  switch (error) {
    case GlicInvokeError::kTimeout:
      return client_connected ? GlicExperimentalTriggeringExecutionOutcome::
                                    kTimeoutWaitingForActuation
                              : GlicExperimentalTriggeringExecutionOutcome::
                                    kTimeoutWaitingForClient;
    case GlicInvokeError::kInvokeInProgress:
      return GlicExperimentalTriggeringExecutionOutcome::
          kInvokeErrorInvokeInProgress;
    case GlicInvokeError::kTabClosed:
      return GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorTabClosed;
    default:
      return GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorOther;
  }
}

bool IsChromeDiagnosticMetadataKey(std::string_view key) {
  static_assert(kMetadataTimeToPanelOpenMsKey.starts_with("t_to_"));
  static_assert(kMetadataTimeToClientConnectedMsKey.starts_with("t_to_"));
  static_assert(kMetadataTimeToFirstResponseMsKey.starts_with("t_to_"));
  static_assert(kMetadataTimeToTerminalMsKey.starts_with("t_to_"));
  return key == kMetadataChromeErrorCodeKey ||
         key == kMetadataChromeInvokeDiagnosticsKey ||
         key == kMetadataPanelOpenedKey || key == kMetadataClientConnectedKey ||
         key.starts_with("t_to_");
}

bool CanActOnWeb(GlicKeyedService& glic_service) {
  return glic_service.HasActorPolicyChecker() &&
         glic_service.actor_policy_checker().CanActOnWeb();
}

}  // namespace

// Handles every request for one context id, and owns the session that those
// requests drive.
//
// Lifetime: the coordinator keeps one handler per context id, so a handler must
// stay alive for as long as it has live work (see HasLiveWork()). Deleting it
// earlier drops the session's web client updates, and the next request creates
// a new handler whose sequence numbers restart at 0, so the server discards its
// replies as stale (b/574916669).
//
// The handler is deleted only in these places:
//  - OnRequest(): if a request reaches a handler with no live work, OnRequest()
//    arms `cleanup_runner`, which deletes the handler when the request is done.
//    A Process* method that starts async work which reports back through this
//    handler must call `cleanup_runner.Release()` after that work has started.
//    A Process* method must not arm `cleanup_runner` itself, unless it ends a
//    live session on purpose (ProcessStopActuationRequest()).
//  - HandleTerminalUpdate(): the handler sent a terminal task update
//    (COMPLETE, STOPPED or FAILED). The server treats every terminal update as
//    the end of the task and sends nothing more for it, so the handler stops
//    the work it still owns (StopOwnedLiveWork()) and ends the session too.
//  - SendDeviceOptInResult() and SendExecuteActionsResponse(): the async work
//    that a request started has finished, and the handler has no other live
//    work.
class ExperimentalTriggeringUpdatesHandler
    : public mojom::ExperimentalTriggeringUpdatesHandler {
 public:
  ExperimentalTriggeringUpdatesHandler(
      Profile* profile,
      const std::string& context_id,
      InvokeWithAutoSubmitPasskey passkey,
      InvokeErrorDiagnosticsPasskey error_diagnostics_passkey,
      base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator)
      : context_id_(context_id),
        passkey_(std::move(passkey)),
        error_diagnostics_passkey_(std::move(error_diagnostics_passkey)),
        coordinator_(std::move(coordinator)),
        receiver_(this),
        metric_reporter_(profile) {}

  ~ExperimentalTriggeringUpdatesHandler() override {
    // Covers destruction without a terminal update.
    RemovePendingTask();
    if (is_actuation_started_) {
      if (!terminal_update_sent_) {
        HandleTerminalUpdate(
            TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
            GlicExperimentalTriggeringExecutionOutcome::
                kDestroyedBeforeCompletion,
            "Experimental triggering handler destroyed before completion.",
            BuildChromeErrorMetadata(kErrorCodeGlicDestroyedBeforeCompletion));
      } else if (!outcome_recorded_) {
        RecordOutcome(GlicExperimentalTriggeringExecutionOutcome::
                          kDestroyedBeforeCompletion);
      }
    }
  }

  void RemovePendingTask() {
    if (!coordinator_ || !coordinator_->profile_) {
      return;
    }
    if (auto* actor_service =
            actor::ActorKeyedService::Get(coordinator_->profile_)) {
      actor_service->RemovePendingTask(context_id_);
    }
  }

  void ObserveActorTaskCreation() {
    if (!instance_ || actuating_subscription_) {
      return;
    }
    if (auto* task_manager = instance_->GetActorTaskManager()) {
      actuating_subscription_ =
          task_manager->AddActuatingChangedCallback(base::BindRepeating(
              &ExperimentalTriggeringUpdatesHandler::OnActuatingChanged,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void OnActuatingChanged(bool is_actuating) {
    if (!is_actuating) {
      return;
    }
    actuating_subscription_ = {};
    task_created_ = true;
    RemovePendingTask();
  }

  std::optional<ExperimentalTriggeringResponse> OnRequest(
      const ExperimentalTriggeringRequest& request,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      tabs::TabInterface* prepared_tab) {
    if (update_callback) {
      update_callback_ = std::move(update_callback);
    }
    // Decide here, once, whether this request may delete the handler: only if
    // the handler has no live work. This also covers every early return below.
    // See "Lifetime" above the class.
    base::ScopedClosureRunner cleanup_runner;
    if (!HasLiveWork()) {
      cleanup_runner = CreateCleanupRunner();
    }

    if (!request.task_metadata.has_value()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kMissingTaskMetadata);
      if (cleanup_runner) {
        terminal_update_sent_ = true;
      }
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Received GlicExperimentalTriggering message with missing task "
          "metadata.",
          /*request_task_metadata=*/nullptr, sequence_generator_.GetNext());
    }

    if (request.task_metadata->sender_sequence_number.has_value()) {
      last_seen_sequence_number_ =
          *request.task_metadata->sender_sequence_number;
    }

    std::optional<ExperimentalTriggeringResponse> response = std::visit(
        absl::Overload{
            [&](const TaskMetadataUpdated&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTaskMetadataUpdated(request, cleanup_runner,
                                                std::move(result_logger));
            },
            [&](const TriggerActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTriggerOrContinueActuationRequest(
                  request, prepared_tab, cleanup_runner,
                  std::move(result_logger));
            },
            [&](const ContinueActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTriggerOrContinueActuationRequest(
                  request, prepared_tab, cleanup_runner,
                  std::move(result_logger));
            },
            [&](const StopActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessStopActuationRequest(&*request.task_metadata,
                                                 cleanup_runner,
                                                 std::move(result_logger));
            },
            [&](const DeviceOptInRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessDeviceOptInRequest(&*request.task_metadata,
                                               cleanup_runner,
                                               std::move(result_logger));
            },
            [&](const GetScreenshotRequest& payload)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessGetScreenshotRequest(
                  payload, &*request.task_metadata, cleanup_runner,
                  std::move(result_logger));
            },
            [&](const ExecuteActionsRequest& payload)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessExecuteActionsRequest(
                  payload, &*request.task_metadata, cleanup_runner,
                  std::move(result_logger));
            },
            [&](const SubmitConfirmation& payload)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessSubmitConfirmation(payload, &*request.task_metadata,
                                               cleanup_runner,
                                               std::move(result_logger));
            },
            [&](const RequestPayloadNotSet&)
                -> std::optional<ExperimentalTriggeringResponse> {
              result_logger.set_result(
                  GlicExperimentalTriggeringIncomingMessageResult::
                      kNoActionableRequest);
              return CreateResponseMessage(
                  context_id_, TaskUpdate::State::kFailed,
                  TaskUpdate::DataType::kErrorMessage,
                  "Received GlicExperimentalTriggering message with no "
                  "actionable request.",
                  &*request.task_metadata, sequence_generator_.GetNext());
            },
            [&](const std::monostate&)
                -> std::optional<ExperimentalTriggeringResponse> {
              result_logger.set_result(
                  GlicExperimentalTriggeringIncomingMessageResult::
                      kUnexpectedRequestPayload);
              return CreateResponseMessage(
                  context_id_, TaskUpdate::State::kFailed,
                  TaskUpdate::DataType::kErrorMessage,
                  "Received GlicExperimentalTriggering message with no "
                  "actionable request.",
                  &*request.task_metadata, sequence_generator_.GetNext());
            },
        },
        request.payload);

    // If `cleanup_runner` is still armed, tearing down this handler on scope
    // exit must not emit a duplicate best-effort `kFailed` update when a
    // terminal `TaskUpdate` is already being returned synchronously.
    if (cleanup_runner && response.has_value() &&
        response->task_update.has_value() &&
        response->task_update->IsTerminal()) {
      terminal_update_sent_ = true;
    }
    return response;
  }

  void OnUpdate(mojom::ExperimentalTriggeringUpdatePtr update,
                mojom::SubscriberObservationType observation) override {
    if (terminal_update_sent_) {
      return;
    }
    LogApiRequestCount(GlicHostApiRequestId::kOnExperimentalTriggeringUpdate);
    switch (observation) {
      case mojom::SubscriberObservationType::kComplete:
        HandleTerminalUpdate(
            TaskUpdate::State::kComplete, TaskUpdate::DataType::kFinalResponse,
            GlicExperimentalTriggeringExecutionOutcome::kSuccess);
        break;
      case mojom::SubscriberObservationType::kError:
        HandleTerminalUpdate(
            TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
            GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed);
        break;
      case mojom::SubscriberObservationType::kUpdate: {
        if (!update) {
          DLOG(ERROR) << "Received kUpdate observation with null update";
          return;
        }
        std::map<std::string, std::string> metadata;
        if (base::FeatureList::IsEnabled(
                features::kGlicStructuredYieldMetadata) &&
            update->metadata.has_value()) {
          for (auto& [key, val] : *update->metadata) {
            if (!IsChromeDiagnosticMetadataKey(key)) {
              metadata[key] = std::move(val);
            }
          }
        }
        if (!first_response_received_ && is_actuation_started_) {
          first_response_received_ = true;
          CancelReconnectGrace();
          const base::TimeTicks now = base::TimeTicks::Now();
          if (!time_to_first_response_.has_value()) {
            time_to_first_response_ = now - task_start_time_;
          }
          metric_reporter_.RecordMediumTimes(
              "ExperimentalTriggering.Latency.ToFirstResponse",
              now - turn_start_time_);
        }
        switch (update->type) {
          case mojom::ExperimentalTriggeringUpdateType::kWorklog:
            SendTaskUpdateMessage(TaskUpdate::State::kRunning,
                                  TaskUpdate::DataType::kWorklog,
                                  std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kPaused:
            SendTaskUpdateMessage(TaskUpdate::State::kPaused, std::nullopt,
                                  std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion:
            HandleTerminalUpdate(
                TaskUpdate::State::kComplete,
                TaskUpdate::DataType::kFinalResponse,
                GlicExperimentalTriggeringExecutionOutcome::kSuccess,
                std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kTerminalStopped:
            HandleTerminalUpdate(
                TaskUpdate::State::kStopped, std::nullopt,
                GlicExperimentalTriggeringExecutionOutcome::kTerminalStopped,
                std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kTerminalFailed:
            HandleTerminalUpdate(
                TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
                GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed,
                std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kYieldToUser:
            SendTaskUpdateMessage(TaskUpdate::State::kYield, std::nullopt,
                                  std::move(update->data), std::move(metadata));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kResumed:
            SendTaskUpdateMessage(TaskUpdate::State::kResumed, std::nullopt,
                                  std::move(update->data));
            break;
          case mojom::ExperimentalTriggeringUpdateType::kUnknown:
            SendTaskUpdateMessage(TaskUpdate::State::kUnknown, std::nullopt,
                                  std::move(update->data), std::move(metadata));
            break;
        }
        break;
      }
    }
  }

 private:
  // Whether this handler has work that still reports back through it: a Glic
  // instance driving an actuation, an in-flight direct action execution, an
  // open device opt-in dialog, or a Glic invocation that has not finished. A
  // request never deletes a handler that has live work, except
  // StopActuationRequest.
  //
  // A yielded turn still counts: `instance_` stays set while the task waits
  // for a continuation, so that the continuation keeps its sequence numbers.
  // So this means "a session is still open", not "something is running right
  // now". A terminal update ends the session (see HandleTerminalUpdate()).
  bool HasLiveWork() const {
    return instance_ || actions_runner_ || awaiting_opt_in_ ||
           !pending_invoke_ids_.empty();
  }

  // Returns a runner that tears this handler down when it goes out of scope.
  base::ScopedClosureRunner CreateCleanupRunner() {
    return base::ScopedClosureRunner(base::BindOnce(
        &GlicExperimentalTriggeringCoordinator::OnUpdatesHandlerCleanup,
        coordinator_, context_id_));
  }

  bool HandleUnavailableExperimentalTriggering(GlicKeyedService* glic_service) {
    auto state = glic_service->enabling().GetExperimentalTriggeringState();
    metric_reporter_.RecordEnumeration(
        "ExperimentalTriggering.StateOnActuationRequest", state);

    if (state == syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady) {
      return false;
    }

    DLOG(WARNING) << "Rejecting remote request: Glic not opted-in for "
                     "experimental triggering.";
    return true;
  }

  // On Android, a continuation can run without a browser window when its
  // conversation still has a live surface (e.g. the offscreen tab of a
  // background task): LastActiveOrNew resolves to that surface. Without one,
  // LastActiveOrNew falls back to NewTab{nullptr}, which fails asynchronously
  // with kTabClosed on Android, so the request is rejected up front instead.
  bool CanContinueWithoutBrowserWindow(
      const ExperimentalTriggeringRequest& request,
      const GlicKeyedService& glic_service) const {
#if BUILDFLAG(IS_ANDROID)
    if (!std::holds_alternative<ContinueActuationRequest>(request.payload) ||
        !request.task_metadata.has_value() ||
        request.task_metadata->conversation_id.empty()) {
      return false;
    }
    const GlicInstance* instance =
        glic_service.instance_coordinator().GetInstanceForConversationId(
            request.task_metadata->conversation_id);
    return instance && instance->HasLiveSurface();
#else
    return false;
#endif
  }
  void SubscribeForTriggeringUpdates(base::WeakPtr<GlicInstance> instance) {
    instance_ = std::move(instance);
    if (!instance_ || receiver_.is_bound()) {
      return;
    }
    auto* manager = instance_->GetExperimentalTriggeringManager();
    if (!manager) {
      DLOG(WARNING) << "GlicExperimentalTriggeringManager is not available.";
      HandleTerminalUpdate(
          TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
          GlicExperimentalTriggeringExecutionOutcome::
              kUpdatesRegistrationFailed,
          "GlicExperimentalTriggeringManager is not available.");
      return;
    }
    mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
    manager->GetExperimentalTriggeringUpdates(
        std::move(remote),
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::
                           OnUpdatesRegistrationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnUpdatesRegistrationComplete(bool success) {
    if (success || terminal_update_sent_ || is_cleaning_up_) {
      return;
    }
    auto* manager =
        instance_ ? instance_->GetExperimentalTriggeringManager() : nullptr;
    if ((!manager || !manager->IsClientBound()) && MaybeEnterReconnectGrace()) {
      return;
    }
    DLOG(WARNING) << "Failed to register experimental triggering updates "
                     "handler.";
    HandleTerminalUpdate(
        TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
        GlicExperimentalTriggeringExecutionOutcome::kUpdatesRegistrationFailed,
        "Failed to register experimental triggering updates handler.");
  }

  std::map<std::string, std::string> WithActuationDiagnosticMetadata(
      std::map<std::string, std::string> metadata) const {
    if (!is_actuation_started_) {
      return metadata;
    }
    metadata.insert_or_assign(std::string(kMetadataPanelOpenedKey),
                              panel_opened_ ? "1" : "0");
    metadata.insert_or_assign(std::string(kMetadataClientConnectedKey),
                              client_connected_ ? "1" : "0");
    if (time_to_panel_open_.has_value()) {
      metadata.insert_or_assign(std::string(kMetadataTimeToPanelOpenMsKey),
                                FormatMetadataDurationMs(*time_to_panel_open_));
    }
    if (time_to_client_connected_.has_value()) {
      metadata.insert_or_assign(
          std::string(kMetadataTimeToClientConnectedMsKey),
          FormatMetadataDurationMs(*time_to_client_connected_));
    }
    if (time_to_first_response_.has_value()) {
      metadata.insert_or_assign(
          std::string(kMetadataTimeToFirstResponseMsKey),
          FormatMetadataDurationMs(*time_to_first_response_));
    }
    if (!task_start_time_.is_null()) {
      metadata.insert_or_assign(
          std::string(kMetadataTimeToTerminalMsKey),
          FormatMetadataDurationMs(base::TimeTicks::Now() - task_start_time_));
    }
    return metadata;
  }

  // Sends the terminal update for the current turn, then ends the session.
  // COMPLETE, STOPPED and FAILED are terminal: after one, the task is over for
  // the server too, and nothing more arrives for it. So Chrome must not keep
  // any work running, or keep the handler, once it has sent one.
  void HandleTerminalUpdate(TaskUpdate::State state,
                            std::optional<TaskUpdate::DataType> data_type,
                            GlicExperimentalTriggeringExecutionOutcome outcome,
                            std::optional<std::string> data = std::nullopt,
                            std::map<std::string, std::string> metadata = {}) {
    DCHECK(TaskUpdate::IsTerminalState(state));
    if (terminal_update_sent_) {
      return;
    }
    CancelReconnectGrace();
    terminal_update_sent_ = true;
    RecordOutcome(outcome);
    if (outcome == GlicExperimentalTriggeringExecutionOutcome::kSuccess &&
        is_actuation_started_) {
      metric_reporter_.RecordLongTimes(
          "ExperimentalTriggering.Latency.ToTerminalCompletion",
          base::TimeTicks::Now() - task_start_time_);
    }
    // After COMPLETE the web client stops its actor task itself, with the
    // completion reason. Cancelling it here could record a successful task as
    // stopped by the user.
    StopOwnedLiveWork(/*cancel_actor_task=*/state !=
                      TaskUpdate::State::kComplete);
    CleanupAsync();
    // Send last: the update callback may end this session synchronously.
    SendTaskUpdateMessage(state, data_type, std::move(data),
                          WithActuationDiagnosticMetadata(std::move(metadata)));
  }

  // Stops the work that this handler started and that may still be running:
  // its direct action execution, and, if `cancel_actor_task`, the actor task
  // that was created after its trigger. Cancelling the actor task also ends an
  // invocation that waits for it. Must only be called once the task is over.
  void StopOwnedLiveWork(bool cancel_actor_task) {
    // The task is over, so it is no longer pending. Don't wait for the posted
    // cleanup, which is skipped if the coordinator goes away first.
    actuating_subscription_ = {};
    RemovePendingTask();
    actions_timeout_timer_.Stop();
    actions_runner_.reset();
    if (!instance_) {
      return;
    }
    if (cancel_actor_task && task_created_) {
      instance_->CancelTask();
    }
  }

  void OnPanelOpened() {
    if (panel_opened_) {
      return;
    }
    panel_opened_ = true;
    if (is_actuation_started_) {
      const base::TimeDelta elapsed = base::TimeTicks::Now() - task_start_time_;
      time_to_panel_open_ = elapsed;
      metric_reporter_.RecordMediumTimes(
          "ExperimentalTriggering.Latency.ToSidePanelOpened", elapsed);
    }
  }

  void OnClientConnected(base::WeakPtr<GlicInstance> instance) {
    if (!client_connected_) {
      client_connected_ = true;
      if (is_actuation_started_) {
        const base::TimeDelta elapsed =
            base::TimeTicks::Now() - task_start_time_;
        time_to_client_connected_ = elapsed;
        metric_reporter_.RecordMediumTimes(
            "ExperimentalTriggering.Latency.ToClientConnected", elapsed);
      }
    }
    SubscribeForTriggeringUpdates(std::move(instance));
  }

  void OnInvokeSuccess(int invoke_id) { pending_invoke_ids_.erase(invoke_id); }

  void OnInvokeError(int invoke_id,
                     GlicInvokeError error,
                     std::string diagnostic_info) {
    pending_invoke_ids_.erase(invoke_id);
    if (error == GlicInvokeError::kSuperseded) {
      // Handlers are shared per context id, so a continuation or reconnect
      // request invokes on the same handler that owns the in-flight invocation
      // it supersedes. If this handler has already started a newer invocation
      // (whether still pending or already succeeded), that newer invocation is
      // responsible for this handler, so it must not be torn down.
      if (!pending_invoke_ids_.empty() || invoke_id + 1 < next_invoke_id_) {
        return;
      }
      // Superseded by some other (non experimental triggering) invocation.
      // Preserve existing ExecutionOutcome semantics while still notifying the
      // remote caller so it does not stall waiting for a terminal update.
      outcome_recorded_ = true;
      HandleTerminalUpdate(TaskUpdate::State::kFailed,
                           TaskUpdate::DataType::kErrorMessage,
                           GlicInvokeErrorToOutcome(error, client_connected_),
                           base::StrCat({"Glic invocation failed: ",
                                         GlicInvokeErrorToString(error)}),
                           BuildInvokeErrorMetadata(error, diagnostic_info));
      return;
    }
    DLOG(WARNING) << "Glic invocation failed with error: "
                  << GlicInvokeErrorToString(error);
    HandleTerminalUpdate(TaskUpdate::State::kFailed,
                         TaskUpdate::DataType::kErrorMessage,
                         GlicInvokeErrorToOutcome(error, client_connected_),
                         base::StrCat({"Glic invocation failed: ",
                                       GlicInvokeErrorToString(error)}),
                         BuildInvokeErrorMetadata(error, diagnostic_info));
  }

  void OnDisconnect() {
    if (MaybeEnterReconnectGrace()) {
      return;
    }
    FailClientDisconnected();
  }

  void CancelReconnectGrace() {
    reconnect_grace_timer_.Stop();
    client_bound_subscription_ = {};
  }

  bool CanEnterReconnectGrace() const {
    return base::FeatureList::IsEnabled(
               features::kGlicExperimentalTriggeringNavigationReconnectGrace) &&
           is_actuation_started_ && !first_response_received_ &&
           !terminal_update_sent_ && !is_cleaning_up_ &&
           !reconnect_attempted_ && last_actuation_request_.has_value() &&
           instance_ && instance_->GetExperimentalTriggeringManager();
  }

  bool MaybeEnterReconnectGrace() {
    if (reconnect_grace_timer_.IsRunning()) {
      return true;
    }
    if (!CanEnterReconnectGrace()) {
      return false;
    }
    reconnect_attempted_ = true;
    receiver_.reset();
    auto* manager = instance_->GetExperimentalTriggeringManager();
    client_bound_subscription_ =
        manager->AddClientBoundCallback(base::BindRepeating(
            &ExperimentalTriggeringUpdatesHandler::OnClientRebound,
            weak_ptr_factory_.GetWeakPtr()));
    reconnect_grace_timer_.Start(
        FROM_HERE,
        features::kGlicExperimentalTriggeringNavigationReconnectGracePeriod
            .Get(),
        base::BindOnce(
            &ExperimentalTriggeringUpdatesHandler::OnReconnectGraceTimeout,
            weak_ptr_factory_.GetWeakPtr()));
    return true;
  }

  void StartInvocation(GlicKeyedService* glic_service,
                       GlicInvokeOptions options,
                       tabs::TabInterface* prepared_tab,
                       bool is_continuation,
                       bool observe_actor_task_creation) {
    options.on_panel_opened =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnPanelOpened,
                       weak_ptr_factory_.GetWeakPtr());
    options.on_client_connected =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnClientConnected,
                       weak_ptr_factory_.GetWeakPtr());
    const int invoke_id = next_invoke_id_++;
    pending_invoke_ids_.insert(invoke_id);
    options.on_success =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnInvokeSuccess,
                       weak_ptr_factory_.GetWeakPtr(), invoke_id);
    options.SetOnErrorWithDiagnostics(
        error_diagnostics_passkey_,
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnInvokeError,
                       weak_ptr_factory_.GetWeakPtr(), invoke_id));

    GlicInvokeWithAutoSubmitOptions auto_submit_options;
#if BUILDFLAG(IS_ANDROID)
    // TODO(b/568925496): Follow up to remove platform ifdefs across
    // experimental triggering.
    // The background check runs only once when Glic is invoked. Handling Chrome
    // moving from foreground to background mid-task would require observing
    // ApplicationStatusListener and dynamically updating offscreen rendering.
    const bool is_chrome_in_background = coordinator_->IsChromeInBackground();
    if ((prepared_tab || is_continuation) && is_chrome_in_background) {
      auto_submit_options.show_panel = false;
    }
#endif

    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    auto instance = glic_service->InvokeWithAutoSubmit(
        passkey_, std::move(options), std::move(auto_submit_options));
    if (weak_this) {
      if (instance || !weak_this->instance_) {
        weak_this->instance_ = std::move(instance);
      }
      if (observe_actor_task_creation) {
        weak_this->ObserveActorTaskCreation();
      }
    }
  }

  void OnClientRebound() {
    if (!reconnect_grace_timer_.IsRunning()) {
      return;
    }
    CancelReconnectGrace();
    if (!coordinator_ || !instance_ || terminal_update_sent_ ||
        is_cleaning_up_ || !last_actuation_request_.has_value()) {
      return;
    }
    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      FailClientDisconnected();
      return;
    }

    const bool is_continuation =
        std::holds_alternative<ContinueActuationRequest>(
            last_actuation_request_->payload);
    tabs::TabInterface* prepared_tab = nullptr;
    BrowserWindowInterface* browser_window = nullptr;
    if (last_prepared_tab_.has_value()) {
      prepared_tab = last_prepared_tab_->get();
      if (!prepared_tab) {
        FailPreparedTabClosed();
        return;
      }
    } else {
      browser_window = coordinator_->GetBrowserWindow();
      if (!browser_window && !CanContinueWithoutBrowserWindow(
                                 *last_actuation_request_, *glic_service)) {
        FailClientDisconnected();
        return;
      }
    }

    // Bind updates to the reloaded guest immediately so early updates are not
    // missed while the superseded invocation re-delivers the prompt.
    SubscribeForTriggeringUpdates(instance_);
    if (terminal_update_sent_ || is_cleaning_up_) {
      return;
    }

    GlicInvokeOptions options = CreateInvokeOptions(
        *last_actuation_request_, browser_window, prepared_tab);
    options.target.conversation = instance_->id();
    options.supersede_if_in_progress = true;
    StartInvocation(glic_service, std::move(options), prepared_tab,
                    is_continuation,
                    /*observe_actor_task_creation=*/false);
  }

  void OnReconnectGraceTimeout() { FailClientDisconnected(); }

  void FailClientDisconnected() {
    CancelReconnectGrace();
    HandleTerminalUpdate(
        TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
        GlicExperimentalTriggeringExecutionOutcome::
            kClientDisconnectedBeforeResponse,
        "Client disconnected unexpectedly.",
        BuildChromeErrorMetadata(kErrorCodeGlicClientDisconnected));
  }

  // The prepared target tab closed while waiting for the reloaded guest to
  // reconnect. This is a tab closure, not a client disconnect: report it the
  // same way GlicInvokeHandler reports GlicInvokeError::kTabClosed.
  void FailPreparedTabClosed() {
    CancelReconnectGrace();
    HandleTerminalUpdate(
        TaskUpdate::State::kFailed, TaskUpdate::DataType::kErrorMessage,
        GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorTabClosed,
        "Target tab closed before completion.",
        BuildChromeErrorMetadata(kErrorCodeGlicTabClosed));
  }

  void CleanupAsync() {
    if (is_cleaning_up_) {
      return;
    }
    is_cleaning_up_ = true;
    CancelReconnectGrace();
    receiver_.reset();
    if (coordinator_) {
      // Only erase this handler. If it is already gone, for example because
      // this runs from the destructor, the entry for `context_id_` may belong
      // to a newer handler.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<GlicExperimentalTriggeringCoordinator>
                     coordinator,
                 base::WeakPtr<ExperimentalTriggeringUpdatesHandler> handler,
                 std::string context_id) {
                if (coordinator && handler) {
                  coordinator->OnUpdatesHandlerCleanup(context_id);
                }
              },
              coordinator_, weak_ptr_factory_.GetWeakPtr(), context_id_));
    }
  }

  void RecordOutcome(GlicExperimentalTriggeringExecutionOutcome outcome) {
    if (outcome_recorded_) {
      return;
    }
    outcome_recorded_ = true;
    metric_reporter_.RecordEnumeration(
        "ExperimentalTriggering.ExecutionOutcome", outcome);
    if (is_actuation_started_) {
      metric_reporter_.RecordBoolean("ExperimentalTriggering.SidePanelOpened",
                                     panel_opened_);
      metric_reporter_.RecordBoolean("ExperimentalTriggering.ClientConnected",
                                     client_connected_);
    }
  }

  std::optional<ExperimentalTriggeringResponse>
  ProcessTriggerOrContinueActuationRequest(
      const ExperimentalTriggeringRequest& request,
      tabs::TabInterface* prepared_tab,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Message handler is no longer available.",
          request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
          sequence_generator_.GetNext());
    }

    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kGlicServiceUnavailable);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "GlicKeyedService is not available.",
          request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
          sequence_generator_.GetNext());
    }

    if (HandleUnavailableExperimentalTriggering(glic_service)) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "User is not opted in to experimental triggering.",
          request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
          sequence_generator_.GetNext(),
          BuildUnavailableTriggeringMetadata(glic_service->enabling()));
    }

    // Mirrors the guard in ProcessExecuteActionsRequest. Handlers are shared
    // per context id, and completing a direct action execution tears the
    // handler down, so starting an actuation session on top of one would drop
    // every subsequent update for that session.
    if (actions_runner_) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Direct action execution is already in progress for this context.",
          request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
          sequence_generator_.GetNext());
    }

    const bool is_continuation =
        std::holds_alternative<ContinueActuationRequest>(request.payload);
    BrowserWindowInterface* browser_window = nullptr;
    if (!prepared_tab) {
      browser_window = coordinator_->GetBrowserWindow();
      if (!browser_window &&
          !CanContinueWithoutBrowserWindow(request, *glic_service)) {
        if (base::FeatureList::IsEnabled(
                features::kGlicExperimentalTriggeringOpenWindowIfNone)) {
#if !BUILDFLAG(IS_ANDROID)
          // TODO(ritikagup): Align window/tab creation lifecycle across
          // platforms. Desktop initializes the target surface synchronously
          // during request processing, whereas Android triggers an async
          // foreground service that sets up the background tab later in
          // different methods.
          browser_window = chrome::OpenEmptyWindow(coordinator_->profile_);
#endif
        }
        if (!browser_window) {
          result_logger.set_result(
              GlicExperimentalTriggeringIncomingMessageResult::
                  kNoBrowserWindow);
          return CreateResponseMessage(
              context_id_, TaskUpdate::State::kFailed,
              TaskUpdate::DataType::kErrorMessage,
              "No browser window found for current profile.",
              request.task_metadata.has_value() ? &*request.task_metadata
                                                : nullptr,
              sequence_generator_.GetNext(),
              BuildChromeErrorMetadata(kErrorCodeEtNoBrowserWindow));
        }
      }
    }

    GlicInvokeOptions options =
        CreateInvokeOptions(request, browser_window, prepared_tab);
    if (!is_continuation && pending_invoke_ids_.empty()) {
      panel_opened_ = false;
      client_connected_ = false;
      time_to_panel_open_.reset();
      time_to_client_connected_.reset();
      time_to_first_response_.reset();
      task_start_time_ = base::TimeTicks::Now();
    } else if (task_start_time_.is_null()) {
      task_start_time_ = base::TimeTicks::Now();
    }
    CancelReconnectGrace();
    reconnect_attempted_ = false;
    last_actuation_request_ = request;
    if (prepared_tab) {
      last_prepared_tab_ = prepared_tab->GetWeakPtr();
    } else {
      last_prepared_tab_.reset();
    }
    turn_start_time_ = base::TimeTicks::Now();
    is_actuation_started_ = true;
    first_response_received_ = false;
    terminal_update_sent_ = false;
    outcome_recorded_ = false;

    auto response = CreateResponseMessage(
        context_id_, TaskUpdate::State::kStarting, std::nullopt, "",
        request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
        sequence_generator_.GetNext());
    // The invocation reports back through this handler, so keep it alive.
    std::ignore = cleanup_runner.Release();

    StartInvocation(glic_service, std::move(options), prepared_tab,
                    is_continuation,
                    /*observe_actor_task_creation=*/!is_continuation);

    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    return response;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessGetScreenshotRequest(
      const GetScreenshotRequest& screenshot_req,
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    // Like `ProcessSubmitConfirmation`, failures are reported as a
    // `ScreenshotResult` rather than a failed `TaskUpdate`: being unable to
    // capture a screenshot says nothing about the health of the actuation task,
    // so it must not end the task.
    auto create_error_response = [&](ScreenshotResult::Status status,
                                     std::string_view error_message) {
      DLOG(WARNING) << error_message;
      return CreateScreenshotResultResponse(
          context_id_,
          CreateScreenshotResult(status, /*file_token=*/std::string_view(),
                                 screenshot_req.request_token),
          task_metadata, sequence_generator_.GetNext());
    };

    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return create_error_response(ScreenshotResult::Status::kErrorCapture,
                                   "Message handler is no longer available.");
    }

    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kGlicServiceUnavailable);
      return create_error_response(ScreenshotResult::Status::kErrorCapture,
                                   "GlicKeyedService is not available.");
    }

    if (HandleUnavailableExperimentalTriggering(glic_service)) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn);
      return create_error_response(
          ScreenshotResult::Status::kErrorDisabled,
          "User is not opted in to experimental triggering.");
    }

    if (!base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggeringScreenshot)) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kUnexpectedRequestPayload);
      return create_error_response(ScreenshotResult::Status::kErrorDisabled,
                                   "Screenshot feature is disabled.");
    }

    if (screenshot_req.request_token.empty()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kUnexpectedRequestPayload);
      return create_error_response(
          ScreenshotResult::Status::kErrorInvalidRequest,
          "Received a screenshot request with an empty request token.");
    }

    GlicInstance* instance = instance_.get();
    if (!instance) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      return create_error_response(
          ScreenshotResult::Status::kErrorCapture,
          "No active Glic instance available for screenshot.");
    }

    GlicExperimentalTriggeringManager* triggering_manager =
        instance->GetExperimentalTriggeringManager();
    if (!triggering_manager) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kTriggeringManagerUnavailable);
      return create_error_response(
          ScreenshotResult::Status::kErrorCapture,
          "GlicExperimentalTriggeringManager is not available.");
    }

    triggering_manager->CaptureAndUploadEncryptedScreenshot(
        screenshot_req.public_key, screenshot_req.auth_secret,
        base::BindOnce(
            [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> handler,
               std::vector<uint8_t> request_token,
               base::expected<std::string, ScreenshotResult::Status> result) {
              if (!handler) {
                return;
              }
              if (!result.has_value() || result.value().empty()) {
                ScreenshotResult::Status status =
                    !result.has_value()
                        ? result.error()
                        : ScreenshotResult::Status::kErrorServer;
                handler->SendScreenshotResult(status,
                                              /*file_token=*/std::string_view(),
                                              std::move(request_token));
                return;
              }
              handler->SendScreenshotResult(ScreenshotResult::Status::kSuccess,
                                            result.value(),
                                            std::move(request_token));
            },
            weak_ptr_factory_.GetWeakPtr(), screenshot_req.request_token));

    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    // No synchronous reply: the `ScreenshotResult` above is the only meaningful
    // response, and emitting a task update here would imply a task state
    // transition that did not happen.
    return std::nullopt;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessSubmitConfirmation(
      const SubmitConfirmation& confirmation_req,
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    // Unlike actuation requests, delivery failures are reported as a
    // `ConfirmationResult` rather than a failed `TaskUpdate`. A confirmation
    // that could not be delivered says nothing about the health of the
    // actuation task, so surfacing it as a task failure would be misleading.
    auto fail = [&](ConfirmationResult::Status status,
                    const std::string& error_message) {
      DLOG(WARNING) << error_message;
      return CreateConfirmationResultResponse(
          context_id_, CreateConfirmationResult(status, error_message),
          task_metadata, sequence_generator_.GetNext());
    };

    if (confirmation_req.confirmation_response.empty()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kUnexpectedRequestPayload);
      return fail(ConfirmationResult::Status::kErrorInvalidRequest,
                  "Received a confirmation with an empty response payload.");
    }

    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return fail(ConfirmationResult::Status::kErrorUnavailable,
                  "Message handler is no longer available.");
    }

    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kGlicServiceUnavailable);
      return fail(ConfirmationResult::Status::kErrorUnavailable,
                  "GlicKeyedService is not available.");
    }

    if (HandleUnavailableExperimentalTriggering(glic_service)) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn);
      return fail(ConfirmationResult::Status::kErrorUnavailable,
                  "User is not opted in to experimental triggering.");
    }

    GlicInstance* instance = instance_.get();
    if (!instance) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      return fail(
          ConfirmationResult::Status::kErrorUnavailable,
          "No active Glic instance available to receive the confirmation.");
    }

    GlicExperimentalTriggeringManager* triggering_manager =
        instance->GetExperimentalTriggeringManager();
    if (!triggering_manager) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kTriggeringManagerUnavailable);
      return fail(ConfirmationResult::Status::kErrorUnavailable,
                  "GlicExperimentalTriggeringManager is not available.");
    }

    triggering_manager->SubmitConfirmationResponse(
        confirmation_req.confirmation_response,
        base::BindOnce(
            [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> handler,
               bool accepted) {
              if (!handler) {
                return;
              }
              handler->SendConfirmationResult(
                  accepted ? ConfirmationResult::Status::kApplied
                           : ConfirmationResult::Status::kNotApplied);
            },
            weak_ptr_factory_.GetWeakPtr()));

    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    // No synchronous reply: the `ConfirmationResult` above is the only
    // meaningful response, and emitting a task update here would imply a task
    // state transition that did not happen.
    return std::nullopt;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessTaskMetadataUpdated(
      const ExperimentalTriggeringRequest& request,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    if (!instance_) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      return std::nullopt;
    }
    if (request.task_metadata.has_value() &&
        request.task_metadata->parent_conversation_metadata.has_value()) {
      const auto& parent_metadata =
          *request.task_metadata->parent_conversation_metadata;
      auto context = mojom::AdditionalContext::New();
      context->source = mojom::AdditionalContextSource::kExperimentalTriggering;
      auto parent_conversation = mojom::ParentConversationMetadata::New();
      if (!parent_metadata.conversation_id.empty()) {
        parent_conversation->conversation_id = parent_metadata.conversation_id;
      }
      if (!parent_metadata.conversation_title.empty()) {
        parent_conversation->conversation_title =
            parent_metadata.conversation_title;
      }
      context->parts.push_back(
          mojom::AdditionalContextPart::NewParentConversationMetadata(
              std::move(parent_conversation)));
      instance_->SendAdditionalContext(std::move(context));
    }
    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    return std::nullopt;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessStopActuationRequest(
      const TaskMetadata* request_metadata,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    // Stopping ends the session, so this tears the handler down even when it
    // has live work and OnRequest() left `cleanup_runner` unarmed.
    if (!cleanup_runner) {
      cleanup_runner = CreateCleanupRunner();
    }
    CancelReconnectGrace();

    std::optional<ExperimentalTriggeringResponse> response;
    if (actions_runner_) {
      actions_timeout_timer_.Stop();
      actions_runner_.reset();
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
      response = CreateResponseMessage(context_id_, TaskUpdate::State::kStopped,
                                       std::nullopt, "", request_metadata,
                                       sequence_generator_.GetNext());
    } else if (!instance_) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      response = CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Failed to stop task due to missing glic instance.", request_metadata,
          sequence_generator_.GetNext(), WithActuationDiagnosticMetadata({}));
    } else {
      instance_->GetActorTaskManager()->CancelTask();
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
      response = CreateResponseMessage(context_id_, TaskUpdate::State::kStopped,
                                       std::nullopt, "", request_metadata,
                                       sequence_generator_.GetNext(),
                                       WithActuationDiagnosticMetadata({}));
    }

    // If task is not yet started by glic session or is in a pending state,
    // clean up any pending state in actor related to the context.
    if (coordinator_) {
      if (auto* actor_service =
              actor::ActorKeyedService::Get(coordinator_->profile_)) {
        actor_service->OnMessageTriggerTaskStopped(context_id_);
      }
    }

    return response;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessExecuteActionsRequest(
      const ExecuteActionsRequest& request,
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    if (!base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggeringScriptTools) ||
        !base::FeatureList::IsEnabled(actor::kGlicActorEnableScriptTools)) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kScriptToolsDisabled);
      return CreateResponseMessage(context_id_, TaskUpdate::State::kFailed,
                                   TaskUpdate::DataType::kErrorMessage,
                                   "Script tool execution is not enabled.",
                                   task_metadata,
                                   sequence_generator_.GetNext());
    }

    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return CreateResponseMessage(context_id_, TaskUpdate::State::kFailed,
                                   TaskUpdate::DataType::kErrorMessage,
                                   "Message handler is no longer available.",
                                   task_metadata,
                                   sequence_generator_.GetNext());
    }

    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kGlicServiceUnavailable);
      return CreateResponseMessage(context_id_, TaskUpdate::State::kFailed,
                                   TaskUpdate::DataType::kErrorMessage,
                                   "GlicKeyedService is not available.",
                                   task_metadata,
                                   sequence_generator_.GetNext());
    }

    if (HandleUnavailableExperimentalTriggering(glic_service)) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "User is not opted in to experimental triggering.", task_metadata,
          sequence_generator_.GetNext(),
          BuildUnavailableTriggeringMetadata(glic_service->enabling()));
    }

    if (actions_runner_ || instance_) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Another task is already executing for this context.", task_metadata,
          sequence_generator_.GetNext());
    }

    if (!actor::ValidateActionsAreScriptTools(request.actions)) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kNonScriptToolAction);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "ExecuteActions contained non-ScriptTool actions.", task_metadata,
          sequence_generator_.GetNext());
    }

    // Actions delivered here originate from the server rather than from a
    // trusted in-browser feature, so this has to apply the same bar as the
    // regular actuation flow, which refuses to start a task unless the profile
    // may act on the web at all. Per-URL blocking is enforced separately by the
    // policy checker handed to ActorActionsRunner below; a policy configuration
    // with a URL allowlist leaves CanActOnWeb() true precisely so that
    // per-URL evaluation gets to make the decision. A profile without a policy
    // checker has no actor service backing it, so it cannot act either.
    if (!CanActOnWeb(*glic_service)) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kCannotActOnWeb);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Acting on the web is not allowed for this profile.", task_metadata,
          sequence_generator_.GetNext(),
          BuildChromeErrorMetadata(kErrorCodeEtCannotActOnWeb));
    }

    actor::TaskSourceInfo source_info(actor::TaskSourceInfo::Client::kGlic,
                                      context_id_);

    // The only thing this flow needs to return to the server is the result of
    // the script tools themselves, so disable everything else. Skipping the
    // async observation collection avoids paying for page context we would
    // throw away, and clearing the screenshot options makes sure this cannot be
    // used as an alternate way to obtain a screenshot: those have to go through
    // GetScreenshotRequest, which is separately gated on
    // kGlicExperimentalTriggeringScreenshot, is limited to tabs the task has
    // acted on, and only ever returns an encrypted, out-of-band uploaded image.
    optimization_guide::proto::Actions sanitized_actions = request.actions;
    sanitized_actions.set_skip_async_observation_collection(true);
    sanitized_actions.clear_screenshot_options();

    if (!actor::PopulateTabIdsForScriptToolActions(
            *coordinator_->profile_, sanitized_actions,
            coordinator_->GetActiveTab())) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kTabNotFound);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Target tab for document identifier could not be found.",
          task_metadata, sequence_generator_.GetNext());
    }

    std::string conversation_id =
        task_metadata ? task_metadata->conversation_id : "";
    std::string task_id = task_metadata ? task_metadata->task_id : "";

    // Passing a raw pointer is safe: the policy checker is owned by the
    // profile's GlicKeyedService, and the runner only consults it while its
    // task is running, which it stops from its destructor.
    actions_runner_ = std::make_unique<actor::ActorActionsRunner>(
        *coordinator_->profile_, std::move(source_info),
        std::move(sanitized_actions),
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnActionsComplete,
                       weak_ptr_factory_.GetWeakPtr(), conversation_id,
                       task_id),
        /*tab_id=*/0, &glic_service->actor_policy_checker());
    actions_runner_->Start();

    // TODO(crbug.com/562073719): Move timeout handling into ActorActionsRunner
    // directly so individual callsites do not need to manage their own timers.
    actions_timeout_timer_.Start(
        FROM_HERE, kGlicExperimentalTriggeringScriptToolsTimeout.Get(),
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnActionsTimeout,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(conversation_id), std::move(task_id)));

    // The execution reports back through this handler, so keep it alive.
    std::ignore = cleanup_runner.Release();
    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    return std::nullopt;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessDeviceOptInRequest(
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner& cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return CreateDeviceOptInResponse(context_id_, DeviceOptInResult::kFailed,
                                       task_metadata,
                                       sequence_generator_.GetNext());
    }

#if BUILDFLAG(IS_ANDROID)
    if (coordinator_->IsChromeInBackground()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kAndroidOptInUnsupported);
      // Chrome isn't in the foreground, so the opt-in UI can't be shown.
      // Reply with a device opt-in result so the server stops waiting.
      return CreateDeviceOptInResponse(context_id_, DeviceOptInResult::kFailed,
                                       task_metadata,
                                       sequence_generator_.GetNext());
    }
#endif

    GlicKeyedService* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(coordinator_->profile_,
                                                     /*create=*/false);
    if (!glic_service) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kGlicServiceUnavailable);
      return CreateDeviceOptInResponse(context_id_, DeviceOptInResult::kFailed,
                                       task_metadata,
                                       sequence_generator_.GetNext());
    }

    content::WebContents* web_contents = nullptr;
    if (base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggeringOptInTabFocus)) {
      web_contents =
          glic_service->opt_in_controller().GetOrCreateSuitableWebContents();
    }
    if (!web_contents) {
      if (tabs::TabInterface* active_tab = coordinator_->GetActiveTab()) {
        web_contents = active_tab->GetContents();
      }
    }

    if (!web_contents) {
      DLOG(ERROR) << "No target web contents found or created for "
                     "GlicExperimentalTriggering";
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kNoWebContentsForOptIn);
      return CreateDeviceOptInResponse(context_id_, DeviceOptInResult::kFailed,
                                       task_metadata,
                                       sequence_generator_.GetNext());
    }

    auto callback = base::BindOnce(
        &ExperimentalTriggeringUpdatesHandler::SendDeviceOptInResult,
        weak_ptr_factory_.GetWeakPtr());

    // Set before ShowDialog(), which may answer right away.
    awaiting_opt_in_ = true;
    glic_service->opt_in_controller().ShowDialog(web_contents,
                                                 std::move(callback));

    // The dialog reports back through SendDeviceOptInResult(), so keep the
    // handler alive until then.
    std::ignore = cleanup_runner.Release();

    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    return std::nullopt;
  }

  void SendTaskUpdateMessage(
      TaskUpdate::State state,
      std::optional<TaskUpdate::DataType> data_type = std::nullopt,
      std::optional<std::string> data = std::nullopt,
      std::map<std::string, std::string> metadata = {}) {
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get());
      TaskUpdate task_update;
      task_update.state = state;
      task_update.data_type = data_type;
      task_update.data = data.value_or("");
      task_update.metadata = std::move(metadata);
      response.task_update = std::move(task_update);
      update_callback_.Run(std::move(response));
    }
  }

  void SendDeviceOptInResult(bool accepted) {
    // A resent DeviceOptInRequest adds a second callback for the same dialog.
    // Send only the first answer.
    if (!awaiting_opt_in_) {
      return;
    }
    awaiting_opt_in_ = false;
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get());
      response.device_opt_in_result = accepted ? DeviceOptInResult::kAccepted
                                               : DeviceOptInResult::kDeclined;
      update_callback_.Run(std::move(response));
    }
    // A session that shares this context id still needs the handler.
    if (coordinator_ && !HasLiveWork()) {
      coordinator_->OnUpdatesHandlerCleanup(context_id_);
    }
  }

  void SendScreenshotResult(ScreenshotResult::Status status,
                            std::string_view file_token = {},
                            std::vector<uint8_t> request_token = {}) {
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get());
      response.screenshot_result =
          CreateScreenshotResult(status, file_token, std::move(request_token));
      update_callback_.Run(std::move(response));
    }
  }

  void OnActionsComplete(std::string conversation_id, std::string task_id) {
    if (!actions_runner_) {
      return;
    }
    actions_timeout_timer_.Stop();
    // Defer completion handling so that ActorActionsRunner::Finish() can unwind
    // cleanly before the runner and updates handler are destroyed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExperimentalTriggeringUpdatesHandler::HandleActionsComplete,
            weak_ptr_factory_.GetWeakPtr(), std::move(conversation_id),
            std::move(task_id)));
  }

  void HandleActionsComplete(std::string conversation_id, std::string task_id) {
    if (!actions_runner_) {
      return;
    }
    actions_timeout_timer_.Stop();
    std::unique_ptr<optimization_guide::proto::ActionsResult> result =
        actions_runner_->TakeResult();
    actions_runner_.reset();
    if (!result) {
      result = std::make_unique<optimization_guide::proto::ActionsResult>(
          actor::BuildErrorActionsResult(
              actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt));
    }
    SendExecuteActionsResponse(std::move(*result), std::move(conversation_id),
                               std::move(task_id));
  }

  void OnActionsTimeout(std::string conversation_id, std::string task_id) {
    if (!actions_runner_) {
      return;
    }
    actions_runner_.reset();
    SendExecuteActionsResponse(
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kToolTimeout, std::nullopt),
        std::move(conversation_id), std::move(task_id));
  }

  void SendExecuteActionsResponse(
      optimization_guide::proto::ActionsResult actions_result,
      std::string conversation_id,
      std::string task_id) {
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get(),
                             conversation_id, task_id);
      ExecuteActionsResponse exec_response;
      exec_response.actions_result = std::move(actions_result);
      response.execute_actions_response = std::move(exec_response);
      update_callback_.Run(std::move(response));
    }
    // The guards in ProcessExecuteActionsRequest and
    // ProcessTriggerOrContinueActuationRequest should keep these two flows from
    // overlapping, but if they ever did, tearing the handler down here would
    // silently disconnect the actuation session.
    if (coordinator_ && !instance_) {
      coordinator_->OnUpdatesHandlerCleanup(context_id_);
    }
  }

  void SendConfirmationResult(ConfirmationResult::Status status,
                              std::string error_message = {}) {
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get());
      response.confirmation_result =
          CreateConfirmationResult(status, std::move(error_message));
      update_callback_.Run(std::move(response));
    }
  }

  std::string context_id_;
  InvokeWithAutoSubmitPasskey passkey_;
  InvokeErrorDiagnosticsPasskey error_diagnostics_passkey_;
  base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtr<GlicInstance> instance_;
  base::CallbackListSubscription actuating_subscription_;
  std::unique_ptr<actor::ActorActionsRunner> actions_runner_;
  base::OneShotTimer actions_timeout_timer_;
  mojo::Receiver<mojom::ExperimentalTriggeringUpdatesHandler> receiver_;
  base::AtomicSequenceNumber sequence_generator_;

  std::optional<int64_t> last_seen_sequence_number_;
  GlicExperimentalTriggeringUpdateCallback update_callback_;
  bool terminal_update_sent_ = false;
  bool is_actuation_started_ = false;
  bool panel_opened_ = false;
  bool client_connected_ = false;
  // True once an actor task was created on `instance_` after this handler's
  // trigger. There is no direct link between an actor task and the
  // invocation that led to it, so the first one created afterwards is
  // treated as this handler's, like WaitForActuationTask does.
  bool task_created_ = false;
  bool first_response_received_ = false;
  bool outcome_recorded_ = false;
  bool is_cleaning_up_ = false;
  bool reconnect_attempted_ = false;
  std::optional<ExperimentalTriggeringRequest> last_actuation_request_;
  std::optional<base::WeakPtr<tabs::TabInterface>> last_prepared_tab_;
  base::OneShotTimer reconnect_grace_timer_;
  base::CallbackListSubscription client_bound_subscription_;
  // Whether a device opt-in dialog is open and its result not yet sent.
  bool awaiting_opt_in_ = false;
  // Ids of invocations started by this handler that have neither succeeded nor
  // failed yet.
  base::flat_set<int> pending_invoke_ids_;
  int next_invoke_id_ = 0;
  base::TimeTicks task_start_time_;
  base::TimeTicks turn_start_time_;
  std::optional<base::TimeDelta> time_to_panel_open_;
  std::optional<base::TimeDelta> time_to_client_connected_;
  std::optional<base::TimeDelta> time_to_first_response_;

  GlicMetricReporter metric_reporter_;

  base::WeakPtrFactory<ExperimentalTriggeringUpdatesHandler> weak_ptr_factory_{
      this};
};

struct GlicExperimentalTriggeringCoordinator::PendingMessageData {
  components_sharing_message::GlicExperimentalTriggering proto;
  ScopedIncomingMessageResultLogger result_logger;
  GlicExperimentalTriggeringUpdateCallback update_callback;
  GlicExperimentalTriggeringResponseCallback response_callback;
  base::TimeTicks queued_time = base::TimeTicks::Now();

  PendingMessageData(
      components_sharing_message::GlicExperimentalTriggering proto,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      GlicExperimentalTriggeringResponseCallback response_callback)
      : proto(std::move(proto)),
        result_logger(std::move(result_logger)),
        update_callback(std::move(update_callback)),
        response_callback(std::move(response_callback)) {}
  PendingMessageData(PendingMessageData&&) = default;
  PendingMessageData& operator=(PendingMessageData&&) = default;
  ~PendingMessageData() = default;
};

GlicExperimentalTriggeringCoordinator::GlicExperimentalTriggeringCoordinator(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

GlicExperimentalTriggeringCoordinator::
    ~GlicExperimentalTriggeringCoordinator() {
  CancelAllPendingMessages();
  // Clear handlers before `weak_ptr_factory_` is destroyed so their destructors
  // can still access `coordinator_` to remove pending tasks.
  context_id_to_updates_handler_map_.clear();
}

BrowserWindowInterface*
GlicExperimentalTriggeringCoordinator::GetBrowserWindow() const {
  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&browser, this](BrowserWindowInterface* b) {
        if (b->GetProfile() == profile_ &&
            b->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            !b->IsDeleteScheduled()) {
          browser = b;
          return false;  // Stop iteration
        }
        return true;  // Continue
      });
  return browser;
}

tabs::TabInterface* GlicExperimentalTriggeringCoordinator::GetActiveTab()
    const {
  BrowserWindowInterface* browser = GetBrowserWindow();
  return browser ? TabListInterface::From(browser)->GetActiveTab() : nullptr;
}

bool GlicExperimentalTriggeringCoordinator::IsChromeInBackground() const {
#if BUILDFLAG(IS_ANDROID)
  return !base::android::ApplicationStatusListener::HasVisibleActivities();
#else
  return false;
#endif
}

void GlicExperimentalTriggeringCoordinator::OnProtoMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto,
    ScopedIncomingMessageResultLogger result_logger,
    GlicExperimentalTriggeringUpdateCallback update_callback,
    GlicExperimentalTriggeringResponseCallback response_callback) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  LogGlicExperimentalTriggeringProto(
      actor_service, "GlicExperimentalTriggering", context_id, proto);

  GlicKeyedService* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false);
  std::optional<int> local_version =
      glic_service ? glic_service->enabling().GetExperimentalTriggeringVersion()
                   : std::nullopt;
  if (proto.has_glic_experimental_triggering_version() &&
      (!local_version.has_value() ||
       proto.glic_experimental_triggering_version() > *local_version)) {
    std::map<std::string, std::string> metadata;
    if (!local_version.has_value() && glic_service &&
        proto.glic_experimental_triggering_version() <=
            GlicEnabling::kExperimentalTriggeringProtocolVersion) {
      metadata = BuildUnavailableTriggeringMetadata(glic_service->enabling());
    }
    RejectProtoMessage(context_id, proto, result_logger,
                       GlicExperimentalTriggeringIncomingMessageResult::
                           kVersionMismatchOrUnavailable,
                       "Rejected: version mismatch or unavailable.",
                       std::move(response_callback), std::move(metadata));
    return;
  }

  const bool has_valid_request =
      proto.has_request() &&
      proto.request().payload_case() !=
          components_sharing_message::GlicExperimentalTriggering::
              ExperimentalTriggeringRequest::PAYLOAD_NOT_SET;
  if (!has_valid_request && !proto.has_task_metadata_updated()) {
    RejectProtoMessage(
        context_id, proto, result_logger,
        GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload,
        "Received GlicExperimentalTriggering message with no request "
        "payload.",
        std::move(response_callback));
    return;
  }

  if (!HasUpdatesHandler(context_id) &&
      !pending_messages_.contains(context_id)) {
    GlicMetricReporter reporter(profile_);
    MaybeRecordInitialSharingMessageDeliveryLatency(reporter, proto);
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kGlicBackgroundTriggering)) {
    if (proto.has_request() && proto.request().has_stop_actuation_request()) {
      if (actor_service) {
        actor_service->RemovePendingTask(context_id);
      }
      FailPendingMessages(
          context_id, TakePendingMessages(context_id),
          GlicExperimentalTriggeringIncomingMessageResult::kNoBrowserWindow,
          "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency."
          "Cancelled",
          "Message cancelled by stop actuation request.");
    } else if (auto it = pending_messages_.find(context_id);
               it != pending_messages_.end()) {
      it->second.emplace_back(proto, std::move(result_logger),
                              std::move(update_callback),
                              std::move(response_callback));
      return;
    } else if (!HasUpdatesHandler(context_id) && proto.has_request() &&
               proto.request().has_trigger_actuation_request() &&
               actor_service && glic_service &&
               glic_service->enabling().GetExperimentalTriggeringState() ==
                   syncer::DeviceInfo::GlicExperimentalTriggeringState::
                       kReady) {
      if (!actor_service_observation_.IsObserving()) {
        actor_service_observation_.Observe(actor_service);
      }
      pending_messages_[context_id].emplace_back(
          proto, std::move(result_logger), std::move(update_callback),
          std::move(response_callback));
      actor_service->AddPendingTask(context_id);
      return;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::move(response_callback)
      .Run(ProcessValidatedProtoMessage(
          context_id, proto, std::move(result_logger),
          std::move(update_callback), /*prepared_tab=*/nullptr));
}

std::optional<ExperimentalTriggeringResponse>
GlicExperimentalTriggeringCoordinator::ProcessValidatedProtoMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto,
    ScopedIncomingMessageResultLogger result_logger,
    GlicExperimentalTriggeringUpdateCallback update_callback,
    tabs::TabInterface* prepared_tab) {
  ExperimentalTriggeringRequest domain_request = ProtoToRequest(proto);
  domain_request.context_id = context_id;

  return OnRequest(context_id, domain_request, std::move(result_logger),
                   std::move(update_callback), prepared_tab);
}

void GlicExperimentalTriggeringCoordinator::OnBackgroundTabPrepared(
    tabs::TabInterface* tab,
    const std::string& context_id) {
  std::vector<PendingMessageData> messages = TakePendingMessages(context_id);
  base::WeakPtr<GlicExperimentalTriggeringCoordinator> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::WeakPtr<tabs::TabInterface> weak_tab =
      tab ? tab->GetWeakPtr() : nullptr;
  for (PendingMessageData& message_data : messages) {
    if (!weak_this) {
      return;
    }
    // Justification: Experimental triggering background setup latency is
    // profile-scoped and recorded outside a GlicInstance session.
    base::UmaHistogramMediumTimes(
        "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Success",
        base::TimeTicks::Now() - message_data.queued_time);
    const bool is_trigger =
        message_data.proto.has_request() &&
        message_data.proto.request().has_trigger_actuation_request();
    std::move(message_data.response_callback)
        .Run(ProcessValidatedProtoMessage(
            context_id, message_data.proto,
            std::move(message_data.result_logger),
            std::move(message_data.update_callback),
            is_trigger ? weak_tab.get() : nullptr));
  }
}

void GlicExperimentalTriggeringCoordinator::OnBackgroundSetupFailed(
    const std::string& context_id) {
  std::vector<PendingMessageData> messages = TakePendingMessages(context_id);
  if (messages.empty()) {
    return;
  }
  if (actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(profile_)) {
    actor_service->RemovePendingTask(context_id);
  }
  FailPendingMessages(
      context_id, std::move(messages),
      GlicExperimentalTriggeringIncomingMessageResult::kNoBrowserWindow,
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Failed",
      "Background setup failed.");
}

std::vector<GlicExperimentalTriggeringCoordinator::PendingMessageData>
GlicExperimentalTriggeringCoordinator::TakePendingMessages(
    const std::string& context_id) {
  auto it = pending_messages_.find(context_id);
  if (it == pending_messages_.end()) {
    return {};
  }
  std::vector<PendingMessageData> messages = std::move(it->second);
  pending_messages_.erase(it);
  if (pending_messages_.empty()) {
    actor_service_observation_.Reset();
  }
  return messages;
}

void GlicExperimentalTriggeringCoordinator::FailPendingMessages(
    const std::string& context_id,
    std::vector<PendingMessageData> messages,
    GlicExperimentalTriggeringIncomingMessageResult result,
    std::string_view latency_histogram_name,
    const std::string& error_message) {
  for (PendingMessageData& message_data : messages) {
    // Justification: Experimental triggering background setup latency is
    // profile-scoped and recorded outside a GlicInstance session.
    base::UmaHistogramMediumTimes(
        latency_histogram_name,
        base::TimeTicks::Now() - message_data.queued_time);
    RejectProtoMessage(context_id, message_data.proto,
                       message_data.result_logger, result, error_message,
                       std::move(message_data.response_callback));
  }
}

void GlicExperimentalTriggeringCoordinator::CancelAllPendingMessages() {
  if (pending_messages_.empty()) {
    return;
  }
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  std::map<std::string, std::vector<PendingMessageData>, std::less<>>
      pending_messages = std::move(pending_messages_);
  pending_messages_.clear();
  actor_service_observation_.Reset();
  for (auto& [context_id, messages] : pending_messages) {
    if (actor_service) {
      actor_service->RemovePendingTask(context_id);
    }
    FailPendingMessages(
        context_id, std::move(messages),
        GlicExperimentalTriggeringIncomingMessageResult::
            kCoordinatorUnavailable,
        "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Shutdown",
        "Message cancelled: Coordinator being destroyed.");
  }
}

std::optional<ExperimentalTriggeringResponse>
GlicExperimentalTriggeringCoordinator::OnRequest(
    const std::string& context_id,
    const ExperimentalTriggeringRequest& request,
    ScopedIncomingMessageResultLogger result_logger,
    GlicExperimentalTriggeringUpdateCallback update_callback,
    tabs::TabInterface* prepared_tab) {
  auto it = context_id_to_updates_handler_map_.find(context_id);
  ExperimentalTriggeringUpdatesHandler* handler = nullptr;
  if (it != context_id_to_updates_handler_map_.end()) {
    handler = it->second.get();
  } else {
    handler =
        context_id_to_updates_handler_map_
            .emplace(context_id,
                     std::make_unique<ExperimentalTriggeringUpdatesHandler>(
                         profile_, context_id,
                         InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                         InvokeErrorDiagnosticsPasskeyProvider::GetPassKey(),
                         weak_ptr_factory_.GetWeakPtr()))
            .first->second.get();
  }

  CHECK(handler);

  return handler->OnRequest(request, std::move(result_logger),
                            std::move(update_callback), prepared_tab);
}

void GlicExperimentalTriggeringCoordinator::OnUpdatesHandlerCleanup(
    const std::string& context_id) {
  if (profile_) {
    if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
      actor_service->RemovePendingTask(context_id);
    }
  }
  auto it = context_id_to_updates_handler_map_.find(context_id);
  if (it != context_id_to_updates_handler_map_.end()) {
    context_id_to_updates_handler_map_.erase(it);
  }
}

}  // namespace glic
