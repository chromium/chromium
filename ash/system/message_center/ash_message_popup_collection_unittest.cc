// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_popup_collection.h"

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
#include "ash/system/message_center/ash_notification_expand_button.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/message_center/message_center_test_util.h"
#include "ash/system/message_center/message_popup_animation_waiter.h"
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
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
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
#include "url/gurl.h"

namespace ash {

namespace {

AshNotificationView* GetNotificationViewFromPopup(
    message_center::MessagePopupView* popup) {
  return static_cast<AshNotificationView*>(popup->message_view());
}

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

class AshMessagePopupCollectionTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<
          /*IsQsRevampEnabled=*/bool,
          /*IsNotifierCollisionEnabled=*/bool>> {
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

    if (IsQsRevampEnabled()) {
      enabled_features.emplace_back(features::kQsRevamp);
    } else {
      disabled_features.emplace_back(features::kQsRevamp);
    }

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

  bool IsQsRevampEnabled() const { return std::get<0>(GetParam()); }
  bool IsNotifierCollisionEnabled() const { return std::get<1>(GetParam()); }

 protected:
  enum Position { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, OUTSIDE };

  AshMessagePopupCollection* GetPrimaryPopupCollection() {
    return GetPrimaryUnifiedSystemTray()->GetMessagePopupCollection();
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

    if (center_point.x() < point.x())
      return (center_point.y() < point.y()) ? BOTTOM_RIGHT : TOP_RIGHT;
    else
      return (center_point.y() < point.y()) ? BOTTOM_LEFT : TOP_LEFT;
  }

  gfx::Rect GetWorkArea() { return GetPrimaryPopupCollection()->work_area_; }

  std::string AddNotification(bool has_image = false,
                              const GURL& origin_url = GURL()) {
    std::string id = base::NumberToString(notification_id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateSimpleNotification(id, has_image, origin_url));
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

INSTANTIATE_TEST_SUITE_P(
    All,
    AshMessagePopupCollectionTest,
    testing::Combine(/*IsQsRevampEnabled()=*/testing::Bool(),
                     /*IsNotifierCollisionEnabled()=*/testing::Bool()));

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
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
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

TEST_P(AshMessagePopupCollectionTest, BaselineOffset) {
  const gfx::Rect popup_size(0, 0, 10, 10);
  UpdateDisplay("601x600");
  auto* popup_collection = GetPrimaryPopupCollection();

  int origin_x = popup_collection->GetPopupOriginX(popup_size);
  int baseline = popup_collection->GetBaseline();

  // Simulate a secondary bubble (e.g. QS slider) being shown on screen.
  const int kSecondaryBubbleHeight = 100;
  popup_collection->SetBaselineOffset(kSecondaryBubbleHeight);

  EXPECT_EQ(origin_x, popup_collection->GetPopupOriginX(popup_size));
  EXPECT_EQ(
      baseline - kSecondaryBubbleHeight - message_center::kMarginBetweenPopups,
      popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, Extended) {
  UpdateDisplay("601x600,801x800");

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection for_2nd_display(second_shelf);
  UpdateWorkArea(&for_2nd_display, second_display);
  // Make sure that the popup position on the secondary display is
  // positioned correctly.
  EXPECT_LT(1300, for_2nd_display.GetPopupOriginX(gfx::Rect(0, 0, 10, 10)));
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

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  ASSERT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  const int baseline_with_hidden_shelf = popup_collection->GetBaseline();
  EXPECT_NE(baseline_with_visible_shelf, baseline_with_hidden_shelf);

  auto* overview_controller = Shell::Get()->overview_controller();
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
  // Wait for animation to end.
  MessagePopupAnimationWaiter(GetPrimaryPopupCollection()).Wait();

  EXPECT_FALSE(GetLastPopUpAdded());
}

// Tests that notification popup baseline is correct when entering and exiting
// tablet mode in a full screen window.
TEST_P(AshMessagePopupCollectionTest, BaselineInTabletMode) {
  UpdateDisplay("800x600");
  ASSERT_TRUE(GetPrimaryShelf()->IsHorizontalAlignment());

  auto* popup_collection = GetPrimaryPopupCollection();

  // Baseline is higher than the top of the shelf in clamshell mode.
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());

  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();

  // Baseline is higher than the top of the shelf after entering tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(tablet_mode_controller->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());

  // Baseline is higher than the top of the shelf after exiting tablet mode.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_FALSE(tablet_mode_controller->InTabletMode());
  EXPECT_GT(GetPrimaryShelf()->GetShelfBoundsInScreen().y(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest, BaselineUpdatesAfterSliderShown) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  AddNotification();
  auto* popup = GetLastPopUpAdded();
  ASSERT_TRUE(popup);

  auto* popup_collection = GetPrimaryPopupCollection();
  auto* system_tray = GetPrimaryUnifiedSystemTray();

  system_tray->ShowVolumeSliderBubble();
  auto* slider_view = system_tray->GetSliderView();
  ASSERT_TRUE(slider_view);

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the slider bubble, separated by
    // a padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
    EXPECT_EQ(slider_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Baseline returns to previous value when the slider bubble is closed.
  system_tray->CloseSecondaryBubbles();
  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());

  // The popup is adjusted to be at the baseline without the offset.
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest,
       BaselineUpdatesAfterSliderShownOnShelfAutohide) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  // Create a window, otherwise autohide doesn't work.
  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
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
    // On hidden shelf, the added popup should appears on top of the slider
    // bubble, separated by a padding of `kMarginBetweenPopups`.
    EXPECT_EQ(shelf_hide_popup_bottom + message_center::kMarginBetweenPopups,
              slider_view->GetBoundsInScreen().y());
  }

  // Move mouse to the shelf to make it shows.
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
  // Notification popups close when Quick Settings is opened pre-QsRevamp.
  if (!IsQsRevampEnabled()) {
    return;
  }

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

TEST_P(AshMessagePopupCollectionTest, AdjustBaselineBasedOnTrayBubble) {
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification();
  auto* popup = GetLastPopUpAdded();

  if (!IsQsRevampEnabled()) {
    // When QsRevamp is not enabled, the popup will not be shown when Quick
    // Settings is open.
    EXPECT_FALSE(popup);
    return;
  }

  ASSERT_TRUE(popup);

  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();
  auto* popup_collection = GetPrimaryPopupCollection();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    EXPECT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
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
              popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // When bubble is closed, no offset should be set.
  // NOTE: We use `CloseNow()` here instead of calling `CloseBubble()` on
  // `unified_system_tray` to avoid the delay in the message loop happen in
  // `Widget::Close()`.
  bubble_widget->CloseNow();
  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());

  // The popup is adjusted to be at the baseline without the offset.
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

TEST_P(AshMessagePopupCollectionTest,
       AdjustBaselineBasedOnTrayBubbleAutoHideShelf) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  // Create a window, otherwise autohide doesn't work.
  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Move mouse to the shelf to make it shows.
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
  auto* popup_collection = GetPrimaryPopupCollection();
  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
  EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
}

// Tests that the baseline will not be adjusted when a tray bubble that is not
// anchored to the shelf corner opens (i.e. the IME tray bubble).
TEST_P(AshMessagePopupCollectionTest,
       NotAdjustBaselineForNonAnchoredTrayBubble) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  auto* ime_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->ime_menu_tray();
  ASSERT_TRUE(ime_tray->GetVisible());

  auto* popup_collection = GetPrimaryPopupCollection();

  ime_tray->ShowBubble();
  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());

  ime_tray->GetBubbleWidget()->CloseNow();
  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
}

TEST_P(AshMessagePopupCollectionTest, AdjustBaselineForTrayBubbleMultiDisplay) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("801x800,801x800");

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection secondary_popup_collection(second_shelf);
  UpdateWorkArea(&secondary_popup_collection, second_display);

  auto* primary_popup_collection = GetPrimaryPopupCollection();

  EXPECT_EQ(0, primary_popup_collection->baseline_offset_for_test());
  EXPECT_EQ(0, secondary_popup_collection.baseline_offset_for_test());

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
        primary_popup_collection->baseline_offset_for_test());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              primary_system_tray->GetBubbleBoundsInScreen().y());

    EXPECT_EQ(0, secondary_popup_collection.baseline_offset_for_test());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(0, primary_popup_collection->baseline_offset_for_test());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(0, secondary_popup_collection.baseline_offset_for_test());
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
    EXPECT_EQ(0, primary_popup_collection->baseline_offset_for_test());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());

    auto* secondary_bubble_view =
        secondary_system_tray->bubble()->GetBubbleView();
    EXPECT_EQ(
        secondary_bubble_view->height() + message_center::kMarginBetweenPopups,
        secondary_popup_collection.baseline_offset_for_test());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              secondary_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup on both display should stay the same if the feature is
    // disabled.
    EXPECT_EQ(0, primary_popup_collection->baseline_offset_for_test());
    EXPECT_EQ(primary_popup->GetBoundsInScreen().bottom(),
              primary_popup_collection->GetBaseline());
    EXPECT_EQ(0, secondary_popup_collection.baseline_offset_for_test());
    EXPECT_EQ(secondary_popup->GetBoundsInScreen().bottom(),
              secondary_popup_collection.GetBaseline());
  }
}

TEST_P(AshMessagePopupCollectionTest, NotificationAddedOnTrayBubbleOpen) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("801x600");

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification();
  auto* popup1 = GetLastPopUpAdded();

  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();
  auto* popup_collection = GetPrimaryPopupCollection();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
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
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
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
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("801x600");

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification();
  auto* popup1 = GetLastPopUpAdded();

  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();
  auto* popup_collection = GetPrimaryPopupCollection();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
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
  EXPECT_EQ(IsNotifierCollisionEnabled()
                ? bubble_view->height() + message_center::kMarginBetweenPopups
                : 0,
            popup_collection->baseline_offset_for_test());

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

  EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
  EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
            popup_collection->GetBaseline());
  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());
}

TEST_P(AshMessagePopupCollectionTest, DisableExpandCollapseNotification) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("801x800");

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification(/*has_image=*/true);
  auto* popup1 = GetLastPopUpAdded();

  auto id2 = AddNotification();
  AnimateUntilIdle();
  auto* popup_collection = GetPrimaryPopupCollection();
  auto* popup2 = popup_collection->GetPopupViewForNotificationID(id2);

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup1->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup1->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  EXPECT_EQ(popup2->GetBoundsInScreen().bottom() +
                message_center::kMarginBetweenPopups,
            popup1->GetBoundsInScreen().y());

  // Since the space left on the screen above the popups is less than the
  // threshold, expand/collapse behavior should be disabled on all the popups.
  // Note that this only works with screen height of 800 (set above), and the
  // test might fail if we change the height of bubble width or notification
  // width in the future.
  EXPECT_EQ(
      IsNotifierCollisionEnabled(),
      GetNotificationViewFromPopup(popup1)->disable_expand_collapse_for_test());
  EXPECT_EQ(
      IsNotifierCollisionEnabled(),
      GetNotificationViewFromPopup(popup2)->disable_expand_collapse_for_test());

  // Close the bubble. The popup should be able to expand/collapse again.
  unified_system_tray->bubble()->GetBubbleWidget()->CloseNow();

  EXPECT_FALSE(
      GetNotificationViewFromPopup(popup1)->disable_expand_collapse_for_test());
  EXPECT_FALSE(
      GetNotificationViewFromPopup(popup2)->disable_expand_collapse_for_test());
}

TEST_P(AshMessagePopupCollectionTest, DisableExpandCollapseGroupNotification) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("1001x1000");

  const GURL url(u"http://test-url.com");

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  // Create a group notification popup by adding notifications with the same
  // notifier url, along with a single popup.
  AddNotification(/*has_image=*/false, /*origin_url=*/url);
  AddNotification(/*has_image=*/false, /*origin_url=*/url);
  AddNotification(/*has_image=*/false, /*origin_url=*/url);
  auto* group_popup = GetLastPopUpAdded();
  ASSERT_TRUE(group_popup);

  auto id2 = AddNotification();
  AnimateUntilIdle();
  auto* popup_collection = GetPrimaryPopupCollection();

  auto* single_popup = popup_collection->GetPopupViewForNotificationID(id2);
  ASSERT_TRUE(single_popup);

  // Even when the space left on the screen above the popups is above the
  // threshold, expand/collapse behavior should still be disabled for the group
  // popup and enabled for the single popup. Note that this only works with
  // screen height of 700 (set above), and the test might fail if we change the
  // height of bubble width or notification width in the future.
  EXPECT_EQ(IsNotifierCollisionEnabled(),
            GetNotificationViewFromPopup(group_popup)
                ->disable_expand_collapse_for_test());
  EXPECT_FALSE(GetNotificationViewFromPopup(single_popup)
                   ->disable_expand_collapse_for_test());

  // Close the bubble. The popup should be able to expand/collapse again.
  unified_system_tray->bubble()->GetBubbleWidget()->CloseNow();

  EXPECT_FALSE(GetNotificationViewFromPopup(group_popup)
                   ->disable_expand_collapse_for_test());
  EXPECT_FALSE(GetNotificationViewFromPopup(single_popup)
                   ->disable_expand_collapse_for_test());
}

TEST_P(AshMessagePopupCollectionTest, NotShowPopupWhenBubbleHeightChanged) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("801x800");

  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  AddNotification(/*has_image=*/true);
  auto* popup = GetLastPopUpAdded();

  ASSERT_TRUE(popup);

  auto* bubble_widget = unified_system_tray->bubble()->GetBubbleWidget();
  auto* bubble_view = unified_system_tray->bubble()->GetBubbleView();
  auto* popup_collection = GetPrimaryPopupCollection();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups`.
    ASSERT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              unified_system_tray->GetBubbleBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Increase the bubble height so that there's not enough space to display the
  // bubble on top of it. Note that this only works with screen height of 800
  // (set above), and the test might fail if we change the height of bubble
  // width or notification width in the future.
  auto bubble_bounds = bubble_widget->GetWindowBoundsInScreen();
  bubble_widget->SetBounds(gfx::Rect(bubble_bounds.x(), bubble_bounds.y() - 100,
                                     bubble_bounds.width(),
                                     bubble_bounds.height() + 100));

  // Since there's not enough space to display the popup. It should disappear
  // and the notification will go to the notification center tray. If the
  // feature is disabled, the notification is still shown.
  EXPECT_EQ(!IsNotifierCollisionEnabled(), !!GetLastPopUpAdded());

  // Baseline offset should still be the same.
  EXPECT_EQ(IsNotifierCollisionEnabled()
                ? bubble_view->height() + message_center::kMarginBetweenPopups
                : 0,
            popup_collection->baseline_offset_for_test());
}

TEST_P(AshMessagePopupCollectionTest,
       PopupAndTrayBubbleOpenInVerticallyStackedDisplays) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  auto verify_move_down_behavior =
      [](UnifiedSystemTray* system_tray,
         AshMessagePopupCollection* popup_collection,
         message_center::MessagePopupView* popup,
         bool is_notifier_collision_enabled) {
        system_tray->ShowBubble();

        auto* bubble_widget = system_tray->bubble()->GetBubbleWidget();
        auto* bubble_view = system_tray->bubble()->GetBubbleView();

        if (is_notifier_collision_enabled) {
          // The added popup should appears on top of the tray bubble, separated
          // by a padding of `kMarginBetweenPopups`.
          ASSERT_EQ(popup->GetBoundsInScreen().bottom() +
                        message_center::kMarginBetweenPopups,
                    system_tray->GetBubbleBoundsInScreen().y());
          ASSERT_EQ(
              bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
        } else {
          // The popup stays the same if the feature is disabled.
          EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
          EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
                    popup_collection->GetBaseline());
        }

        // Increase the bubble height so that there's not enough space to
        // display the bubble on top of it. Note that this only works with
        // screen height of 800 (set above), and the test might fail if we
        // change the height of bubble width or notification width in the
        // future.
        auto bubble_bounds = bubble_widget->GetWindowBoundsInScreen();
        bubble_widget->SetBounds(
            gfx::Rect(bubble_bounds.x(), bubble_bounds.y() - 100,
                      bubble_bounds.width(), bubble_bounds.height() + 100));

        // Since there's not enough space to display the popup. The popups
        // should disappear and the notification will go to the notification
        // center tray. If the feature is disabled, the notification is still
        // shown.
        EXPECT_EQ(is_notifier_collision_enabled ? 0u : 1u,
                  popup_collection->GetPopupItemsCount());

        // Baseline offset should be the same.
        EXPECT_EQ(
            is_notifier_collision_enabled
                ? bubble_view->height() + message_center::kMarginBetweenPopups
                : 0,
            popup_collection->baseline_offset_for_test());
      };

  UpdateDisplay("0+0-801x800,0+800-801x800");

  display::Display second_display = GetSecondaryDisplay();
  Shelf* second_shelf =
      Shell::GetRootWindowControllerWithDisplayId(second_display.id())->shelf();
  AshMessagePopupCollection secondary_popup_collection(second_shelf);
  UpdateWorkArea(&secondary_popup_collection, second_display);

  AddNotification(/*has_image=*/true);
  auto* primary_popup = GetLastPopUpAdded();
  auto* secondary_popup =
      GetLastPopUpAddedForCollection(&secondary_popup_collection);
  EXPECT_TRUE(primary_popup);
  EXPECT_TRUE(secondary_popup);

  // Make sure that the move down behavior when expand notification works on
  // each display when they are vertically stacked.
  verify_move_down_behavior(
      /*system_tray=*/GetPrimaryUnifiedSystemTray(),
      /*popup_collection=*/GetPrimaryPopupCollection(),
      /*popup=*/primary_popup,
      /*is_notifier_collision_enabled=*/IsNotifierCollisionEnabled());

  verify_move_down_behavior(
      /*system_tray=*/second_shelf->status_area_widget()->unified_system_tray(),
      /*popup_collection=*/&secondary_popup_collection,
      /*popup=*/secondary_popup,
      /*is_notifier_collision_enabled=*/IsNotifierCollisionEnabled());
}

// Tests that when a shelf pod bubble other than the main status area bubbles
// (QS, calendar, notifications) is shown and a slider appears, the popup will
// be on top of the shelf pod bubble, not the slider. We will use the phone hub
// tray for this test.
TEST_P(AshMessagePopupCollectionTest, AdjustBaselineForTrayBubbleAndSlider) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("1001x900");

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

  auto* popup_collection = GetPrimaryPopupCollection();
  auto* bubble_view = phone_hub_tray->GetBubbleView();

  if (IsNotifierCollisionEnabled()) {
    // The added popup should appears on top of the tray bubble, separated by a
    // padding of `kMarginBetweenPopups` (not on top of the slider).
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              bubble_view->GetBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Close the slider. Popup should stay the same.
  system_tray->CloseSecondaryBubbles();

  if (IsNotifierCollisionEnabled()) {
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              bubble_view->GetBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }

  // Show the slider. Popup should stay the same.
  system_tray->ShowVolumeSliderBubble();

  if (IsNotifierCollisionEnabled()) {
    EXPECT_EQ(popup->GetBoundsInScreen().bottom() +
                  message_center::kMarginBetweenPopups,
              bubble_view->GetBoundsInScreen().y());
    ASSERT_EQ(bubble_view->height() + message_center::kMarginBetweenPopups,
              popup_collection->baseline_offset_for_test());
  } else {
    // The popup stays the same if the feature is disabled.
    EXPECT_EQ(0, popup_collection->baseline_offset_for_test());
    EXPECT_EQ(popup->GetBoundsInScreen().bottom(),
              popup_collection->GetBaseline());
  }
}

// b/291988617
TEST_P(AshMessagePopupCollectionTest, QsBubbleNotCloseWhenPopupClose) {
  // Skip since b/291988617 only happens when both features are enabled.
  if (!IsQsRevampEnabled() || !IsNotifierCollisionEnabled()) {
    return;
  }

  // Create a window to simulate the step from b/291988617.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 50, 50));

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
  if (!IsQsRevampEnabled() || !IsNotifierCollisionEnabled()) {
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

}  // namespace ash
