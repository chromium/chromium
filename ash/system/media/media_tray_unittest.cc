// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/shell.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/mock_media_notification_provider.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"

using ::testing::_;

namespace ash {

class MediaTrayTest : public AshTestBase {
 public:
  MediaTrayTest() = default;
  ~MediaTrayTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    provider_ = std::make_unique<MockMediaNotificationProvider>();
    media_tray_ = status_area_widget()->media_tray();
    ASSERT_TRUE(MediaTray::IsPinnedToShelf());
  }

  void TearDown() override {
    provider_.reset();
    AshTestBase::TearDown();
  }

  void SimulateNotificationListChanged() {
    media_tray_->OnNotificationListChanged();
  }

  void SimulateTapOnPinButton() {
    ASSERT_TRUE(media_tray_->pin_button_for_testing());
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(media_tray_->pin_button_for_testing()
                               ->GetBoundsInScreen()
                               .CenterPoint());
    generator->ClickLeftButton();
  }

  TrayBubbleWrapper* GetBubbleWrapper() {
    return media_tray_->tray_bubble_wrapper_for_testing();
  }

  StatusAreaWidget* status_area_widget() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

  MediaTray* media_tray() { return media_tray_; }

  views::View* empty_state_view() { return media_tray_->empty_state_view_; }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
  raw_ptr<MediaTray, DanglingUntriaged> media_tray_;
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

  // Media tray should be hidden when screen is locked.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->FlushForTest();
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible again when we unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Media tray should not be visible if global media controls is
  // not pinned to shelf.
  MediaTray::SetPinnedToShelf(false);
  EXPECT_FALSE(media_tray()->GetVisible());
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
  EXPECT_CALL(*provider(), GetMediaNotificationListView(
                               _, /*should_clip_height=*/true, _, _));
  GestureTapOn(media_tray());
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // Tap again should close the bubble and MediaNotificationProvider should
  // be notified.
  EXPECT_CALL(*provider(), OnBubbleClosing());
  GestureTapOn(media_tray());
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());
}

// Tests that the shelf is forced to show when the bubble is visible (this is so
// that the shelf doesn't hide when shelf auto-hide is enabled).
TEST_F(MediaTrayTest, OpenBubbleForcesShelfToShow) {
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  ASSERT_TRUE(media_tray()->GetVisible());

  // The shelf should not be forced to show initially.
  EXPECT_FALSE(status_area_widget()->ShouldShowShelf());

  // Open the media tray bubble and verify that the shelf is forced to show.
  GestureTapOn(media_tray());
  ;
  EXPECT_TRUE(status_area_widget()->ShouldShowShelf());
}

TEST_F(MediaTrayTest, ShowEmptyStateWhenNoActiveNotification) {
  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Bubble should not exist initially, and media tray should not
  // be active.
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());

  // Tap and show bubble.
  GestureTapOn(media_tray());
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // We should display empty state if no media is playing.
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_NE(empty_state_view(), nullptr);
  EXPECT_TRUE(empty_state_view()->GetVisible());

  // Empty state should be hidden if a new media starts playing.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_FALSE(empty_state_view()->GetVisible());
}

TEST_F(MediaTrayTest, PinButtonTest) {
  // Media tray should be invisible initially.
  ASSERT_TRUE(media_tray());
  EXPECT_FALSE(media_tray()->GetVisible());

  // Open global media controls dialog.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());
  GestureTapOn(media_tray());
  EXPECT_NE(GetBubbleWrapper(), nullptr);

  // Tapping the pin button while the media controls dialog is opened should not
  // hide the media tray.
  SimulateTapOnPinButton();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());

  // Tap the pin button again and the media tray should still be visible.
  SimulateTapOnPinButton();
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_TRUE(MediaTray::IsPinnedToShelf());
}

TEST_F(MediaTrayTest, PinToShelfDefaultBehavior) {
  // Media controls should be pinned on shelf by default on a
  // 10 inch display.
  UpdateDisplay("800x530");
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());

  // Media controls should be pinned on shelf by default on a
  // display larger than 10 inches.
  UpdateDisplay("800x600");
  EXPECT_TRUE(MediaTray::IsPinnedToShelf());
}

TEST_F(MediaTrayTest, BubbleGetsFocusWhenOpenWithKeyboard) {
  media_tray()->ShowBubble();

  // Generate a tab key press.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);

  EXPECT_TRUE(GetBubbleWrapper()->GetBubbleWidget()->IsActive());
}

TEST_F(MediaTrayTest, ShowBubble) {
  // We start with no bubble view.
  EXPECT_EQ(nullptr, media_tray()->GetBubbleView());

  EXPECT_CALL(*provider(), GetMediaNotificationListView(_, _, _, ""));
  media_tray()->ShowBubble();
  EXPECT_NE(nullptr, media_tray()->GetBubbleView());
}

TEST_F(MediaTrayTest, ShowBubbleWithItem) {
  // We start with no bubble view.
  EXPECT_EQ(nullptr, media_tray()->GetBubbleView());

  const std::string item_id = "my-item-id";
  EXPECT_CALL(*provider(), GetMediaNotificationListView(_, _, _, item_id));
  media_tray()->ShowBubbleWithItem(item_id);
  EXPECT_NE(nullptr, media_tray()->GetBubbleView());
}

TEST_F(MediaTrayTest, CloseBubbleIsNoopWhenNoBubble) {
  // Start out with no bubble.
  ASSERT_EQ(nullptr, media_tray()->GetBubbleView());

  // `OnBubbleClosing()` should not be called when there is no bubble to close.
  EXPECT_CALL(*provider(), OnBubbleClosing).Times(0);
  media_tray()->CloseBubble();
}

}  // namespace ash
