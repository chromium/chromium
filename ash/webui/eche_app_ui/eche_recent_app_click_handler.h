// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_

#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/notification_click_handler.h"
#include "ash/components/phonehub/notification_interaction_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/components/phonehub/recent_app_click_observer.h"
#include "ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/callback.h"

namespace ash {
namespace eche_app {

class LaunchAppHelper;

// Handles recent app clicks originating from Phone Hub recent apps.
class EcheRecentAppClickHandler : public phonehub::NotificationClickHandler,
                                  public FeatureStatusProvider::Observer,
                                  public phonehub::RecentAppClickObserver {
 public:
  EcheRecentAppClickHandler(phonehub::PhoneHubManager* phone_hub_manager,
                            FeatureStatusProvider* feature_status_provider,
                            LaunchAppHelper* launch_app_helper);
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
      const phonehub::Notification::AppMetadata& app_metadata) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

 private:
  bool IsClickable(FeatureStatus status);

  phonehub::NotificationInteractionHandler* notification_handler_;
  phonehub::RecentAppsInteractionHandler* recent_apps_handler_;
  FeatureStatusProvider* feature_status_provider_;
  LaunchAppHelper* launch_app_helper_;
  bool is_click_handler_set_ = false;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_RECENT_APP_CLICK_HANDLER_H_
