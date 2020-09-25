// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_detailed_view_controller.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace ash {

namespace {

class MockMediaNotificationProvider : MediaNotificationProvider {
 public:
  MockMediaNotificationProvider() {
    MediaNotificationProvider::Set(this);

    ON_CALL(*this, GetMediaNotificationListView(_, _))
        .WillByDefault(
            [](auto, auto) { return std::make_unique<views::View>(); });
  }

  ~MockMediaNotificationProvider() override {
    MediaNotificationProvider::Set(nullptr);
  }

  // MediaNotificationProvider implementations.
  MOCK_METHOD2(GetMediaNotificationListView,
               std::unique_ptr<views::View>(SkColor, int));
  MOCK_METHOD0(OnBubbleClosing, void());
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override {
    return std::make_unique<views::View>();
  }
  void AddObserver(MediaNotificationProviderObserver* observer) override {}
  void RemoveObserver(MediaNotificationProviderObserver* observer) override {}
  bool HasActiveNotifications() override { return has_active_notifications_; }
  bool HasFrozenNotifications() override { return has_frozen_notifications_; }

  void SetHasActiveNotifications(bool has_active_notifications) {
    has_active_notifications_ = has_active_notifications;
  }

  void SetHasFrozenNotifications(bool has_frozen_notifications) {
    has_frozen_notifications_ = has_frozen_notifications;
  }

 private:
  bool has_active_notifications_ = false;
  bool has_frozen_notifications_ = false;
};

}  // namespace

class UnifiedMediaControlsDetailedViewControllerTest : public AshTestBase {
 public:
  UnifiedMediaControlsDetailedViewControllerTest() = default;
  ~UnifiedMediaControlsDetailedViewControllerTest() override = default;

  void SetUp() override {
    provider_ = std::make_unique<MockMediaNotificationProvider>();
    AshTestBase::SetUp();

    // Ensure media tray is not pinned to shelf so that media controls
    // show up in quick settings.
    MediaTray::SetPinnedToShelf(false);

    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->ShowBubble(false /* show_by_click */);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    provider_.reset();
  }

  UnifiedSystemTrayController* system_tray_controller() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->controller_for_test();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
};

TEST_F(UnifiedMediaControlsDetailedViewControllerTest,
       ExitDetailedViewWhenNoMediaIsPlaying) {
  // UnifiedSystemTrayController should have no DetailedViewController
  // initially.
  EXPECT_EQ(system_tray_controller()->detailed_view_controller(), nullptr);

  // We should get a MediaNotificationProvider::GetMediaNotificationListView
  // call when creating the detailed view.
  EXPECT_CALL(*provider(), GetMediaNotificationListView);
  system_tray_controller()->OnMediaControlsViewClicked();
  EXPECT_NE(system_tray_controller()->detailed_view_controller(), nullptr);

  // Notification list update with neither active nor frozen session should
  // close the detailed view and get back to main view.
  EXPECT_CALL(*provider(), OnBubbleClosing);
  static_cast<UnifiedMediaControlsDetailedViewController*>(
      system_tray_controller()->detailed_view_controller())
      ->OnNotificationListChanged();
  EXPECT_EQ(system_tray_controller()->detailed_view_controller(), nullptr);
}

}  // namespace ash
