// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/notification_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keyboard {

TEST(NotificationManagerTest, DoesItConsolidate) {
  NotificationManager manager;

  ASSERT_TRUE(manager.ShouldSendVisibilityNotification(false));
  ASSERT_FALSE(manager.ShouldSendVisibilityNotification(false));
  ASSERT_TRUE(manager.ShouldSendVisibilityNotification(true));
  ASSERT_TRUE(manager.ShouldSendVisibilityNotification(false));
  ASSERT_FALSE(manager.ShouldSendVisibilityNotification(false));

  ASSERT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 10, 10)));
  ASSERT_FALSE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 10, 10)));
  ASSERT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 20, 20)));

  // This is technically empty
  ASSERT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(0, 0, 0, 100)));

  // This is still empty
  ASSERT_FALSE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(0, 0, 100, 0)));

  // Occluded bounds...

  // Start the field with an empty value
  ASSERT_TRUE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(0, 0, 0, 0)));

  // Still empty
  ASSERT_FALSE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(0, 0, 10, 0)));

  ASSERT_TRUE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(0, 0, 10, 10)));

  // Different bounds, same size
  ASSERT_TRUE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(30, 30, 10, 10)));
}

}  // namespace keyboard
