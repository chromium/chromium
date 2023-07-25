// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"

#include <string>
#include <vector>
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

namespace ash {

SensorDisabledNotificationDelegate::~SensorDisabledNotificationDelegate() =
    default;

ScopedSensorDisabledNotificationDelegateForTest::
    ScopedSensorDisabledNotificationDelegateForTest(
        std::unique_ptr<SensorDisabledNotificationDelegate> delegate) {
  previous_ = PrivacyHubNotificationController::Get()
                  ->SetSensorDisabledNotificationDelegate(std::move(delegate));
}

ScopedSensorDisabledNotificationDelegateForTest::
    ~ScopedSensorDisabledNotificationDelegateForTest() {
  PrivacyHubNotificationController::Get()
      ->SetSensorDisabledNotificationDelegate(std::move(previous_));
}

}  // namespace ash
