// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/optimization_guide/optimization_guide_message_handler.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/push_notification.pb.h"
#include "components/sharing_message/proto/optimization_guide_push_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

using components_sharing_message::OptimizationGuidePushNotification;

// static
std::unique_ptr<OptimizationGuideMessageHandler>
OptimizationGuideMessageHandler::Create(Profile* profile) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());
  DCHECK(optimization_guide::features::IsPushNotificationsEnabled());
  optimization_guide::PushNotificationManager* push_notification_manager =
      nullptr;
  auto* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  OptimizationGuideLogger* optimization_guide_logger = nullptr;
  if (optimization_guide_service) {
    auto* hint_manager = optimization_guide_service->GetHintsManager();
    DCHECK(hint_manager);
    push_notification_manager = hint_manager->push_notification_manager();
    optimization_guide_logger =
        optimization_guide_service->GetOptimizationGuideLogger();
  }

  return std::make_unique<OptimizationGuideMessageHandler>(
      push_notification_manager, optimization_guide_logger);
}

OptimizationGuideMessageHandler::OptimizationGuideMessageHandler(
    optimization_guide::PushNotificationManager* push_notification_manager,
    OptimizationGuideLogger* optimization_guide_logger)
    : push_notification_manager_(push_notification_manager),
      optimization_guide_logger_(optimization_guide_logger) {}

OptimizationGuideMessageHandler::~OptimizationGuideMessageHandler() = default;

void OptimizationGuideMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_optimization_guide_push_notification());

  // Parse the payload to HintNotificationPayload.
  const OptimizationGuidePushNotification& notification_proto =
      message.optimization_guide_push_notification();
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  if (!hint_notification_payload.ParseFromString(
          notification_proto.hint_notification_payload_bytes())) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::HINTS_NOTIFICATIONS,
        optimization_guide_logger_,
        "Can't parse the HintNotificationPayload proto from "
        "OptimizationGuidePushNotification.");
    std::move(done_callback).Run(/*response=*/nullptr);
    return;
  }
  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::HINTS_NOTIFICATIONS,
      optimization_guide_logger_,
      base::StrCat({"Received push notification for type:",
                    optimization_guide::GetStringNameForOptimizationType(
                        hint_notification_payload.optimization_type()),
                    " hint_key:", hint_notification_payload.hint_key()}));

  // Pass the HintNotificationPayload to optimization guide, to delete
  // hints data from hints database.
  if (push_notification_manager_) {
    push_notification_manager_->OnNewPushNotification(
        hint_notification_payload);
  }

  std::move(done_callback).Run(/*response=*/nullptr);
}
