// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
  if (!coordinator_) {
    coordinator_ =
        std::make_unique<MessageHandlerCoordinatorDelegate>(profile_, this);
  }
}

GlicExperimentalTriggeringMessageHandler::
    ~GlicExperimentalTriggeringMessageHandler() = default;

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

tabs::TabInterface* GlicExperimentalTriggeringMessageHandler::GetActiveTab()
    const {
  BrowserWindowInterface* browser = GetBrowserWindow();
  return browser ? TabListInterface::From(browser)->GetActiveTab() : nullptr;
}

void GlicExperimentalTriggeringMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CheckFeatureFlags();
  CHECK(message.has_glic_experimental_triggering());

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kGlicForceFcmExperimentalTriggerFailure) &&
      message.glic_experimental_triggering().has_request() &&
      message.glic_experimental_triggering()
          .request()
          .has_trigger_actuation_request()) {
    // Test only: act as if the FCM message never arrived. Do not reply, do not
    // run `done_callback` (so no ack is sent) and do not record metrics.
    LOG(WARNING) << "GlicTrigger: Dropping TriggerActuationRequest for "
                 << message.glic_experimental_triggering().context_id()
                 << " because kGlicForceFcmExperimentalTriggerFailure is "
                    "enabled.";
    return;
  }
#endif

  glic::ScopedIncomingMessageResultLogger result_logger(
      glic::GlicMetricReporter(profile_),
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

  glic::GlicExperimentalTriggeringUpdateCallback update_callback =
      GetUpdateCallback(message);
  coordinator_->OnProtoMessage(
      context_id, request, std::move(result_logger), std::move(update_callback),
      base::BindOnce(
          [](SharingMessageHandler::DoneCallback callback,
             std::optional<glic::ExperimentalTriggeringResponse>
                 domain_response) {
            if (domain_response.has_value()) {
              std::move(callback).Run(
                  ResponseToResponseMessageProto(*domain_response));
            } else {
              std::move(callback).Run(nullptr);
            }
          },
          std::move(done_callback)));
}

void GlicExperimentalTriggeringMessageHandler::CheckFeatureFlags() const {
  CHECK(glic::GetGlicProviderSpec(profile_).SupportsFeature(
      glic::GlicFeature::kExperimentalTriggering));
}

glic::GlicExperimentalTriggeringUpdateCallback
GlicExperimentalTriggeringMessageHandler::GetUpdateCallback(
    components_sharing_message::SharingMessage& message) {
  return base::BindRepeating(
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
          glic::LogGlicExperimentalTriggeringProto(
              actor_service, "GlicExperimentalTriggering", response.context_id,
              outgoing_message.glic_experimental_triggering());
        }
        if (weak_message_handler->message_sender_) {
          std::optional<glic::GlicMetricReporter> terminal_metric_reporter;
          if (response.task_update.has_value() &&
              response.task_update->IsTerminal()) {
            terminal_metric_reporter.emplace(weak_message_handler->profile_);
          }
          weak_message_handler->message_sender_->SendMessageToServerTarget(
              server_channel, kUpdateMessageTimeout,
              std::move(outgoing_message),
              base::BindOnce(
                  [](std::optional<glic::GlicMetricReporter>
                         terminal_metric_reporter,
                     SharingSendMessageResult result,
                     std::unique_ptr<
                         components_sharing_message::ResponseMessage> unused) {
                    if (terminal_metric_reporter.has_value()) {
                      terminal_metric_reporter->RecordEnumeration(
                          "ExperimentalTriggering.TerminalUpdateSendResult",
                          result);
                    }
                    if (result != SharingSendMessageResult::kSuccessful) {
                      DLOG(ERROR)
                          << "Failed to send experimental triggering update "
                             "to server: "
                          << static_cast<int>(result);
                    }
                  },
                  std::move(terminal_metric_reporter)));
        }
      },
      weak_ptr_factory_.GetWeakPtr(),
      *message.mutable_server_channel_configuration());
}
