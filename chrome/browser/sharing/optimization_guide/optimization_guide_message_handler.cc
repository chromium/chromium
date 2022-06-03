// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/optimization_guide/optimization_guide_message_handler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/sharing/proto/optimization_guide_push_notification.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/push_notification.pb.h"

using chrome_browser_sharing::OptimizationGuidePushNotification;

// static
std::unique_ptr<OptimizationGuideMessageHandler>
OptimizationGuideMessageHandler::Create(Profile* profile) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());
  DCHECK(optimization_guide::features::IsPushNotificationsEnabled());
  optimization_guide::PushNotificationManager* push_notification_manager =
      nullptr;
  auto* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_service) {
    auto* hint_manager = optimization_guide_service->GetHintsManager();
    DCHECK(hint_manager);
    push_notification_manager = hint_manager->push_notification_manager();
  }

  return std::make_unique<OptimizationGuideMessageHandler>(
      push_notification_manager);
}

OptimizationGuideMessageHandler::OptimizationGuideMessageHandler(
    optimization_guide::PushNotificationManager* push_notification_manager)
    : push_notification_manager_(push_notification_manager) {}

OptimizationGuideMessageHandler::~OptimizationGuideMessageHandler() = default;

void OptimizationGuideMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_optimization_guide_push_notification());

  // Parse the payload to HintNotificationPayload.
  const OptimizationGuidePushNotification& notification_proto =
      message.optimization_guide_push_notification();
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  if (!hint_notification_payload.ParseFromString(
          notification_proto.hint_notification_payload_bytes())) {
    LOG(ERROR) << "Can't parse the HintNotificationPayload proto from "
                  "OptimizationGuidePushNotification.";
    std::move(done_callback).Run(/*response=*/nullptr);
    return;
  }

  // Pass the HintNotificationPayload to optimization guide, to delete
  // hints data from hints database.
  if (push_notification_manager_) {
    push_notification_manager_->OnNewPushNotification(
        hint_notification_payload);
  }

  std::move(done_callback).Run(/*response=*/nullptr);
}
