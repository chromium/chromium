// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_popup_collection.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {
namespace {

class TestMessagePopupCollection : public AshMessagePopupCollection {
 public:
  explicit TestMessagePopupCollection(Shelf* shelf)
      : AshMessagePopupCollection(shelf) {}

  TestMessagePopupCollection(const TestMessagePopupCollection&) = delete;
  TestMessagePopupCollection& operator=(const TestMessagePopupCollection&) =
      delete;
  ~TestMessagePopupCollection() override = default;

  bool popup_shown() const { return popup_shown_; }

 protected:
  void NotifyPopupAdded(message_center::MessagePopupView* popup) override {
    AshMessagePopupCollection::NotifyPopupAdded(popup);
    popup_shown_ = true;
    notification_id_ = popup->message_view()->notification_id();
  }

  void NotifyPopupRemoved(const std::string& notification_id) override {
    AshMessagePopupCollection::NotifyPopupRemoved(notification_id);
    EXPECT_EQ(notification_id_, notification_id);
    popup_shown_ = false;
    notification_id_.clear();
  }

 private:
  bool popup_shown_ = false;
  std::string notification_id_;
};

}  // namespace

class AshMessagePopupCollectionTest : public AshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  AshMessagePopupCollectionTest() = default;

  AshMessagePopupCollectionTest(const AshMessagePopupCollectionTest&) = delete;
  AshMessagePopupCollectionTest& operator=(
      const AshMessagePopupCollectionTest&) = delete;

  ~AshMessagePopupCollectionTest() override = default;

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kNotificationsRefresh,
                                               IsNotificationsRefreshEnabled());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    SetPopupCollection(
        std::make_unique<AshMessagePopupCollection>(GetPrimaryShelf()));
  }

  void TearDown() override {
    popup_collection_.reset();
    AshTestBase::TearDown();
  }

  bool IsNotificationsRefreshEnabled() const { return GetParam(); }

 protected:
  enum Position { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, OUTSIDE };

  AshMessagePopupCollection* popup_collection() {
    return popup_collection_.get();
  }

  void UpdateWorkArea(AshMessagePopupCollection* popup_collection,
                      const display::Display& display) {
    popup_collection->StartObserving(display::Screen::GetScreen(), display);
    // Update the layout
    popup_collection->UpdateWorkArea();
  }

  void SetPopupCollection(std::unique_ptr<AshMessagePopupCollection> delegate) {
    if (!delegate.get()) {
      popup_collection_.reset();
      return;
    }
    popup_collection_ = std::move(delegate);
    UpdateWorkArea(popup_collection_.get(),
                   display::Screen::GetScreen()->GetPrimaryDisplay());
  }

  message_center::MessagePopupView* GetLastPopUpAdded() {
    return popup_collection()->last_pop_up_added_;
  }

  Position GetPositionInDisplay(const gfx::Point& point) {
    const gfx::Rect work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    const gfx::Point center_point = work_area.CenterPoint();
    if (work_area.x() > point.x() || work_area.y() > point.y() ||
        work_area.right() < point.x() || work_area.bottom() < point.y()) {
      return OUTSIDE;
    }

    if (center_point.x() < point.x())
      return (center_point.y() < point.y()) ? BOTTOM_RIGHT : TOP_RIGHT;
    else
      return (center_point.y() < point.y()) ? BOTTOM_LEFT : TOP_LEFT;
  }

  gfx::Rect GetWorkArea() { return popup_collection_->work_area_; }

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id) {
    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test_title",
        u"test message", ui::ImageModel(),
        std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
  }

  std::string AddNotification() {
    std::string id = base::NumberToString(notification_id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateNotification(id));
    return id;
  }

 private:
  int notification_id_ = 0;
  std::unique_ptr<AshMessagePopupCollection> popup_collection_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AshMessagePopupCollectionTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled() */);

TEST_P(AshMessagePopupCollectionTest, ShelfAlignment) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  gfx::Point toast_point;
  toast_point.set_x(popup_collection()->GetToastOriginX(toast_size));
  toast_point.set_y(popup_collection()->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(popup_collection()->IsTopDown());
  EXPECT_FALSE(popup_collection()->IsFromLeft());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  toast_point.set_x(popup_collection()->GetToastOriginX(toast_size));
  toast_point.set_y(popup_collection()->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(popup_collection()->IsTopDown());
  EXPECT_FALSE(popup_collection()->IsFromLeft());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  toast_point.set_x(popup_collection()->GetToastOriginX(toast_size));
  toast_point.set_y(popup_collection()->GetBaseline());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(popup_collection()->IsTopDown());
  EXPECT_TRUE(popup_collection()->IsFromLeft());
}

TEST_P(AshMessagePopupCollectionTest, LockScreen) {
  const gfx::Rect toast_size(0, 0, 10, 10);

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  gfx::Point toast_point;
  toast_point.set_x(popup_collection()->GetToastOriginX(toast_size));
  toast_point.set_y(popup_collection()->GetBaseline());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(popup_collection()->IsTopDown());
  EXPECT_TRUE(popup_collection()->IsFromLeft());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  toast_point.set_x(popup_collection()->GetToastOriginX(toast_size));
  toast_point.set_y(popup_collection()->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(popup_collection()->IsTopDown());
  EXPECT_FALSE(popup_collection()->IsFromLeft());
}

TEST_P(AshMessagePopupCollectionTest, AutoHide) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  int origin_x = popup_collection()->GetToastOriginX(toast_size);
  int baseline = popup_collection()->GetBaseline();

  // Create a window, otherwise autohide doesn't work.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(origin_x, popup_collection()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, popup_collection()->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, DisplayResize) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  int origin_x = popup_collection()->GetToastOriginX(toast_size);
  int baseline = popup_collection()->GetBaseline();

  UpdateDisplay("801x800");
  EXPECT_LT(origin_x, popup_collection()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, popup_collection()->GetBaseline());

  UpdateDisplay("500x400");
  EXPECT_GT(origin_x, popup_collection()->GetToastOriginX(toast_size));
  EXPECT_GT(baseline, popup_collection()->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, DockedMode) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  int origin_x = popup_collection()->GetToastOriginX(toast_size);
  int baseline = popup_collection()->GetBaseline();

  // Emulate the docked mode; enter to an extended mode, then invoke
  // OnNativeDisplaysChanged() with the info for the secondary display only.
  UpdateDisplay("601x600,801x800");

  std::vector<display::ManagedDisplayInfo> new_info;
  new_info.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(1u).id()));
  display_manager()->OnNativeDisplaysChanged(new_info);

  EXPECT_LT(origin_x, popup_collection()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, popup_collection()->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, TrayHeight) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  int origin_x = popup_collection()->GetToastOriginX(toast_size);
  int baseline = popup_collection()->GetBaseline();

  // Simulate the system tray bubble being open.
  const int kTrayHeight = 100;
  popup_collection()->SetTrayBubbleHeight(kTrayHeight);

  EXPECT_EQ(origin_x, popup_collection()->GetToastOriginX(toast_size));
  EXPECT_EQ(baseline - kTrayHeight - message_center::kMarginBetweenPopups,
            popup_collection()->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, Extended) {
  UpdateDisplay("601x600,801x800");
  SetPopupCollection(
      std::make_unique<AshMessagePopupCollection>(GetPrimaryShelf()));

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection for_2nd_display(second_shelf);
  UpdateWorkArea(&for_2nd_display, second_display);
  // Make sure that the toast position on the secondary display is
  // positioned correctly.
  EXPECT_LT(1300, for_2nd_display.GetToastOriginX(gfx::Rect(0, 0, 10, 10)));
  EXPECT_LT(700, for_2nd_display.GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, MixedFullscreenNone) {
  UpdateDisplay("601x600,801x800");
  Shelf* shelf1 = GetPrimaryShelf();
  TestMessagePopupCollection collection1(shelf1);
  UpdateWorkArea(&collection1, GetPrimaryDisplay());

  Shelf* shelf2 =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  TestMessagePopupCollection collection2(shelf2);
  UpdateWorkArea(&collection2, GetSecondaryDisplay());

  // No fullscreens, both receive notification.
  std::unique_ptr<views::Widget> widget1 = CreateTestWidget();
  widget1->SetFullscreen(false);
  AddNotification();
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());

  // Set screen 1 to fullscreen, popup closes on screen 1, stays on screen 2.
  widget1->SetFullscreen(true);
  EXPECT_FALSE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());
}

TEST_P(AshMessagePopupCollectionTest, MixedFullscreenSome) {
  UpdateDisplay("601x600,801x800");
  Shelf* shelf1 = GetPrimaryShelf();
  TestMessagePopupCollection collection1(shelf1);
  UpdateWorkArea(&collection1, GetPrimaryDisplay());

  Shelf* shelf2 =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  TestMessagePopupCollection collection2(shelf2);
  UpdateWorkArea(&collection2, GetSecondaryDisplay());

  // One fullscreen, non-fullscreen receives notification.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  AddNotification();
  EXPECT_FALSE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());

  // Fullscreen toggles, notification now on both.
  widget->SetFullscreen(false);
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());
}

TEST_P(AshMessagePopupCollectionTest, MixedFullscreenAll) {
  UpdateDisplay("601x600,801x800");
  Shelf* shelf1 = GetPrimaryShelf();
  TestMessagePopupCollection collection1(shelf1);
  UpdateWorkArea(&collection1, GetPrimaryDisplay());

  Shelf* shelf2 =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  TestMessagePopupCollection collection2(shelf2);
  UpdateWorkArea(&collection2, GetSecondaryDisplay());

  std::unique_ptr<views::Widget> widget1 = CreateTestWidget();
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(nullptr, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(700, 0, 50, 50));

  // Both fullscreen, no notifications.
  widget1->SetFullscreen(true);
  widget2->SetFullscreen(true);
  AddNotification();
  EXPECT_FALSE(collection1.popup_shown());
  EXPECT_FALSE(collection2.popup_shown());

  // Toggle 1, then the other.
  widget1->SetFullscreen(false);
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_FALSE(collection2.popup_shown());
  widget2->SetFullscreen(false);
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());
}

TEST_P(AshMessagePopupCollectionTest, Unified) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Reset the delegate as the primary display's shelf will be destroyed during
  // transition.
  SetPopupCollection(nullptr);

  UpdateDisplay("601x600,801x800");
  SetPopupCollection(
      std::make_unique<AshMessagePopupCollection>(GetPrimaryShelf()));

  EXPECT_GT(600, popup_collection()->GetToastOriginX(gfx::Rect(0, 0, 10, 10)));
}

// Tests that when the keyboard is showing that notifications appear above it,
// and that they return to normal once the keyboard is gone.
TEST_P(AshMessagePopupCollectionTest, KeyboardShowing) {
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(
      keyboard::KeyboardUIController::Get()->IsKeyboardOverscrollEnabled());

  UpdateDisplay("601x600");
  int baseline = popup_collection()->GetBaseline();

  Shelf* shelf = GetPrimaryShelf();
  gfx::Rect keyboard_bounds(0, 300, 601, 300);
  shelf->SetVirtualKeyboardBoundsForTesting(keyboard_bounds);
  int keyboard_baseline = popup_collection()->GetBaseline();
  EXPECT_NE(baseline, keyboard_baseline);
  EXPECT_GT(keyboard_bounds.y(), keyboard_baseline);

  shelf->SetVirtualKeyboardBoundsForTesting(gfx::Rect());
  EXPECT_EQ(baseline, popup_collection()->GetBaseline());
}

// Tests that notification bubble baseline is correct when entering and exiting
// overview with a full screen window.
TEST_P(AshMessagePopupCollectionTest, BaselineInOverview) {
  UpdateDisplay("800x600");

  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  const int baseline_with_visible_shelf = popup_collection()->GetBaseline();

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  ASSERT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  const int baseline_with_hidden_shelf = popup_collection()->GetBaseline();
  EXPECT_NE(baseline_with_visible_shelf, baseline_with_hidden_shelf);

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const int baseline_in_overview = popup_collection()->GetBaseline();
  EXPECT_EQ(baseline_in_overview, baseline_with_visible_shelf);

  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  const int baseline_no_overview = popup_collection()->GetBaseline();
  EXPECT_EQ(baseline_no_overview, baseline_with_hidden_shelf);
}

class NotificationDestructingNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  NotificationDestructingNotificationDelegate() = default;
  NotificationDestructingNotificationDelegate(
      const NotificationDestructingNotificationDelegate&) = delete;
  NotificationDestructingNotificationDelegate& operator=(
      const NotificationDestructingNotificationDelegate&) = delete;

 private:
  ~NotificationDestructingNotificationDelegate() override = default;

  // NotificationObserver:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    // Show the UnifiedSystemTrayBubble, which will force all popups to be
    // destroyed.
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->ShowBubble();
  }
};

// Regression test for crbug/1316656. Tests that pressing a button resulting in
// the notification popup getting destroyed does not crash.
TEST_P(AshMessagePopupCollectionTest, PopupDestroyedDuringClick) {
  // Create a Notification popup with 1 action button.
  message_center::RichNotificationData notification_data;
  std::u16string button_text = u"BUTTON_TEXT";
  notification_data.buttons.emplace_back(button_text);

  auto to_be_destroyed_notification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, "id1",
          u"Test Web Notification", u"Notification message body.",
          ui::ImageModel(), u"www.test.org", GURL(),
          message_center::NotifierId(), notification_data,
          new NotificationDestructingNotificationDelegate()));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(to_be_destroyed_notification));
  EXPECT_TRUE(GetLastPopUpAdded());

  // Get the view for the button added earlier.
  auto* message_view = GetLastPopUpAdded()->message_view();
  auto* action_button =
      message_view
          ->GetViewByID(
              message_center::NotificationViewBase::ViewId::kActionButtonsRow)
          ->children()[0];
  EXPECT_EQ(static_cast<views::LabelButton*>(action_button)->GetText(),
            button_text);

  // Click the action button.
  // `NotificationDestructingNotificationDelegate::Click()` will destroy the
  // popup during `NotificationViewBase::ActionButtonPressed()`. There should be
  // no crash.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      action_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  EXPECT_FALSE(GetLastPopUpAdded());
}

// Tests that notification bubble baseline is correct when entering and exiting
// tablet mode in a full screen window.
TEST_P(AshMessagePopupCollectionTest, BaselineInTabletMode) {
  UpdateDisplay("800x600");
  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());

  // Baseline is higher than the top of the shelf in clamshell mode.
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection()->GetBaseline());

  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();

  // Baseline is higher than the top of the shelf after entering tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(tablet_mode_controller->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection()->GetBaseline());

  // Baseline is higher than the top of the shelf after exiting tablet mode.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_FALSE(tablet_mode_controller->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection()->GetBaseline());
}

}  // namespace ash
