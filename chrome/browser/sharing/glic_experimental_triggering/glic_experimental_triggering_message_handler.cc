// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
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
  options.target.surface = tab;

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
      components_sharing_message::ServerChannelConfiguration server_channel)
      : message_sender_(message_sender),
        server_channel_(std::move(server_channel)) {}

  void OnUpdate(glic::mojom::ExperimentalTriggeringUpdatePtr update,
                glic::mojom::SubscriberObservationType observation) override {
    components_sharing_message::SharingMessage message;
    auto* glic_response =
        message.mutable_glic_experimental_triggering()->mutable_response();
    auto* task_update = glic_response->mutable_task_update();

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
  return browser ? browser->GetActiveTabInterface() : nullptr;
}

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

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                         /*create=*/false);
  CHECK(glic_service);

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      CreateInvokeOptions(request, active_tab));

  if (message.has_server_channel_configuration()) {
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
        remote;
    auto listener_receiver = remote.InitWithNewPipeAndPassReceiver();

    glic_service->instance_coordinator().GetExperimentalTriggeringUpdates(
        std::move(remote), base::DoNothing());

    listeners_.Add(
        std::make_unique<UpdatesListener>(
            message_sender_,
            std::move(*message.mutable_server_channel_configuration())),
        std::move(listener_receiver));
  }

  std::move(done_callback).Run(nullptr);
}
