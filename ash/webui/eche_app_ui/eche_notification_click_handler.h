// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_

#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/notification_click_handler.h"
#include "ash/components/phonehub/notification_interaction_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/webui/eche_app_ui/eche_display_stream_handler.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/callback.h"

namespace ash {
namespace eche_app {

class LaunchAppHelper;

// Handles notification clicks originating from Phone Hub notifications.
class EcheNotificationClickHandler : public phonehub::NotificationClickHandler,
                                     public FeatureStatusProvider::Observer,
                                     public EcheDisplayStreamHandler::Observer {
 public:
  EcheNotificationClickHandler(
      phonehub::PhoneHubManager* phone_hub_manager,
      FeatureStatusProvider* feature_status_provider,
      LaunchAppHelper* launch_app_helper,
      EcheDisplayStreamHandler* display_stream_handler);
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

  // EcheDisplayStreamHandler::Observer:
  void OnStartStreaming() override;

  // Test helpers, we need this to confirm the streaming will work as expected.
  bool waiting_for_streaming_to_show() {
    return is_waiting_for_streaming_to_show_;
  }

 private:
  bool IsClickable(FeatureStatus status);

  bool NeedClose(FeatureStatus status);

  phonehub::NotificationInteractionHandler* handler_;
  FeatureStatusProvider* feature_status_provider_;
  LaunchAppHelper* launch_app_helper_;
  EcheDisplayStreamHandler* display_stream_handler_;
  bool is_click_handler_set_ = false;
  bool is_waiting_for_streaming_to_show_ = false;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_CLICK_HANDLER_H_
