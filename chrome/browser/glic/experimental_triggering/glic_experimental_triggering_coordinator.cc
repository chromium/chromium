// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
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
    int64_t sender_sequence_number) {
  if (data_type == TaskUpdate::DataType::kErrorMessage) {
    DLOG(WARNING) << message;
  }
  ExperimentalTriggeringResponse response = CreateBaseResponseMessage(
      context_id, request_task_metadata, sender_sequence_number);
  TaskUpdate task_update;
  task_update.state = state;
  task_update.data_type = data_type;
  task_update.data = message;
  response.task_update = std::move(task_update);
  return response;
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

// Builds base response metadata for asynchronous Mojo updates and callbacks
// (using instance state).
ExperimentalTriggeringResponse CreateBaseResponse(
    const std::string& context_id,
    int64_t sender_sequence_number,
    std::optional<int64_t> last_seen_sequence_number,
    const GlicInstance* instance) {
  ExperimentalTriggeringResponse response;
  response.context_id = context_id;
  TaskMetadata metadata;
  metadata.sender_sequence_number = sender_sequence_number;
  if (last_seen_sequence_number.has_value()) {
    metadata.last_seen_sequence_number = last_seen_sequence_number;
  }
  if (instance && instance->conversation_id()) {
    metadata.conversation_id = *instance->conversation_id();
  }
  response.task_metadata = std::move(metadata);
  return response;
}

#if BUILDFLAG(IS_ANDROID)
bool CanShowDeviceOptInUi() {
  return base::android::ApplicationStatusListener::HasVisibleActivities();
}
#endif

std::string_view GlicInvokeErrorToString(GlicInvokeError error) {
  switch (error) {
    case GlicInvokeError::kUnknown:
      return "Unknown error";
    case GlicInvokeError::kTimeout:
      return "Invocation timed out";
    case GlicInvokeError::kInvalidConversationId:
      return "Invalid conversation ID";
    case GlicInvokeError::kInvalidTab:
      return "Target tab was invalid";
    case GlicInvokeError::kTabClosed:
      return "Target tab was closed";
    case GlicInvokeError::kInstanceDestroyed:
      return "Glic instance was destroyed";
    case GlicInvokeError::kInvokeInProgress:
      return "Another invocation is already in progress";
    case GlicInvokeError::kInvalidConfiguration:
      return "Invalid configuration for invocation";
    case GlicInvokeError::kAdditionalContextSawNavigation:
    case GlicInvokeError::kAdditionalContextFailedCopyPolicy:
    case GlicInvokeError::kAdditionalContextFailedPastePolicy:
    case GlicInvokeError::kAdditionalContextNoSourceFrame:
    case GlicInvokeError::kAdditionalContextNoClientFrame:
    case GlicInvokeError::kAdditionalContextNoClipboardMetadata:
      return "Additional context error";
    case GlicInvokeError::kInstanceNotFound:
      return "Glic instance not found";
    case GlicInvokeError::kProfileNotEnabled:
      return "Glic is not enabled for the profile";
    case GlicInvokeError::kCancelled:
      return "Invocation was cancelled";
    case GlicInvokeError::kSuperseded:
      return "Invocation was superseded";
  }
  NOTREACHED();
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

}  // namespace

class ExperimentalTriggeringUpdatesHandler
    : public mojom::ExperimentalTriggeringUpdatesHandler {
 public:
  ExperimentalTriggeringUpdatesHandler(
      const std::string& context_id,
      InvokeWithAutoSubmitPasskey passkey,
      base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator)
      : context_id_(context_id),
        passkey_(std::move(passkey)),
        coordinator_(std::move(coordinator)),
        receiver_(this) {}

  ~ExperimentalTriggeringUpdatesHandler() override {
    if (is_actuation_started_ && !outcome_recorded_) {
      RecordOutcome(GlicExperimentalTriggeringExecutionOutcome::
                        kDestroyedBeforeCompletion);
    }
  }

  std::optional<ExperimentalTriggeringResponse> OnRequest(
      const ExperimentalTriggeringRequest& request,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      tabs::TabInterface* prepared_tab) {
    if (update_callback) {
      update_callback_ = std::move(update_callback);
    }
    if (!request.task_metadata.has_value()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kMissingTaskMetadata);
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

    base::ScopedClosureRunner cleanup_runner(base::BindOnce(
        &GlicExperimentalTriggeringCoordinator::OnUpdatesHandlerCleanup,
        coordinator_, context_id_));

    return std::visit(
        absl::Overload{
            [&](const TaskMetadataUpdated&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTaskMetadataUpdated(
                  request, std::move(cleanup_runner), std::move(result_logger));
            },
            [&](const TriggerActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTriggerOrContinueActuationRequest(
                  request, prepared_tab, std::move(cleanup_runner),
                  std::move(result_logger));
            },
            [&](const ContinueActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessTriggerOrContinueActuationRequest(
                  request, prepared_tab, std::move(cleanup_runner),
                  std::move(result_logger));
            },
            [&](const StopActuationRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessStopActuationRequest(&*request.task_metadata,
                                                 std::move(cleanup_runner),
                                                 std::move(result_logger));
            },
            [&](const DeviceOptInRequest&)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessDeviceOptInRequest(&*request.task_metadata,
                                               std::move(cleanup_runner),
                                               std::move(result_logger));
            },
            [&](const GetScreenshotRequest& payload)
                -> std::optional<ExperimentalTriggeringResponse> {
              return ProcessGetScreenshotRequest(
                  payload, &*request.task_metadata, std::move(cleanup_runner),
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
  }

  void OnUpdate(mojom::ExperimentalTriggeringUpdatePtr update,
                mojom::SubscriberObservationType observation) override {
    if (terminal_update_sent_) {
      return;
    }
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
            metadata[key] = std::move(val);
          }
        }
        if (!first_response_received_ && is_actuation_started_) {
          first_response_received_ = true;
          base::UmaHistogramMediumTimes(
              "Glic.ExperimentalTriggering.Latency.ToFirstResponse",
              base::TimeTicks::Now() - turn_start_time_);
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
  bool HandleUnavailableExperimentalTriggering(GlicKeyedService* glic_service) {
    auto state = glic_service->enabling().GetExperimentalTriggeringState();
    base::UmaHistogramEnumeration(
        "Glic.ExperimentalTriggering.StateOnActuationRequest", state);

    if (state == syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady) {
      return false;
    }

    DLOG(WARNING) << "Rejecting remote request: Glic not opted-in for "
                     "experimental triggering.";
    return true;
  }

 private:
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
      CleanupAsync();
      return;
    }
    mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
    manager->GetExperimentalTriggeringUpdates(
        std::move(remote),
        base::BindOnce(
            [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> handler,
               bool success) {
              if (!success && handler) {
                DLOG(WARNING) << "Failed to register experimental triggering "
                                 "updates handler.";
                handler->HandleTerminalUpdate(
                    TaskUpdate::State::kFailed,
                    TaskUpdate::DataType::kErrorMessage,
                    GlicExperimentalTriggeringExecutionOutcome::
                        kUpdatesRegistrationFailed,
                    "Failed to register experimental triggering updates "
                    "handler.");
                handler->CleanupAsync();
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void HandleTerminalUpdate(TaskUpdate::State state,
                            std::optional<TaskUpdate::DataType> data_type,
                            GlicExperimentalTriggeringExecutionOutcome outcome,
                            std::optional<std::string> data = std::nullopt,
                            std::map<std::string, std::string> metadata = {}) {
    if (terminal_update_sent_) {
      return;
    }
    terminal_update_sent_ = true;
    RecordOutcome(outcome);
    if (outcome == GlicExperimentalTriggeringExecutionOutcome::kSuccess &&
        is_actuation_started_) {
      base::UmaHistogramLongTimes(
          "Glic.ExperimentalTriggering.Latency.ToTerminalCompletion",
          base::TimeTicks::Now() - task_start_time_);
    }
    SendTaskUpdateMessage(state, data_type, std::move(data),
                          std::move(metadata));
  }

  void OnPanelOpened() {
    if (panel_opened_) {
      return;
    }
    panel_opened_ = true;
    if (is_actuation_started_) {
      base::UmaHistogramMediumTimes(
          "Glic.ExperimentalTriggering.Latency.ToSidePanelOpened",
          base::TimeTicks::Now() - task_start_time_);
    }
  }

  void OnClientConnected(base::WeakPtr<GlicInstance> instance) {
    if (client_connected_) {
      return;
    }
    client_connected_ = true;
    if (is_actuation_started_) {
      base::UmaHistogramMediumTimes(
          "Glic.ExperimentalTriggering.Latency.ToClientConnected",
          base::TimeTicks::Now() - task_start_time_);
    }
    SubscribeForTriggeringUpdates(std::move(instance));
  }

  void OnInvokeError(GlicInvokeError error) {
    if (error == GlicInvokeError::kSuperseded) {
      outcome_recorded_ = true;
      CleanupAsync();
      return;
    }
    DLOG(WARNING) << "Glic invocation failed with error: "
                  << GlicInvokeErrorToString(error);
    HandleTerminalUpdate(TaskUpdate::State::kFailed,
                         TaskUpdate::DataType::kErrorMessage,
                         GlicInvokeErrorToOutcome(error, client_connected_),
                         base::StrCat({"Glic invocation failed: ",
                                       GlicInvokeErrorToString(error)}));
    CleanupAsync();
  }

  void OnDisconnect() {
    HandleTerminalUpdate(TaskUpdate::State::kFailed,
                         TaskUpdate::DataType::kErrorMessage,
                         GlicExperimentalTriggeringExecutionOutcome::
                             kClientDisconnectedBeforeResponse,
                         "Client disconnected unexpectedly.");
    CleanupAsync();
  }

  void CleanupAsync() {
    if (is_cleaning_up_) {
      return;
    }
    is_cleaning_up_ = true;
    receiver_.reset();
    if (coordinator_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GlicExperimentalTriggeringCoordinator::OnUpdatesHandlerCleanup,
              coordinator_, context_id_));
    }
  }

  void RecordOutcome(GlicExperimentalTriggeringExecutionOutcome outcome) {
    if (outcome_recorded_) {
      return;
    }
    outcome_recorded_ = true;
    base::UmaHistogramEnumeration(
        "Glic.ExperimentalTriggering.ExecutionOutcome", outcome);
    if (is_actuation_started_) {
      base::UmaHistogramBoolean("Glic.ExperimentalTriggering.SidePanelOpened",
                                panel_opened_);
      base::UmaHistogramBoolean("Glic.ExperimentalTriggering.ClientConnected",
                                client_connected_);
    }
  }

  std::optional<ExperimentalTriggeringResponse>
  ProcessTriggerOrContinueActuationRequest(
      const ExperimentalTriggeringRequest& request,
      tabs::TabInterface* prepared_tab,
      base::ScopedClosureRunner cleanup_runner,
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
          sequence_generator_.GetNext());
    }

    BrowserWindowInterface* browser_window = nullptr;
    if (!prepared_tab) {
      browser_window = coordinator_->GetBrowserWindow();
      if (!browser_window) {
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
              sequence_generator_.GetNext());
        }
      }
    }

    GlicInvokeOptions options =
        CreateInvokeOptions(request, browser_window, prepared_tab);
    bool is_continuation =
        std::holds_alternative<ContinueActuationRequest>(request.payload);
    if (!is_continuation || task_start_time_.is_null()) {
      task_start_time_ = base::TimeTicks::Now();
    }
    turn_start_time_ = base::TimeTicks::Now();
    is_actuation_started_ = true;
    first_response_received_ = false;
    terminal_update_sent_ = false;
    outcome_recorded_ = false;
    options.on_panel_opened =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnPanelOpened,
                       weak_ptr_factory_.GetWeakPtr());
    options.on_client_connected =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnClientConnected,
                       weak_ptr_factory_.GetWeakPtr());
    options.on_error =
        base::BindOnce(&ExperimentalTriggeringUpdatesHandler::OnInvokeError,
                       weak_ptr_factory_.GetWeakPtr());

    auto response = CreateResponseMessage(
        context_id_, TaskUpdate::State::kStarting, std::nullopt, "",
        request.task_metadata.has_value() ? &*request.task_metadata : nullptr,
        sequence_generator_.GetNext());
    std::ignore = cleanup_runner.Release();

    GlicInvokeWithAutoSubmitOptions auto_submit_options;
#if BUILDFLAG(IS_ANDROID)
    if (prepared_tab) {
      auto_submit_options.show_panel = false;
    }
#endif

    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    auto instance = glic_service->InvokeWithAutoSubmit(
        passkey_, std::move(options), std::move(auto_submit_options));
    if (weak_this) {
      weak_this->instance_ = std::move(instance);
    }

    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    return response;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessGetScreenshotRequest(
      const GetScreenshotRequest& screenshot_req,
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
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
          sequence_generator_.GetNext());
    }

    GlicInstance* instance = instance_.get();
    if (!instance) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "No active Glic instance available for screenshot.", task_metadata,
          sequence_generator_.GetNext());
    }

    GlicExperimentalTriggeringManager* triggering_manager =
        instance->GetExperimentalTriggeringManager();
    if (!triggering_manager) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kTriggeringManagerUnavailable);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "GlicExperimentalTriggeringManager is not available.", task_metadata,
          sequence_generator_.GetNext());
    }

    if (screenshot_req.request_token.empty()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kUnexpectedRequestPayload);
      return CreateScreenshotResultResponse(
          context_id_,
          CreateScreenshotResult(
              ScreenshotResult::Status::kErrorInvalidRequest),
          task_metadata, sequence_generator_.GetNext());
    }

    auto response = CreateResponseMessage(
        context_id_, TaskUpdate::State::kStarting, std::nullopt, "",
        task_metadata, sequence_generator_.GetNext());
    std::ignore = cleanup_runner.Release();

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
    return response;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessTaskMetadataUpdated(
      const ExperimentalTriggeringRequest& request,
      base::ScopedClosureRunner cleanup_runner,
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
    std::ignore = cleanup_runner.Release();
    return std::nullopt;
  }

  std::optional<ExperimentalTriggeringResponse> ProcessStopActuationRequest(
      const TaskMetadata* request_metadata,
      base::ScopedClosureRunner cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
    std::optional<ExperimentalTriggeringResponse> response;
    if (!instance_) {
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kNoInstance);
      response = CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Failed to stop task due to missing glic instance.", request_metadata,
          sequence_generator_.GetNext());
    } else {
      instance_->GetActorTaskManager()->CancelTask();
      result_logger.set_result(
          GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
      response = CreateResponseMessage(context_id_, TaskUpdate::State::kStopped,
                                       std::nullopt, "", request_metadata,
                                       sequence_generator_.GetNext());
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

  std::optional<ExperimentalTriggeringResponse> ProcessDeviceOptInRequest(
      const TaskMetadata* task_metadata,
      base::ScopedClosureRunner cleanup_runner,
      ScopedIncomingMessageResultLogger result_logger) {
#if BUILDFLAG(IS_ANDROID)
    if (!CanShowDeviceOptInUi()) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kAndroidOptInUnsupported);
      return CreateResponseMessage(
          context_id_, TaskUpdate::State::kFailed,
          TaskUpdate::DataType::kErrorMessage,
          "Ignoring unexpected Android Opt-in request.", task_metadata,
          sequence_generator_.GetNext());
    }
#endif
    if (!coordinator_) {
      result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                   kCoordinatorUnavailable);
      return CreateDeviceOptInResponse(context_id_, DeviceOptInResult::kFailed,
                                       task_metadata,
                                       sequence_generator_.GetNext());
    }

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

    glic_service->opt_in_controller().ShowDialog(web_contents,
                                                 std::move(callback));

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
    if (update_callback_) {
      ExperimentalTriggeringResponse response =
          CreateBaseResponse(context_id_, sequence_generator_.GetNext(),
                             last_seen_sequence_number_, instance_.get());
      response.device_opt_in_result = accepted ? DeviceOptInResult::kAccepted
                                               : DeviceOptInResult::kDeclined;
      update_callback_.Run(std::move(response));
    }
    if (coordinator_) {
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

  std::string context_id_;
  InvokeWithAutoSubmitPasskey passkey_;
  base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtr<GlicInstance> instance_;
  mojo::Receiver<mojom::ExperimentalTriggeringUpdatesHandler> receiver_;
  base::AtomicSequenceNumber sequence_generator_;

  std::optional<int64_t> last_seen_sequence_number_;
  GlicExperimentalTriggeringUpdateCallback update_callback_;
  bool terminal_update_sent_ = false;
  bool is_actuation_started_ = false;
  bool panel_opened_ = false;
  bool client_connected_ = false;
  bool first_response_received_ = false;
  bool outcome_recorded_ = false;
  bool is_cleaning_up_ = false;
  base::TimeTicks task_start_time_;
  base::TimeTicks turn_start_time_;

  base::WeakPtrFactory<ExperimentalTriggeringUpdatesHandler> weak_ptr_factory_{
      this};
};

GlicExperimentalTriggeringCoordinator::GlicExperimentalTriggeringCoordinator(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

GlicExperimentalTriggeringCoordinator::
    ~GlicExperimentalTriggeringCoordinator() = default;

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

std::optional<ExperimentalTriggeringResponse>
GlicExperimentalTriggeringCoordinator::OnProtoMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto,
    ScopedIncomingMessageResultLogger result_logger,
    GlicExperimentalTriggeringUpdateCallback update_callback,
    tabs::TabInterface* prepared_tab) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  LogGlicExperimentalTriggeringProto(
      actor_service, "GlicExperimentalTriggering", context_id, proto);

  if (!HasUpdatesHandler(context_id)) {
    MaybeRecordInitialSharingMessageDeliveryLatency(proto);
  }

  auto request_metadata = ProtoToTaskMetadata(proto);
  const TaskMetadata* request_metadata_ptr =
      request_metadata.has_value() ? &*request_metadata : nullptr;

  GlicKeyedService* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false);
  auto local_version =
      glic_service ? glic_service->enabling().GetExperimentalTriggeringVersion()
                   : std::nullopt;
  if (proto.has_glic_experimental_triggering_version() &&
      (!local_version.has_value() ||
       proto.glic_experimental_triggering_version() > *local_version)) {
    result_logger.set_result(GlicExperimentalTriggeringIncomingMessageResult::
                                 kVersionMismatchOrUnavailable);
    return CreateResponseMessage(context_id, TaskUpdate::State::kFailed,
                                 TaskUpdate::DataType::kErrorMessage,
                                 "Rejected: version mismatch or unavailable.",
                                 request_metadata_ptr,
                                 /*sender_sequence_number=*/0);
  }

  const bool has_valid_request =
      proto.has_request() &&
      proto.request().payload_case() !=
          components_sharing_message::GlicExperimentalTriggering::
              ExperimentalTriggeringRequest::PAYLOAD_NOT_SET;
  if (!has_valid_request && !proto.has_task_metadata_updated()) {
    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload);
    return CreateResponseMessage(
        context_id, TaskUpdate::State::kFailed,
        TaskUpdate::DataType::kErrorMessage,
        "Received GlicExperimentalTriggering message with no request payload.",
        request_metadata_ptr,
        /*sender_sequence_number=*/0);
  }

  ExperimentalTriggeringRequest domain_request = ProtoToRequest(proto);
  domain_request.context_id = context_id;

  return OnRequest(context_id, domain_request, std::move(result_logger),
                   std::move(update_callback), prepared_tab);
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
                         context_id,
                         InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                         weak_ptr_factory_.GetWeakPtr()))
            .first->second.get();
  }

  CHECK(handler);

  return handler->OnRequest(request, std::move(result_logger),
                            std::move(update_callback), prepared_tab);
}

void GlicExperimentalTriggeringCoordinator::OnUpdatesHandlerCleanup(
    const std::string& context_id) {
  auto it = context_id_to_updates_handler_map_.find(context_id);
  if (it != context_id_to_updates_handler_map_.end()) {
    context_id_to_updates_handler_map_.erase(it);
  }
}

}  // namespace glic
