// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/ash_message_popup_collection.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/notification_center/message_center_test_util.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/wm/core/window_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

class TestMessagePopupCollection : public AshMessagePopupCollection {
 public:
  explicit TestMessagePopupCollection(Shelf* shelf)
      : AshMessagePopupCollection(display::Screen::GetScreen(), shelf) {}

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
                                      public testing::WithParamInterface<
                                          /*IsNotifierCollisionEnabled=*/bool> {
 public:
  AshMessagePopupCollectionTest() = default;

  AshMessagePopupCollectionTest(const AshMessagePopupCollectionTest&) = delete;
  AshMessagePopupCollectionTest& operator=(
      const AshMessagePopupCollectionTest&) = delete;

  ~AshMessagePopupCollectionTest() override = default;

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsNotifierCollisionEnabled()) {
      enabled_features.emplace_back(features::kNotifierCollision);
    } else {
      disabled_features.emplace_back(features::kNotifierCollision);
    }

    scoped_feature_list_->InitWithFeatures(enabled_features, disabled_features);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

  // Trigger the timeout so that shelf will show/hide immediately.
  bool TriggerShelfAutoHideTimeout() {
    auto* layout_manager = GetPrimaryShelf()->shelf_layout_manager();

    if (!layout_manager->auto_hide_timer_.IsRunning()) {
      return false;
    }

    layout_manager->auto_hide_timer_.FireNow();
    return true;
  }

  void AnimateUntilIdle() {
    auto* animation = GetPrimaryPopupCollection()->animation();

    while (animation->is_animating()) {
      animation->SetCurrentValue(1.0);
      animation->End();
    }
  }

  // TODO(b/305075031) clean up after the flag is removed.
  bool IsNotifierCollisionEnabled() const { return GetParam(); }

 protected:
  enum Position { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, OUTSIDE };

  AshMessagePopupCollection* GetPrimaryPopupCollection() {
    return GetPrimaryNotificationCenterTray()->popup_collection();
  }

  void UpdateWorkArea(AshMessagePopupCollection* popup_collection,
                      const display::Display& display) {
    popup_collection->StartObserving(display::Screen::GetScreen(), display);
    // Update the layout
    popup_collection->UpdateWorkArea();
  }

  message_center::MessagePopupView* GetLastPopUpAdded() {
    return GetPrimaryPopupCollection()->last_pop_up_added_;
  }

  message_center::MessagePopupView* GetLastPopUpAddedForCollection(
      AshMessagePopupCollection* collection) {
    return collection->last_pop_up_added_;
  }

  Position GetPositionInDisplay(const gfx::Point& point) {
    const gfx::Rect work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    const gfx::Point center_point = work_area.CenterPoint();
    if (work_area.x() > point.x() || work_area.y() > point.y() ||
        work_area.right() < point.x() || work_area.bottom() < point.y()) {
      return OUTSIDE;
    }

    if (center_point.x() < point.x()) {
      return (center_point.y() < point.y()) ? BOTTOM_RIGHT : TOP_RIGHT;
    } else {
      return (center_point.y() < point.y()) ? BOTTOM_LEFT : TOP_LEFT;
    }
  }

  gfx::Rect GetWorkArea() { return GetPrimaryPopupCollection()->work_area_; }

  std::string AddNotification(bool has_image = false,
                              const GURL& origin_url = GURL(),
                              bool has_inline_reply = false) {
    std::string id = base::NumberToString(notification_id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateSimpleNotification(id, has_image, origin_url, has_inline_reply));
    return id;
  }

  phonehub::FakePhoneHubManager* phone_hub_manager() {
    return &phone_hub_manager_;
  }

 private:
  int notification_id_ = 0;

  // Fake phone hub manager to show the phone hub tray. Used to test the popup
  // collection when the phone hub bubble is showing.
  phonehub::FakePhoneHubManager phone_hub_manager_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AshMessagePopupCollectionTest,
                         /*IsNotifierCollisionEnabled()=*/testing::Bool());

TEST_P(AshMessagePopupCollectionTest, ShelfAlignment) {
  const gfx::Rect popup_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  gfx::Point popup_point;

  auto* popup_collection = GetPrimaryPopupCollection();

  popup_point.set_x(popup_collection->GetPopupOriginX(popup_size));
  popup_point.set_y(popup_collection->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(popup_point));
  EXPECT_FALSE(popup_collection->IsTopDown());
  EXPECT_FALSE(popup_collection->IsFromLeft());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  popup_point.set_x(popup_collection->GetPopupOriginX(popup_size));
  popup_point.set_y(popup_collection->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(popup_point));
  EXPECT_FALSE(popup_collection->IsTopDown());
  EXPECT_FALSE(popup_collection->IsFromLeft());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  popup_point.set_x(popup_collection->GetPopupOriginX(popup_size));
  popup_point.set_y(popup_collection->GetBaseline());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(popup_point));
  EXPECT_FALSE(popup_collection->IsTopDown());
  EXPECT_TRUE(popup_collection->IsFromLeft());
}

TEST_P(AshMessagePopupCollectionTest, LockScreen) {
  const gfx::Rect popup_size(0, 0, 10, 10);

  auto* popup_collection = GetPrimaryPopupCollection();

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  gfx::Point popup_point;
  popup_point.set_x(popup_collection->GetPopupOriginX(popup_size));
  popup_point.set_y(popup_collection->GetBaseline());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(popup_point));
  EXPECT_FALSE(popup_collection->IsTopDown());
  EXPECT_TRUE(popup_collection->IsFromLeft());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  popup_point.set_x(popup_collection->GetPopupOriginX(popup_size));
  popup_point.set_y(popup_collection->GetBaseline());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(popup_point));
  EXPECT_FALSE(popup_collection->IsTopDown());
  EXPECT_FALSE(popup_collection->IsFromLeft());
}

TEST_P(AshMessagePopupCollectionTest, AutoHide) {
  const gfx::Rect popup_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  auto* popup_collection = GetPrimaryPopupCollection();

  int origin_x = popup_collection->GetPopupOriginX(popup_size);
  int shelf_show_baseline = popup_collection->GetBaseline();

  // Create a window, otherwise autohide doesn't work.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(origin_x, popup_collection->GetPopupOriginX(popup_size));

  // The baseline when shelf shows should be less than when it hides.
  int shelf_hide_baseline = popup_collection->GetBaseline();
  EXPECT_LT(shelf_show_baseline, shelf_hide_baseline);

  // Tests that popup baseline changes when shelf shows/hides.
  // Move down the mouse to show shelf. Popup should move up.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  generator->MoveMouseTo(display_bounds.bottom_center());
  ASSERT_TRUE(TriggerShelfAutoHideTimeout());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  EXPECT_EQ(shelf_show_baseline, popup_collection->GetBaseline());

  // Move the mouse away to hide shelf. Popup should move down.
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerShelfAutoHideTimeout());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  EXPECT_EQ(shelf_hide_baseline, popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, DisplayResize) {
  const gfx::Rect popup_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  auto* popup_collection = GetPrimaryPopupCollection();

  int origin_x = popup_collection->GetPopupOriginX(popup_size);
  int baseline = popup_collection->GetBaseline();

  UpdateDisplay("801x800");
  EXPECT_LT(origin_x, popup_collection->GetPopupOriginX(popup_size));
  EXPECT_LT(baseline, popup_collection->GetBaseline());

  UpdateDisplay("500x400");
  EXPECT_GT(origin_x, popup_collection->GetPopupOriginX(popup_size));
  EXPECT_GT(baseline, popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, DockedMode) {
  const gfx::Rect popup_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  auto* popup_collection = GetPrimaryPopupCollection();

  int origin_x = popup_collection->GetPopupOriginX(popup_size);
  int baseline = popup_collection->GetBaseline();

  // Emulate the docked mode; enter to an extended mode, then invoke
  // OnNativeDisplaysChanged() with the info for the secondary display only.
  UpdateDisplay("601x600,801x800");

  std::vector<display::ManagedDisplayInfo> new_info;
  new_info.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(1u).id()));
  display_manager()->OnNativeDisplaysChanged(new_info);

  EXPECT_LT(origin_x, popup_collection->GetPopupOriginX(popup_size));
  EXPECT_LT(baseline, popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, Extended) {
  UpdateDisplay("601x600,801x800");

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection for_2nd_display(display::Screen::GetScreen(),
                                            second_shelf);
  UpdateWorkArea(&for_2nd_display, second_display);
  // Make sure that the popup position on the secondary display is
  // positioned correctly.
  EXPECT_LT(1300, for_2nd_display.GetPopupOriginX(gfx::Rect(0, 0, 10, 10)));
  EXPECT_LT(700, for_2nd_display.GetBaseline());
}

// TODO(b/301625873): Fix notification pop-up dismissal on full-screen activated
// with multiple displays. The unit test is passing but the behavior it is
// testing does not work in production.
TEST_P(AshMessagePopupCollectionTest, DISABLED_MixedFullscreenNone) {
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
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget1->SetFullscreen(false);
  AddNotification();
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());

  // Set screen 1 to fullscreen, popup closes on screen 1, stays on screen 2.
  widget1->SetFullscreen(true);
  EXPECT_FALSE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());
}

// TODO(b/301625873): Fix notification pop-up dismissal on full-screen activated
// with multiple displays. The unit test is passing but the behavior it is
// testing does not work in production.
TEST_P(AshMessagePopupCollectionTest, DISABLED_MixedFullscreenSome) {
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
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  AddNotification();
  EXPECT_FALSE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());

  // Fullscreen toggles, notification now on both.
  widget->SetFullscreen(false);
  EXPECT_TRUE(collection1.popup_shown());
  EXPECT_TRUE(collection2.popup_shown());
}

// TODO(b/301625873): Fix notification pop-up dismissal on full-screen activated
// with multiple displays. The unit test is passing but the behavior it is
// testing does not work in production.
TEST_P(AshMessagePopupCollectionTest, DISABLED_MixedFullscreenAll) {
  UpdateDisplay("601x600,801x800");
  Shelf* shelf1 = GetPrimaryShelf();
  TestMessagePopupCollection collection1(shelf1);
  UpdateWorkArea(&collection1, GetPrimaryDisplay());

  Shelf* shelf2 =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  TestMessagePopupCollection collection2(shelf2);
  UpdateWorkArea(&collection2, GetSecondaryDisplay());

  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> widget2 = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(700, 0, 50, 50));

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

TEST_P(AshMessagePopupCollectionTest, PopupCollectionOriginX) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("601x600,801x800");

  EXPECT_GT(600, GetPrimaryPopupCollection()->GetPopupOriginX(
                     gfx::Rect(0, 0, 10, 10)));
}

// Tests that when the keyboard is showing that notifications appear above it,
// and that they return to normal once the keyboard is gone.
TEST_P(AshMessagePopupCollectionTest, KeyboardShowing) {
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(
      keyboard::KeyboardUIController::Get()->IsKeyboardOverscrollEnabled());

  UpdateDisplay("601x600");
  auto* popup_collection = GetPrimaryPopupCollection();

  int baseline = popup_collection->GetBaseline();

  Shelf* shelf = GetPrimaryShelf();
  gfx::Rect keyboard_bounds(0, 300, 601, 300);
  shelf->SetVirtualKeyboardBoundsForTesting(keyboard_bounds);
  int keyboard_baseline = popup_collection->GetBaseline();
  EXPECT_NE(baseline, keyboard_baseline);
  EXPECT_GT(keyboard_bounds.y(), keyboard_baseline);

  shelf->SetVirtualKeyboardBoundsForTesting(gfx::Rect());
  EXPECT_EQ(baseline, popup_collection->GetBaseline());
}

// Tests that notification bubble baseline is correct when entering and exiting
// overview with a full screen window.
TEST_P(AshMessagePopupCollectionTest, BaselineInOverview) {
  UpdateDisplay("800x600");

  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());

  auto* popup_collection = GetPrimaryPopupCollection();

  const int baseline_with_visible_shelf = popup_collection->GetBaseline();

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  ASSERT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  const int baseline_with_hidden_shelf = popup_collection->GetBaseline();
  EXPECT_NE(baseline_with_visible_shelf, baseline_with_hidden_shelf);

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const int baseline_in_overview = popup_collection->GetBaseline();
  EXPECT_EQ(baseline_in_overview, baseline_with_visible_shelf);

  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  const int baseline_no_overview = popup_collection->GetBaseline();
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
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    // Show the UnifiedSystemTrayBubble, which will force all popups to be
    // destroyed.
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
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
          ->children()[0]
          .get();
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
  // Wait for animation to end.
  MessagePopupAnimationWaiter(GetPrimaryPopupCollection()).Wait();

  EXPECT_FALSE(GetLastPopUpAdded());
}

// Tests that notification popup baseline is correct when entering and exiting
// tablet mode in a full screen window.
TEST_P(AshMessagePopupCollectionTest, BaselineUpdates_InTabletMode) {
  UpdateDisplay("800x600");
  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());

  auto* popup_collection = GetPrimaryPopupCollection();

  // Baseline is higher than the top of the shelf in clamshell mode.
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());

  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();

  // Baseline is higher than the top of the shelf after entering tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());

  // Baseline is higher than the top of the shelf after exiting tablet mode.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, BaselineUpdates_InAppMode) {
  UpdateDisplay("800x600");
  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());

  auto* popup_collection = GetPrimaryPopupCollection();

  // Enable tablet mode without an open window.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  auto previous_popup_collection_bottom =
      popup_collection->popup_collection_bounds().bottom();
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            GetPrimaryShelf()->shelf_layout_manager()->shelf_background_type());

  // Enter app mode by showing a window, pop-up collection bottom should update
  // its bounds to be lower down the screen than before.
  std::unique_ptr<aura::Window> window(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  EXPECT_GT(popup_collection->popup_collection_bounds().bottom(),
            previous_popup_collection_bottom);
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetPrimaryShelf()->shelf_layout_manager()->shelf_background_type());

  // Exit app mode by hiding the window, popup collection bottom should return
  // to its previous value.
  window->Hide();
  EXPECT_EQ(popup_collection->popup_collection_bounds().bottom(),
            previous_popup_collection_bottom);
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            GetPrimaryShelf()->shelf_layout_manager()->shelf_background_type());
}

TEST_P(AshMessagePopupCollectionTest, BaselineUpdates_OnSliderShown) {
  auto* popup_collection = GetPrimaryPopupCollection();
  auto* system_tray = GetPrimaryUnifiedSystemTray();
  int previous_baseline = popup_collection->GetBaseline();

  // Add a notification.
  AddNotification();
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  // Show a volume slider bubble, baseline should be updated if notifier
  // collision is enabled.
  system_tray->ShowVolumeSliderBubble();
  auto* slider_view = system_tray->GetSliderView();
  ASSERT_TRUE(slider_view);

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appear on top of the slider bubble, separated by
    // a padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
    EXPECT_EQ(slider_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if notifier collision is disabled.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
  }

  // Baseline returns to previous value when the slider bubble is closed.
  system_tray->CloseSecondaryBubbles();
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdates_OnSliderShownWithAutoHideShelf) {
  // Create a window, otherwise autohide doesn't work.
  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  AddNotification();
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  auto* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowVolumeSliderBubble();
  auto* slider_view = system_tray->GetSliderView();
  ASSERT_TRUE(slider_view);

  int shelf_hide_popup_bottom = popup->GetBoundsInScreen().bottom();

  if (IsNotifierCollisionEnabled()) {
    // On hidden shelf, the added popup should appear on top of the slider
    // bubble, separated by a padding of `kMarginBetweenPopups`.
    EXPECT_EQ(shelf_hide_popup_bottom + message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
  }

  // Move mouse to the shelf to make it show.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  generator->MoveMouseTo(display_bounds.bottom_center());
  ASSERT_TRUE(TriggerShelfAutoHideTimeout());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Popup should move up when shelf is shown.
  int shelf_show_popup_bottom = popup->GetBoundsInScreen().bottom();
  EXPECT_GT(shelf_hide_popup_bottom, shelf_show_popup_bottom);

  if (IsNotifierCollisionEnabled()) {
    // Should still be on top of slider view.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
  }

  // Move the mouse away to hide the shelf. The shelf should hide now and the
  // popup is adjusted correctly.
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerShelfAutoHideTimeout());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Popup should move down and still on top of slider view.
  EXPECT_EQ(shelf_hide_popup_bottom, popup->GetBoundsInScreen().bottom());

  if (IsNotifierCollisionEnabled()) {
    // Should still be on top of slider view.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
  }
}

// Tests that `TrayBubbleView` elements (e.g. Quick Settings) and popups
// are placed on top of each other based on which was shown most recently.
TEST_P(AshMessagePopupCollectionTest, PopupsAndTrayBubbleViewsZOrdering) {
  // Add a notification popup.
  AddNotification();
  auto* popup = GetLastPopUpAdded();
  EXPECT_TRUE(popup);

  // Opening Quick Settings makes its bubble show in front of the previously
  // shown notification pop-up.
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();
  auto* bubble_native_view =
      unified_system_tray->bubble()->GetBubbleWidget()->GetNativeView();
  EXPECT_FALSE(popup->GetWidget()->IsStackedAbove(bubble_native_view));

  // Adding another popup moves Quick Settings to the back, bringing all popups
  // to the top level, showing them in front of the Quick Settings bubble.
  AddNotification();
  // Wait until the notification popup shows.
  MessagePopupAnimationWaiter(GetPrimaryPopupCollection()).Wait();
  EXPECT_TRUE(popup->GetWidget()->IsStackedAbove(bubble_native_view));
}

TEST_P(AshMessagePopupCollectionTest, BaselineUpdates_OnTrayBubbleShown) {
  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  // Show a corner anchored shelf pod bubble. Baseline should update if notifier
  // collision is enabled.
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  // Attempt showing a notification when Quick Settings is open.
  AddNotification();
  auto* popup = GetLastPopUpAdded();

  ASSERT_TRUE(popup);

  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appear on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    EXPECT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Change the bubble height.
  auto bubble_bounds = bubble_widget->GetWindowBoundsInScreen();
  bubble_widget->SetBounds(gfx::Rect(bubble_bounds.x(), bubble_bounds.y() + 20,
                                     bubble_bounds.width(),
                                     bubble_bounds.height() - 20));

  if (IsNotifierCollisionEnabled()) {
    // The baseline for the popup should be adjusted based on the new bubble
    // height.
    EXPECT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // When bubble is closed, no offset should be set.
  // NOTE: We use `CloseNow()` here instead of calling `CloseBubble()` on
  // `unified_system_tray` to avoid the delay in the message loop happen in
  // `Widget::Close()`.
  bubble_widget->CloseNow();
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());

  // The popup is adjusted to be at the baseline without the offset.
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdates_OnTrayBubbleShownWithAutoHideShelf) {
  // Create a window, otherwise autohide doesn't work.
  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  // Move mouse to the shelf to make it show.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  generator->MoveMouseTo(display_bounds.bottom_center());
  ASSERT_TRUE(TriggerShelfAutoHideTimeout());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Test showing a bubble with the shelf showing in auto-hide state.
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification();
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
  }

  int old_popup_bottom = popup->GetBoundsInScreen().bottom();

  // Click on the screen corner to hide the shelf and the bubble. The shelf
  // should hide now and the popup is adjusted correctly to the baseline.
  generator->MoveMouseTo(0, 0);
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // The popup is moved down to be at the baseline without the offset.
  EXPECT_LT(old_popup_bottom, popup->GetBoundsInScreen().bottom());
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

// Tests that the baseline will not be adjusted when a tray bubble that is not
// anchored to the shelf corner opens (i.e. the IME tray bubble).
TEST_P(AshMessagePopupCollectionTest,
       BaselineDoesNotUpdate_OnNonAnchoredTrayBubbleShown) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  auto* ime_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->ime_menu_tray();
  ASSERT_TRUE(ime_tray->GetVisible());

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  ime_tray->ShowBubble();
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());

  ime_tray->GetBubbleWidget()->CloseNow();
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdates_OnTrayBubbleShownWithMultiDisplay) {
  UpdateDisplay("801x800,801x800");

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection secondary_popup_collection(
      display::Screen::GetScreen(), second_shelf);
  UpdateWorkArea(&secondary_popup_collection, second_display);

  auto* primary_popup_collection = GetPrimaryPopupCollection();
  int previous_primary_baseline = primary_popup_collection->GetBaseline();
  int previous_secondary_baseline = secondary_popup_collection.GetBaseline();

  // Add a notification popup.
  AddNotification();
  auto* primary_popup = GetLastPopUpAdded();
  auto* secondary_popup =
      GetLastPopUpAddedForCollection(&secondary_popup_collection);
  EXPECT_TRUE(primary_popup);
  EXPECT_TRUE(secondary_popup);

  // Open primary system tray bubble.
  auto* primary_system_tray = GetPrimaryUnifiedSystemTray();
  LeftClickOn(primary_system_tray);

  if (IsNotifierCollisionEnabled()) {
    // The primary popup collection should update the baseline and the secondary
    // one should reset.
    auto* primary_bubble_view = primary_system_tray->bubble()->GetBubbleView();
    EXPECT_EQ(
        primary_bubble_view->height() + message_center::kMarginBetweenPopups,
        previous_primary_baseline - primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              primary_system_tray->GetBubbleBoundsInScreen().y());

    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(previous_primary_baseline,
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  }

  // Open secondary system tray bubble.
  auto* secondary_system_tray =
      second_shelf->GetStatusAreaWidget()->unified_system_tray();
  LeftClickOn(secondary_system_tray);

  if (IsNotifierCollisionEnabled()) {
    // The secondary popup collection should update the baseline and the primary
    // one should reset.
    EXPECT_EQ(previous_primary_baseline,
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());

    auto* secondary_bubble_view =
        secondary_system_tray->bubble()->GetBubbleView();
    EXPECT_EQ(
        secondary_bubble_view->height() + message_center::kMarginBetweenPopups,
        previous_secondary_baseline - secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              secondary_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(previous_primary_baseline,
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  }
}

TEST_P(AshMessagePopupCollectionTest, HistogramRecordedForShelfPodBubble) {
  using SurfaceType = AshMessagePopupCollection::NotifierCollisionSurfaceType;

  base::HistogramTester histogram_tester;
  const std::string popup_count_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesPopupCount";
  const std::string surface_type_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesType";

  AddNotification();

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  if (IsNotifierCollisionEnabled()) {
    // The popup should appear on top of the bubble and histogram is recorded.
    histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 1);
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kShelfPodBubble, 1);
  } else {
    // The popup stays the same if the feature is disabled.
    histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 0);
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kShelfPodBubble, 0);
  }

  // Add another notification. Histogram should also be recorded with the
  // correct bucket for 2 notifications.
  AddNotification();
  AnimateUntilIdle();

  histogram_tester.ExpectBucketCount(popup_count_histogram_name, 2,
                                     IsNotifierCollisionEnabled() ? 1 : 0);

  // Close and re-open the bubble. Histogram should be recorded again.
  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  bubble_widget->CloseNow();
  unified_system_tray->ShowBubble();

  histogram_tester.ExpectBucketCount(popup_count_histogram_name, 2,
                                     IsNotifierCollisionEnabled() ? 2 : 0);
  histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                     SurfaceType::kShelfPodBubble,
                                     IsNotifierCollisionEnabled() ? 2 : 0);
}

TEST_P(AshMessagePopupCollectionTest, HistogramRecordedForSliderAndHotseat) {
  using SurfaceType = AshMessagePopupCollection::NotifierCollisionSurfaceType;

  base::HistogramTester histogram_tester;
  const std::string popup_count_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesPopupCount";
  const std::string surface_type_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesType";

  AddNotification();

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowVolumeSliderBubble();

  if (IsNotifierCollisionEnabled()) {
    // The popup should appear on top of the bubble and histogram is recorded.
    histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 1);
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kSliderBubble, 1);
  } else {
    // The popup stays the same if the feature is disabled.
    histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 0);
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kSliderBubble, 0);
  }

  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_EQ(HotseatState::kHidden,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // Dragging up to show the hotseat.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start = display_bounds.bottom_center();
  const gfx::Point end = start + gfx::Vector2d(0, -80);
  GetEventGenerator()->GestureScrollSequence(
      start, end, /*duration=*/base::Milliseconds(100),
      /*steps=*/4);
  ASSERT_EQ(HotseatState::kExtended,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // Histogram should be recorded accordingly to the adjusted baseline.
  if (IsNotifierCollisionEnabled()) {
    histogram_tester.ExpectBucketCount(
        surface_type_histogram_name,
        SurfaceType::kSliderBubbleAndExtendedHotseat, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        surface_type_histogram_name,
        SurfaceType::kSliderBubbleAndExtendedHotseat, 0);
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kExtendedHotseat, 1);
  }

  unified_system_tray->CloseSecondaryBubbles();
  if (IsNotifierCollisionEnabled()) {
    histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                       SurfaceType::kExtendedHotseat, 1);
  }
}

TEST_P(AshMessagePopupCollectionTest, HistogramNotRecordedWhenAllPopupsClosed) {
  if (!IsNotifierCollisionEnabled()) {
    return;
  }

  using SurfaceType = AshMessagePopupCollection::NotifierCollisionSurfaceType;

  base::HistogramTester histogram_tester;
  const std::string popup_count_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesPopupCount";
  const std::string surface_type_histogram_name =
      "Ash.NotificationPopup.OnTopOfSurfacesType";

  UpdateDisplay("801x800");

  AddNotification(/*has_image=*/true);
  ASSERT_TRUE(GetLastPopUpAdded());

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  // Histogram is recorded when open the bubble.
  histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 1);
  histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                     SurfaceType::kShelfPodBubble, 1);

  // Increase the bubble height so that there's not enough space to display the
  // popups on top of it. Note that this only works with screen height of 800
  // (set above), and the test might fail if we change the height of bubble
  // width or notification width in the future.
  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  auto bubble_bounds = bubble_widget->GetWindowBoundsInScreen();
  bubble_widget->SetBounds(gfx::Rect(bubble_bounds.x(), bubble_bounds.y() - 100,
                                     bubble_bounds.width(),
                                     bubble_bounds.height() + 100));

  ASSERT_FALSE(GetLastPopUpAdded());

  // Histogram should not be recorded in this case.
  histogram_tester.ExpectBucketCount(popup_count_histogram_name, 1, 1);
  histogram_tester.ExpectBucketCount(surface_type_histogram_name,
                                     SurfaceType::kShelfPodBubble, 1);
}

TEST_P(AshMessagePopupCollectionTest, NotificationAddedOnTrayBubbleOpen) {
  UpdateDisplay("801x600");

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  // Show a tray bubble.
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();

  // Add a notification with a tray bubble open.
  AddNotification();
  auto* popup1 = GetLastPopUpAdded();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Add more popup so that there's not enough space to display the popup above
  // the tray bubble. Note that this only works with screen height of 600 (set
  // above), and the test might fail if we change the height of bubble width or
  // notification width in the future.
  auto id2 = AddNotification();
  auto id3 = AddNotification();

  AnimateUntilIdle();

  // The baseline should still be the same when there's notification added.
  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    EXPECT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Popup 2 should be right above the first one.
  auto* popup2 = popup_collection->GetPopupViewForNotificationID(id2);
  ASSERT_TRUE(popup2);
  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());

  if (IsNotifierCollisionEnabled()) {
    // Popup for the third notification should not be displayed since there's
    // not enough space.
    EXPECT_FALSE(popup_collection->GetPopupViewForNotificationID(id3));
  } else {
    // The popup is still displayed if the feature is disabled.
    EXPECT_TRUE(popup_collection->GetPopupViewForNotificationID(id3));
  }
}

TEST_P(AshMessagePopupCollectionTest, NotificationUpdatedOnTrayBubbleOpen) {
  UpdateDisplay("801x600");

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();

  AddNotification();
  auto* popup1 = GetLastPopUpAdded();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Add a second popup, it should be on top of the first one and baseline
  // offset should stay the same.
  auto id2 = AddNotification();

  AnimateUntilIdle();

  auto* popup2 = popup_collection->GetPopupViewForNotificationID(id2);

  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());

  // Update the notification to have an image now, which increases the height of
  // the notification and make it not fit above the tray bubble anymore. In this
  // case, all the notifications should move down to make room for the change.
  // Note that this only works with screen height of 600 (set above), and the
  // test might fail if we change the height of bubble width or notification
  // width in the future.
  message_center::MessageCenter::Get()->UpdateNotification(
      id2, CreateSimpleNotification(id2, /*has_image=*/true));
  popup2 = popup_collection->GetPopupViewForNotificationID(id2);
  AnimateUntilIdle();

  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
  EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());
}

// Tests that a corner anchored shelf pod bubble closes when the popup
// collection height expands and it needs more space for it to be displayed.
TEST_P(AshMessagePopupCollectionTest,
       BubbleCloses_OnPopupExpandedUsedAvailableSpace) {
  UpdateDisplay("801x800");

  AddNotification(/*has_image=*/true);
  auto* popup1 = GetLastPopUpAdded();

  auto id2 = AddNotification();
  AnimateUntilIdle();
  auto* popup_collection = GetPrimaryPopupCollection();
  auto* popup2 = popup_collection->GetPopupViewForNotificationID(id2);

  int previous_baseline = popup_collection->GetBaseline();

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
    return;
  }

  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());

  LeftClickOn(static_cast<AshNotificationView*>(popup1->message_view())
                  ->expand_button_for_test());

  // Since the space left on the screen above the bubble is not enough to
  // display the popup collection when the popup is expanded, the bubble will be
  // closed to make room for it and we move down the baseline. Note that this
  // only works with screen height of 800 (set above), and the test might fail
  // if we change the height of the bubble or notification in the future.
  EXPECT_FALSE(unified_system_tray->bubble());
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
}

// Tests that popups will be closed when a tray bubble visibility or bounds
// change and there is not enough space for the popups to be displayed.
TEST_P(AshMessagePopupCollectionTest,
       PopupsClose_OnBubbleHeightChangedUsedAvailableSpace) {
  UpdateDisplay("801x800");

  AddNotification(/*has_image=*/true);
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // The popup collection height before the bubble height is increased.
  int previous_popup_collection_height =
      popup_collection->popup_collection_bounds().height();

  // Increase the bubble height so that there's not enough space to display the
  // popups on top of it. Note that this only works with screen height of 800
  // (set above), and the test might fail if we change the height of bubble
  // width or notification width in the future.
  auto bubble_bounds = bubble_widget->GetWindowBoundsInScreen();
  bubble_widget->SetBounds(gfx::Rect(bubble_bounds.x(), bubble_bounds.y() - 100,
                                     bubble_bounds.width(),
                                     bubble_bounds.height() + 100));

  if (IsNotifierCollisionEnabled()) {
    // Since there was not enough space to display the popup, all popups should
    // be closed and will go to the notification center tray.
    EXPECT_GT(previous_popup_collection_height,
              popup_collection->GetBaseline());
    EXPECT_FALSE(GetLastPopUpAdded());
  } else {
    EXPECT_TRUE(GetLastPopUpAdded());
  }
}

// Tests that when a shelf pod bubble other than the main status area bubbles
// (e.g. phone hub) is shown and a slider appears, the popup will be on top of
// the shelf pod bubble, not the slider.
TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdates_OnTrayBubbleAndSliderShown) {
  UpdateDisplay("1001x900");

  auto* popup_collection = GetPrimaryPopupCollection();
  int previous_baseline = popup_collection->GetBaseline();

  phone_hub_manager()->fake_feature_status_provider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  auto* phone_hub_tray =
      GetPrimaryShelf()->status_area_widget()->phone_hub_tray();
  phone_hub_tray->SetPhoneHubManager(phone_hub_manager());
  ASSERT_TRUE(phone_hub_tray->GetVisible());

  phone_hub_tray->ShowBubble();

  auto* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowVolumeSliderBubble();
  auto* slider_view = system_tray->GetSliderView();
  ASSERT_TRUE(slider_view);

  AddNotification(/*has_image=*/true);
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  auto* bubble_view = phone_hub_tray->GetBubbleView();
  auto* bubble_widget = phone_hub_tray->GetBubbleWidget();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appear on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups` (not on top of the slider).
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              bubble_view->GetBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Close the phone hub bubble. Popup should sit above slider.
  bubble_widget->CloseNow();

  if (IsNotifierCollisionEnabled()) {
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
    EXPECT_EQ(slider_view->height() + message_center::kMarginBetweenPopups,
              previous_baseline - popup_collection->GetBaseline());
  } else {
    // The popup stays the same if notifier collision is disabled.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
    EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
  }

  // Close the slider. Popup should go back to original baseline.
  system_tray->CloseSecondaryBubbles();
  EXPECT_EQ(previous_baseline, popup_collection->GetBaseline());
}

// b/293660273
TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdates_OnSliderShownWithMultiDisplay) {
  UpdateDisplay("0+0-801x800,0+800-801x800");

  display::Display second_display = GetSecondaryDisplay();
  AshMessagePopupCollection secondary_popup_collection(
      display::Screen::GetScreen(),
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())
          ->shelf());
  UpdateWorkArea(&secondary_popup_collection, second_display);

  auto* primary_popup_collection = GetPrimaryPopupCollection();
  int previous_primary_baseline = primary_popup_collection->GetBaseline();
  int previous_secondary_baseline = secondary_popup_collection.GetBaseline();

  // Add a notification popup.
  AddNotification();
  auto* primary_popup = GetLastPopUpAdded();
  auto* secondary_popup =
      GetLastPopUpAddedForCollection(&secondary_popup_collection);
  EXPECT_TRUE(primary_popup);
  EXPECT_TRUE(secondary_popup);

  // Show a slider on the primary display only.
  auto* primary_system_tray = GetPrimaryUnifiedSystemTray();
  primary_system_tray->ShowVolumeSliderBubble();
  auto* slider_view = primary_system_tray->GetSliderView();
  ASSERT_TRUE(slider_view);

  if (IsNotifierCollisionEnabled()) {
    // Popup on primary display should move up, and popup on secondary display
    // stay the same.
    EXPECT_EQ(
        slider_view->height() + message_center::kMarginBetweenPopups,
        previous_primary_baseline - primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());

    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(previous_primary_baseline,
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  }

  // Show a slider on the secondary display.
  auto* secondary_system_tray =
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
          ->unified_system_tray();
  secondary_system_tray->ShowVolumeSliderBubble();
  auto* secondary_slider_view = secondary_system_tray->GetSliderView();
  ASSERT_TRUE(secondary_slider_view);

  if (IsNotifierCollisionEnabled()) {
    // Popup on both displays should move up since there are sliders on both
    // displays.
    EXPECT_EQ(
        slider_view->height() + message_center::kMarginBetweenPopups,
        previous_primary_baseline - primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());

    EXPECT_EQ(
        secondary_slider_view->height() + message_center::kMarginBetweenPopups,
        previous_secondary_baseline - secondary_popup_collection.GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              secondary_slider_view->GetBoundsInScreen().y());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(previous_primary_baseline,
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(previous_secondary_baseline,
              secondary_popup_collection.GetBaseline());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  }
}

// b/291988617
TEST_P(AshMessagePopupCollectionTest, QsBubbleNotCloseWhenPopupClose) {
  // Skip since b/291988617 only happens when both features are enabled.
  if (!IsNotifierCollisionEnabled()) {
    return;
  }

  // Create a window to simulate the step from b/291988617.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  auto id = AddNotification();

  auto* popup_collection = GetPrimaryPopupCollection();
  auto* popup = popup_collection->GetPopupViewForNotificationID(id);

  ASSERT_TRUE(unified_system_tray->bubble());
  ASSERT_TRUE(popup);

  AnimateUntilIdle();

  // Click the notification close button, the popup should disappear. However,
  // the bubble show still remain open.
  LeftClickOn(static_cast<AshNotificationView*>(popup->message_view())
                  ->control_buttons_view_for_test()
                  ->close_button());

  AnimateUntilIdle();

  EXPECT_FALSE(popup_collection->GetPopupViewForNotificationID(id));
  EXPECT_TRUE(unified_system_tray->bubble());
}

// Same as the above test. But now test with a bubble created by
// `TrayBubbleWrapper` instead of the QS bubble. We will use Phone Hub bubble in
// this case.
TEST_P(AshMessagePopupCollectionTest, BubbleNotCloseWhenPopupClose) {
  // Skip since b/291988617 only happens when both features are enabled.
  if (!IsNotifierCollisionEnabled()) {
    return;
  }

  // Update display so that notification fit on top of phone hub bubble.
  UpdateDisplay("1001x900");

  phone_hub_manager()->fake_feature_status_provider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  auto* phone_hub_tray =
      GetPrimaryShelf()->status_area_widget()->phone_hub_tray();
  phone_hub_tray->SetPhoneHubManager(phone_hub_manager());
  ASSERT_TRUE(phone_hub_tray->GetVisible());

  phone_hub_tray->ShowBubble();

  auto id = AddNotification();

  auto* popup_collection = GetPrimaryPopupCollection();
  auto* popup = popup_collection->GetPopupViewForNotificationID(id);

  ASSERT_TRUE(phone_hub_tray->GetBubbleView());
  ASSERT_TRUE(popup);

  AnimateUntilIdle();

  // Click the notification close button, the popup should disappear. However,
  // the bubble show still remain open.
  LeftClickOn(static_cast<AshNotificationView*>(popup->message_view())
                  ->control_buttons_view_for_test()
                  ->close_button());

  AnimateUntilIdle();

  EXPECT_FALSE(popup_collection->GetPopupViewForNotificationID(id));
  EXPECT_TRUE(phone_hub_tray->GetBubbleView());
}

// For b/346641561
TEST_P(AshMessagePopupCollectionTest, InlineReplyTextfield) {
  if (!IsNotifierCollisionEnabled()) {
    GTEST_SKIP() << "Popup notifications does not show when notifier collision "
                    "is enabled";
  }

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  // Attempt showing a notification when Quick Settings is open.
  AddNotification(/*has_image=*/false,
                  /*origin_url=*/GURL(),
                  /*has_inline_reply=*/true);
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  AnimateUntilIdle();

  auto* message_view =
      static_cast<AshNotificationView*>(GetLastPopUpAdded()->message_view());
  ASSERT_TRUE(message_view);

  LeftClickOn(message_view->GetActionButtonsForTest().front());

  auto* textfield = message_view->GetInlineReplyForTest()->textfield();
  EXPECT_TRUE(textfield->GetVisible());
  EXPECT_TRUE(textfield->HasFocus());

  PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // Make sure that inline reply textfield can receive keyboard events.
  EXPECT_EQ(u"aa", textfield->GetText());
}

class AshMessagePopupCollectionMockTimeTest : public ash::AshTestBase {
 public:
  AshMessagePopupCollectionMockTimeTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AshMessagePopupCollectionMockTimeTest(
      const AshMessagePopupCollectionMockTimeTest&) = delete;
  AshMessagePopupCollectionMockTimeTest& operator=(
      const AshMessagePopupCollectionMockTimeTest&) = delete;
  ~AshMessagePopupCollectionMockTimeTest() override = default;
};

TEST_F(AshMessagePopupCollectionMockTimeTest, PopupTimeouts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::features::kNotificationsIgnoreRequireInteraction);

  auto* popup_collection =
      GetPrimaryNotificationCenterTray()->popup_collection();
  auto* message_center = message_center::MessageCenter::Get();
  std::string id = "0";
  auto notification_priorities = {
      message_center::DEFAULT_PRIORITY, message_center::HIGH_PRIORITY,
      message_center::MAX_PRIORITY, message_center::SYSTEM_PRIORITY};

  // Make sure all notification popups below `SYSTEM_PRIORITY` are dismissed
  // after `kAutocloseShortDelaySeconds` seconds. Also, make sure
  // `SYSTEM_PRIORITY` notifications are dismissed after
  // `kAutocloseCrosHighPriorityDelaySeconds`.
  for (auto priority : notification_priorities) {
    auto notification = CreateSimpleNotification(id);
    notification->set_priority(priority);
    if (priority == message_center::SYSTEM_PRIORITY) {
      notification->SetSystemPriority();
    }
    message_center->AddNotification(std::move(notification));
    EXPECT_TRUE(popup_collection->GetPopupViewForNotificationID(id));

    int timeout = priority == message_center::SYSTEM_PRIORITY
                      ? message_center::kAutocloseCrosHighPriorityDelaySeconds
                      : message_center::kAutocloseShortDelaySeconds;
    task_environment()->FastForwardBy(base::Seconds(timeout - 1));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(popup_collection->GetPopupViewForNotificationID(id));

    task_environment()->FastForwardBy(base::Seconds(timeout));
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(popup_collection->GetPopupViewForNotificationID(id));

    message_center->RemoveNotification(id,
                                       /*by_user=*/false);
  }
}

}  // namespace ash
