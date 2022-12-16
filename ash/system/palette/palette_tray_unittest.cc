// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include <memory>
#include <string>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/assistant/assistant_state.h"
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
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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
  PaletteTrayTest() {
    feature_list_.InitAndDisableFeature(
        ash::features::kDeprecateAssistantStylusFeatures);
  }

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

  PaletteTray* palette_tray_ = nullptr;  // not owned

  std::unique_ptr<PaletteTrayTestApi> test_api_;

  base::test::ScopedFeatureList feature_list_;
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

// Base class for tests that rely on Assistant enabled.
class PaletteTrayTestWithAssistant : public PaletteTrayTest {
 public:
  PaletteTrayTestWithAssistant() = default;

  PaletteTrayTestWithAssistant(const PaletteTrayTestWithAssistant&) = delete;
  PaletteTrayTestWithAssistant& operator=(const PaletteTrayTestWithAssistant&) =
      delete;

  ~PaletteTrayTestWithAssistant() override = default;

  // PaletteTrayTest:
  void SetUp() override {
    assistant::util::OverrideIsGoogleDeviceForTesting(true);

    PaletteTrayTest::SetUp();

    // Instantiate EventGenerator now so that its constructor does not overwrite
    // the simulated clock that is being installed below.
    GetEventGenerator();

    // Tests fail if event time is ever 0.
    simulated_clock_.Advance(base::Milliseconds(10));
    ui::SetEventTickClockForTesting(&simulated_clock_);

    highlighter_test_api_ = std::make_unique<HighlighterControllerTestApi>(
        Shell::Get()->highlighter_controller());
  }

  void TearDown() override {
    assistant::util::OverrideIsGoogleDeviceForTesting(false);
    ui::SetEventTickClockForTesting(nullptr);
    // This needs to be called first to reset the controller state before the
    // shell instance gets torn down.
    highlighter_test_api_.reset();
    PaletteTrayTest::TearDown();
  }

 protected:
  bool metalayer_enabled() const {
    return test_api_->palette_tool_manager()->IsToolActive(
        PaletteToolId::METALAYER);
  }

  bool highlighter_showing() const {
    return highlighter_test_api_->IsShowingHighlighter();
  }

  AssistantState* assistant_state() const { return AssistantState::Get(); }

  void DragAndAssertMetalayer(const std::string& context,
                              const gfx::Point& origin,
                              int event_flags,
                              bool expected,
                              bool expected_on_press) {
    SCOPED_TRACE(context);

    ui::test::EventGenerator* generator = GetEventGenerator();
    gfx::Point pos = origin;
    generator->MoveTouch(pos);
    generator->set_flags(event_flags);
    generator->PressTouch();
    // If this gesture is supposed to enable the tool, it should have done it by
    // now.
    EXPECT_EQ(expected, metalayer_enabled());
    // Unlike the tool, the highlighter might become visible only after the
    // first move, hence a separate parameter to check against.
    EXPECT_EQ(expected_on_press, highlighter_showing());
    pos += gfx::Vector2d(1, 1);
    generator->MoveTouch(pos);
    // If this gesture is supposed to show the highlighter, it should have done
    // it by now.
    EXPECT_EQ(expected, highlighter_showing());
    EXPECT_EQ(expected, metalayer_enabled());
    generator->set_flags(ui::EF_NONE);
    pos += gfx::Vector2d(1, 1);
    generator->MoveTouch(pos);
    EXPECT_EQ(expected, highlighter_showing());
    EXPECT_EQ(expected, metalayer_enabled());
    generator->ReleaseTouch();
    // If the tool is not enabled, the gesture may open a context menu instead.
    // Press escape to close the menu.
    generator->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  }

  void WaitDragAndAssertMetalayer(const std::string& context,
                                  const gfx::Point& origin,
                                  int event_flags,
                                  bool expected,
                                  bool expected_on_press) {
    const int kStrokeGap = 1000;
    simulated_clock_.Advance(base::Milliseconds(kStrokeGap));
    DragAndAssertMetalayer(context, origin, event_flags, expected,
                           expected_on_press);
  }

  std::unique_ptr<HighlighterControllerTestApi> highlighter_test_api_;

 private:
  base::SimpleTestTickClock simulated_clock_;
  assistant::ScopedAssistantBrowserDelegate delegate_;
};

TEST_F(PaletteTrayTestWithAssistant, MetalayerToolViewCreated) {
  EXPECT_TRUE(
      test_api_->palette_tool_manager()->HasTool(PaletteToolId::METALAYER));
}

TEST_F(PaletteTrayTestWithAssistant, MetalayerToolActivatesHighlighter) {
  ui::ScopedAnimationDurationScaleMode animation_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_state()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(assistant::AssistantStatus::READY);
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, true);
  prefs()->SetBoolean(assistant::prefs::kAssistantContextEnabled, true);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();

  const gfx::Point origin(1, 1);
  const gfx::Vector2d step(1, 1);
  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin + step));
  EXPECT_FALSE(
      palette_utils::PaletteContainsPointInScreen(origin + step + step));

  // Press/drag does not activate the highlighter unless the palette tool is
  // activated.
  DragAndAssertMetalayer("tool disabled", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);

  // Activate the palette tool, still no highlighter.
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::METALAYER);
  EXPECT_FALSE(highlighter_showing());

  // Press/drag over a regular (non-palette) location. This should activate the
  // highlighter. Note that a diagonal stroke does not create a valid selection.
  highlighter_test_api_->ResetSelection();
  DragAndAssertMetalayer("tool enabled", origin, ui::EF_NONE,
                         true /* metalayer stays enabled after the press */,
                         true /* highlighter shown on press */);
  // When metalayer is entered normally (not via stylus button), a failed
  // selection should not exit the mode.
  EXPECT_FALSE(highlighter_test_api_->HandleSelectionCalled());
  EXPECT_TRUE(metalayer_enabled());

  // A successfull selection should exit the metalayer mode.
  SCOPED_TRACE("horizontal stroke");
  highlighter_test_api_->ResetSelection();
  generator->MoveTouch(gfx::Point(100, 100));
  generator->PressTouch();
  EXPECT_TRUE(metalayer_enabled());
  generator->MoveTouch(gfx::Point(300, 100));
  generator->ReleaseTouch();
  EXPECT_TRUE(highlighter_test_api_->HandleSelectionCalled());
  EXPECT_FALSE(metalayer_enabled());

  SCOPED_TRACE("drag over palette");
  highlighter_test_api_->DestroyPointerView();
  // Press/drag over the palette button. This should not activate the
  // highlighter, but should disable the palette tool instead.
  gfx::Point palette_point = palette_tray_->GetBoundsInScreen().CenterPoint();
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  generator->MoveTouch(palette_point);
  generator->PressTouch();
  EXPECT_FALSE(highlighter_showing());
  palette_point += gfx::Vector2d(1, 1);
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  generator->MoveTouch(palette_point);
  EXPECT_FALSE(highlighter_showing());
  generator->ReleaseTouch();
  EXPECT_FALSE(metalayer_enabled());

  // Disabling metalayer support in the delegate should disable the palette
  // tool.
  test_api_->palette_tool_manager()->ActivateTool(PaletteToolId::METALAYER);
  prefs()->SetBoolean(assistant::prefs::kAssistantContextEnabled, false);
  EXPECT_FALSE(metalayer_enabled());

  // With the metalayer disabled again, press/drag does not activate the
  // highlighter.
  DragAndAssertMetalayer("tool disabled again", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
}

TEST_F(PaletteTrayTestWithAssistant, StylusBarrelButtonActivatesHighlighter) {
  ui::ScopedAnimationDurationScaleMode animation_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_state()->NotifyStatusChanged(assistant::AssistantStatus::NOT_READY);
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, false);
  prefs()->SetBoolean(assistant::prefs::kAssistantContextEnabled, false);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->EnterPenPointerMode();

  const gfx::Point origin(1, 1);
  const gfx::Vector2d step(1, 1);

  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin));
  EXPECT_FALSE(palette_utils::PaletteContainsPointInScreen(origin + step));
  EXPECT_FALSE(
      palette_utils::PaletteContainsPointInScreen(origin + step + step));

  // Press and drag while holding down the stylus button, no highlighter unless
  // the metalayer support is fully enabled and the the framework is ready.
  WaitDragAndAssertMetalayer("nothing enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Enable one of the two user prefs, should not be sufficient.
  prefs()->SetBoolean(assistant::prefs::kAssistantContextEnabled, true);
  WaitDragAndAssertMetalayer("one pref enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Enable the other user pref, still not sufficient.
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, true);
  WaitDragAndAssertMetalayer("two prefs enabled", origin,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Once the service is ready, the button should start working.
  assistant_state()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  // Press and drag with no button, still no highlighter.
  WaitDragAndAssertMetalayer("all enabled, no button ", origin, ui::EF_NONE,
                             false /* no metalayer */,
                             false /* no highlighter on press */);

  // Press/drag with while holding down the stylus button, but over the palette
  // tray. This should activate neither the palette tool nor the highlighter.
  gfx::Point palette_point = palette_tray_->GetBoundsInScreen().CenterPoint();
  EXPECT_TRUE(palette_utils::PaletteContainsPointInScreen(palette_point));
  EXPECT_TRUE(
      palette_utils::PaletteContainsPointInScreen(palette_point + step));
  EXPECT_TRUE(
      palette_utils::PaletteContainsPointInScreen(palette_point + step + step));
  WaitDragAndAssertMetalayer("drag over palette", palette_point,
                             ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                             false /* no highlighter on press */);

  // Perform a regular stroke (no button), followed by a button-down stroke
  // without a pause. This should not trigger metalayer.
  DragAndAssertMetalayer("writing, no button", origin, ui::EF_NONE,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
  DragAndAssertMetalayer("writing, with button ", origin,
                         ui::EF_LEFT_MOUSE_BUTTON, false /* no metalayer */,
                         false /* no highlighter on press */);

  // Wait, then press/drag while holding down the stylus button over a regular
  // location. This should activate the palette tool and the highlighter.
  WaitDragAndAssertMetalayer("with button", origin, ui::EF_LEFT_MOUSE_BUTTON,
                             true /* enables metalayer */,
                             false /* no highlighter on press */);
  // Metalayer mode entered via the stylus button should not be sticky.
  EXPECT_FALSE(metalayer_enabled());

  // Repeat the previous step without a pause, make sure that the palette tool
  // is not toggled, and the highlighter is enabled immediately.
  DragAndAssertMetalayer("with button, again", origin, ui::EF_LEFT_MOUSE_BUTTON,
                         true /* enables metalayer */,
                         true /* highlighter shown on press */);

  // Same after a pause.
  WaitDragAndAssertMetalayer(
      "with button, after a pause", origin, ui::EF_LEFT_MOUSE_BUTTON,
      true /* enables metalayer */, true /* highlighter shown on press */);

  // The barrel button should not work on the lock screen.
  highlighter_test_api_->DestroyPointerView();
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::METALAYER));
  WaitDragAndAssertMetalayer("screen locked", origin, ui::EF_LEFT_MOUSE_BUTTON,
                             false /* no metalayer */,
                             false /* no highlighter on press */);

  // Unlock the screen, the barrel button should work again.
  GetSessionControllerClient()->UnlockScreen();
  WaitDragAndAssertMetalayer(
      "screen unlocked", origin, ui::EF_LEFT_MOUSE_BUTTON,
      true /* enables metalayer */, false /* no highlighter on press */);

  // Disable the metalayer support.
  // This should deactivate both the palette tool and the highlighter.
  prefs()->SetBoolean(assistant::prefs::kAssistantContextEnabled, false);
  EXPECT_FALSE(test_api_->palette_tool_manager()->IsToolActive(
      PaletteToolId::METALAYER));

  highlighter_test_api_->DestroyPointerView();
  EXPECT_FALSE(highlighter_showing());
  DragAndAssertMetalayer("disabled", origin, ui::EF_LEFT_MOUSE_BUTTON,
                         false /* no metalayer */,
                         false /* no highlighter on press */);
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
TEST_F(PaletteTrayTestWithInternalStylus, WelcomeBubbleNotShownIfShownBefore) {
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

  // Performs a tap on the palette tray button.
  void PerformTap(PaletteTray* tray) {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    tray->PerformAction(tap);
  }

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
  PaletteTray* palette_tray_external_ = nullptr;

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
  PaletteTrayTestWithProjector() {
    scoped_feature_list_.InitWithFeatures({features::kProjector}, {});
  }

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
  ProjectorSessionImpl* projector_session_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  projector_session_->Start("projector_data");
  EXPECT_FALSE(palette_tray_->GetVisible());
  EXPECT_EQ(
      test_api_->palette_tool_manager()->GetActiveTool(PaletteGroup::MODE),
      PaletteToolId::NONE);

  // Verify palette tray is visible when Projector session ends.
  projector_session_->Stop();
  EXPECT_TRUE(palette_tray_->GetVisible());
}

}  // namespace ash
