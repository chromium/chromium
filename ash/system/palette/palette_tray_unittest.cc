// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray_test_api.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/files/safe_base_name.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class PaletteTrayTest : public AshTestBase {
 public:
  PaletteTrayTest() = default;

  PaletteTrayTest(const PaletteTrayTest&) = delete;
  PaletteTrayTest& operator=(const PaletteTrayTest&) = delete;

  ~PaletteTrayTest() override = default;

  // Fake a stylus ejection.
  void EjectStylus() {
    test_api_->OnStylusStateChanged(ui::StylusState::REMOVED);
  }

  // Fake a stylus insertion.
  void InsertStylus() {
    test_api_->OnStylusStateChanged(ui::StylusState::INSERTED);
  }

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnablePaletteOnAllDisplays);

    stylus_utils::SetHasStylusInputForTesting();

    AshTestBase::SetUp();

    palette_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray();
    test_api_ = std::make_unique<PaletteTrayTestApi>(palette_tray_);

    display::test::DisplayManagerTestApi(display_manager())
        .SetFirstDisplayAsInternalDisplay();
  }

  // Sends a stylus event, which makes the `PaletteTray` show up.
  void ShowPaletteTray() {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->EnterPenPointerMode();
    generator->PressTouch();
    generator->ReleaseTouch();
    generator->ExitPenPointerMode();
    ASSERT_TRUE(palette_tray_->GetVisible());
  }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  }

 protected:
  PrefService* active_user_pref_service() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }

  raw_ptr<PaletteTray, DanglingUntriaged> palette_tray_ = nullptr;  // not owned

  std::unique_ptr<PaletteTrayTestApi> test_api_;
};

// Verify the palette tray button exists and but is not visible initially.
TEST_F(PaletteTrayTest, PaletteTrayIsInvisible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_FALSE(palette_tray_->GetVisible());
}

// Verify if the has seen stylus pref is not set initially, the palette tray
// should become visible after seeing a stylus event.
TEST_F(PaletteTrayTest, PaletteTrayVisibleAfterStylusSeen) {
  ASSERT_FALSE(palette_tray_->GetVisible());
  ASSERT_FALSE(local_state()->GetBoolean(prefs::kHasSeenStylus));

  // Send a stylus event.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();
  generator->ExitPenPointerMode();

  EXPECT_TRUE(palette_tray_->GetVisible());
}

// Verify if the has seen stylus pref is initially set, the palette tray is
// visible.
TEST_F(PaletteTrayTest, StylusSeenPrefInitiallySet) {
  ASSERT_FALSE(palette_tray_->GetVisible());

  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  local_state()->SetBoolean(prefs::kHasSeenStylus, true);

  EXPECT_TRUE(palette_tray_->GetVisible());
}

// Verify if the kEnableStylusTools pref was never set the stylus
// should become visible after a stylus event. Even if kHasSeenStylus
// has been previously set.
TEST_F(PaletteTrayTest, PaletteTrayVisibleIfEnableStylusToolsNotSet) {
  local_state()->SetBoolean(prefs::kHasSeenStylus, true);
  ASSERT_FALSE(palette_tray_->GetVisible());

  // Send a stylus event.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();
  generator->ExitPenPointerMode();

  EXPECT_TRUE(palette_tray_->GetVisible());

  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, false);
  EXPECT_FALSE(palette_tray_->GetVisible());

  // Send a stylus event.
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();
  generator->ExitPenPointerMode();

  EXPECT_FALSE(palette_tray_->GetVisible());
}

// A basic test to ensure the OnPressedCallback is triggered on tap.
TEST_F(PaletteTrayTest, PressingTrayButton) {
  ShowPaletteTray();

  GestureTapOn(palette_tray_);

  EXPECT_TRUE(palette_tray_->is_active());
}

// Verify taps on the palette tray button results in expected behaviour.
TEST_F(PaletteTrayTest, PaletteTrayWorkflow) {
  ShowPaletteTray();

  // Verify the palette tray button is not active, and the palette tray bubble
  // is not shown initially.
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->tray_bubble_wrapper());

  // Verify that by tapping the palette tray button, the button will become
  // active and the palette tray bubble will be open.
  GestureTapOn(palette_tray_);
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_TRUE(test_api_->tray_bubble_wrapper());

  // Verify that activating a mode tool will close the palette tray bubble, but
  // leave the palette tray button active.
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->tray_bubble_wrapper());

  // Verify that tapping the palette tray while a tool is active will deactivate
  // the tool, and the palette tray button will not be active.
  GestureTapOn(palette_tray_);
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));

  // Verify that activating a action tool will close the palette tray bubble and
  // the palette tray button is will not be active.
  GestureTapOn(palette_tray_);
  ASSERT_TRUE(test_api_->tray_bubble_wrapper());
  const auto capture_tool_id = PaletteToolId::ENTER_CAPTURE_MODE;
  test_api_->palette_tool_manager()->ActivateTool(capture_tool_id);
  EXPECT_FALSE(
      test_api_->palette_tool_manager()->IsToolActive(capture_tool_id));
  // Wait for the tray bubble widget to close.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_api_->tray_bubble_wrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

// Verify that the palette tray button and bubble are as expected when modes
// that can be deactivated without pressing the palette tray button (such as
// capture region) are deactivated.
TEST_F(PaletteTrayTest, ModeToolDeactivatedAutomatically) {
  // Open the palette tray with a tap.
  ShowPaletteTray();
  GestureTapOn(palette_tray_);
  ASSERT_TRUE(palette_tray_->is_active());
  ASSERT_TRUE(test_api_->tray_bubble_wrapper());

  // Activate and deactivate the laser pointer tool.
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::LASER_POINTER);
  ASSERT_TRUE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  test_api_->palette_tool_manager()->DeactivateTool(
      PaletteToolId::LASER_POINTER);

  // Verify the bubble is hidden and the button is inactive after deactivating
  // the capture region tool.
  EXPECT_FALSE(test_api_->tray_bubble_wrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

TEST_F(PaletteTrayTest, EnableStylusPref) {
  local_state()->SetBoolean(prefs::kHasSeenStylus, true);

  // kEnableStylusTools is false by default
  ASSERT_FALSE(
      active_user_pref_service()->GetBoolean(prefs::kEnableStylusTools));
  EXPECT_FALSE(palette_tray_->GetVisible());

  // Setting the pref again shows the palette tray.
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  EXPECT_TRUE(palette_tray_->GetVisible());

  // Resetting the pref hides the palette tray.
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, false);
  EXPECT_FALSE(palette_tray_->GetVisible());
}

// Verify that the kEnableStylusTools pref is switched to true automatically
// when a stylus is detected for the first time.
TEST_F(PaletteTrayTest, EnableStylusPrefSwitchedOnStylusEvent) {
  ASSERT_FALSE(
      active_user_pref_service()->GetBoolean(prefs::kEnableStylusTools));

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();

  EXPECT_TRUE(
      active_user_pref_service()->GetBoolean(prefs::kEnableStylusTools));
}

TEST_F(PaletteTrayTest, WelcomeBubbleVisibility) {
  ASSERT_FALSE(active_user_pref_service()->GetBoolean(
      prefs::kShownPaletteWelcomeBubble));
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  // Verify that the welcome bubble does not shown up after tapping the screen
  // with a finger.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressTouch();
  generator->ReleaseTouch();
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  // Verify that the welcome bubble shows up after tapping the screen with a
  // stylus for the first time.
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();
  EXPECT_TRUE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
}

// Base class for tests that need to simulate an internal stylus.
class PaletteTrayTestWithInternalStylus : public PaletteTrayTest {
 public:
  PaletteTrayTestWithInternalStylus() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kHasInternalStylus);
  }

  PaletteTrayTestWithInternalStylus(const PaletteTrayTestWithInternalStylus&) =
      delete;
  PaletteTrayTestWithInternalStylus& operator=(
      const PaletteTrayTestWithInternalStylus&) = delete;

  ~PaletteTrayTestWithInternalStylus() override = default;

  // PaletteTrayTest:
  void SetUp() override {
    PaletteTrayTest::SetUp();
    test_api_->SetDisplayHasStylus();
  }
};

// Verify the palette tray button exists and is visible if the device has an
// internal stylus.
TEST_F(PaletteTrayTestWithInternalStylus, Visible) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  ASSERT_TRUE(palette_tray_);
  EXPECT_TRUE(palette_tray_->GetVisible());
}

// Verify that when entering or exiting the lock screen, the behavior of the
// palette tray button is as expected.
TEST_F(PaletteTrayTestWithInternalStylus, PaletteTrayOnLockScreenBehavior) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  ASSERT_TRUE(palette_tray_->GetVisible());

  PaletteToolManager* manager = test_api_->palette_tool_manager();
  manager->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(manager->IsToolActive(PaletteToolId::LASER_POINTER));

  // Verify that when entering the lock screen, the palette tray button is
  // hidden, and the tool that was active is no longer active.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
  EXPECT_FALSE(palette_tray_->GetVisible());

  // Verify that when logging back in the tray is visible, but the tool that was
  // active before locking the screen is still inactive.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(palette_tray_->GetVisible());
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
}

// Verify a tool deactivates when the palette bubble is opened while the tool
// is active.
TEST_F(PaletteTrayTestWithInternalStylus, ToolDeactivatesWhenOpeningBubble) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  ASSERT_TRUE(palette_tray_->GetVisible());

  palette_tray_->ShowBubble();
  EXPECT_TRUE(test_api_->tray_bubble_wrapper());
  PaletteToolManager* manager = test_api_->palette_tool_manager();
  manager->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
  EXPECT_FALSE(test_api_->tray_bubble_wrapper());

  palette_tray_->ShowBubble();
  EXPECT_TRUE(test_api_->tray_bubble_wrapper());
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
}

// Verify the palette welcome bubble is shown the first time the stylus is
// removed.
TEST_F(PaletteTrayTestWithInternalStylus, WelcomeBubbleShownOnEject) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  active_user_pref_service()->SetBoolean(prefs::kLaunchPaletteOnEjectEvent,
                                         false);
  ASSERT_FALSE(active_user_pref_service()->GetBoolean(
      prefs::kShownPaletteWelcomeBubble));
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  EjectStylus();
  EXPECT_TRUE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
}

// Verify if the pref which tracks if the welcome bubble has been shown before
// is true, the welcome bubble is not shown when the stylus is removed.
// TODO(crbug.com/1423035): Disabled due to flakiness.
TEST_F(PaletteTrayTestWithInternalStylus,
       DISABLED_WelcomeBubbleNotShownIfShownBefore) {
  active_user_pref_service()->SetBoolean(prefs::kLaunchPaletteOnEjectEvent,
                                         false);
  active_user_pref_service()->SetBoolean(prefs::kShownPaletteWelcomeBubble,
                                         true);
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  EjectStylus();
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
}

// Verify that the bubble does not get shown if the auto open palette setting is
// true.
TEST_F(PaletteTrayTestWithInternalStylus,
       WelcomeBubbleNotShownIfAutoOpenPaletteTrue) {
  ASSERT_TRUE(active_user_pref_service()->GetBoolean(
      prefs::kLaunchPaletteOnEjectEvent));
  active_user_pref_service()->SetBoolean(prefs::kShownPaletteWelcomeBubble,
                                         false);
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  EjectStylus();
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
}

// Verify that the bubble does not get shown if a stylus event has been seen by
// the tray prior to the first stylus ejection.
TEST_F(PaletteTrayTestWithInternalStylus,
       WelcomeBubbleNotShownIfStylusTouchTray) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  ASSERT_FALSE(active_user_pref_service()->GetBoolean(
      prefs::kShownPaletteWelcomeBubble));
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();
  generator->set_current_screen_location(
      palette_tray_->GetBoundsInScreen().CenterPoint());
  generator->PressTouch();
  generator->ReleaseTouch();

  EXPECT_TRUE(active_user_pref_service()->GetBoolean(
      prefs::kShownPaletteWelcomeBubble));
  EjectStylus();
  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
}

// Verify that palette bubble is shown/hidden on stylus eject/insert iff the
// auto open palette setting is true.
TEST_F(PaletteTrayTestWithInternalStylus, PaletteBubbleShownOnEject) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  // kLaunchPaletteOnEjectEvent is true by default.
  ASSERT_TRUE(active_user_pref_service()->GetBoolean(
      prefs::kLaunchPaletteOnEjectEvent));

  // Removing the stylus shows the bubble.
  EjectStylus();
  EXPECT_TRUE(palette_tray_->GetBubbleView());

  // Inserting the stylus hides the bubble.
  InsertStylus();
  EXPECT_FALSE(palette_tray_->GetBubbleView());

  // Removing the stylus while kLaunchPaletteOnEjectEvent==false does nothing.
  active_user_pref_service()->SetBoolean(prefs::kLaunchPaletteOnEjectEvent,
                                         false);
  EjectStylus();
  EXPECT_FALSE(palette_tray_->GetBubbleView());
  InsertStylus();

  // Removing the stylus while kEnableStylusTools==false does nothing.
  active_user_pref_service()->SetBoolean(prefs::kLaunchPaletteOnEjectEvent,
                                         true);
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, false);
  EjectStylus();
  EXPECT_FALSE(palette_tray_->GetBubbleView());
  InsertStylus();

  // Set both prefs to true, removing should work again.
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  EjectStylus();
  EXPECT_TRUE(palette_tray_->GetBubbleView());

  // Inserting the stylus should disable a currently selected tool.
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  InsertStylus();
  EXPECT_FALSE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
}

// Base class for tests that need to simulate an internal stylus, and need to
// start without an active session.
class PaletteTrayNoSessionTestWithInternalStylus : public PaletteTrayTest {
 public:
  PaletteTrayNoSessionTestWithInternalStylus() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kHasInternalStylus);
    stylus_utils::SetHasStylusInputForTesting();
  }

  PaletteTrayNoSessionTestWithInternalStylus(
      const PaletteTrayNoSessionTestWithInternalStylus&) = delete;
  PaletteTrayNoSessionTestWithInternalStylus& operator=(
      const PaletteTrayNoSessionTestWithInternalStylus&) = delete;

  ~PaletteTrayNoSessionTestWithInternalStylus() override = default;

 protected:
  PrefService* active_user_pref_service() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }
};

// Verify that the palette tray is created on an external display, but it is not
// shown, and the palette tray bubble does not appear when the stylus is
// ejected.
TEST_F(PaletteTrayNoSessionTestWithInternalStylus,
       ExternalMonitorBubbleNotShownOnEject) {
  // Fakes a stylus event with state |state| on all palette trays.
  auto fake_stylus_event_on_all_trays = [](ui::StylusState state) {
    Shell::RootWindowControllerList controllers =
        Shell::GetAllRootWindowControllers();
    for (size_t i = 0; i < controllers.size(); ++i) {
      PaletteTray* tray = controllers[i]->GetStatusAreaWidget()->palette_tray();
      PaletteTrayTestApi api(tray);
      api.OnStylusStateChanged(state);
    }
  };

  // Add a external display, then sign in.
  UpdateDisplay("300x200,300x200");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  ASSERT_EQ(2u, controllers.size());
  SimulateUserLogin("test@test.com");

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kAshEnablePaletteOnAllDisplays);
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  PaletteTray* main_tray =
      controllers[0]->GetStatusAreaWidget()->palette_tray();
  PaletteTray* external_tray =
      controllers[1]->GetStatusAreaWidget()->palette_tray();

  test_api_->SetDisplayHasStylus();

  // The palette tray on the external monitor is not visible.
  EXPECT_TRUE(main_tray->GetVisible());
  EXPECT_FALSE(external_tray->GetVisible());

  // Removing the stylus shows the bubble only on the main palette tray.
  fake_stylus_event_on_all_trays(ui::StylusState::REMOVED);
  EXPECT_TRUE(main_tray->GetBubbleView());
  EXPECT_FALSE(external_tray->GetBubbleView());

  // Inserting the stylus hides the bubble on both palette trays.
  fake_stylus_event_on_all_trays(ui::StylusState::INSERTED);
  EXPECT_FALSE(main_tray->GetBubbleView());
  EXPECT_FALSE(external_tray->GetBubbleView());
}

class PaletteTrayTestWithOOBE : public PaletteTrayTest {
 public:
  PaletteTrayTestWithOOBE() = default;
  ~PaletteTrayTestWithOOBE() override = default;

  // PalatteTrayTest:
  void SetUp() override {
    set_start_session(false);
    PaletteTrayTest::SetUp();
  }
};

// Verify there are no crashes if the stylus is used during OOBE.
TEST_F(PaletteTrayTestWithOOBE, StylusEventsSafeDuringOOBE) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();
  generator->PressTouch();
  generator->ReleaseTouch();
}

// Base class for tests that need to simulate multiple pen
// capable displays.
class PaletteTrayTestMultiDisplay : public PaletteTrayTest {
 public:
  PaletteTrayTestMultiDisplay() = default;
  ~PaletteTrayTestMultiDisplay() override = default;
  PaletteTrayTestMultiDisplay(const PaletteTrayTestMultiDisplay&) = delete;
  PaletteTrayTestMultiDisplay& operator=(const PaletteTrayTestMultiDisplay&) =
      delete;

  // Fake a stylus ejection.
  void EjectStylus() {
    test_api_->OnStylusStateChanged(ui::StylusState::REMOVED);
    test_api_external_->OnStylusStateChanged(ui::StylusState::REMOVED);
  }

  // Fake a stylus insertion.
  void InsertStylus() {
    test_api_->OnStylusStateChanged(ui::StylusState::INSERTED);
    test_api_external_->OnStylusStateChanged(ui::StylusState::INSERTED);
  }

  // PaletteTrayTest:
  void SetUp() override {
    PaletteTrayTest::SetUp();

    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kAshEnablePaletteOnAllDisplays);

    // Add a external display, then sign in.
    UpdateDisplay("300x200,300x200");
    display::test::DisplayManagerTestApi(display_manager())
        .SetFirstDisplayAsInternalDisplay();
    Shell::RootWindowControllerList controllers =
        Shell::GetAllRootWindowControllers();
    ASSERT_EQ(2u, controllers.size());
    SimulateUserLogin("test@test.com");

    palette_tray_ = controllers[0]->GetStatusAreaWidget()->palette_tray();
    palette_tray_external_ =
        controllers[1]->GetStatusAreaWidget()->palette_tray();

    ASSERT_TRUE(palette_tray_);
    ASSERT_TRUE(palette_tray_external_);

    test_api_external_ =
        std::make_unique<PaletteTrayTestApi>(palette_tray_external_);
  }

 protected:
  raw_ptr<PaletteTray, DanglingUntriaged> palette_tray_external_ = nullptr;

  std::unique_ptr<PaletteTrayTestApi> test_api_external_;
};

// Verify the palette welcome bubble is shown only on the internal display
// the first time the stylus is removed.
TEST_F(PaletteTrayTestMultiDisplay, WelcomeBubbleShownOnEject) {
  test_api_->SetDisplayHasStylus();
  test_api_external_->SetDisplayHasStylus();

  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  active_user_pref_service()->SetBoolean(prefs::kLaunchPaletteOnEjectEvent,
                                         false);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kHasInternalStylus);
  ASSERT_FALSE(active_user_pref_service()->GetBoolean(
      prefs::kShownPaletteWelcomeBubble));

  EXPECT_FALSE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
  EXPECT_FALSE(test_api_external_->welcome_bubble()->GetBubbleViewForTesting());

  EjectStylus();
  EXPECT_TRUE(test_api_->welcome_bubble()->GetBubbleViewForTesting());
  EXPECT_FALSE(test_api_external_->welcome_bubble()->GetBubbleViewForTesting());
}

// Verify that palette bubble does not open on the external display
// on stylus eject/insert.
TEST_F(PaletteTrayTestMultiDisplay, PaletteBubbleShownOnEject) {
  test_api_->SetDisplayHasStylus();
  test_api_external_->SetDisplayHasStylus();

  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kHasInternalStylus);

  // kLaunchPaletteOnEjectEvent is true by default.
  ASSERT_TRUE(active_user_pref_service()->GetBoolean(
      prefs::kLaunchPaletteOnEjectEvent));

  // Removing the stylus shows the bubble on the internal display.
  EjectStylus();
  EXPECT_TRUE(palette_tray_->GetBubbleView());
  EXPECT_FALSE(palette_tray_external_->GetBubbleView());

  // Inserting the stylus hides the bubble.
  InsertStylus();
  EXPECT_FALSE(palette_tray_->GetBubbleView());
  EXPECT_FALSE(palette_tray_external_->GetBubbleView());

  // Inserting the stylus should disable a currently selected tool
  // even if it is on an external display.
  test_api_external_->palette_tool_manager()->ActivateTool(
      PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(test_api_external_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  InsertStylus();
  EXPECT_FALSE(test_api_external_->palette_tool_manager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
}

void addStylusToDisplay(int64_t display_id) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  int stylus_device_id = 10;

  base::RunLoop().RunUntilIdle();

  if (device_data_manager->GetTouchscreenDevices().size() == 0) {
    ui::TouchscreenDevice stylus_device = ui::TouchscreenDevice(
        stylus_device_id, ui::InputDeviceType::INPUT_DEVICE_USB,
        std::string("Stylus"), gfx::Size(1, 1), 1, true);

    std::vector<ui::TouchscreenDevice> devices;
    devices.push_back(stylus_device);

    static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
        ->OnTouchscreenDevicesUpdated(devices);
  }

  std::vector<ui::TouchDeviceTransform> device_transforms(1);
  device_transforms[0].display_id = display_id;
  device_transforms[0].device_id = stylus_device_id;
  device_data_manager->ConfigureTouchDevices(device_transforms);

  ASSERT_EQ(1U, device_data_manager->GetTouchscreenDevices().size());
  ASSERT_EQ(display_id,
            device_data_manager->GetTouchscreenDevices()[0].target_display_id);
  ASSERT_TRUE(device_data_manager->AreTouchscreenTargetDisplaysValid());
}

// Verify that palette state is refreshed when the display
// layout changes.
TEST_F(PaletteTrayTestMultiDisplay, MirrorModeEnable) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);

  // We should already by in extended mode with two displays
  ASSERT_EQ(2U, Shell::Get()->display_manager()->GetNumDisplays());

  const int64_t external_display_id =
      display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
          .GetSecondaryDisplay()
          .id();

  addStylusToDisplay(external_display_id);

  // The palette tray on the internal monitor is not visible.
  EXPECT_FALSE(palette_tray_->GetVisible());
  EXPECT_TRUE(palette_tray_external_->GetVisible());

  // Enable mirror mode
  Shell::Get()->display_manager()->SetMultiDisplayMode(
      display::DisplayManager::MIRRORING);
  Shell::Get()->display_manager()->UpdateDisplays();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, Shell::Get()->display_manager()->GetNumDisplays());

  addStylusToDisplay(external_display_id);

  // The external tray will have been deleted, so only check if
  // we're visible on the internal display now.
  EXPECT_TRUE(palette_tray_->GetVisible());
}

class PaletteTrayTestWithProjector : public PaletteTrayTest {
 public:
  PaletteTrayTestWithProjector() = default;

  PaletteTrayTestWithProjector(const PaletteTrayTestWithProjector&) = delete;
  PaletteTrayTestWithProjector& operator=(const PaletteTrayTestWithProjector&) =
      delete;

  ~PaletteTrayTestWithProjector() override = default;

  // AshTestBase:
  void SetUp() override {
    PaletteTrayTest::SetUp();
    projector_session_ = ProjectorControllerImpl::Get()->projector_session();
  }

 protected:
  raw_ptr<ProjectorSessionImpl, DanglingUntriaged> projector_session_;
};

// Verify that the palette tray is hidden during a Projector session.
TEST_F(PaletteTrayTestWithProjector,
       PaletteTrayNotVisibleDuringProjectorSession) {
  active_user_pref_service()->SetBoolean(prefs::kEnableStylusTools, true);
  local_state()->SetBoolean(prefs::kHasSeenStylus, true);
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::LASER_POINTER);

  EXPECT_TRUE(palette_tray_->GetVisible());
  EXPECT_EQ(
      test_api_->palette_tool_manager()->GetActiveTool(PaletteGroup::MODE),
      PaletteToolId::LASER_POINTER);

  // Verify palette tray is hidden and the active tool is deactivated during
  // Projector session.
  projector_session_->Start(
      base::SafeBaseName::Create("projector_data").value());
  EXPECT_FALSE(palette_tray_->GetVisible());
  EXPECT_EQ(
      test_api_->palette_tool_manager()->GetActiveTool(PaletteGroup::MODE),
      PaletteToolId::NONE);

  // Verify palette tray is visible when Projector session ends.
  projector_session_->Stop();
  EXPECT_TRUE(palette_tray_->GetVisible());
}

}  // namespace ash
