// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/security_curtain_controller.h"

#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace aura {
// This improves the error output for our tests that compare `OcclusionState`.
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

// Simple event handler that can track any events.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;
  ~TestEventHandler() override = default;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    has_seen_event_ = true;

    event->SetHandled();
    event->StopPropagation();
  }

  bool HasSeenAnyEvent() { return has_seen_event_; }

 private:
  bool has_seen_event_ = false;
};

// Helper class that allows observing of all `ui::Event`'s on a given target
// window, and which allows easy generation of input events.
class EventTester {
 public:
  EventTester(std::unique_ptr<aura::Window> window,
              ui::test::EventGenerator& event_generator)
      : window_(std::move(window)), event_generator_(event_generator) {
    window_->SetTargetHandler(&event_handler_);
    event_generator_->SetTargetWindow(window_.get());
  }

  EventTester(const EventTester&) = delete;
  EventTester& operator=(const EventTester&) = delete;
  ~EventTester() = default;

  ui::test::EventGenerator& event_generator() { return *event_generator_; }
  TestEventHandler& event_handler() { return event_handler_; }

  gfx::Point location() const { return window_->bounds().CenterPoint(); }

 private:
  std::unique_ptr<aura::Window> window_;
  TestEventHandler event_handler_;
  raw_ref<ui::test::EventGenerator> event_generator_;
};

class SecurityCurtainControllerImplTest : public AshTestBase {
 public:
  SecurityCurtainControllerImplTest() = default;
  SecurityCurtainControllerImplTest(const SecurityCurtainControllerImplTest&) =
      delete;
  SecurityCurtainControllerImplTest& operator=(
      const SecurityCurtainControllerImplTest&) = delete;
  ~SecurityCurtainControllerImplTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

  void TearDown() override { AshTestBase::TearDown(); }

  SecurityCurtainController& security_curtain_controller() {
    return ash::Shell::Get()->security_curtain_controller();
  }

  SecurityCurtainController::InitParams init_params() {
    return SecurityCurtainController::InitParams();
  }

  bool IsNativeCursorEnabled() {
    return !Shell::Get()
                ->window_tree_host_manager()
                ->cursor_window_controller()
                ->is_cursor_compositing_enabled();
  }

  SecurityCurtainController::InitParams WithEventFilter(EventFilter filter) {
    return SecurityCurtainController::InitParams{filter};
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

  EventTester CreateEventTester() {
    return EventTester(CreateTestWindow({0, 0, 1000, 5000}),
                       *GetEventGenerator());
  }

  bool IsOutputMuted() {
    return CrasAudioHandler::Get()->IsOutputMutedBySecurityCurtain() &&
           CrasAudioHandler::Get()->IsOutputMuted();
  }

  EventTester CreateEventTesterOnDisplay(const display::Display& display) {
    return EventTester(CreateTestWindow(display.bounds()),
                       *GetEventGenerator());
  }
};

EventFilter only_mouse_events_filter() {
  return base::BindRepeating([](const ui::Event& event) {
    return event.IsMouseEvent() ? FilterResult::kKeepEvent
                                : FilterResult::kSuppressEvent;
  });
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledBeforeEnableIsCalled) {
  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       NoCurtainsShouldBeCreatedBeforeEnableIsCalled) {
  CreateMultipleDisplays();

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest, ShouldBeEnabledWhenCallingEnable) {
  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledWhenCallingDisable) {
  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeCreatedWhenCallingEnable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeDestroyedWhenCallingDisable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainsShouldCoverTheEntireDisplay) {
  CreateMultipleDisplays();
  security_curtain_controller().Enable(init_params());

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());
    const views::Widget& curtain = GetCurtainForDisplay(display);
    EXPECT_THAT(curtain.IsVisible(), Eq(true));
    EXPECT_THAT(curtain.GetWindowBoundsInScreen(), Eq(display.bounds()));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldKeepCoveringTheEntireDisplayAfterResizing) {
  display::Display display = CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());

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

  security_curtain_controller().Enable(init_params());

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

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldBlockMouseEvents) {
  security_curtain_controller().Enable(init_params());
  EventTester tester = CreateEventTester();

  tester.event_generator().ClickLeftButton();

  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldBlockTouchEvents) {
  security_curtain_controller().Enable(init_params());
  EventTester tester = CreateEventTester();

  tester.event_generator().PressTouch();

  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldBlockGestureEvents) {
  security_curtain_controller().Enable(init_params());
  EventTester tester = CreateEventTester();

  tester.event_generator().GestureTapAt(tester.location());

  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldBlockKeyboardEvents) {
  security_curtain_controller().Enable(init_params());
  EventTester tester = CreateEventTester();

  tester.event_generator().PressAndReleaseKey(ui::KeyboardCode::VKEY_B);

  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldRespectEventFilter) {
  security_curtain_controller().Enable(
      WithEventFilter(only_mouse_events_filter()));
  EventTester tester = CreateEventTester();

  // With our 'only mouse' filter touch events should be suppressed.
  tester.event_generator().PressTouch();
  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));

  // ... and mouse events should go through
  tester.event_generator().ClickLeftButton();
  EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldRespectEventFilterOnAllDisplays) {
  CreateMultipleDisplays();
  security_curtain_controller().Enable(
      WithEventFilter(only_mouse_events_filter()));

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EventTester tester = CreateEventTesterOnDisplay(display);

    // With our 'only mouse' filter touch events should be suppressed.
    tester.event_generator().PressTouch();
    EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));

    // ... and mouse events should go through
    tester.event_generator().ClickLeftButton();
    EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldRespectEventFilterOnNewlyAddedDisplays) {
  CreateSingleDisplay();
  security_curtain_controller().Enable(
      WithEventFilter(only_mouse_events_filter()));

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EventTester tester = CreateEventTesterOnDisplay(display);

    // With our 'only mouse' filter touch events should be suppressed.
    tester.event_generator().PressTouch();
    EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(false));

    // ... and mouse events should go through
    tester.event_generator().ClickLeftButton();
    EXPECT_THAT(tester.event_handler().HasSeenAnyEvent(), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotOccludeOtherWindows) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->TrackOcclusionState();
  ASSERT_THAT(other_window->GetOcclusionState(),
              Eq(aura::Window::OcclusionState::VISIBLE));

  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(other_window->GetOcclusionState(),
              Ne(aura::Window::OcclusionState::OCCLUDED));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotStealFocus) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->Focus();
  ASSERT_THAT(other_window->HasFocus(), Eq(true));

  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(other_window->HasFocus(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest, ShouldAddCurtainToNewDisplays) {
  CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldRemoveCurtainFromRemovedDisplays) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());
  DisplayId removed_display_id = RemoveAllButFirstDisplay();

  EXPECT_THAT(IsCurtainShownOnDisplay(removed_display_id), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotAddCurtainToNewDisplaysAfterCallingDisable) {
  CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldToggleAudioHandlerWhenEnabledAndDisabled) {
  CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());
  EXPECT_TRUE(IsOutputMuted());

  security_curtain_controller().Disable();
  EXPECT_FALSE(IsOutputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldDisableNativeMouseCursorWhenEnableIsCalled) {
  ASSERT_THAT(IsNativeCursorEnabled(), Eq(true));

  security_curtain_controller().Enable(init_params());

  ASSERT_THAT(IsNativeCursorEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldReenableNativeMouseCursorWhenDisableIsCalled) {
  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  ASSERT_THAT(IsNativeCursorEnabled(), Eq(true));
}

}  // namespace
}  // namespace ash::curtain
