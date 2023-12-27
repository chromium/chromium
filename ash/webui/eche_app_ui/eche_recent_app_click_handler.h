// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_

#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_click_handler.h"
#include "chromeos/ash/components/phonehub/notification_interaction_handler.h"
#include "chromeos/ash/components/phonehub/recent_app_click_observer.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

class LaunchAppHelper;
class AppsLaunchInfoProvider;

// Handles recent app clicks originating from Phone Hub recent apps.
class EcheRecentAppClickHandler
    : public phonehub::NotificationClickHandler,
      public FeatureStatusProvider::Observer,
      public phonehub::RecentAppClickObserver,
      public EcheStreamStatusChangeHandler::Observer {
 public:
  EcheRecentAppClickHandler(
      phonehub::PhoneHubManager* phone_hub_manager,
      FeatureStatusProvider* feature_status_provider,
      LaunchAppHelper* launch_app_helper,
      EcheStreamStatusChangeHandler* stream_status_change_handler,
      AppsLaunchInfoProvider* apps_launch_info_provider);
  ~EcheRecentAppClickHandler() override;

  EcheRecentAppClickHandler(const EcheRecentAppClickHandler&) = delete;
  EcheRecentAppClickHandler& operator=(const EcheRecentAppClickHandler&) =
      delete;

  // phonehub::NotificationClickHandler:
  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) override;

  // phonehub::RecentAppClickObserver:
  void OnRecentAppClicked(
      const phonehub::Notification::AppMetadata& app_metadata,
      mojom::AppStreamLaunchEntryPoint entrypoint) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // EcheStreamStatusChangeHandler::Observer:
  void OnStartStreaming() override {}
  void OnStreamStatusChanged(mojom::StreamStatus status) override;

 private:
  bool IsClickable(FeatureStatus status);

  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;
  raw_ptr<phonehub::NotificationInteractionHandler> notification_handler_;
  raw_ptr<phonehub::RecentAppsInteractionHandler> recent_apps_handler_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<LaunchAppHelper, DanglingUntriaged> launch_app_helper_;
  raw_ptr<EcheStreamStatusChangeHandler> stream_status_change_handler_;
  std::vector<phonehub::Notification::AppMetadata> to_stream_apps_;
  raw_ptr<AppsLaunchInfoProvider, DanglingUntriaged> apps_launch_info_provider_;
  bool is_click_handler_set_ = false;
  bool is_stream_started_ = false;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
