// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

// Timeout for sending experimental triggering updates to the server.
constexpr base::TimeDelta kUpdateMessageTimeout = base::Seconds(10);

glic::GlicInvokeOptions CreateInvokeOptions(
    const components_sharing_message::GlicExperimentalTriggering& request,
    tabs::TabInterface* tab) {
  glic::GlicInvokeOptions options{
      glic::mojom::InvocationSource::kExperimentalTriggering};

  glic::NewTab new_tab;
  new_tab.window = tab->GetBrowserWindowInterface();
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
      request.task_metadata().has_conversation_id() &&
      !request.task_metadata().conversation_id().empty()) {
    options.target.conversation =
        glic::ConversationId(request.task_metadata().conversation_id());
  } else {
    options.target.conversation = glic::NewConversation();
  }

  return options;
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

  void OnRequest(
      const components_sharing_message::GlicExperimentalTriggering& request) {
    if (request.has_task_metadata() &&
        request.task_metadata().has_sender_sequence_number()) {
      last_seen_sequence_number_ =
          request.task_metadata().sender_sequence_number();
    }

    switch (request.request().payload_case()) {
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kTriggerActuationRequest: {
        ProcessTriggerActuationRequest(request);
        return;
      }

      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kStopActuationRequest: {
        ProcessStopActuationRequest();
        return;
      }

      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::PAYLOAD_NOT_SET: {
        DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                         "actionable request.";
        return;
      }

      default:
        DLOG(WARNING)
            << "Ignoring unexpected GlicExperimentalTriggering  request";
        return;
    }
  }

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
    SendTaskUpdateMessage(TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                          "User is not opted in to experimental triggering.");
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
      instance_->GetExperimentalTriggeringUpdates(std::move(remote),
                                                  base::DoNothing());
    }
  }

  void ProcessTriggerActuationRequest(
      const components_sharing_message::GlicExperimentalTriggering& request) {
    if (!message_handler_) {
      return;
    }

    glic::GlicKeyedService* glic_service =
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(
            message_handler_->profile_, /*create=*/false);
    CHECK(glic_service);

    if (HandleUnavailableExperimentalTriggering(glic_service, request)) {
      message_handler_->OnUpdatesHandlerCleanup(context_id_);
      return;
    }

    tabs::TabInterface* active_tab = message_handler_->GetActiveTab();
    if (!active_tab) {
      DLOG(ERROR)
          << "No active tab found for Profile for GlicExperimentalTriggering";
      message_handler_->OnUpdatesHandlerCleanup(context_id_);
      return;
    }

    auto options = CreateInvokeOptions(request, active_tab);
    options.on_client_connected = base::BindOnce(
        [](base::WeakPtr<ExperimentalTriggeringUpdatesHandler> updates_handler,
           base::WeakPtr<glic::GlicInstance> instance) {
          if (updates_handler) {
            updates_handler->SubscribeForTriggeringUpdates(std::move(instance));
          }
        },
        weak_ptr_factory_.GetWeakPtr());
    options.on_error = base::BindOnce(
        [](base::WeakPtr<GlicExperimentalTriggeringMessageHandler>
               message_handler,
           const std::string& context_id, glic::GlicInvokeError error) {
          // TODO(b/505825633): Propagate the error to the actuation
          // request sender.
          DLOG(WARNING) << "Glic invocation failed with error: "
                        << static_cast<int>(error);

          if (message_handler) {
            message_handler->OnUpdatesHandlerCleanup(context_id);
          }
        },
        message_handler_, context_id_);

    instance_ =
        glic_service->InvokeWithAutoSubmit(passkey_, std::move(options));
  }

  void ProcessStopActuationRequest() {
    if (!instance_) {
      SendTaskUpdateMessage(
          TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
          "Failed to stop task due to missing glic instance.");
    } else {
      instance_->CancelTask();
      SendTaskUpdateMessage(TaskUpdate::STOPPED);
    }

    if (message_handler_) {
      message_handler_->OnUpdatesHandlerCleanup(context_id_);
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
          case glic::mojom::ExperimentalTriggeringUpdateType::kUnknown:
            SendTaskUpdateMessage(TaskUpdate::UNKNOWN_STATE, std::nullopt,
                                  std::move(update->data));
            break;
        }
        break;
    }
  }

 private:
  void SendTaskUpdateMessage(
      TaskUpdate::State state,
      std::optional<TaskUpdate::DataType> data_type = std::nullopt,
      std::optional<std::string> data = std::nullopt) {
    components_sharing_message::SharingMessage message;
    auto* triggering = message.mutable_glic_experimental_triggering();
    triggering->set_context_id(context_id_);

    auto* task_metadata = triggering->mutable_task_metadata();
    task_metadata->set_sender_sequence_number(++sequence_number_);
    if (last_seen_sequence_number_.has_value()) {
      task_metadata->set_last_seen_sequence_number(*last_seen_sequence_number_);
    }

    auto* task_update = triggering->mutable_response()->mutable_task_update();
    task_update->set_state(state);
    if (data_type.has_value()) {
      task_update->set_data_type(*data_type);
    }
    if (data.has_value()) {
      task_update->set_data(std::move(*data));
    }

    if (message_sender_) {
      message_sender_->SendMessageToServerTarget(
          server_channel_, kUpdateMessageTimeout, std::move(message),
          SharingMessageSender::DelegateType::kFCM,
          base::BindOnce([](SharingSendMessageResult result,
                            std::unique_ptr<
                                components_sharing_message::ResponseMessage>
                                response) {
            if (result != SharingSendMessageResult::kSuccessful) {
              DLOG(ERROR)
                  << "Failed to send experimental triggering update to server: "
                  << static_cast<int>(result);
            }
          }));
    }
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
  int64_t sequence_number_ = -1;
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

tabs::TabInterface* GlicExperimentalTriggeringMessageHandler::GetActiveTab()
    const {
  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&browser, this](BrowserWindowInterface* b) {
        if (b->GetProfile() == profile_) {
          browser = b;
          return false;  // Stop iteration
        }
        return true;  // Continue
      });
  return browser ? TabListInterface::From(browser)->GetActiveTab() : nullptr;
}

// TODO(b/505825633): Refine ResponseMessage errors for experimental triggering.
void GlicExperimentalTriggeringMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering));
  CHECK(message.has_glic_experimental_triggering());

  if (!message.has_server_channel_configuration()) {
    DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                     "server configuration channel data.";
    std::move(done_callback).Run(nullptr);
    return;
  }

  const auto& request = message.glic_experimental_triggering();
  if (!request.has_request()) {
    DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                     "request payload.";
    std::move(done_callback).Run(nullptr);
    return;
  }

  if (request.request().has_device_opt_in_request()) {
    tabs::TabInterface* active_tab = GetActiveTab();
    if (active_tab) {
      ProcessDeviceOptInRequest(active_tab);
    } else {
      DLOG(ERROR) << "No active tab found for Profile for "
                     "GlicExperimentalTriggering";
    }
    std::move(done_callback).Run(nullptr);
    return;
  }

  // If no `context_id` is present in the request, we generate one that
  // may be used by the sender in follow up actuation requests.
  const std::string context_id =
      (request.has_context_id() && !request.context_id().empty())
          ? request.context_id()
          : base::Uuid::GenerateRandomV4().AsLowercaseString();

  auto it = context_id_to_updates_handler_map_.find(context_id);
  ExperimentalTriggeringUpdatesHandler* handler = nullptr;
  if (it != context_id_to_updates_handler_map_.end()) {
    handler = it->second.get();
  } else {
    using ExperimentalTriggeringRequest = components_sharing_message::
        GlicExperimentalTriggering::ExperimentalTriggeringRequest;
    const auto payload_case = request.request().payload_case();
    if (payload_case ==
        ExperimentalTriggeringRequest::kTriggerActuationRequest) {
      handler =
          context_id_to_updates_handler_map_
              .emplace(
                  context_id,
                  std::make_unique<ExperimentalTriggeringUpdatesHandler>(
                      message_sender_,
                      std::move(
                          *message.mutable_server_channel_configuration()),
                      context_id,
                      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                      weak_ptr_factory_.GetWeakPtr()))
              .first->second.get();
    }
  }

  if (!handler) {
    DLOG(WARNING) << "No updates handler for request.";
    std::move(done_callback).Run(nullptr);
    return;
  }

  handler->OnRequest(request);
  std::move(done_callback).Run(nullptr);
}

void GlicExperimentalTriggeringMessageHandler::ProcessDeviceOptInRequest(
    tabs::TabInterface* active_tab) {
#if !BUILDFLAG(IS_ANDROID)
  if (!opt_in_controller_) {
    opt_in_controller_ =
        std::make_unique<glic::GlicExperimentalOptInController>(profile_);
  }

  opt_in_controller_->ShowDialog(active_tab->GetContents());
#endif
}

void GlicExperimentalTriggeringMessageHandler::OnUpdatesHandlerCleanup(
    std::string context_id) {
  context_id_to_updates_handler_map_.erase(context_id);
}
