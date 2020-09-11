// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"

using ::testing::_;

namespace ash {
namespace {

class MockMediaNotificationProvider : public MediaNotificationProvider {
 public:
  MockMediaNotificationProvider() {
    MediaNotificationProvider::Set(this);

    ON_CALL(*this, GetMediaNotificationListView(_, _))
        .WillByDefault(
            [](auto, auto) { return std::make_unique<views::View>(); });
  }

  // Medianotificationprovider implementations.
  MOCK_METHOD2(GetMediaNotificationListView,
               std::unique_ptr<views::View>(SkColor, int));
  MOCK_METHOD0(GetActiveMediaNotificationView, std::unique_ptr<views::View>());
  MOCK_METHOD0(OnBubbleClosing, void());
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

class MediaTrayTest : public AshTestBase {
 public:
  MediaTrayTest() = default;
  ~MediaTrayTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsForChromeOS);
    provider_ = std::make_unique<MockMediaNotificationProvider>();
    AshTestBase::SetUp();

    media_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->media_tray();
  }

  void SimulateNotificationListChanged() {
    media_tray_->OnNotificationListChanged();
  }

  void SimulateTapOnMediaTray() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    media_tray_->PerformAction(tap);
  }

  TrayBubbleWrapper* GetBubbleWrapper() {
    return media_tray_->tray_bubble_wrapper_for_testing();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

  MediaTray* media_tray() { return media_tray_; }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
  MediaTray* media_tray_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaTrayTest, MediaTrayVisibilityTest) {
  // Media tray should be invisible initially.
  ASSERT_TRUE(media_tray());
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Media tray should hide itself when no media is playing.
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible when there is frozen notification.
  provider()->SetHasFrozenNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());
}

TEST_F(MediaTrayTest, ShowAndHideBubbleTest) {
  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Bubble should not exist initially, and media tray should not
  // be active.
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());

  // Tap the media tray should show the bubble, and media tray should
  // be active. GetMediaNotificationlistview also should be called for
  // getting active notifications.
  EXPECT_CALL(*provider(), GetMediaNotificationListView(_, _));
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // Tap again should close the bubble and MediaNotificationProvider should
  // be notified.
  EXPECT_CALL(*provider(), OnBubbleClosing());
  SimulateTapOnMediaTray();
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());
}

TEST_F(MediaTrayTest, DialogCloseWhenNoActiveNotificationTest) {
  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Bubble should not exist initially, and media tray should not
  // be active.
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());

  // Tap and show bubble.
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // Bubble should close if there's no active sessions.
  EXPECT_CALL(*provider(), OnBubbleClosing());
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->GetVisible());
}

}  // namespace ash
