// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_LAUNCH_APP_HELPER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_LAUNCH_APP_HELPER_H_

#include "ash/webui/eche_app_ui/launch_app_helper.h"

namespace ash {
namespace eche_app {

class FakeLaunchAppHelper : public LaunchAppHelper {
 public:
  FakeLaunchAppHelper(phonehub::PhoneHubManager* phone_hub_manager,
                      LaunchEcheAppFunction launch_eche_app_function,
                      LaunchNotificationFunction launch_notification_function,
                      CloseNotificationFunction close_notification_function);
  ~FakeLaunchAppHelper() override;
  FakeLaunchAppHelper(const FakeLaunchAppHelper&) = delete;
  FakeLaunchAppHelper& operator=(const FakeLaunchAppHelper&) = delete;

  void SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason reason);

  // LaunchAppHelper:
  LaunchAppHelper::AppLaunchProhibitedReason CheckAppLaunchProhibitedReason(
      FeatureStatus status) const override;

 private:
  LaunchAppHelper::AppLaunchProhibitedReason prohibited_reason_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_LAUNCH_APP_HELPER_H_
