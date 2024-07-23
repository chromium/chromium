// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/constants/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_hud_renderer.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TouchHudTestBase : public AshTestBase {
 public:
  TouchHudTestBase() = default;

  TouchHudTestBase(const TouchHudTestBase&) = delete;
  TouchHudTestBase& operator=(const TouchHudTestBase&) = delete;

  ~TouchHudTestBase() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Initialize display infos. They should be initialized after Ash
    // environment is set up, i.e., after AshTestBase::SetUp().
    internal_display_id_ =
        display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
            .SetFirstDisplayAsInternalDisplay();
    external_display_id_ = 10;
    mirrored_display_id_ = 11;

    internal_display_info_ =
        CreateDisplayInfo(internal_display_id_, gfx::Rect(0, 0, 600, 500));
    external_display_info_ =
        CreateDisplayInfo(external_display_id_, gfx::Rect(1, 1, 200, 100));
    mirrored_display_info_ =
        CreateDisplayInfo(mirrored_display_id_, gfx::Rect(0, 0, 200, 100));
  }

  display::Display GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay();
  }

  display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                                const gfx::Rect& bounds) {
    display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
    // Each display should have at least one native mode.
    display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                     /*is_interlaced=*/true,
                                     /*native=*/true);
    info.SetManagedDisplayModes({mode});
    return info;
  }

  void SetupSingleDisplay() {
    display_info_list_.clear();
    display_info_list_.push_back(internal_display_info_);
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void SetupDualDisplays() {
    display_info_list_.clear();
    display_info_list_.push_back(internal_display_info_);
    display_info_list_.push_back(external_display_info_);
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void SetInternalAsPrimary() {
    GetWindowTreeHostManager()->SetPrimaryDisplayId(internal_display_id_);
  }

  void SetExternalAsPrimary() {
    GetWindowTreeHostManager()->SetPrimaryDisplayId(external_display_id_);
  }

  void MirrorDisplays() {
    DCHECK_EQ(2U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    DCHECK_EQ(external_display_id_, display_info_list_[1].id());
    display_info_list_[1] = mirrored_display_info_;
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void UnmirrorDisplays() {
    DCHECK_EQ(2U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    DCHECK_EQ(mirrored_display_id_, display_info_list_[1].id());
    display_info_list_[1] = external_display_info_;
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void RemoveInternalDisplay() {
    DCHECK_LT(0U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    display_info_list_.erase(display_info_list_.begin());
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void RemoveExternalDisplay() {
    DCHECK_EQ(2U, display_info_list_.size());
    display_info_list_.pop_back();
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void AddInternalDisplay() {
    DCHECK_EQ(0U, display_info_list_.size());
    display_info_list_.push_back(internal_display_info_);
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void AddExternalDisplay() {
    DCHECK_EQ(1U, display_info_list_.size());
    display_info_list_.push_back(external_display_info_);
    display_manager()->OnNativeDisplaysChanged(display_info_list_);
  }

  int64_t internal_display_id() const { return internal_display_id_; }

  int64_t external_display_id() const { return external_display_id_; }

 protected:
  WindowTreeHostManager* GetWindowTreeHostManager() {
    return Shell::Get()->window_tree_host_manager();
  }

  const display::Display& GetInternalDisplay() {
    return display_manager()->GetDisplayForId(internal_display_id_);
  }

  const display::Display& GetExternalDisplay() {
    return display_manager()->GetDisplayForId(external_display_id_);
  }

  aura::Window* GetInternalRootWindow() {
    return Shell::GetRootWindowForDisplayId(internal_display_id_);
  }

  aura::Window* GetExternalRootWindow() {
    return Shell::GetRootWindowForDisplayId(external_display_id_);
  }

  aura::Window* GetPrimaryRootWindow() {
    const display::Display& display = GetPrimaryDisplay();
    return Shell::GetRootWindowForDisplayId(display.id());
  }

  aura::Window* GetSecondaryRootWindow() {
    const display::Display& display =
        display::test::DisplayManagerTestApi(display_manager())
            .GetSecondaryDisplay();
    return Shell::GetRootWindowForDisplayId(display.id());
  }

  RootWindowController* GetInternalRootController() {
    aura::Window* root = GetInternalRootWindow();
    return RootWindowController::ForWindow(root);
  }

  RootWindowController* GetExternalRootController() {
    aura::Window* root = GetExternalRootWindow();
    return RootWindowController::ForWindow(root);
  }

  RootWindowController* GetPrimaryRootController() {
    aura::Window* root = GetPrimaryRootWindow();
    return RootWindowController::ForWindow(root);
  }

  RootWindowController* GetSecondaryRootController() {
    aura::Window* root = GetSecondaryRootWindow();
    return RootWindowController::ForWindow(root);
  }

  aura::Window* GetRootWindowForTouchHud(TouchObserverHud* hud) {
    return hud->root_window_;
  }

  views::Widget* GetWidgetForTouchHud(TouchObserverHud* hud) {
    return hud->widget_;
  }

  int64_t internal_display_id_;
  int64_t external_display_id_;
  int64_t mirrored_display_id_;
  display::ManagedDisplayInfo internal_display_info_;
  display::ManagedDisplayInfo external_display_info_;
  display::ManagedDisplayInfo mirrored_display_info_;

  std::vector<display::ManagedDisplayInfo> display_info_list_;
};

class TouchHudDebugTest : public TouchHudTestBase {
 public:
  TouchHudDebugTest() = default;

  TouchHudDebugTest(const TouchHudDebugTest&) = delete;
  TouchHudDebugTest& operator=(const TouchHudDebugTest&) = delete;

  ~TouchHudDebugTest() override = default;

  void SetUp() override {
    // Add ash-touch-hud flag to enable debug touch HUD. This flag should be set
    // before Ash environment is set up, i.e., before TouchHudTestBase::SetUp().
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshTouchHud);

    TouchHudTestBase::SetUp();
  }

  void CheckInternalDisplay() {
    EXPECT_NE(nullptr, GetInternalTouchHudDebug());
    EXPECT_EQ(internal_display_id(), GetInternalTouchHudDebug()->display_id());
    EXPECT_EQ(GetInternalRootWindow(),
              GetRootWindowForTouchHud(GetInternalTouchHudDebug()));
    EXPECT_EQ(GetInternalRootWindow(),
              GetWidgetForTouchHud(GetInternalTouchHudDebug())
                  ->GetNativeView()
                  ->GetRootWindow());
    EXPECT_EQ(GetInternalDisplay().size(),
              GetWidgetForTouchHud(GetInternalTouchHudDebug())
                  ->GetWindowBoundsInScreen()
                  .size());
  }

  void CheckExternalDisplay() {
    EXPECT_NE(nullptr, GetExternalTouchHudDebug());
    EXPECT_EQ(external_display_id(), GetExternalTouchHudDebug()->display_id());
    EXPECT_EQ(GetExternalRootWindow(),
              GetRootWindowForTouchHud(GetExternalTouchHudDebug()));
    EXPECT_EQ(GetExternalRootWindow(),
              GetWidgetForTouchHud(GetExternalTouchHudDebug())
                  ->GetNativeView()
                  ->GetRootWindow());
    EXPECT_EQ(GetExternalDisplay().size(),
              GetWidgetForTouchHud(GetExternalTouchHudDebug())
                  ->GetWindowBoundsInScreen()
                  .size());
  }

 private:
  TouchHudDebug* GetInternalTouchHudDebug() {
    return GetInternalRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetExternalTouchHudDebug() {
    return GetExternalRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetPrimaryTouchHudDebug() {
    return GetPrimaryRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetSecondaryTouchHudDebug() {
    return GetSecondaryRootController()->touch_hud_debug();
  }
};

class TouchHudProjectionTest : public TouchHudTestBase {
 public:
  TouchHudProjectionTest() = default;

  TouchHudProjectionTest(const TouchHudProjectionTest&) = delete;
  TouchHudProjectionTest& operator=(const TouchHudProjectionTest&) = delete;

  ~TouchHudProjectionTest() override = default;

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kShowTaps);
    TouchHudTestBase::SetUp();
  }

  TouchHudProjection* GetInternalTouchHudProjection() {
    return GetInternalRootController()->touch_hud_projection();
  }

  int GetInternalTouchPointsCount() {
    return GetInternalTouchHudProjection()->touch_hud_renderer_->points_.size();
  }

  void SendTouchEventToInternalHud(ui::EventType type,
                                   const gfx::Point& location,
                                   int touch_id) {
    ui::TouchEvent event(
        type, location, event_time,
        ui::PointerDetails(ui::EventPointerType::kTouch, touch_id));
    GetInternalTouchHudProjection()->OnTouchEvent(&event);

    // Advance time for next event.
    event_time += base::Milliseconds(100);
  }

 private:
  base::TimeTicks event_time;
};

// Checks if debug touch HUD is correctly initialized for a single display.
TEST_F(TouchHudDebugTest, SingleDisplay) {
  // Setup a single display setting.
  SetupSingleDisplay();

  // Check if touch HUD is set correctly and associated with appropriate
  // display.
  CheckInternalDisplay();
}

// Checks if debug touch HUDs are correctly initialized for two displays.
TEST_F(TouchHudDebugTest, DualDisplays) {
  // Setup a dual display setting.
  SetupDualDisplays();

  // Check if touch HUDs are set correctly and associated with appropriate
  // displays.
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when primary display is
// changed.
TEST_F(TouchHudDebugTest, SwapPrimaryDisplay) {
  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  // Check if displays' touch HUDs are not swapped as root windows are.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(),
            display_manager_test.GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();

  // Set the primary display back to the internal one.
  SetInternalAsPrimary();

  // Check if displays' touch HUDs are not swapped back as root windows are.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(),
            display_manager_test.GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when displays are mirrored.
TEST_F(TouchHudDebugTest, MirrorDisplays) {
  // Disable restoring mirror mode to prevent interference from previous
  // display configuration.
  display_manager()->set_disable_restoring_mirror_mode_for_test(true);

  // Setup a dual display setting.
  SetupDualDisplays();

  // Mirror displays.
  MirrorDisplays();

  // Check if the internal display is intact.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Unmirror displays.
  UnmirrorDisplays();

  // Check if external display is added back correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(),
            display::test::DisplayManagerTestApi(display_manager())
                .GetSecondaryDisplay()
                .id());
  CheckInternalDisplay();
  CheckExternalDisplay();

  display_manager()->set_disable_restoring_mirror_mode_for_test(false);
}

// Checks if debug touch HUDs are correctly handled when displays are mirrored
// after setting the external display as the primary one.
TEST_F(TouchHudDebugTest, SwapPrimaryThenMirrorDisplays) {
  display_manager()->set_disable_restoring_mirror_mode_for_test(true);

  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  // Mirror displays.
  MirrorDisplays();

  // Check if the internal display is set as the primary one.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Unmirror displays.
  UnmirrorDisplays();

  // Check if the external display is added back as the primary display and
  // touch HUDs are set correctly.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(),
            display::test::DisplayManagerTestApi(display_manager())
                .GetSecondaryDisplay()
                .id());
  CheckInternalDisplay();
  CheckExternalDisplay();

  display_manager()->set_disable_restoring_mirror_mode_for_test(false);
}

// Checks if debug touch HUDs are correctly handled when the external display,
// which is the secondary one, is removed.
TEST_F(TouchHudDebugTest, RemoveSecondaryDisplay) {
  // Setup a dual display setting.
  SetupDualDisplays();

  // Remove external display which is the secondary one.
  RemoveExternalDisplay();

  // Check if the internal display is intact.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Add external display back.
  AddExternalDisplay();

  // Check if displays' touch HUDs are set correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(),
            display::test::DisplayManagerTestApi(display_manager())
                .GetSecondaryDisplay()
                .id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when the external display,
// which is set as the primary display, is removed.
TEST_F(TouchHudDebugTest, RemovePrimaryDisplay) {
  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  // Remove the external display which is the primary display.
  RemoveExternalDisplay();

  // Check if the internal display is set as the primary one.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Add the external display back.
  AddExternalDisplay();

  // Check if the external display is set as primary and touch HUDs are set
  // correctly.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(),
            display::test::DisplayManagerTestApi(display_manager())
                .GetSecondaryDisplay()
                .id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when all displays are
// removed.
TEST_F(TouchHudDebugTest, Headless) {
  // Setup a single display setting.
  SetupSingleDisplay();

  // Remove the only display which is the internal one.
  RemoveInternalDisplay();

  // Add the internal display back.
  AddInternalDisplay();

  // Check if the display's touch HUD is set correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();
}

// Checks projection touch HUD with a sequence of touch-pressed, touch-moved,
// and touch-released events.
// Test if the WM sets correct work area under different density.
TEST_F(TouchHudProjectionTest, TouchMoveRelease) {
  SetupSingleDisplay();
  EXPECT_NE(nullptr, GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchPressed, gfx::Point(10, 10),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchMoved, gfx::Point(10, 20),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchReleased, gfx::Point(10, 20),
                              1);
  EXPECT_EQ(0, GetInternalTouchPointsCount());
}

// Checks projection touch HUD with a sequence of touch-pressed, touch-moved,
// and touch-cancelled events.
TEST_F(TouchHudProjectionTest, TouchMoveCancel) {
  SetupSingleDisplay();
  EXPECT_NE(nullptr, GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchPressed, gfx::Point(10, 10),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchMoved, gfx::Point(10, 20),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchCancelled,
                              gfx::Point(10, 20), 1);
  EXPECT_EQ(0, GetInternalTouchPointsCount());
}

// Checks projection touch HUD with two simultaneous touches.
TEST_F(TouchHudProjectionTest, DoubleTouch) {
  SetupSingleDisplay();
  EXPECT_NE(nullptr, GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchPressed, gfx::Point(10, 10),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchPressed, gfx::Point(20, 10),
                              2);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchMoved, gfx::Point(10, 20),
                              1);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchMoved, gfx::Point(20, 20),
                              2);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchReleased, gfx::Point(10, 20),
                              1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::EventType::kTouchReleased, gfx::Point(20, 20),
                              2);
  EXPECT_EQ(0, GetInternalTouchPointsCount());
}

}  // namespace ash
