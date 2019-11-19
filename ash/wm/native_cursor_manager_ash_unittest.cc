// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/native_cursor_manager_ash.h"

#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

// A delegate for recording a mouse event location.
class MouseEventLocationDelegate : public aura::test::TestWindowDelegate {
 public:
  MouseEventLocationDelegate() = default;
  ~MouseEventLocationDelegate() override = default;

  gfx::Point GetMouseEventLocationAndReset() {
    gfx::Point p = mouse_event_location_;
    mouse_event_location_.SetPoint(-100, -100);
    return p;
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_event_location_ = event->location();
    event->SetHandled();
  }

 private:
  gfx::Point mouse_event_location_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventLocationDelegate);
};

}  // namespace

using NativeCursorManagerAshTest = AshTestBase;

TEST_F(NativeCursorManagerAshTest, LockCursor) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::CursorType::kCopy);
  EXPECT_EQ(ui::CursorType::kCopy, test_api.GetCurrentCursor().native_type());
  UpdateDisplay("800x800*2/r");
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Cursor type does not change while cursor is locked.
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, test_api.GetCurrentCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::CursorType::kPointer);
  EXPECT_EQ(ui::CursorType::kCopy, test_api.GetCurrentCursor().native_type());

  // Device scale factor and rotation do change even while cursor is locked.
  UpdateDisplay("800x800/u");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_180, test_api.GetCurrentCursorRotation());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::CursorType::kPointer,
            test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(NativeCursorManagerAshTest, SetCursor) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  cursor_manager->SetCursor(ui::CursorType::kCopy);
  EXPECT_EQ(ui::CursorType::kCopy, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
  cursor_manager->SetCursor(ui::CursorType::kPointer);
  EXPECT_EQ(ui::CursorType::kPointer,
            test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(NativeCursorManagerAshTest, SetCursorSize) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, test_api.GetCurrentCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, test_api.GetCurrentCursorSize());
}

TEST_F(NativeCursorManagerAshTest, SetDeviceScaleFactorAndRotation) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  UpdateDisplay("800x100*2");
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("800x100/l");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_270, test_api.GetCurrentCursorRotation());
}

TEST_F(NativeCursorManagerAshTest, RotationWithPanelOrientation) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  // The panel is portrait but its orientation is landscape.
  display::ManagedDisplayInfo native_display_info =
      display::CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1920, 1080));
  native_display_info.set_panel_orientation(
      display::PanelOrientation::kRightUp);
  std::vector<display::ManagedDisplayInfo> display_info_list{
      native_display_info};
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  ASSERT_EQ(gfx::Size(1080, 1920),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

TEST_F(NativeCursorManagerAshTest, FractionalScale) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  // Cursor should use the resource scale factor.
  UpdateDisplay("800x100*1.25");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
}

}  // namespace ash
