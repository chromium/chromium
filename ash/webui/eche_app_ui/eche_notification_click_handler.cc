// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"

namespace ash::eche_app {

EcheNotificationClickHandler::EcheNotificationClickHandler(
    phonehub::PhoneHubManager* phone_hub_manager,
    FeatureStatusProvider* feature_status_provider,
    LaunchAppHelper* launch_app_helper,
    AppsLaunchInfoProvider* apps_launch_info_provider)
    : feature_status_provider_(feature_status_provider),
      launch_app_helper_(launch_app_helper),
      apps_launch_info_provider_(apps_launch_info_provider) {
  handler_ = phone_hub_manager->GetNotificationInteractionHandler();
  phone_model_ = phone_hub_manager->GetPhoneModel();
  feature_status_provider_->AddObserver(this);
  if (handler_ && IsClickable(feature_status_provider_->GetStatus())) {
    handler_->AddNotificationClickHandler(this);
    is_click_handler_set_ = true;
  } else {
    PA_LOG(INFO)
        << "No Phone Hub interaction handler to set Eche click handler";
  }
}

EcheNotificationClickHandler::~EcheNotificationClickHandler() {
  feature_status_provider_->RemoveObserver(this);
  if (is_click_handler_set_ && handler_)
    handler_->RemoveNotificationClickHandler(this);
}

void EcheNotificationClickHandler::HandleNotificationClick(
    int64_t notification_id,
    const phonehub::Notification::AppMetadata& app_metadata) {
  const LaunchAppHelper::AppLaunchProhibitedReason prohibited_reason =
      launch_app_helper_->CheckAppLaunchProhibitedReason(
          feature_status_provider_->GetStatus());

  switch (prohibited_reason) {
    case LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited:
      base::UmaHistogramEnumeration(
          "Eche.AppStream.LaunchAttempt",
          mojom::AppStreamLaunchEntryPoint::NOTIFICATION);
      apps_launch_info_provider_->SetAppLaunchInfo(
          mojom::AppStreamLaunchEntryPoint::NOTIFICATION);
      launch_app_helper_->LaunchEcheApp(
          notification_id, app_metadata.package_name,
          app_metadata.visible_app_name, app_metadata.user_id,
          app_metadata.color_icon,
          phone_model_->phone_name().value_or(std::u16string()),
          apps_launch_info_provider_);
      break;
    case LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock:
      launch_app_helper_->ShowNotification(
          app_metadata.visible_app_name, /* message= */ std::nullopt,
          std::make_unique<LaunchAppHelper::NotificationInfo>(
              LaunchAppHelper::NotificationInfo::Category::kNative,
              LaunchAppHelper::NotificationInfo::NotificationType::
                  kScreenLock));
      break;
  }
}

void EcheNotificationClickHandler::OnFeatureStatusChanged() {
  if (!handler_) {
    PA_LOG(INFO)
        << "No Phone Hub interaction handler to set Eche click handler";
    return;
  }

  bool clickable = IsClickable(feature_status_provider_->GetStatus());
  if (!is_click_handler_set_ && clickable) {
    handler_->AddNotificationClickHandler(this);
    is_click_handler_set_ = true;
  } else if (is_click_handler_set_ && !clickable) {
    handler_->RemoveNotificationClickHandler(this);
    is_click_handler_set_ = false;
  }
}

bool EcheNotificationClickHandler::IsClickable(FeatureStatus status) {
  return status == FeatureStatus::kDisconnected ||
         status == FeatureStatus::kConnecting ||
         status == FeatureStatus::kConnected;
}

}  // namespace ash::eche_app
