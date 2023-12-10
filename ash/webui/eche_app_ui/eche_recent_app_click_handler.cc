// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"

namespace ash {
namespace eche_app {

EcheRecentAppClickHandler::EcheRecentAppClickHandler(
    phonehub::PhoneHubManager* phone_hub_manager,
    FeatureStatusProvider* feature_status_provider,
    LaunchAppHelper* launch_app_helper,
    EcheStreamStatusChangeHandler* stream_status_change_handler,
    AppsLaunchInfoProvider* apps_launch_info_provider)
    : phone_hub_manager_(phone_hub_manager),
      feature_status_provider_(feature_status_provider),
      launch_app_helper_(launch_app_helper),
      stream_status_change_handler_(stream_status_change_handler),
      apps_launch_info_provider_(apps_launch_info_provider) {
  notification_handler_ =
      phone_hub_manager->GetNotificationInteractionHandler();
  recent_apps_handler_ = phone_hub_manager->GetRecentAppsInteractionHandler();
  feature_status_provider_->AddObserver(this);
  stream_status_change_handler_->AddObserver(this);

  if (notification_handler_ && recent_apps_handler_ &&
      IsClickable(feature_status_provider_->GetStatus())) {
    notification_handler_->AddNotificationClickHandler(this);
    recent_apps_handler_->AddRecentAppClickObserver(this);
    is_click_handler_set_ = true;
  }
}

EcheRecentAppClickHandler::~EcheRecentAppClickHandler() {
  feature_status_provider_->RemoveObserver(this);
  stream_status_change_handler_->RemoveObserver(this);
  if (notification_handler_)
    notification_handler_->RemoveNotificationClickHandler(this);
  if (recent_apps_handler_)
    recent_apps_handler_->RemoveRecentAppClickObserver(this);
}

void EcheRecentAppClickHandler::HandleNotificationClick(
    int64_t notification_id,
    const phonehub::Notification::AppMetadata& app_metadata) {
  DCHECK(phone_hub_manager_);
  // Add the notification’s `app_metadata` that the user clicks to recents list
  // if the stream is already started. If the stream hasn't started yet, we keep
  // this notification’s `app_metadata` until this notification is streaming
  // successfully.
  if (is_stream_started_) {
    const LaunchAppHelper::AppLaunchProhibitedReason prohibited_reason =
        launch_app_helper_->CheckAppLaunchProhibitedReason(
            feature_status_provider_->GetStatus());
    if (recent_apps_handler_ &&
        prohibited_reason ==
            LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited) {
      recent_apps_handler_->NotifyRecentAppAddedOrUpdated(app_metadata,
                                                          base::Time::Now());
    }
  } else {
    // Only cache the last `app_metadata` when we receive multiple notification
    // click events.
    to_stream_apps_.clear();
    to_stream_apps_.emplace_back(app_metadata);
  }
}

void EcheRecentAppClickHandler::OnRecentAppClicked(
    const phonehub::Notification::AppMetadata& app_metadata,
    mojom::AppStreamLaunchEntryPoint entrypoint) {
  const LaunchAppHelper::AppLaunchProhibitedReason prohibited_reason =
      launch_app_helper_->CheckAppLaunchProhibitedReason(
          feature_status_provider_->GetStatus());
  switch (prohibited_reason) {
    case LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited:
      base::UmaHistogramEnumeration("Eche.AppStream.LaunchAttempt", entrypoint);

      to_stream_apps_.emplace_back(app_metadata);
      apps_launch_info_provider_->SetAppLaunchInfo(entrypoint);
      launch_app_helper_->LaunchEcheApp(
          /*notification_id=*/std::nullopt, app_metadata.package_name,
          app_metadata.visible_app_name, app_metadata.user_id,
          app_metadata.color_icon,
          phone_hub_manager_->GetPhoneModel()->phone_name().value_or(
              std::u16string()),
          apps_launch_info_provider_);
      break;
    case LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock:
      launch_app_helper_->ShowNotification(
          /* title= */ app_metadata.visible_app_name,
          /* message= */ std::nullopt,
          std::make_unique<LaunchAppHelper::NotificationInfo>(
              LaunchAppHelper::NotificationInfo::Category::kNative,
              LaunchAppHelper::NotificationInfo::NotificationType::
                  kScreenLock));
      break;
  }
}

void EcheRecentAppClickHandler::OnFeatureStatusChanged() {
  if (!notification_handler_ || !recent_apps_handler_) {
    return;
  }
  bool clickable = IsClickable(feature_status_provider_->GetStatus());
  if (!is_click_handler_set_ && clickable) {
    notification_handler_->AddNotificationClickHandler(this);
    recent_apps_handler_->AddRecentAppClickObserver(this);
    is_click_handler_set_ = true;
  } else if (is_click_handler_set_ && !clickable) {
    notification_handler_->RemoveNotificationClickHandler(this);
    recent_apps_handler_->RemoveRecentAppClickObserver(this);
    is_click_handler_set_ = false;
  }
}

void EcheRecentAppClickHandler::OnStreamStatusChanged(
    mojom::StreamStatus status) {
  if (status == eche_app::mojom::StreamStatus::kStreamStatusStarted) {
    is_stream_started_ = true;
    const LaunchAppHelper::AppLaunchProhibitedReason prohibited_reason =
        launch_app_helper_->CheckAppLaunchProhibitedReason(
            feature_status_provider_->GetStatus());
    if (recent_apps_handler_ &&
        prohibited_reason ==
            LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited)
      for (const auto& app_metadata : to_stream_apps_) {
        recent_apps_handler_->NotifyRecentAppAddedOrUpdated(app_metadata,
                                                            base::Time::Now());
      }
  } else if (status == eche_app::mojom::StreamStatus::kStreamStatusStopped) {
    is_stream_started_ = false;
    to_stream_apps_.clear();
  }
}

bool EcheRecentAppClickHandler::IsClickable(FeatureStatus status) {
  return status == FeatureStatus::kDisconnected ||
         status == FeatureStatus::kConnecting ||
         status == FeatureStatus::kConnected;
}

}  // namespace eche_app
}  // namespace ash
