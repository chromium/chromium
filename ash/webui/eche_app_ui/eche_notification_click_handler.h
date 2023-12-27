// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_

#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_click_handler.h"
#include "chromeos/ash/components/phonehub/notification_interaction_handler.h"
#include "chromeos/ash/components/phonehub/phone_model.h"

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

class LaunchAppHelper;
class AppsLaunchInfoProvider;

// Handles notification clicks originating from Phone Hub notifications.
class EcheNotificationClickHandler : public phonehub::NotificationClickHandler,
                                     public FeatureStatusProvider::Observer {
 public:
  EcheNotificationClickHandler(
      phonehub::PhoneHubManager* phone_hub_manager,
      FeatureStatusProvider* feature_status_provider,
      LaunchAppHelper* launch_app_helper,
      AppsLaunchInfoProvider* apps_launch_info_provider);
  ~EcheNotificationClickHandler() override;

  EcheNotificationClickHandler(const EcheNotificationClickHandler&) = delete;
  EcheNotificationClickHandler& operator=(const EcheNotificationClickHandler&) =
      delete;

  // phonehub::NotificationClickHandler:
  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

 private:
  bool IsClickable(FeatureStatus status);

  raw_ptr<phonehub::NotificationInteractionHandler> handler_;
  raw_ptr<phonehub::PhoneModel> phone_model_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<LaunchAppHelper, DanglingUntriaged> launch_app_helper_;
  raw_ptr<AppsLaunchInfoProvider, DanglingUntriaged> apps_launch_info_provider_;
  bool is_click_handler_set_ = false;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
