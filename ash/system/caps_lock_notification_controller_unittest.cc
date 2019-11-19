// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/caps_lock_notification_controller.h"

#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"

namespace ash {

using CapsLockNotificationControllerTest = AshTestBase;

// Tests that a11y alert is sent on toggling caps lock.
TEST_F(CapsLockNotificationControllerTest, A11yAlert) {
  auto caps_lock = std::make_unique<CapsLockNotificationController>();
  TestAccessibilityControllerClient client;

  // Simulate turning on caps lock.
  caps_lock->OnCapsLockChanged(true);
  EXPECT_EQ(AccessibilityAlert::CAPS_ON, client.last_a11y_alert());

  // Simulate turning off caps lock.
  caps_lock->OnCapsLockChanged(false);
  EXPECT_EQ(AccessibilityAlert::CAPS_OFF, client.last_a11y_alert());
}

}  // namespace ash
