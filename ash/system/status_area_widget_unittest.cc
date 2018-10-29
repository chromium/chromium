// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/message_center/notification_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/system_tray_focus_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/test/keyboard_test_util.h"

using session_manager::SessionState;

namespace ash {

using StatusAreaWidgetTest = AshTestBase;

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

  // Needed because NotificationTray updates its initial visibility
  // asynchronously.
  RunAllPendingInMessageLoop();

  // Default trays are visible.
  EXPECT_FALSE(status->overview_button_tray()->visible());
  EXPECT_TRUE(status->unified_system_tray()->visible());
  EXPECT_FALSE(status->logout_button_tray_for_testing()->visible());
  EXPECT_FALSE(status->ime_menu_tray()->visible());
  EXPECT_FALSE(status->virtual_keyboard_tray_for_testing()->visible());
}

class SystemTrayFocusTestObserver : public SystemTrayFocusObserver {
 public:
  SystemTrayFocusTestObserver() = default;
  ~SystemTrayFocusTestObserver() override = default;

  int focus_out_count() { return focus_out_count_; }
  int reverse_focus_out_count() { return reverse_focus_out_count_; }

 protected:
  // SystemTrayFocusObserver:
  void OnFocusLeavingSystemTray(bool reverse) override {
    reverse ? ++reverse_focus_out_count_ : ++focus_out_count_;
  }

 private:
  int focus_out_count_ = 0;
  int reverse_focus_out_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayFocusTestObserver);
};

class StatusAreaWidgetFocusTest : public AshTestBase {
 public:
  StatusAreaWidgetFocusTest() = default;
  ~StatusAreaWidgetFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowWebUiLock);

    AshTestBase::SetUp();
    test_observer_.reset(new SystemTrayFocusTestObserver);
    Shell::Get()->system_tray_notifier()->AddSystemTrayFocusObserver(
        test_observer_.get());
  }

  // AshTestBase:
  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveSystemTrayFocusObserver(
        test_observer_.get());
    test_observer_.reset();
    AshTestBase::TearDown();
  }

  void GenerateTabEvent(bool reverse) {
    ui::KeyEvent tab_pressed(ui::ET_KEY_PRESSED, ui::VKEY_TAB,
                             reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->OnKeyEvent(&tab_pressed);
  }

 protected:
  std::unique_ptr<SystemTrayFocusTestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetFocusTest);
};

// Tests that tab traversal through status area widget in non-active session
// could properly send FocusOut event.
TEST_F(StatusAreaWidgetFocusTest, FocusOutObserverUnified) {
  // Set session state to LOCKED.
  SessionController* session = Shell::Get()->session_controller();
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

  // Needed because NotificationTray updates its initial visibility
  // asynchronously.
  base::RunLoop().RunUntilIdle();

  // Default trays are visible.
  ASSERT_FALSE(status->overview_button_tray()->visible());
  ASSERT_TRUE(status->unified_system_tray()->visible());
  ASSERT_FALSE(status->logout_button_tray_for_testing()->visible());
  ASSERT_FALSE(status->ime_menu_tray()->visible());
  ASSERT_FALSE(status->virtual_keyboard_tray_for_testing()->visible());

  // In Unified, we don't have notification tray, so ImeMenuTray is used for
  // tab testing.
  status->ime_menu_tray()->OnIMEMenuActivationChanged(true);
  ASSERT_TRUE(status->ime_menu_tray()->visible());

  // Set focus to status area widget, which will be be system tray.
  ASSERT_TRUE(Shell::Get()->focus_cycler()->FocusWidget(status));
  views::FocusManager* focus_manager = status->GetFocusManager();
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());

  // A tab key event will move focus to notification tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(0, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // Another tab key event will send FocusOut event, since we are not handling
  // this event, focus will still be moved to system tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // A reverse tab key event will send reverse FocusOut event, since we are not
  // handling this event, focus will still be moved to notification tray.
  GenerateTabEvent(true);
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());
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
  ~UnifiedStatusAreaWidgetTest() override = default;

  // AshTestBase:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    // Initializing NetworkHandler before ash is more like production.
    chromeos::NetworkHandler::Initialize();
    AshTestBase::SetUp();
    chromeos::NetworkHandler::Get()->InitializePrefServices(&profile_prefs_,
                                                            &local_state_);
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // This roughly matches production shutdown order.
    chromeos::NetworkHandler::Get()->ShutdownPrefServices();
    AshTestBase::TearDown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 private:
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedStatusAreaWidgetTest);
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

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    keyboard_controller()->SetContainerType(keyboard::ContainerType::FLOATING,
                                            base::nullopt, base::DoNothing());
    keyboard_controller()->GetKeyboardWindow()->SetBounds(
        gfx::Rect(0, 0, 10, 10));
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       ClickingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisible(true);
  status->ime_menu_tray()->SetVisible(true);

  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard_controller()->NotifyKeyboardWindowLoaded();
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // The keyboard should hide when clicked.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(status->virtual_keyboard_tray_for_testing()
                                      ->GetBoundsInScreen()
                                      .CenterPoint());
  generator->ClickLeftButton();
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       TappingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisible(true);
  status->ime_menu_tray()->SetVisible(true);

  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard_controller()->NotifyKeyboardWindowLoaded();
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // The keyboard should hide when tapped.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(status->virtual_keyboard_tray_for_testing()
                              ->GetBoundsInScreen()
                              .CenterPoint());
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, ClickingHidesVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard_controller()->NotifyKeyboardWindowLoaded();
  ASSERT_TRUE(keyboard_controller()->IsKeyboardVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->ClickLeftButton();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, TappingHidesVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard_controller()->NotifyKeyboardWindowLoaded();
  ASSERT_TRUE(keyboard_controller()->IsKeyboardVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->PressTouch();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, DoesNotHideLockedVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(true /* locked */);
  keyboard_controller()->NotifyKeyboardWindowLoaded();
  ASSERT_TRUE(keyboard_controller()->IsKeyboardVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());

  generator->ClickLeftButton();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());

  generator->PressTouch();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

}  // namespace ash
