// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_popup_animation_waiter.h"

#include "ash/system/notification_center/message_center_test_util.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"

namespace ash {
using MessagePopupAnimationWaiterTest = AshTestBase;

TEST_F(MessagePopupAnimationWaiterTest, Basic) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  message_center::MessageCenter::Get()->AddNotification(
      CreateSimpleNotification(/*id=*/"id"));
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();
}

// Verifies that `MessagePopupAnimationWaiter` works for an idle message
// popup collection.
TEST_F(MessagePopupAnimationWaiterTest, NoOperate) {
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();
}

}  // namespace ash
