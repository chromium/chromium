// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <optional>

#include "base/atomic_sequence_num.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/glic_experimental_triggering/actor_log.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"           // nogncheck
#include "chrome/browser/ui/browser_commands.h"  // nogncheck
#include "chrome/browser/ui/browser_window.h"    // nogncheck
#endif
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

using ExperimentalTriggeringResponse = components_sharing_message::
    GlicExperimentalTriggering::ExperimentalTriggeringResponse;
using TaskUpdate = ExperimentalTriggeringResponse::TaskUpdate;
using DeviceOptInResult = ExperimentalTriggeringResponse::DeviceOptInResult;

// Timeout for sending experimental triggering updates to the server.
constexpr base::TimeDelta kUpdateMessageTimeout = base::Seconds(10);

// A default starting sequence number returned to the sender for initial
// request failure cases where the message was not associated to a recognized
// context ID.
constexpr int64_t kDefaultStartingRequestFailureSequenceNumber = 0;

glic::GlicInvokeOptions CreateInvokeOptions(
    const components_sharing_message::GlicExperimentalTriggering& request,
    BrowserWindowInterface* window) {
  glic::GlicInvokeOptions options{
      glic::mojom::InvocationSource::kExperimentalTriggering};

  glic::NewTab new_tab;
  new_tab.window = window;
  new_tab.open_in_foreground = false;
  options.target.surface = new_tab;

  options.feature_mode = glic::mojom::FeatureMode::kExperimentalTriggering;

  if (request.has_request() &&
      request.request().has_trigger_actuation_request() &&
      request.request().trigger_actuation_request().has_initial_prompt()) {
    options.prompts.push_back(
        request.request().trigger_actuation_request().initial_prompt());
  }

  if (request.has_task_metadata() &&
      request.task_metadata().has_parent_conversation_metadata()) {
    const auto& parent_metadata =
        request.task_metadata().parent_conversation_metadata();
    auto context = glic::mojom::AdditionalContext::New();
    context->source =
        glic::mojom::AdditionalContextSource::kExperimentalTriggering;
    auto parent_conversation = glic::mojom::ParentConversationMetadata::New();
    if (parent_metadata.has_conversation_id()) {
      parent_conversation->conversation_id = parent_metadata.conversation_id();
    }
    if (parent_metadata.has_conversation_title()) {
      parent_conversation->conversation_title =
          parent_metadata.conversation_title();
    }
    context->parts.push_back(
        glic::mojom::AdditionalContextPart::NewParentConversationMetadata(
            std::move(parent_conversation)));
    options.additional_context = glic::AdditionalTabContext(
        std::move(context), content::GlobalRenderFrameHostId(),
        glic::PolicyCheck::kNone);
  }

  if (request.has_task_metadata() &&
      request.task_metadata().has_conversation_id() &&
      !request.task_metadata().conversation_id().empty()) {
    options.target.conversation =
        glic::ConversationId(request.task_metadata().conversation_id());
  } else {
    options.target.conversation = glic::NewConversation();
  }

  return options;
}

std::unique_ptr<components_sharing_message::ResponseMessage>
CreateResponseMessage(
    const std::string& context_id,
    TaskUpdate::State state,
    TaskUpdate::DataType data_type,
    const std::string& message,
    const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
        request_task_metadata,
    int64_t sender_sequence_number =
        kDefaultStartingRequestFailureSequenceNumber) {
  if (data_type == TaskUpdate::ERROR_MESSAGE) {
    DLOG(WARNING) << message;
  }

  if (!request_task_metadata) {
    return nullptr;
  }

  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  auto* triggering = response->mutable_glic_experimental_triggering();
  triggering->set_context_id(context_id);
  auto* task_metadata = triggering->mutable_task_metadata();
  if (request_task_metadata->has_conversation_id()) {
    task_metadata->set_conversation_id(
        request_task_metadata->conversation_id());
  }
  if (request_task_metadata->has_task_id()) {
    task_metadata->set_task_id(request_task_metadata->task_id());
  }
  task_metadata->set_sender_sequence_number(sender_sequence_number);
  if (request_task_metadata->has_sender_sequence_number()) {
    task_metadata->set_last_seen_sequence_number(
        request_task_metadata->sender_sequence_number());
  }
  auto* task_update = triggering->mutable_response()->mutable_task_update();
  task_update->set_state(state);
  task_update->set_data_type(data_type);
  task_update->set_data(message);
  return response;
}

std::unique_ptr<components_sharing_message::ResponseMessage>
CreateResponseMessage(
    const std::string& context_id,
    DeviceOptInResult opt_in_result,
    const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
        request_task_metadata,
    int64_t sender_sequence_number =
        kDefaultStartingRequestFailureSequenceNumber) {
  if (!request_task_metadata) {
    return nullptr;
  }

  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  auto* triggering = response->mutable_glic_experimental_triggering();
  triggering->set_context_id(context_id);
  auto* task_metadata = triggering->mutable_task_metadata();
  if (request_task_metadata->has_conversation_id()) {
    task_metadata->set_conversation_id(
        request_task_metadata->conversation_id());
  }
  if (request_task_metadata->has_task_id()) {
    task_metadata->set_task_id(request_task_metadata->task_id());
  }
  task_metadata->set_sender_sequence_number(sender_sequence_number);
  if (request_task_metadata->has_sender_sequence_number()) {
    task_metadata->set_last_seen_sequence_number(
        request_task_metadata->sender_sequence_number());
  }
  triggering->mutable_response()->set_device_opt_in_result(opt_in_result);
  return response;
}

}  // namespace

class ExperimentalTriggeringUpdatesHandler
    : public glic::mojom::ExperimentalTriggeringUpdatesHandler {
 public:
  using TaskUpdate = components_sharing_message::GlicExperimentalTriggering::
      ExperimentalTriggeringResponse::TaskUpdate;

  ExperimentalTriggeringUpdatesHandler(
      SharingMessageSender* message_sender,
      components_sharing_message::ServerChannelConfiguration server_channel,
      const std::string& context_id,
      glic::InvokeWithAutoSubmitPasskey passkey,
      base::WeakPtr<GlicExperimentalTriggeringMessageHandler> message_handler)
      : message_sender_(message_sender),
        server_channel_(std::move(server_channel)),
        context_id_(context_id),
        passkey_(std::move(passkey)),
        message_handler_(std::move(message_handler)),
        receiver_(this) {}

  std::unique_ptr<components_sharing_message::ResponseMessage> OnRequest(
      const components_sharing_message::GlicExperimentalTriggering& request) {
    if (!request.has_task_metadata()) {
      return nullptr;
    }

    const auto* task_metadata = &request.task_metadata();
    if (task_metadata->has_sender_sequence_number()) {
      last_seen_sequence_number_ = task_metadata->sender_sequence_number();
    }

    base::ScopedClosureRunner cleanup_runner(base::BindOnce(
        [](base::WeakPtr<GlicExperimentalTriggeringMessageHandler>
               message_handler,
           std::string context_id) {
          if (message_handler) {
            message_handler->OnUpdatesHandlerCleanup(context_id);
          }
        },
        message_handler_, context_id_));

    if (request.has_task_metadata_updated()) {
      return ProcessTaskMetadataUpdated(request, std::move(cleanup_runner));
    }

    switch (request.request().payload_case()) {
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kTriggerActuationRequest: {
        return ProcessTriggerActuationRequest(request,
                                              std::move(cleanup_runner));
      }

      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kStopActuationRequest: {
        return ProcessStopActuationRequest(task_metadata,
                                           std::move(cleanup_runner));
      }

      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kDeviceOptInRequest: {
#if !BUILDFLAG(IS_ANDROID)
        return ProcessDeviceOptInRequest(task_metadata,
                                         std::move(cleanup_runner));
#else
        return CreateResponseMessage(
            context_id_, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Ignoring unexpected Android Opt-in request.", task_metadata,
            sequence_generator_.GetNext());
#endif
      }

      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::PAYLOAD_NOT_SET: {
        DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                         "actionable request.";
        return nullptr;
      }

      default:
        return CreateResponseMessage(
            context_id_, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Ignoring unexpected GlicExperimentalTriggering request.",
            task_metadata, sequence_generator_.GetNext());
    }
  }

  void OnUpdate(glic::mojom::ExperimentalTriggeringUpdatePtr update,
                glic::mojom::SubscriberObservationType observation) override {
    switch (observation) {
      case glic::mojom::SubscriberObservationType::kComplete:
        SendTaskUpdateMessage(TaskUpdate::COMPLETE);
        break;
      case glic::mojom::SubscriberObservationType::kError:
        SendTaskUpdateMessage(TaskUpdate::FAILED);
        break;
      case glic::mojom::SubscriberObservationType::kUpdate:
        if (!update) {
          DLOG(ERROR) << "Received kUpdate observation with null update";
          return;
        }
        switch (update->type) {
          case glic::mojom::ExperimentalTriggeringUpdateType::kWorklog:
            SendTaskUpdateMessage(TaskUpdate::RUNNING, TaskUpdate::WORKLOG,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kPaused:
            SendTaskUpdateMessage(TaskUpdate::PAUSED, std::nullopt,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::
              kTerminalCompletion:
            SendTaskUpdateMessage(TaskUpdate::COMPLETE,
                                  TaskUpdate::FINAL_RESPONSE,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalStopped:
            SendTaskUpdateMessage(TaskUpdate::STOPPED, std::nullopt,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalFailed:
            SendTaskUpdateMessage(TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kYieldToUser:
            SendTaskUpdateMessage(TaskUpdate::YIELD, std::nullopt,
                                  std::move(update->data));
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kUnknown:
            SendTaskUpdateMessage(TaskUpdate::UNKNOWN_STATE, std::nullopt,
                                  std::move(update->data));
            break;
        }
        break;
    }
  }

 private:
  // Checks if experimental triggering is allowed for the profile. If NOT
  // allowed, handles the rejection by logging metrics, sending a FAILED
  // response back to the server (if FCM is configured),  and returning true to
  // indicate the message has been fully handled. If allowed, returns false to
  // indicate that normal handling should proceed.
  bool HandleUnavailableExperimentalTriggering(
      glic::GlicKeyedService* glic_service,
      const components_sharing_message::GlicExperimentalTriggering& request) {
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

  void SubscribeForTriggeringUpdates(
      base::WeakPtr<glic::GlicInstance> instance) {
    instance_ = std::move(instance);
    if (instance_ && !receiver_.is_bound()) {
      mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
          remote;
      receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
      receiver_.set_disconnect_handler(base::BindOnce(
          [](base::WeakPtr<GlicExperimentalTriggeringMessageHandler>
                 message_handler,
             std::string context_id) {
            if (message_handler) {
              message_handler->OnUpdatesHandlerCleanup(context_id);
            }
          },
          message_handler_, context_id_));
      instance_->GetExperimentalTriggeringUpdates(
          std::move(remote), base::BindOnce([](bool success) {
            if (!success) {
              DLOG(WARNING) << "Failed to register experimental triggering "
                               "updates handler.";
            }
          }));
    }
  }

  std::unique_ptr<components_sharing_message::ResponseMessage>
  ProcessTriggerActuationRequest(
      const components_sharing_message::GlicExperimentalTriggering& request,
      base::ScopedClosureRunner cleanup_runner) {
    CHECK(request.has_task_metadata());
    if (!message_handler_) {
      return nullptr;
    }

    glic::GlicKeyedService* glic_service =
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(
            message_handler_->profile_, /*create=*/false);
    CHECK(glic_service);

    if (HandleUnavailableExperimentalTriggering(glic_service, request)) {
      return CreateResponseMessage(
          context_id_, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
          "User is not opted in to experimental triggering.",
          &request.task_metadata(), sequence_generator_.GetNext());
    }

    // Find or create a valid browser window. On Android, or if window
    // creation is disabled/fails, clean up and abort.
    BrowserWindowInterface* browser_window =
        message_handler_->GetBrowserWindow();
    if (!browser_window) {
      if (base::FeatureList::IsEnabled(
              features::kGlicExperimentalTriggeringOpenWindowIfNone)) {
#if !BUILDFLAG(IS_ANDROID)
        browser_window = chrome::OpenEmptyWindow(message_handler_->profile_);
#endif
      }
      if (!browser_window) {
        return CreateResponseMessage(
            context_id_, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "No browser window found for current profile",
            &request.task_metadata(), sequence_generator_.GetNext());
      }
    }

    auto options = CreateInvokeOptions(request, browser_window);
    options.on_client_connected = base::BindOnce(
        [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> updates_handler,
           base::WeakPtr<glic::GlicInstance> instance) {
          if (updates_handler) {
            updates_handler->SubscribeForTriggeringUpdates(std::move(instance));
          }
        },
        weak_ptr_factory_.GetWeakPtr());
    options.on_error = base::BindOnce(
        [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> updates_handler,
           const std::string& context_id, glic::GlicInvokeError error) {
          DLOG(WARNING) << "Glic invocation failed with error: "
                        << static_cast<int>(error);
          if (updates_handler) {
            updates_handler->SendTaskUpdateMessage(
                TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                "Glic invocation failed with error: " +
                    base::NumberToString(static_cast<int>(error)));
            if (updates_handler->message_handler_) {
              updates_handler->message_handler_->OnUpdatesHandlerCleanup(
                  context_id);
            }
          }
        },
        weak_ptr_factory_.GetWeakPtr(), context_id_);

    instance_ =
        glic_service->InvokeWithAutoSubmit(passkey_, std::move(options));

    // The flow has been successfully initiated, so either Mojo disconnect
    // or options.on_error will take care of cleaning up this updates handler.
    std::ignore = cleanup_runner.Release();

    return nullptr;
  }

  std::unique_ptr<components_sharing_message::ResponseMessage>
  ProcessTaskMetadataUpdated(
      const components_sharing_message::GlicExperimentalTriggering& request,
      base::ScopedClosureRunner cleanup_runner) {
    if (!instance_) {
      // Let cleanup_runner clean up this unused handler.
      return nullptr;
    }
    if (request.has_task_metadata() &&
        request.task_metadata().has_parent_conversation_metadata()) {
      const auto& parent_metadata =
          request.task_metadata().parent_conversation_metadata();
      auto context = glic::mojom::AdditionalContext::New();
      context->source =
          glic::mojom::AdditionalContextSource::kExperimentalTriggering;
      auto parent_conversation = glic::mojom::ParentConversationMetadata::New();
      if (parent_metadata.has_conversation_id()) {
        parent_conversation->conversation_id =
            parent_metadata.conversation_id();
      }
      if (parent_metadata.has_conversation_title()) {
        parent_conversation->conversation_title =
            parent_metadata.conversation_title();
      }
      context->parts.push_back(
          glic::mojom::AdditionalContextPart::NewParentConversationMetadata(
              std::move(parent_conversation)));
      instance_->SendAdditionalContext(std::move(context));
    }
    std::ignore = cleanup_runner.Release();
    return nullptr;
  }

  std::unique_ptr<components_sharing_message::ResponseMessage>
  ProcessStopActuationRequest(
      const components_sharing_message::GlicExperimentalTriggering::
          TaskMetadata* request_metadata,
      base::ScopedClosureRunner cleanup_runner) {
    std::unique_ptr<components_sharing_message::ResponseMessage> response;
    if (!instance_) {
      response = CreateResponseMessage(
          context_id_, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
          "Failed to stop task due to missing glic instance.", request_metadata,
          sequence_generator_.GetNext());
    } else {
      instance_->GetActorTaskManager()->CancelTask();
      response = CreateResponseMessage(
          context_id_, TaskUpdate::STOPPED, TaskUpdate::FINAL_RESPONSE, "",
          request_metadata, sequence_generator_.GetNext());
    }

    return response;
  }

  std::unique_ptr<components_sharing_message::ResponseMessage>
  ProcessDeviceOptInRequest(
      const components_sharing_message::GlicExperimentalTriggering::
          TaskMetadata* task_metadata,
      base::ScopedClosureRunner cleanup_runner) {
    if (!message_handler_) {
      return nullptr;
    }

    // TODO(b/515766485): Introduce (default on) feature flag to create a new
    // tab if none active.
    tabs::TabInterface* active_tab = message_handler_->GetActiveTab();
    if (!active_tab) {
      DLOG(ERROR) << "No active tab found for Profile for "
                     "GlicExperimentalTriggering";
      return CreateResponseMessage(
          context_id_, ExperimentalTriggeringResponse::FAILED, task_metadata,
          sequence_generator_.GetNext());
    }

    glic::GlicKeyedService* glic_service =
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(
            message_handler_->profile_, /*create=*/false);
    if (!glic_service) {
      return CreateResponseMessage(
          context_id_, ExperimentalTriggeringResponse::FAILED, task_metadata,
          sequence_generator_.GetNext());
    }

#if !BUILDFLAG(IS_ANDROID)
    auto callback = base::BindOnce(
        &ExperimentalTriggeringUpdatesHandler::SendDeviceOptInResult,
        weak_ptr_factory_.GetWeakPtr());

    glic_service->opt_in_controller().ShowDialog(active_tab->GetContents(),
                                                 std::move(callback));
#endif

    // The dialog is shown, and SendDeviceOptInResult will be called when the
    // dialog is accepted or declined. SendDeviceOptInResult will clean up the
    // updates handler when it runs. So we can safely release/disarm the
    // cleanup_runner here.
    std::ignore = cleanup_runner.Release();

    return nullptr;
  }

  components_sharing_message::SharingMessage CreateBaseResponse() {
    components_sharing_message::SharingMessage message;
    auto* triggering = message.mutable_glic_experimental_triggering();
    triggering->set_context_id(context_id_);

    auto* task_metadata = triggering->mutable_task_metadata();
    task_metadata->set_sender_sequence_number(sequence_generator_.GetNext());
    if (last_seen_sequence_number_.has_value()) {
      task_metadata->set_last_seen_sequence_number(*last_seen_sequence_number_);
    }
    if (instance_ && instance_->conversation_id()) {
      task_metadata->set_conversation_id(*instance_->conversation_id());
    }
    return message;
  }

  void SendResponse(components_sharing_message::SharingMessage message,
                    const std::string& error_log_description) {
    if (message_handler_ && message_handler_->profile_) {
      actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(message_handler_->profile_);
      LogGlicExperimentalTriggeringProto(
          actor_service, "GlicExperimentalTriggering", context_id_,
          message.glic_experimental_triggering());
    }

    if (message_sender_) {
      message_sender_->SendMessageToServerTarget(
          server_channel_, kUpdateMessageTimeout, std::move(message),
          base::BindOnce(
              [](std::string error_log_description,
                 SharingSendMessageResult result,
                 std::unique_ptr<components_sharing_message::ResponseMessage>
                     unused) {
                if (result != SharingSendMessageResult::kSuccessful) {
                  DLOG(ERROR) << "Failed to send " << error_log_description
                              << " to server: " << static_cast<int>(result);
                }
              },
              error_log_description));
    }
  }

  void SendDeviceOptInResult(bool accepted) {
    components_sharing_message::SharingMessage message = CreateBaseResponse();
    auto* triggering = message.mutable_glic_experimental_triggering();
    triggering->mutable_response()->set_device_opt_in_result(
        accepted ? components_sharing_message::GlicExperimentalTriggering::
                       ExperimentalTriggeringResponse::ACCEPTED
                 : components_sharing_message::GlicExperimentalTriggering::
                       ExperimentalTriggeringResponse::DECLINED);

    SendResponse(std::move(message), "device opt-in result");

    if (message_handler_) {
      message_handler_->OnUpdatesHandlerCleanup(context_id_);
    }
  }

  void SendTaskUpdateMessage(
      TaskUpdate::State state,
      std::optional<TaskUpdate::DataType> data_type = std::nullopt,
      std::optional<std::string> data = std::nullopt) {
    components_sharing_message::SharingMessage message = CreateBaseResponse();
    auto* triggering = message.mutable_glic_experimental_triggering();
    auto* task_update = triggering->mutable_response()->mutable_task_update();
    task_update->set_state(state);
    if (data_type.has_value()) {
      task_update->set_data_type(*data_type);
    }
    if (data.has_value()) {
      task_update->set_data(std::move(*data));
    }

    SendResponse(std::move(message), "experimental triggering update");
  }

  raw_ptr<SharingMessageSender> message_sender_;
  const components_sharing_message::ServerChannelConfiguration server_channel_;
  // Tracks the highest sequence number received from the server for actuation
  // on a specific glic instance.
  std::optional<int64_t> last_seen_sequence_number_;
  // Identifier that may be used for follow-up server actuation requests to a
  // specific glic instance.
  std::string context_id_;
  glic::InvokeWithAutoSubmitPasskey passkey_;
  base::WeakPtr<GlicExperimentalTriggeringMessageHandler> message_handler_;
  base::WeakPtr<glic::GlicInstance> instance_;
  mojo::Receiver<glic::mojom::ExperimentalTriggeringUpdatesHandler> receiver_;
  base::AtomicSequenceNumber sequence_generator_;

  base::WeakPtrFactory<ExperimentalTriggeringUpdatesHandler> weak_ptr_factory_{
      this};
};

GlicExperimentalTriggeringMessageHandler::
    GlicExperimentalTriggeringMessageHandler(
        Profile* profile,
        SharingMessageSender* message_sender)
    : profile_(profile), message_sender_(message_sender) {
  CHECK(profile_);
  CHECK(message_sender_);
}

GlicExperimentalTriggeringMessageHandler::
    ~GlicExperimentalTriggeringMessageHandler() = default;

BrowserWindowInterface*
GlicExperimentalTriggeringMessageHandler::GetBrowserWindow() const {
  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&browser, this](BrowserWindowInterface* b) {
        if (b->GetProfile() == profile_) {
          browser = b;
          return false;  // Stop iteration
        }
        return true;  // Continue
      });
  return browser;
}

tabs::TabInterface* GlicExperimentalTriggeringMessageHandler::GetActiveTab()
    const {
  BrowserWindowInterface* browser = GetBrowserWindow();
  return browser ? TabListInterface::From(browser)->GetActiveTab() : nullptr;
}

void GlicExperimentalTriggeringMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering));
  CHECK(message.has_glic_experimental_triggering());

  const auto& request = message.glic_experimental_triggering();
  // If no `context_id` is present in the request, we generate one that
  // may be used by the sender in follow up actuation requests.
  const std::string context_id =
      (request.has_context_id() && !request.context_id().empty())
          ? request.context_id()
          : base::Uuid::GenerateRandomV4().AsLowercaseString();

  const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
      request_metadata =
          request.has_task_metadata() ? &request.task_metadata() : nullptr;

  if (request.has_glic_experimental_triggering_version() &&
      !IsVersionSupported(request.glic_experimental_triggering_version())) {
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Rejected: version mismatch or unavailable.", request_metadata));
    return;
  }

  if (!message.has_server_channel_configuration()) {
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Received GlicExperimentalTriggering message "
            "with no server configuration channel data.",
            request_metadata));
    return;
  }

  if (!request.has_request() && !request.has_task_metadata_updated()) {
    if (profile_) {
      actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(profile_);
      LogGlicExperimentalTriggeringProto(
          actor_service, "GlicExperimentalTriggering", "", request);
    }

    std::move(done_callback)
        .Run(CreateResponseMessage(context_id, TaskUpdate::FAILED,
                                   TaskUpdate::ERROR_MESSAGE,
                                   "Received GlicExperimentalTriggering "
                                   "message with no request payload.",
                                   request_metadata));
    return;
  }

  if (profile_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(profile_);
    LogGlicExperimentalTriggeringProto(
        actor_service, "GlicExperimentalTriggering", context_id, request);
  }

  auto it = context_id_to_updates_handler_map_.find(context_id);
  ExperimentalTriggeringUpdatesHandler* handler = nullptr;
  if (it != context_id_to_updates_handler_map_.end()) {
    handler = it->second.get();
  } else {
    handler =
        context_id_to_updates_handler_map_
            .emplace(
                context_id,
                std::make_unique<ExperimentalTriggeringUpdatesHandler>(
                    message_sender_,
                    std::move(*message.mutable_server_channel_configuration()),
                    context_id,
                    glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                    weak_ptr_factory_.GetWeakPtr()))
            .first->second.get();
  }

  if (!handler) {
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "No updates handler for request.", request_metadata));
    return;
  }

  std::move(done_callback).Run(handler->OnRequest(request));
}

void GlicExperimentalTriggeringMessageHandler::OnUpdatesHandlerCleanup(
    std::string context_id) {
  context_id_to_updates_handler_map_.erase(context_id);
}

bool GlicExperimentalTriggeringMessageHandler::IsVersionSupported(
    int incoming_version) const {
  std::optional<int> local_version = GetLocalTriggeringVersion();
  return local_version.has_value() && incoming_version <= *local_version;
}

std::optional<int>
GlicExperimentalTriggeringMessageHandler::GetLocalTriggeringVersion() const {
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                         /*create=*/false);
  return glic_service
             ? glic_service->enabling().GetExperimentalTriggeringVersion()
             : std::nullopt;
}
