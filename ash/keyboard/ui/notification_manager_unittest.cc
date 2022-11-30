// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/notification_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keyboard {

TEST(NotificationManagerTest, DoesNotSendIfSameAsInitialState) {
  NotificationManager manager;

  EXPECT_FALSE(manager.ShouldSendVisibilityNotification(false));
  EXPECT_FALSE(manager.ShouldSendVisualBoundsNotification(gfx::Rect()));
  EXPECT_FALSE(manager.ShouldSendOccludedBoundsNotification(gfx::Rect()));
  EXPECT_FALSE(
      manager.ShouldSendWorkspaceDisplacementBoundsNotification(gfx::Rect()));
}

TEST(NotificationManagerTest, ConsolidatesVisibilityChanges) {
  NotificationManager manager;

  EXPECT_TRUE(manager.ShouldSendVisibilityNotification(true));
  EXPECT_FALSE(manager.ShouldSendVisibilityNotification(true));
  EXPECT_TRUE(manager.ShouldSendVisibilityNotification(false));
}

TEST(NotificationManagerTest, ConsolidatesVisualBoundsChanges) {
  NotificationManager manager;

  EXPECT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 10, 10)));
  EXPECT_FALSE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 10, 10)));
  EXPECT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(10, 10, 20, 20)));
  // This is technically empty
  EXPECT_TRUE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(0, 0, 0, 100)));
  // This is still empty
  EXPECT_FALSE(
      manager.ShouldSendVisualBoundsNotification(gfx::Rect(0, 0, 100, 0)));
}

TEST(NotificationManagerTest, ConsolidatesOccludedBoundsChanges) {
  NotificationManager manager;

  // Still empty
  EXPECT_FALSE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(0, 0, 10, 0)));

  EXPECT_TRUE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(0, 0, 10, 10)));
  // Different bounds, same size
  EXPECT_TRUE(
      manager.ShouldSendOccludedBoundsNotification(gfx::Rect(30, 30, 10, 10)));
}

}  // namespace keyboard
