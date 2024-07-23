// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/status_area_overflow_button_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/system_tray_observer.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"

using session_manager::SessionState;

namespace ash {

class StatusAreaWidgetTest : public AshTestBase {
 protected:
  TrayBackgroundView::RoundedCornerBehavior GetTrayCornerBehavior(
      TrayBackgroundView* tray) {
    return tray->corner_behavior_;
  }
};

// Tests that status area trays are constructed.
TEST_F(StatusAreaWidgetTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();

  // Status area is visible by default.
  EXPECT_TRUE(status->IsVisible());

  // No bubbles are open at startup.
  EXPECT_FALSE(status->IsMessageBubbleShown());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());

  // Default trays are constructed.
  EXPECT_TRUE(status->overview_button_tray());
  EXPECT_TRUE(status->unified_system_tray());
  EXPECT_TRUE(status->logout_button_tray_for_testing());
  EXPECT_TRUE(status->ime_menu_tray());
  EXPECT_TRUE(status->virtual_keyboard_tray_for_testing());
  EXPECT_TRUE(status->palette_tray());

  // Default trays are visible.
  EXPECT_FALSE(status->overview_button_tray()->GetVisible());
  EXPECT_TRUE(status->unified_system_tray()->GetVisible());
  EXPECT_FALSE(status->logout_button_tray_for_testing()->GetVisible());
  EXPECT_FALSE(status->ime_menu_tray()->GetVisible());
  EXPECT_FALSE(status->virtual_keyboard_tray_for_testing()->GetVisible());
}

// Tests that the IME menu shows up when adding a secondary display if the IME
// menu was active.
TEST_F(StatusAreaWidgetTest, MultiDisplayIME) {
  // Typical flow to enable the IME menu is to rely on InputMethodManager
  // observers (of which ImeMenuTray is one) getting notified upon activation of
  // the ime menu. When a new display is added, the IME menu pod should check
  // whether the menu is already active and set visibility.
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  // Create a second display, the IME menu pod should be visible.
  UpdateDisplay("500x400,500x400");
  EXPECT_TRUE(StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
                  ->ime_menu_tray()
                  ->GetVisible());
}

// Tests that the IME menu does not show up when adding a secondary display if
// the IME menu was not active.
TEST_F(StatusAreaWidgetTest, MultiDisplayIMENotActive) {
  // Create a second display, the IME menu pod should not be visible.
  UpdateDisplay("500x400,500x400");
  EXPECT_FALSE(StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
                   ->ime_menu_tray()
                   ->GetVisible());
}

TEST_F(StatusAreaWidgetTest, HandleOnLocaleChange) {
  base::i18n::SetRTLForTesting(false);

  StatusAreaWidget* status_area =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  TrayBackgroundView* ime_menu = status_area->ime_menu_tray();
  TrayBackgroundView* palette = status_area->palette_tray();
  TrayBackgroundView* dictation_button = status_area->dictation_button_tray();
  TrayBackgroundView* select_to_speak = status_area->select_to_speak_tray();

  ime_menu->SetVisiblePreferred(true);
  palette->SetVisiblePreferred(true);
  dictation_button->SetVisiblePreferred(true);
  select_to_speak->SetVisiblePreferred(true);

  // From left to right: `dictation_button`, `select_to_speak`, `ime_menu`,
  // palette.
  EXPECT_GT(palette->layer()->bounds().x(), ime_menu->layer()->bounds().x());
  EXPECT_GT(ime_menu->layer()->bounds().x(),
            select_to_speak->layer()->bounds().x());
  EXPECT_GT(select_to_speak->layer()->bounds().x(),
            dictation_button->layer()->bounds().x());

  // Switch to RTL mode.
  base::i18n::SetRTLForTesting(true);
  // Trigger the LocaleChangeObserver, which should cause a layout of the menu.
  ash::LocaleUpdateController::Get()->OnLocaleChanged();

  // From left to right: palette, ime_menu_, select_to_speak,
  // dictation_button_.
  EXPECT_LT(palette->layer()->bounds().x(), ime_menu->layer()->bounds().x());
  EXPECT_LT(ime_menu->layer()->bounds().x(),
            select_to_speak->layer()->bounds().x());
  EXPECT_LT(select_to_speak->layer()->bounds().x(),
            dictation_button->layer()->bounds().x());

  base::i18n::SetRTLForTesting(false);
}

TEST_F(StatusAreaWidgetTest, OpenTrayBubble) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  StatusAreaWidget* status_area = GetPrimaryShelf()->GetStatusAreaWidget();
  TrayBackgroundView* ime_menu = status_area->ime_menu_tray();
  UnifiedSystemTray* system_tray = status_area->unified_system_tray();

  // Clicking on the system tray should set the open tray bubble in
  // `status_area`.
  LeftClickOn(system_tray);

  EXPECT_EQ(status_area->open_shelf_pod_bubble(),
            system_tray->bubble()->GetBubbleView());

  // Clicking on the ime menu should set the open tray bubble in
  // `status_area`.
  LeftClickOn(ime_menu);

  EXPECT_EQ(status_area->open_shelf_pod_bubble(), ime_menu->GetBubbleView());
}

TEST_F(StatusAreaWidgetTest, OnlyOneOpenTrayBubble) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  StatusAreaWidget* status_area = GetPrimaryShelf()->GetStatusAreaWidget();
  TrayBackgroundView* ime_menu = status_area->ime_menu_tray();
  UnifiedSystemTray* system_tray = status_area->unified_system_tray();

  LeftClickOn(ime_menu);
  ASSERT_EQ(status_area->open_shelf_pod_bubble(), ime_menu->GetBubbleView());

  // Open Quick Settings through the accelerator.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kToggleSystemTrayBubble, {});

  // When there's an open shelf pod bubble and we open another bubble through
  // shortcuts, the previous bubble should hide for the next one to show.
  EXPECT_FALSE(ime_menu->GetBubbleView());
  ASSERT_TRUE(system_tray->bubble());

  EXPECT_EQ(status_area->open_shelf_pod_bubble(),
            system_tray->bubble()->GetBubbleView());
}

// The corner radius of the date tray changes based on the visibility of the
// `NotificationCenterTray`. The date tray should have rounded corners on the
// left if the `NotificationCenterTray` is not visible and no rounded corners
// otherwise.
TEST_F(StatusAreaWidgetTest, DateTrayRoundedCornerBehavior) {
  StatusAreaWidget* status_area =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status_area->notification_center_tray()->GetVisible());
  EXPECT_EQ(GetTrayCornerBehavior(status_area->date_tray()),
            TrayBackgroundView::RoundedCornerBehavior::kStartRounded);

  status_area->notification_center_tray()->SetVisiblePreferred(true);

  EXPECT_EQ(GetTrayCornerBehavior(status_area->date_tray()),
            TrayBackgroundView::RoundedCornerBehavior::kNotRounded);

  status_area->notification_center_tray()->SetVisiblePreferred(false);

  EXPECT_EQ(GetTrayCornerBehavior(status_area->date_tray()),
            TrayBackgroundView::RoundedCornerBehavior::kStartRounded);
}

class SystemTrayFocusTestObserver : public SystemTrayObserver {
 public:
  SystemTrayFocusTestObserver() = default;

  SystemTrayFocusTestObserver(const SystemTrayFocusTestObserver&) = delete;
  SystemTrayFocusTestObserver& operator=(const SystemTrayFocusTestObserver&) =
      delete;

  ~SystemTrayFocusTestObserver() override = default;

  int focus_out_count() { return focus_out_count_; }
  int reverse_focus_out_count() { return reverse_focus_out_count_; }

 protected:
  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override {
    reverse ? ++reverse_focus_out_count_ : ++focus_out_count_;
  }

 private:
  int focus_out_count_ = 0;
  int reverse_focus_out_count_ = 0;
};

class StatusAreaWidgetFocusTest : public AshTestBase {
 public:
  StatusAreaWidgetFocusTest() = default;

  StatusAreaWidgetFocusTest(const StatusAreaWidgetFocusTest&) = delete;
  StatusAreaWidgetFocusTest& operator=(const StatusAreaWidgetFocusTest&) =
      delete;

  ~StatusAreaWidgetFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_observer_ = std::make_unique<SystemTrayFocusTestObserver>();
    Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(
        test_observer_.get());
  }

  // AshTestBase:
  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(
        test_observer_.get());
    test_observer_.reset();
    AshTestBase::TearDown();
  }

  void GenerateTabEvent(bool reverse) {
    ui::KeyEvent tab_pressed(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                             reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->OnKeyEvent(&tab_pressed);
  }

 protected:
  std::unique_ptr<SystemTrayFocusTestObserver> test_observer_;
};

// Tests that tab traversal through status area widget in non-active session
// could properly send FocusOut event.
// TODO(crbug.com/934939): Failing on trybot.
TEST_F(StatusAreaWidgetFocusTest, DISABLED_FocusOutObserverUnified) {
  // Set session state to LOCKED.
  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_TRUE(session->IsActiveUserSessionStarted());
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->SetSessionState(SessionState::LOCKED);
  ASSERT_TRUE(session->IsScreenLocked());

  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  // Default trays are constructed.
  ASSERT_TRUE(status->overview_button_tray());
  ASSERT_TRUE(status->unified_system_tray());
  ASSERT_TRUE(status->logout_button_tray_for_testing());
  ASSERT_TRUE(status->ime_menu_tray());
  ASSERT_TRUE(status->virtual_keyboard_tray_for_testing());

  // Default trays are visible.
  ASSERT_FALSE(status->overview_button_tray()->GetVisible());
  ASSERT_TRUE(status->unified_system_tray()->GetVisible());
  ASSERT_FALSE(status->logout_button_tray_for_testing()->GetVisible());
  ASSERT_FALSE(status->ime_menu_tray()->GetVisible());
  ASSERT_FALSE(status->virtual_keyboard_tray_for_testing()->GetVisible());

  // In Unified, we don't have notification tray, so ImeMenuTray is used for
  // tab testing.
  status->ime_menu_tray()->OnIMEMenuActivationChanged(true);
  ASSERT_TRUE(status->ime_menu_tray()->GetVisible());

  // Set focus to status area widget. The first focused view will be the IME
  // tray.
  ASSERT_TRUE(Shell::Get()->focus_cycler()->FocusWidget(status));
  views::FocusManager* focus_manager = status->GetFocusManager();
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());

  // A tab key event will move focus to the system tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(0, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // Another tab key event will send FocusOut event, since we are not handling
  // this event, focus will remain within the status widhet and will be
  // moved to the IME tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // A reverse tab key event will send reverse FocusOut event, since we are not
  // handling this event, focus will remain within the status widget and will
  // be moved to the system tray.
  GenerateTabEvent(true);
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(1, test_observer_->reverse_focus_out_count());
}

class StatusAreaWidgetPaletteTest : public AshTestBase {
 public:
  StatusAreaWidgetPaletteTest() = default;
  ~StatusAreaWidgetPaletteTest() override = default;

  // testing::Test:
  void SetUp() override {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(switches::kAshForceEnableStylusTools);
    // It's difficult to write a test that marks the primary display as
    // internal before the status area is constructed. Just force the palette
    // for all displays.
    cmd->AppendSwitch(switches::kAshEnablePaletteOnAllDisplays);
    AshTestBase::SetUp();
  }
};

// Tests that the stylus palette tray is constructed.
TEST_F(StatusAreaWidgetPaletteTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->palette_tray());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());
}

class UnifiedStatusAreaWidgetTest : public AshTestBase {
 public:
  UnifiedStatusAreaWidgetTest() = default;

  UnifiedStatusAreaWidgetTest(const UnifiedStatusAreaWidgetTest&) = delete;
  UnifiedStatusAreaWidgetTest& operator=(const UnifiedStatusAreaWidgetTest&) =
      delete;

  ~UnifiedStatusAreaWidgetTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Initializing NetworkHandler before ash is more like production.
    AshTestBase::SetUp();
    network_handler_test_helper_.RegisterPrefs(profile_prefs_.registry(),
                                               local_state_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(profile_prefs_.registry());

    network_handler_test_helper_.InitializePrefs(&profile_prefs_,
                                                 &local_state_);

    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // This roughly matches production shutdown order.
    NetworkHandler::Get()->ShutdownPrefServices();
    AshTestBase::TearDown();
  }

 private:
  NetworkHandlerTestHelper network_handler_test_helper_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(UnifiedStatusAreaWidgetTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->unified_system_tray());
}

class StatusAreaWidgetVirtualKeyboardTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    const gfx::Rect keyboard_bounds(0, 0, 1, 1);
    keyboard_ui_controller()->SetContainerType(
        keyboard::ContainerType::kFloating, keyboard_bounds, base::DoNothing());
  }

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       ClickingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisiblePreferred(true);
  status->ime_menu_tray()->SetVisiblePreferred(true);

  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // The keyboard should hide when clicked.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      status->virtual_keyboard_tray_for_testing()
          ->GetBoundsInScreen()
          .CenterPoint());
  generator->ClickLeftButton();
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       TappingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisiblePreferred(true);
  status->ime_menu_tray()->SetVisiblePreferred(true);

  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // The keyboard should hide when tapped.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(status->virtual_keyboard_tray_for_testing()
                              ->GetBoundsInScreen()
                              .CenterPoint());
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, ClickingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard_ui_controller()->IsKeyboardVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->ClickLeftButton();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, TappingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->PressTouch();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, DoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());

  generator->ClickLeftButton();
  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());

  generator->PressTouch();
  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());
}

class StatusAreaWidgetCollapseStateTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    status_area_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
    overflow_button_ = status_area_->overflow_button_tray();
    virtual_keyboard_ = status_area_->virtual_keyboard_tray_for_testing();
    ime_menu_ = status_area_->ime_menu_tray();
    palette_ = status_area_->palette_tray();
    dictation_button_ = status_area_->dictation_button_tray();
    select_to_speak_ = status_area_->select_to_speak_tray();

    virtual_keyboard_->SetVisiblePreferred(true);
    ime_menu_->SetVisiblePreferred(true);
    palette_->SetVisiblePreferred(true);
    dictation_button_->SetVisiblePreferred(true);
    select_to_speak_->SetVisiblePreferred(true);
  }

  void SetCollapseState(StatusAreaWidget::CollapseState collapse_state) {
    status_area_->set_collapse_state_for_test(collapse_state);

    virtual_keyboard_->UpdateAfterStatusAreaCollapseChange();
    ime_menu_->UpdateAfterStatusAreaCollapseChange();
    palette_->UpdateAfterStatusAreaCollapseChange();
    dictation_button_->UpdateAfterStatusAreaCollapseChange();
    select_to_speak_->UpdateAfterStatusAreaCollapseChange();
  }

  StatusAreaWidget::CollapseState collapse_state() const {
    return status_area_->collapse_state();
  }

  raw_ptr<StatusAreaWidget, DanglingUntriaged> status_area_;
  raw_ptr<StatusAreaOverflowButtonTray, DanglingUntriaged> overflow_button_;
  raw_ptr<TrayBackgroundView, DanglingUntriaged> virtual_keyboard_;
  raw_ptr<TrayBackgroundView, DanglingUntriaged> ime_menu_;
  raw_ptr<TrayBackgroundView, DanglingUntriaged> palette_;
  raw_ptr<TrayBackgroundView, DanglingUntriaged> dictation_button_;
  raw_ptr<TrayBackgroundView, DanglingUntriaged> select_to_speak_;
};

TEST_F(StatusAreaWidgetCollapseStateTest, TrayVisibility) {
  // Initial visibility.
  ime_menu_->SetVisiblePreferred(false);
  virtual_keyboard_->set_show_when_collapsed(false);
  palette_->set_show_when_collapsed(true);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // Post-collapse visibility.
  SetCollapseState(StatusAreaWidget::CollapseState::COLLAPSED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // Expanded visibility.
  SetCollapseState(StatusAreaWidget::CollapseState::EXPANDED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, ImeMenuShownWithVirtualKeyboard) {
  // Set up tray items.
  ime_menu_->set_show_when_collapsed(false);
  palette_->set_show_when_collapsed(true);

  // Collapsing the status area should hide the IME menu tray item.
  SetCollapseState(StatusAreaWidget::CollapseState::COLLAPSED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // But only the IME menu tray item should be shown after showing keyboard,
  // simulated here by OnArcInputMethodSurfaceBoundsChanged().
  Shell::Get()
      ->system_tray_model()
      ->virtual_keyboard()
      ->OnArcInputMethodBoundsChanged(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(ime_menu_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(dictation_button_->GetVisible());
  EXPECT_FALSE(select_to_speak_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, OverflowButtonShownWhenCollapsible) {
  EXPECT_FALSE(overflow_button_->GetVisible());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_TRUE(overflow_button_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, ClickOverflowButton) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();

  // By default, status area is collapsed.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(select_to_speak_->GetVisible());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());

  // Click overflow button.
  LeftClickOn(overflow_button_);

  // All tray buttons should be visible in the expanded state.
  EXPECT_EQ(StatusAreaWidget::CollapseState::EXPANDED, collapse_state());
  EXPECT_TRUE(select_to_speak_->GetVisible());
  EXPECT_TRUE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());

  // Clicking the overflow button again should go back to the collapsed state.
  LeftClickOn(overflow_button_);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(select_to_speak_->GetVisible());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, NewTrayShownWhileCollapsed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  palette_->SetVisiblePreferred(false);
  status_area_->UpdateCollapseState();

  // The palette tray button should not be visible initially.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());

  // Showing it should replace the virtual keyboard tray button as it has higher
  // priority.
  palette_->SetVisiblePreferred(true);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  // We should also check the opacity to make sure the tray isn't visible with
  // zero opacity; see b/265165818.
  EXPECT_EQ(palette_->layer()->opacity(), 1);
}

TEST_F(StatusAreaWidgetCollapseStateTest, TrayHiddenWhileCollapsed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();

  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());

  // The palette tray button should visible initially.
  EXPECT_TRUE(palette_->GetVisible());

  // Hiding it should make the virtual keyboard tray button replace it.
  palette_->SetVisiblePreferred(false);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, AllTraysFitInCollapsedState) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());

  // If all tray buttons can fit in the available space, the overflow button is
  // not shown.
  select_to_speak_->SetVisiblePreferred(false);
  ime_menu_->SetVisiblePreferred(false);
  dictation_button_->SetVisiblePreferred(false);
  EXPECT_EQ(StatusAreaWidget::CollapseState::NOT_COLLAPSIBLE, collapse_state());
  EXPECT_FALSE(overflow_button_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest,
       HideDragHandleOnOverlapInExpandedState) {
  std::unique_ptr<aura::Window> test_window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  ash::TabletModeControllerTestApi().EnterTabletMode();
  status_area_->UpdateCollapseState();

  // By default, status area is collapsed.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  ShelfWidget* const shelf_widget =
      AshTestBase::GetPrimaryShelf()->shelf_widget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();
  ASSERT_TRUE(drag_handle);
  EXPECT_TRUE(drag_handle->GetVisible());

  // Expand the status area.
  GetEventGenerator()->GestureTapAt(
      overflow_button_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(StatusAreaWidget::CollapseState::EXPANDED, collapse_state());

  // Verify that the drag handle was hidden.
  EXPECT_FALSE(drag_handle->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest,
       HideDragHandleWithNudgeOnOverlapInExpandedState) {
  std::unique_ptr<aura::Window> test_window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  ash::TabletModeControllerTestApi().EnterTabletMode();
  status_area_->UpdateCollapseState();

  // By default, status area is collapsed.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());

  ShelfWidget* const shelf_widget =
      AshTestBase::GetPrimaryShelf()->shelf_widget();

  DragHandle* const drag_handle = shelf_widget->GetDragHandle();
  ASSERT_TRUE(drag_handle);
  EXPECT_TRUE(drag_handle->GetVisible());

  // Tap on the drag handle to show drag handle nudge.
  GetEventGenerator()->GestureTapAt(
      drag_handle->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(drag_handle->drag_handle_nudge());
  base::WeakPtr<views::Widget> drag_handle_widget =
      drag_handle->drag_handle_nudge()->GetWidget()->GetWeakPtr();

  // Expand the status area.
  GetEventGenerator()->GestureTapAt(
      overflow_button_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(StatusAreaWidget::CollapseState::EXPANDED, collapse_state());

  // Verify that the drag handle, and drag handle nudge were hidden.
  EXPECT_FALSE(drag_handle->GetVisible());
  EXPECT_FALSE(drag_handle->drag_handle_nudge());
  EXPECT_TRUE(!drag_handle_widget || drag_handle_widget->IsClosed());
}

class StatusAreaWidgetEcheTest : public AshTestBase {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    DCHECK(test_web_view_factory_.get());
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

// Tests that Eche Tray is shown or hidden
TEST_F(StatusAreaWidgetEcheTest, EcheTrayShowHide) {
  StatusAreaWidget* status_area =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(30, 30);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_skia.MakeThreadSafe();
  status_area->eche_tray()->LoadBubble(
      GURL("http://google.com"), gfx::Image(image_skia), u"app 1",
      u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  status_area->eche_tray()->ShowBubble();

  // Auto-hidden shelf would be forced to be visible.
  EXPECT_TRUE(status_area->ShouldShowShelf());

  status_area->eche_tray()->HideBubble();

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status_area->ShouldShowShelf());
}

// Tests that `StatusAreaWidget` keep track of its `open_shelf_pod_bubble()`
// when eche is showing/hiding its bubble.
TEST_F(StatusAreaWidgetEcheTest, StatusAreaOpenTrayBubble) {
  StatusAreaWidget* status_area =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  auto* eche_tray = status_area->eche_tray();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(30, 30);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_skia.MakeThreadSafe();
  eche_tray->LoadBubble(
      GURL("http://google.com"), gfx::Image(image_skia), u"app 1",
      u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray->ShowBubble();

  EXPECT_EQ(eche_tray->GetBubbleView(), status_area->open_shelf_pod_bubble());

  eche_tray->HideBubble();

  EXPECT_EQ(nullptr, status_area->open_shelf_pod_bubble());
}

}  // namespace ash
