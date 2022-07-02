// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_

#include "ash/components/phonehub/fake_browser_tabs_model_provider.h"
#include "ash/components/phonehub/fake_camera_roll_manager.h"
#include "ash/components/phonehub/fake_connection_scheduler.h"
#include "ash/components/phonehub/fake_do_not_disturb_controller.h"
#include "ash/components/phonehub/fake_feature_status_provider.h"
#include "ash/components/phonehub/fake_find_my_device_controller.h"
#include "ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "ash/components/phonehub/fake_notification_interaction_handler.h"
#include "ash/components/phonehub/fake_notification_manager.h"
#include "ash/components/phonehub/fake_onboarding_ui_tracker.h"
#include "ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "ash/components/phonehub/fake_screen_lock_manager.h"
#include "ash/components/phonehub/fake_tether_controller.h"
#include "ash/components/phonehub/fake_user_action_recorder.h"
#include "ash/components/phonehub/mutable_phone_model.h"
#include "ash/components/phonehub/phone_hub_manager.h"

namespace ash {
namespace phonehub {

// This class initializes fake versions of the core business logic of Phone Hub.
class FakePhoneHubManager : public PhoneHubManager {
 public:
  FakePhoneHubManager();
  ~FakePhoneHubManager() override;

  FakeDoNotDisturbController* fake_do_not_disturb_controller() {
    return &fake_do_not_disturb_controller_;
  }

  FakeFeatureStatusProvider* fake_feature_status_provider() {
    return &fake_feature_status_provider_;
  }

  FakeFindMyDeviceController* fake_find_my_device_controller() {
    return &fake_find_my_device_controller_;
  }

  FakeMultideviceFeatureAccessManager*
  fake_multidevice_feature_access_manager() {
    return &fake_multidevice_feature_access_manager_;
  }

  FakeNotificationInteractionHandler* fake_notification_interaction_handler() {
    return &fake_notification_interaction_handler_;
  }

  FakeNotificationManager* fake_notification_manager() {
    return &fake_notification_manager_;
  }

  FakeOnboardingUiTracker* fake_onboarding_ui_tracker() {
    return &fake_onboarding_ui_tracker_;
  }

  FakeRecentAppsInteractionHandler* fake_recent_apps_interaction_handler() {
    return &fake_recent_apps_interaction_handler_;
  }

  FakeScreenLockManager* fake_screen_lock_manager() {
    return &fake_screen_lock_manager_;
  }

  MutablePhoneModel* mutable_phone_model() { return &mutable_phone_model_; }

  FakeTetherController* fake_tether_controller() {
    return &fake_tether_controller_;
  }

  FakeConnectionScheduler* fake_connection_scheduler() {
    return &fake_connection_scheduler_;
  }

  FakeUserActionRecorder* fake_user_action_recorder() {
    return &fake_user_action_recorder_;
  }

  FakeBrowserTabsModelProvider* fake_browser_tabs_model_provider() {
    return &fake_browser_tabs_model_provider_;
  }

  FakeCameraRollManager* fake_camera_roll_manager() {
    return &fake_camera_roll_manager_;
  }

  void set_host_last_seen_timestamp(absl::optional<base::Time> timestamp) {
    host_last_seen_timestamp_ = timestamp;
  }

 private:
  // PhoneHubManager:
  BrowserTabsModelProvider* GetBrowserTabsModelProvider() override;
  CameraRollManager* GetCameraRollManager() override;
  DoNotDisturbController* GetDoNotDisturbController() override;
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  FindMyDeviceController* GetFindMyDeviceController() override;
  MultideviceFeatureAccessManager* GetMultideviceFeatureAccessManager()
      override;
  NotificationInteractionHandler* GetNotificationInteractionHandler() override;
  NotificationManager* GetNotificationManager() override;
  OnboardingUiTracker* GetOnboardingUiTracker() override;
  PhoneModel* GetPhoneModel() override;
  RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() override;
  ScreenLockManager* GetScreenLockManager() override;
  TetherController* GetTetherController() override;
  ConnectionScheduler* GetConnectionScheduler() override;
  UserActionRecorder* GetUserActionRecorder() override;
  void GetHostLastSeenTimestamp(
      base::OnceCallback<void(absl::optional<base::Time>)> callback) override;

  FakeDoNotDisturbController fake_do_not_disturb_controller_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  FakeFindMyDeviceController fake_find_my_device_controller_;
  FakeMultideviceFeatureAccessManager fake_multidevice_feature_access_manager_;
  FakeNotificationInteractionHandler fake_notification_interaction_handler_;
  FakeNotificationManager fake_notification_manager_;
  FakeOnboardingUiTracker fake_onboarding_ui_tracker_;
  MutablePhoneModel mutable_phone_model_;
  FakeRecentAppsInteractionHandler fake_recent_apps_interaction_handler_;
  FakeScreenLockManager fake_screen_lock_manager_;
  FakeTetherController fake_tether_controller_;
  FakeConnectionScheduler fake_connection_scheduler_;
  FakeUserActionRecorder fake_user_action_recorder_;
  FakeBrowserTabsModelProvider fake_browser_tabs_model_provider_;
  FakeCameraRollManager fake_camera_roll_manager_;
  absl::optional<base::Time> host_last_seen_timestamp_ = absl::nullopt;
};

}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
using ::ash::phonehub::FakePhoneHubManager;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
