// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/media/media_notification_provider.h"
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
namespace {

constexpr gfx::Size kMockTraySize = gfx::Size(48, 48);

class MockMediaNotificationProvider : public MediaNotificationProvider {
 public:
  MockMediaNotificationProvider()
      : old_provider_(MediaNotificationProvider::Get()) {
    MediaNotificationProvider::Set(this);

    ON_CALL(*this, GetMediaNotificationListView(_, _, _))
        .WillByDefault([](auto, auto, const auto&) {
          return std::make_unique<views::View>();
        });
  }

  ~MockMediaNotificationProvider() override {
    MediaNotificationProvider::Set(old_provider_);
  }

  // MediaNotificationProvider implementations.
  MOCK_METHOD((std::unique_ptr<views::View>),
              GetMediaNotificationListView,
              (int, bool, const std::string&));
  MOCK_METHOD(void, OnBubbleClosing, ());
  MOCK_METHOD(global_media_controls::MediaItemManager*,
              GetMediaItemManager,
              ());
  void AddObserver(MediaNotificationProviderObserver* observer) override {}
  void RemoveObserver(MediaNotificationProviderObserver* observer) override {}
  bool HasActiveNotifications() override { return has_active_notifications_; }
  bool HasFrozenNotifications() override { return has_frozen_notifications_; }
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override {}

  void SetHasActiveNotifications(bool has_active_notifications) {
    has_active_notifications_ = has_active_notifications;
  }

  void SetHasFrozenNotifications(bool has_frozen_notifications) {
    has_frozen_notifications_ = has_frozen_notifications;
  }

 private:
  bool has_active_notifications_ = false;
  bool has_frozen_notifications_ = false;
  const raw_ptr<MediaNotificationProvider, ExperimentalAsh> old_provider_;
};

// Mock tray button used to test media tray bubble's anchor update.
class MockTrayBackgroundView : public ash::TrayBackgroundView {
 public:
  explicit MockTrayBackgroundView(Shelf* shelf)
      : TrayBackgroundView(shelf,
                           TrayBackgroundViewCatalogName::kTestCatalogName) {
    SetSize(kMockTraySize);
  }

  ~MockTrayBackgroundView() override = default;

  // TrayBackgroundview implementations
  std::u16string GetAccessibleNameForTray() override { return u""; }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override {}
};

}  // namespace

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
    mock_tray_.reset();
    provider_.reset();
    AshTestBase::TearDown();
  }

  // Insert mock tray to status area widget right before system tray (The last
  // two tray buttons are always system tray and overview button tray).
  void InsertMockTray() {
    mock_tray_ =
        std::make_unique<MockTrayBackgroundView>(status_area_widget()->shelf());
    status_area_widget()->tray_buttons_.insert(
        status_area_widget()->tray_buttons_.end() - 2, mock_tray_.get());
  }

  void SimulateNotificationListChanged() {
    media_tray_->OnNotificationListChanged();
  }

  void SimulateTapOnMediaTray() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    media_tray_->PerformAction(tap);
  }

  void SimulateTapOnPinButton() {
    ASSERT_TRUE(media_tray_->pin_button_for_testing());
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(media_tray_->pin_button_for_testing()
                               ->GetBoundsInScreen()
                               .CenterPoint());
    generator->ClickLeftButton();
  }

  void SimulateMockTrayVisibilityChanged(bool visible) {
    mock_tray_->SetVisible(visible);
    media_tray_->AnchorUpdated();
  }

  TrayBubbleWrapper* GetBubbleWrapper() {
    return media_tray_->tray_bubble_wrapper_for_testing();
  }

  gfx::Rect GetBubbleBounds() {
    return GetBubbleWrapper()->GetBubbleView()->GetBoundsInScreen();
  }

  StatusAreaWidget* status_area_widget() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

  MediaTray* media_tray() { return media_tray_; }

  views::View* empty_state_view() { return media_tray_->empty_state_view_; }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
  raw_ptr<MediaTray, ExperimentalAsh> media_tray_;
  std::unique_ptr<MockTrayBackgroundView> mock_tray_;
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
  EXPECT_CALL(*provider(),
              GetMediaNotificationListView(_, /*should_clip_height=*/true, _));
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

// Tests that the shelf is forced to show when the bubble is visible (this is so
// that the shelf doesn't hide when shelf auto-hide is enabled).
TEST_F(MediaTrayTest, OpenBubbleForcesShelfToShow) {
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  ASSERT_TRUE(media_tray()->GetVisible());

  // The shelf should not be forced to show initially.
  EXPECT_FALSE(status_area_widget()->ShouldShowShelf());

  // Open the media tray bubble and verify that the shelf is forced to show.
  SimulateTapOnMediaTray();
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
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // We should display empty state if no media is playing.
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->GetVisible());
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
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);

  // Tapping the pin button while the media controls dialog is opened
  // should have the media tray hidden.
  SimulateTapOnPinButton();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->GetVisible());
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());

  // Tap pin button again should bring back media tray.
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

TEST_F(MediaTrayTest, DialogAnchor) {
  InsertMockTray();

  // Simulate active notification and tap media tray to show dialog.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);

  EXPECT_TRUE(status_area_widget()->shelf()->IsHorizontalAlignment());
  gfx::Rect initial_bounds = GetBubbleBounds();

  // Simulate mock tray becoming visible, bubble should shift left.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  // Simulate mock tray disappearing, bubble should shift back to the
  // original position.
  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Simulate tapping pin button to hide media tray, bubble position
  // should not change.
  SimulateTapOnPinButton();
  EXPECT_FALSE(media_tray()->GetVisible());
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Simlate mock tray appearing and disappearing while the media tray
  // is hidden. Bubble should shift accordingly.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Tap pin button and bring back media tray, bubble position should
  // stay the same.
  SimulateTapOnPinButton();
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Hide bubble, change shelf alignment to left (vertical), and open
  // bubble again.
  SimulateTapOnMediaTray();
  status_area_widget()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  SimulateTapOnMediaTray();

  // Get new bounds.
  initial_bounds = GetBubbleBounds();

  // Simulate mock tray appears and disappears while the shelf alignment is
  // vertical. The bubble should shift vertically.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(0, kMockTraySize.height()),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Hide bubble, change shelf alignment back to bottom and switch ui
  // direction to RTL.
  SimulateTapOnMediaTray();
  status_area_widget()->shelf()->SetAlignment(ShelfAlignment::kBottom);
  base::i18n::SetRTLForTesting(true);
  status_area_widget()->UpdateLayout(false);
  SimulateTapOnMediaTray();

  // Get new bounds.
  initial_bounds = GetBubbleBounds();

  // Simulate tray appears and triggers while ui direction is RTL,
  // bubble should shift to the right.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds + gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());
}

TEST_F(MediaTrayTest, ShowBubble) {
  // We start with no bubble view.
  EXPECT_EQ(nullptr, media_tray()->GetBubbleView());

  EXPECT_CALL(*provider(), GetMediaNotificationListView(_, _, ""));
  media_tray()->ShowBubble();
  EXPECT_NE(nullptr, media_tray()->GetBubbleView());
}

TEST_F(MediaTrayTest, ShowBubbleWithItem) {
  // We start with no bubble view.
  EXPECT_EQ(nullptr, media_tray()->GetBubbleView());

  const std::string item_id = "my-item-id";
  EXPECT_CALL(*provider(), GetMediaNotificationListView(_, _, item_id));
  media_tray()->ShowBubbleWithItem(item_id);
  EXPECT_NE(nullptr, media_tray()->GetBubbleView());
}

}  // namespace ash
