// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_launch_app_helper.h"

namespace ash {
namespace eche_app {

FakeLaunchAppHelper::FakeLaunchAppHelper(
    phonehub::PhoneHubManager* phone_hub_manager,
    LaunchEcheAppFunction launch_eche_app_function,
    LaunchNotificationFunction launch_notification_function,
    CloseNotificationFunction close_notification_function)
    : LaunchAppHelper(phone_hub_manager,
                      launch_eche_app_function,
                      launch_notification_function,
                      close_notification_function),
      prohibited_reason_(
          LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited) {}

FakeLaunchAppHelper::~FakeLaunchAppHelper() = default;

void FakeLaunchAppHelper::SetAppLaunchProhibitedReason(
    LaunchAppHelper::AppLaunchProhibitedReason reason) {
  prohibited_reason_ = reason;
}

LaunchAppHelper::AppLaunchProhibitedReason
FakeLaunchAppHelper::CheckAppLaunchProhibitedReason(
    FeatureStatus status) const {
  return prohibited_reason_;
}

}  // namespace eche_app
}  // namespace ash
