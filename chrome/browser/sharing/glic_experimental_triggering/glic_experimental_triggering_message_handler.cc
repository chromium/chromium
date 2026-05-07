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

class UpdatesListener
    : public glic::mojom::ExperimentalTriggeringUpdatesHandler {
 public:
  UpdatesListener(
      SharingMessageSender* message_sender,
      components_sharing_message::ServerChannelConfiguration server_channel,
      std::optional<int64_t> last_seen_sequence_number)
      : message_sender_(message_sender),
        server_channel_(std::move(server_channel)),
        last_seen_sequence_number_(last_seen_sequence_number) {}

  void OnUpdate(glic::mojom::ExperimentalTriggeringUpdatePtr update,
                glic::mojom::SubscriberObservationType observation) override {
    components_sharing_message::SharingMessage message;
    auto* glic_experimental_triggering =
        message.mutable_glic_experimental_triggering();
    auto* glic_response = glic_experimental_triggering->mutable_response();
    auto* task_update = glic_response->mutable_task_update();

    auto* task_metadata = glic_experimental_triggering->mutable_task_metadata();
    task_metadata->set_sender_sequence_number(++sequence_number_);
    if (last_seen_sequence_number_.has_value()) {
      task_metadata->set_last_seen_sequence_number(*last_seen_sequence_number_);
    }

    switch (observation) {
      case glic::mojom::SubscriberObservationType::kComplete:
        task_update->set_state(
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::COMPLETE);
        break;
      case glic::mojom::SubscriberObservationType::kError:
        task_update->set_state(
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::FAILED);
        break;
      case glic::mojom::SubscriberObservationType::kUpdate:
        if (!update) {
          DLOG(ERROR) << "Received kUpdate observation with null update";
          return;
        }
        switch (update->type) {
          case glic::mojom::ExperimentalTriggeringUpdateType::kWorklog:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::RUNNING);
            task_update->set_data_type(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::WORKLOG);
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kPaused:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::PAUSED);
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::
              kTerminalCompletion:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::COMPLETE);
            task_update->set_data_type(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::FINAL_RESPONSE);
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalStopped:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::STOPPED);
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalFailed:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::FAILED);
            task_update->set_data_type(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::ERROR_MESSAGE);
            break;
          case glic::mojom::ExperimentalTriggeringUpdateType::kUnknown:
            task_update->set_state(
                components_sharing_message::GlicExperimentalTriggering::
                    ExperimentalTriggeringResponse::TaskUpdate::UNKNOWN_STATE);
            break;
        }
        break;
    }

    if (update) {
      task_update->set_data(update->data);
    }

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

 private:
  raw_ptr<SharingMessageSender> message_sender_;
  const components_sharing_message::ServerChannelConfiguration server_channel_;
  std::optional<int64_t> last_seen_sequence_number_;
  int64_t sequence_number_ = -1;
};

}  // namespace

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

bool GlicExperimentalTriggeringMessageHandler::
    HandleUnavailableExperimentalTriggering(
        glic::GlicKeyedService* glic_service,
        const components_sharing_message::SharingMessage& message,
        SharingMessageHandler::DoneCallback& done_callback) {
  auto state = glic_service->enabling().GetExperimentalTriggeringState();
  base::UmaHistogramEnumeration(
      "Glic.ExperimentalTriggering.StateOnActuationRequest", state);

  if (state == syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady) {
    return false;
  }

  DLOG(WARNING) << "Rejecting remote request: Glic not opted-in for "
                   "experimental triggering.";
  if (message.has_server_channel_configuration() && message_sender_) {
    components_sharing_message::SharingMessage response_message;
    auto* triggering_response =
        response_message.mutable_glic_experimental_triggering();
    auto* response = triggering_response->mutable_response();
    auto* task_update = response->mutable_task_update();

    task_update->set_state(
        components_sharing_message::GlicExperimentalTriggering::
            ExperimentalTriggeringResponse::TaskUpdate::FAILED);
    task_update->set_data_type(
        components_sharing_message::GlicExperimentalTriggering::
            ExperimentalTriggeringResponse::TaskUpdate::ERROR_MESSAGE);
    task_update->set_data("User is not opted in to experimental triggering.");

    message_sender_->SendMessageToServerTarget(
        message.server_channel_configuration(), kUpdateMessageTimeout,
        std::move(response_message), SharingMessageSender::DelegateType::kFCM,
        base::BindOnce(
            [](SharingSendMessageResult result,
               std::unique_ptr<components_sharing_message::ResponseMessage>
                   response) {
              if (result != SharingSendMessageResult::kSuccessful) {
                DLOG(WARNING) << "Failed to send rejection response to server. "
                                 "Result: "
                              << static_cast<int>(result);
              }
            }));
  }
  std::move(done_callback).Run(nullptr);
  return true;
}

// TODO(b/505825633): Refine ResponseMessage errors for experimental triggering.
void GlicExperimentalTriggeringMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering));
  CHECK(message.has_glic_experimental_triggering());

  const auto& request = message.glic_experimental_triggering();

  tabs::TabInterface* active_tab = GetActiveTab();
  if (!active_tab) {
    DLOG(ERROR) << "No active tab found for Profile for "
                   "GlicExperimentalTriggering";
    std::move(done_callback).Run(nullptr);
    return;
  }

  if (!request.has_request()) {
    DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                     "request payload.";
    std::move(done_callback).Run(nullptr);
    return;
  }

  if (request.request().has_device_opt_in_request()) {
    ProcessDeviceOptInRequest(std::move(message), active_tab,
                              std::move(done_callback));
    return;
  }

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                         /*create=*/false);
  CHECK(glic_service);

  if (request.request().has_stop_actuation_request()) {
    ProcessStopActionRequest(std::move(message), active_tab, glic_service,
                             std::move(done_callback));
    return;
  }

  if (request.request().has_trigger_actuation_request()) {
    if (HandleUnavailableExperimentalTriggering(glic_service, message,
                                                done_callback)) {
      return;
    }

    auto options = CreateInvokeOptions(request, active_tab);
    if (message.has_server_channel_configuration()) {
      std::optional<int64_t> last_seen_sequence_number;
      if (request.has_task_metadata() &&
          request.task_metadata().has_sender_sequence_number()) {
        last_seen_sequence_number =
            request.task_metadata().sender_sequence_number();
      }

      options.on_client_connected = base::BindOnce(
          &GlicExperimentalTriggeringMessageHandler::
              OnClientConnectedForUpdates,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(*message.mutable_server_channel_configuration()),
          last_seen_sequence_number);
    }

    glic_service->InvokeWithAutoSubmit(
        glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
        std::move(options));

    std::move(done_callback).Run(nullptr);
    return;
  }

  DLOG(WARNING) << "Received GlicExperimentalTriggering message with no "
                   "actionable request.";
  std::move(done_callback).Run(nullptr);
}

void GlicExperimentalTriggeringMessageHandler::OnClientConnectedForUpdates(
    components_sharing_message::ServerChannelConfiguration server_channel,
    std::optional<int64_t> last_seen_sequence_number,
    base::WeakPtr<glic::GlicInstance> instance) {
  CHECK(instance);

  mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler> remote;
  auto listener_receiver = remote.InitWithNewPipeAndPassReceiver();

  instance->GetExperimentalTriggeringUpdates(std::move(remote),
                                             base::DoNothing());

  listeners_.Add(std::make_unique<UpdatesListener>(message_sender_,
                                                   std::move(server_channel),
                                                   last_seen_sequence_number),
                 std::move(listener_receiver));
}

void GlicExperimentalTriggeringMessageHandler::ProcessDeviceOptInRequest(
    components_sharing_message::SharingMessage message,
    tabs::TabInterface* active_tab,
    DoneCallback done_callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (!opt_in_controller_) {
    opt_in_controller_ =
        std::make_unique<glic::GlicExperimentalOptInController>(profile_);
  }

  opt_in_controller_->ShowDialog(active_tab->GetContents());
#endif

  std::move(done_callback).Run(nullptr);
}

void GlicExperimentalTriggeringMessageHandler::ProcessStopActionRequest(
    components_sharing_message::SharingMessage message,
    tabs::TabInterface* active_tab,
    glic::GlicKeyedService* glic_service,
    DoneCallback done_callback) {
  const auto& request = message.glic_experimental_triggering();

  glic::GlicInstance* instance = glic_service->GetInstanceForTab(active_tab);
  if (!instance) {
    DLOG(WARNING) << "GlicInstance not found for active tab, cannot stop task.";
    std::move(done_callback).Run(nullptr);
    return;
  }

  bool stopped = false;
  if (!request.has_task_metadata() || !request.task_metadata().has_task_id()) {
    DLOG(WARNING)
        << "Missing task metadata or task ID in StopActuationRequest.";
  } else {
    int task_id_int = 0;
    if (!base::StringToInt(request.task_metadata().task_id(), &task_id_int)) {
      DLOG(WARNING) << "Invalid task ID format: "
                    << request.task_metadata().task_id();
    } else if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
      actor_service->StopTask(actor::TaskId(task_id_int),
                              actor::ActorTask::StoppedReason::kStoppedByUser);
      stopped = true;
    }
  }

  if (message.has_server_channel_configuration() && message_sender_) {
    components_sharing_message::SharingMessage response_message;
    auto* triggering_response =
        response_message.mutable_glic_experimental_triggering();
    auto* response = triggering_response->mutable_response();
    auto* task_update = response->mutable_task_update();

    if (stopped) {
      task_update->set_state(
          components_sharing_message::GlicExperimentalTriggering::
              ExperimentalTriggeringResponse::TaskUpdate::STOPPED);
    } else {
      task_update->set_state(
          components_sharing_message::GlicExperimentalTriggering::
              ExperimentalTriggeringResponse::TaskUpdate::FAILED);
      task_update->set_data_type(
          components_sharing_message::GlicExperimentalTriggering::
              ExperimentalTriggeringResponse::TaskUpdate::ERROR_MESSAGE);
      task_update->set_data(
          "Failed to stop task due to missing or invalid metadata.");
    }

    message_sender_->SendMessageToServerTarget(
        message.server_channel_configuration(), kUpdateMessageTimeout,
        std::move(response_message), SharingMessageSender::DelegateType::kFCM,
        base::BindOnce(
            [](SharingSendMessageResult result,
               std::unique_ptr<components_sharing_message::ResponseMessage>
                   response) {
              if (result != SharingSendMessageResult::kSuccessful) {
                DLOG(WARNING) << "Failed to send response to server. Result: "
                              << static_cast<int>(result);
              }
            }));
  }

  std::move(done_callback).Run(nullptr);
}
