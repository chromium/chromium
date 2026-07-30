// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/glic_experimental_triggering/actor_log.h"
#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_send_message_result.h"

namespace {

using TaskUpdate = components_sharing_message::GlicExperimentalTriggering::
    ExperimentalTriggeringResponse::TaskUpdate;

constexpr base::TimeDelta kUpdateMessageTimeout = base::Seconds(10);
constexpr int64_t kDefaultStartingRequestFailureSequenceNumber = 0;

std::unique_ptr<components_sharing_message::ResponseMessage>
CreateBaseResponseMessage(
    const std::string& context_id,
    const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
        request_task_metadata,
    int64_t sender_sequence_number) {
  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  auto* triggering = response->mutable_glic_experimental_triggering();
  triggering->set_context_id(context_id);
  auto* task_metadata = triggering->mutable_task_metadata();
  if (request_task_metadata) {
    if (request_task_metadata->has_conversation_id()) {
      task_metadata->set_conversation_id(
          request_task_metadata->conversation_id());
    }
    if (request_task_metadata->has_task_id()) {
      task_metadata->set_task_id(request_task_metadata->task_id());
    }
    if (request_task_metadata->has_sender_sequence_number()) {
      task_metadata->set_last_seen_sequence_number(
          request_task_metadata->sender_sequence_number());
    }
  }
  task_metadata->set_sender_sequence_number(sender_sequence_number);
  return response;
}

std::unique_ptr<components_sharing_message::ResponseMessage>
CreateResponseMessage(
    const std::string& context_id,
    TaskUpdate::State state,
    std::optional<TaskUpdate::DataType> data_type,
    const std::string& message,
    const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
        request_task_metadata,
    int64_t sender_sequence_number) {
  if (data_type == TaskUpdate::ERROR_MESSAGE) {
    DLOG(WARNING) << message;
  }
  if (!request_task_metadata) {
    return nullptr;
  }
  auto response = CreateBaseResponseMessage(context_id, request_task_metadata,
                                            sender_sequence_number);
  auto* triggering = response->mutable_glic_experimental_triggering();
  auto* task_update = triggering->mutable_response()->mutable_task_update();
  task_update->set_state(state);
  if (data_type.has_value()) {
    task_update->set_data_type(*data_type);
  }
  task_update->set_data(message);
  return response;
}

std::unique_ptr<components_sharing_message::ResponseMessage>
ResponseToResponseMessageProto(
    const glic::ExperimentalTriggeringResponse& domain_response) {
  components_sharing_message::SharingMessage message =
      glic::ResponseToProto(domain_response);
  if (!message.has_glic_experimental_triggering()) {
    return nullptr;
  }
  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  *response->mutable_glic_experimental_triggering() =
      std::move(*message.mutable_glic_experimental_triggering());
  return response;
}

}  // namespace

// TODO(crbug.com/533526458): Cleanup this wrapper delegate after refactoring
// migration completes.
class GlicExperimentalTriggeringMessageHandler::
    MessageHandlerCoordinatorDelegate
    : public glic::GlicExperimentalTriggeringCoordinator {
 public:
  MessageHandlerCoordinatorDelegate(
      Profile* profile,
      GlicExperimentalTriggeringMessageHandler* message_handler)
      : glic::GlicExperimentalTriggeringCoordinator(profile),
        message_handler_(message_handler) {}

 protected:
  BrowserWindowInterface* GetBrowserWindow() const override {
    return message_handler_->GetBrowserWindow();
  }

  tabs::TabInterface* GetActiveTab() const override {
    return message_handler_->GetActiveTab();
  }

 private:
  const raw_ptr<GlicExperimentalTriggeringMessageHandler> message_handler_;
};

GlicExperimentalTriggeringMessageHandler::
    GlicExperimentalTriggeringMessageHandler(
        Profile* profile,
        SharingMessageSender* message_sender)
    : GlicExperimentalTriggeringMessageHandler(profile,
                                               message_sender,
                                               nullptr) {}

GlicExperimentalTriggeringMessageHandler::
    GlicExperimentalTriggeringMessageHandler(
        Profile* profile,
        SharingMessageSender* message_sender,
        std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator>
            coordinator)
    : profile_(profile),
      message_sender_(message_sender),
      coordinator_(std::move(coordinator)) {
  CHECK(profile_);
  CHECK(message_sender_);
  if (!coordinator_) {
    coordinator_ =
        std::make_unique<MessageHandlerCoordinatorDelegate>(profile_, this);
  }
}

GlicExperimentalTriggeringMessageHandler::
    ~GlicExperimentalTriggeringMessageHandler() {
  CancelAllPendingMessages();
}

void GlicExperimentalTriggeringMessageHandler::CancelAllPendingMessages() {
  for (auto& [context_id, message_list] : pending_messages_) {
    for (auto& pending_message : message_list) {
      if (pending_message.done_callback) {
        std::move(pending_message.done_callback)
            .Run(CreateResponseMessage(
                context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                "Message cancelled: Handler being destroyed.", nullptr,
                kDefaultStartingRequestFailureSequenceNumber));
      }
    }
  }
}

size_t
GlicExperimentalTriggeringMessageHandler::GetUpdatesHandlerMapSizeForTesting()
    const {
  return coordinator_ ? coordinator_->GetUpdatesHandlerMapSizeForTesting() : 0;
}

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

  glic::ScopedIncomingMessageResultLogger result_logger(
      glic::ScopedIncomingMessageResultLogger::Channel::kSharingMessage);

  // If no `context_id` is present in the request, we generate one that
  // may be used by the sender in follow up actuation requests.
  std::string context_id = message.glic_experimental_triggering().context_id();
  if (context_id.empty()) {
    context_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    message.mutable_glic_experimental_triggering()->set_context_id(context_id);
  }
  const auto& request = message.glic_experimental_triggering();

  const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
      request_metadata =
          request.has_task_metadata() ? &request.task_metadata() : nullptr;

  if (request.has_glic_experimental_triggering_version() &&
      !IsVersionSupported(request.glic_experimental_triggering_version())) {
    result_logger.set_result(
        glic::GlicExperimentalTriggeringIncomingMessageResult::
            kVersionMismatchOrUnavailable);
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Rejected: version mismatch or unavailable.", request_metadata,
            kDefaultStartingRequestFailureSequenceNumber));
    return;
  }

  if (!message.has_server_channel_configuration()) {
    result_logger.set_result(
        glic::GlicExperimentalTriggeringIncomingMessageResult::
            kMissingServerChannel);
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Received GlicExperimentalTriggering message "
            "with no server configuration channel data.",
            request_metadata, kDefaultStartingRequestFailureSequenceNumber));
    return;
  }

  if (!request.has_request() && !request.has_task_metadata_updated()) {
    if (profile_) {
      actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(profile_);
      LogGlicExperimentalTriggeringProto(
          actor_service, "GlicExperimentalTriggering", "", request);
    }

    result_logger.set_result(
        glic::GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload);
    std::move(done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Received GlicExperimentalTriggering "
            "message with no request payload.",
            request_metadata, kDefaultStartingRequestFailureSequenceNumber));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kGlicBackgroundTriggering)) {
    VLOG(1) << "GlicTrigger: Triggering ActorForegroundService from native";
    if (profile_) {
      actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(profile_);
      if (actor_service) {
        if (!actor_service_observation_.IsObserving()) {
          actor_service_observation_.Observe(actor_service);
        }
        bool already_pending = pending_messages_.contains(context_id);
        pending_messages_[context_id].push_back(
            MessageData{std::move(message), std::move(done_callback),
                        std::move(result_logger)});
        if (!already_pending) {
          // TODO(crbug.com/540892797): Avoid starting an Android Foreground
          // Service (FGS) if it is redundant or unnecessary:
          // 1. If Chrome is already in the foreground, starting an FGS is
          //    unnecessary and adds asynchronous JNI/IPC overhead.
          // 2. If a task is already active and executing (meaning the initial
          //    tab preparation is complete and the queue is cleared),
          //    subsequent control messages (e.g., INTERRUPT or STOP) should not
          //    trigger EnsureForegroundServiceStarted() again.
          actor_service->EnsureForegroundServiceStarted(context_id);
        }
        return;
      }
    }
  } else {
    VLOG(1) << "GlicTrigger: GlicBackgroundTriggering feature disabled, "
               "skipping FGS trigger";
  }
#endif

  ProcessValidatedMessage(std::move(message), context_id,
                          std::move(done_callback), std::move(result_logger),
                          nullptr);
}

void GlicExperimentalTriggeringMessageHandler::ProcessValidatedMessage(
    components_sharing_message::SharingMessage message,
    const std::string& context_id,
    SharingMessageHandler::DoneCallback done_callback,
    glic::ScopedIncomingMessageResultLogger result_logger,
    tabs::TabInterface* prepared_tab) {
  const auto& request = message.glic_experimental_triggering();
  CHECK(!context_id.empty());

  if (profile_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(profile_);
    LogGlicExperimentalTriggeringProto(
        actor_service, "GlicExperimentalTriggering", context_id, request);
  }

  glic::ExperimentalTriggeringRequest domain_request =
      glic::ProtoToRequest(request);
  domain_request.context_id = context_id;

  auto update_callback = base::BindRepeating(
      [](base::WeakPtr<GlicExperimentalTriggeringMessageHandler>
             weak_message_handler,
         components_sharing_message::ServerChannelConfiguration server_channel,
         glic::ExperimentalTriggeringResponse response) {
        if (!weak_message_handler) {
          return;
        }
        components_sharing_message::SharingMessage outgoing_message =
            glic::ResponseToProto(response);
        if (weak_message_handler->profile_) {
          actor::ActorKeyedService* actor_service =
              actor::ActorKeyedService::Get(weak_message_handler->profile_);
          LogGlicExperimentalTriggeringProto(
              actor_service, "GlicExperimentalTriggering", response.context_id,
              outgoing_message.glic_experimental_triggering());
        }
        if (weak_message_handler->message_sender_) {
          weak_message_handler->message_sender_->SendMessageToServerTarget(
              server_channel, kUpdateMessageTimeout,
              std::move(outgoing_message),
              base::BindOnce(
                  [](SharingSendMessageResult result,
                     std::unique_ptr<
                         components_sharing_message::ResponseMessage> unused) {
                    if (result != SharingSendMessageResult::kSuccessful) {
                      DLOG(ERROR)
                          << "Failed to send experimental triggering update "
                             "to server: "
                          << static_cast<int>(result);
                    }
                  }));
        }
      },
      weak_ptr_factory_.GetWeakPtr(),
      *message.mutable_server_channel_configuration());

  std::optional<glic::ExperimentalTriggeringResponse> domain_response =
      coordinator_->OnRequest(context_id, domain_request,
                              std::move(result_logger),
                              std::move(update_callback), prepared_tab);

  if (domain_response.has_value()) {
    std::move(done_callback)
        .Run(ResponseToResponseMessageProto(*domain_response));
  } else {
    std::move(done_callback).Run(nullptr);
  }
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

void GlicExperimentalTriggeringMessageHandler::OnBackgroundTabPrepared(
    tabs::TabInterface* tab,
    const std::string& context_id) {
  auto it = pending_messages_.find(context_id);
  if (it == pending_messages_.end()) {
    return;
  }
  std::vector<MessageData> messages = std::move(it->second);
  pending_messages_.erase(it);

  if (pending_messages_.empty()) {
    actor_service_observation_.Reset();
  }

  for (auto& pending_message : messages) {
    ProcessValidatedMessage(std::move(pending_message.message), context_id,
                            std::move(pending_message.done_callback),
                            std::move(pending_message.result_logger), tab);
  }
}

void GlicExperimentalTriggeringMessageHandler::OnBackgroundSetupFailed(
    const std::string& context_id) {
  auto it = pending_messages_.find(context_id);
  if (it == pending_messages_.end()) {
    return;
  }
  std::vector<MessageData> messages = std::move(it->second);
  pending_messages_.erase(it);

  if (pending_messages_.empty()) {
    actor_service_observation_.Reset();
  }

  for (auto& pending_message : messages) {
    const auto& request =
        pending_message.message.glic_experimental_triggering();
    const components_sharing_message::GlicExperimentalTriggering::TaskMetadata*
        request_metadata =
            request.has_task_metadata() ? &request.task_metadata() : nullptr;

    std::move(pending_message.done_callback)
        .Run(CreateResponseMessage(
            context_id, TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
            "Background setup failed.", request_metadata,
            kDefaultStartingRequestFailureSequenceNumber));
  }
}
