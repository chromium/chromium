// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/security_curtain_controller_impl.h"

#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/widget/widget.h"

namespace aura {
// This improves the error output for our tests that compare OcclusionState.
std::ostream& operator<<(std::ostream& os, Window::OcclusionState state) {
  os << Window::OcclusionStateToString(state);
  return os;
}
}  // namespace aura

namespace ash::curtain {
namespace {

using ::testing::Eq;
using ::testing::Ne;

using DisplayId = uint64_t;

// Simple event handler that can track mouse presses.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;
  ~TestEventHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() != ui::ET_MOUSE_PRESSED)
      return;

    has_seen_mouse_press_ = true;
    event->SetHandled();
    event->StopPropagation();
  }
  bool HasSeenMousePress() { return has_seen_mouse_press_; }

 private:
  bool has_seen_mouse_press_ = false;
};

class SecurityCurtainControllerImplTest : public AshTestBase {
 public:
  SecurityCurtainControllerImplTest() = default;
  SecurityCurtainControllerImplTest(const SecurityCurtainControllerImplTest&) =
      delete;
  SecurityCurtainControllerImplTest& operator=(
      const SecurityCurtainControllerImplTest&) = delete;
  ~SecurityCurtainControllerImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    security_curtain_controller_.emplace(ash::Shell::Get());
  }

  void TearDown() override {
    // SecurityCurtainController cannot outlive Ash::Shell();
    security_curtain_controller_.reset();
    AshTestBase::TearDown();
  }

  SecurityCurtainController& security_curtain_controller() {
    return security_curtain_controller_.value();
  }

  bool IsCurtainShownOnDisplay(const display::Display& display) {
    return IsCurtainShownOnDisplay(display.id());
  }

  bool IsCurtainShownOnDisplay(DisplayId display_id) {
    auto* root_window_controller =
        Shell::GetRootWindowControllerWithDisplayId(display_id);
    if (!root_window_controller) {
      return false;
    }

    const auto* controller =
        root_window_controller->security_curtain_widget_controller();

    return controller != nullptr;
  }

  views::Widget& GetCurtainForDisplay(const display::Display& display) {
    auto* controller = Shell::GetRootWindowControllerWithDisplayId(display.id())
                           ->security_curtain_widget_controller();
    EXPECT_THAT(controller, testing::NotNull())
        << "Missing curtain widget for display " << display.ToString();

    return controller->GetWidget();
  }

  display::Displays GetDisplays() {
    return display_manager()->active_display_list();
  }

  display::Display GetFirstDisplay() {
    DCHECK(!GetDisplays().empty());
    return GetDisplays().front();
  }

  display::Display CreateSingleDisplay() {
    UpdateDisplay("1111x111");
    return GetFirstDisplay();
  }
  void CreateMultipleDisplays() { UpdateDisplay("1111x111,2222x222,3333x333"); }

  void ResizeDisplay(const display::Display& display,
                     gfx::Size new_resolution) {
    // display::DisplayManagerTestApi offers a `SetDisplayResolution()` method,
    // but that does not seem to work (it does not relay the new resolution to
    // root window associated with the display).
    // So instead of using that method, we simply call `UpdateDisplay`.
    CHECK_EQ(GetDisplays().size(), 1u)
        << "This method only support single display setups!";

    UpdateDisplay(new_resolution.ToString());

    // Sanity check to ensure UpdateDisplay() didn't remote the existing display
    CHECK_EQ(GetFirstDisplay().id(), display.id());
  }

  DisplayId RemoveAllButFirstDisplay() {
    CHECK_GT(GetDisplays().size(), 1u)
        << "This method only works in multi display setups!";

    // UpdateDisplay() will reuse the existing display ids, so by calling it
    // with only a single display, all but the first display will be deleted.
    DisplayId last_display_id = GetDisplays().back().id();
    UpdateDisplay("1111x111");

    CHECK(!display_manager()->IsActiveDisplayId(last_display_id));
    return last_display_id;
  }

  void SimulateMouseClickOn(const aura::Window& window) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(window.GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  bool IsOutputMuted() {
    return CrasAudioHandler::Get()->IsOutputMutedBySecurityCurtain() &&
           CrasAudioHandler::Get()->IsOutputMuted();
  }

 private:
  absl::optional<SecurityCurtainControllerImpl> security_curtain_controller_;
};

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledBeforeEnableIsCalled) {
  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       NoCurtainsShouldBeCreatedBeforeEnableIsCalled) {
  CreateMultipleDisplays();

  for (auto display : GetDisplays()) {
    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false))
        << "Curtain should not be shown on display " << display.ToString();
  }
}

TEST_F(SecurityCurtainControllerImplTest, ShouldBeEnabledWhenCallingEnable) {
  security_curtain_controller().Enable();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledWhenCallingDisable) {
  security_curtain_controller().Enable();
  security_curtain_controller().Disable();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeCreatedWhenCallingEnable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable();

  for (auto display : GetDisplays()) {
    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true))
        << "Curtain should be shown on display " << display.ToString();
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeDestroyedWhenCallingDisable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable();
  security_curtain_controller().Disable();

  for (auto display : GetDisplays()) {
    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false))
        << "Curtain should no longer be shown on display "
        << display.ToString();
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainsShouldCoverTheEntireDisplay) {
  CreateMultipleDisplays();
  security_curtain_controller().Enable();

  for (auto display : GetDisplays()) {
    const views::Widget& curtain = GetCurtainForDisplay(display);
    EXPECT_THAT(curtain.IsVisible(), Eq(true));
    EXPECT_THAT(curtain.GetWindowBoundsInScreen(), Eq(display.bounds()));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldKeepCoveringTheEntireDisplayAfterResizing) {
  display::Display display = CreateSingleDisplay();

  security_curtain_controller().Enable();

  const views::Widget& curtain = GetCurtainForDisplay(display);

  for (gfx::Size new_resolution :
       {gfx::Size(1000, 500), gfx::Size(2000, 1000)}) {
    ResizeDisplay(display, new_resolution);
    EXPECT_THAT(curtain.GetWindowBoundsInScreen().size(), Eq(new_resolution));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       UncurtainedContainerShouldKeepCoveringTheEntireDisplayAfterResizing) {
  // This test ensures the uncurtained container also has the correct size.
  // That's very important for Chrome Remote Desktop as it streams this
  // container, and if it has the wrong size the stream will fail.
  display::Display display = CreateSingleDisplay();

  security_curtain_controller().Enable();

  const aura::Window* curtained_off_container = Shell::GetContainer(
      Shell::GetRootWindowForDisplayId(GetFirstDisplay().id()),
      kShellWindowId_ScreenAnimationContainer);

  EXPECT_THAT(curtained_off_container->bounds().size(),
              Eq(display.bounds().size()));

  for (gfx::Size new_resolution :
       {gfx::Size(1000, 500), gfx::Size(2000, 1000)}) {
    ResizeDisplay(display, new_resolution);
    EXPECT_THAT(curtained_off_container->bounds().size(), Eq(new_resolution));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotConsumeMouseEvents) {
  security_curtain_controller().Enable();

  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  TestEventHandler other_window_event_handler;
  other_window->SetTargetHandler(&other_window_event_handler);

  SimulateMouseClickOn(*other_window);

  EXPECT_THAT(other_window_event_handler.HasSeenMousePress(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotOccludeOtherWindows) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->TrackOcclusionState();
  ASSERT_THAT(other_window->GetOcclusionState(),
              Eq(aura::Window::OcclusionState::VISIBLE));

  security_curtain_controller().Enable();

  EXPECT_THAT(other_window->GetOcclusionState(),
              Ne(aura::Window::OcclusionState::OCCLUDED));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotStealFocus) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->Focus();
  ASSERT_THAT(other_window->HasFocus(), Eq(true));

  security_curtain_controller().Enable();

  EXPECT_THAT(other_window->HasFocus(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest, ShouldAddCurtainToNewDisplays) {
  CreateSingleDisplay();

  security_curtain_controller().Enable();

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true))
        << "Curtain should be shown on display " << display.ToString();
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldRemoveCurtainFromRemovedDisplays) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable();
  DisplayId removed_display_id = RemoveAllButFirstDisplay();

  EXPECT_THAT(IsCurtainShownOnDisplay(removed_display_id), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotAddCurtainToNewDisplaysAfterCallingDisable) {
  CreateSingleDisplay();

  security_curtain_controller().Enable();
  security_curtain_controller().Disable();

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false))
        << "No curtain should be shown on display " << display.ToString();
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldToggleAudioHandlerWhenEnabledAndDisabled) {
  CreateSingleDisplay();

  security_curtain_controller().Enable();
  EXPECT_TRUE(IsOutputMuted());

  security_curtain_controller().Disable();
  EXPECT_FALSE(IsOutputMuted());
}

}  // namespace
}  // namespace ash::curtain
