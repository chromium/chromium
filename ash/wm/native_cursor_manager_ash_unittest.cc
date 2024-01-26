// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/native_cursor_manager_ash.h"

#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

using NativeCursorManagerAshTest = AshTestBase;

TEST_F(NativeCursorManagerAshTest, LockCursor) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  CursorManagerTestApi test_api;

  cursor_manager->SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager->GetCursor().type());
  UpdateDisplay("800x700*2/r");
  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
  EXPECT_TRUE(cursor_manager->GetCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Cursor type does not change while cursor is locked.
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, cursor_manager->GetCursorSize());
  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager->GetCursor().type());

  // Device scale factor and rotation do change even while cursor is locked.
  UpdateDisplay("800x700/u");
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_180, test_api.GetCurrentCursorRotation());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            cursor_manager->GetCursor().type());
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_TRUE(cursor_manager->GetCursor().platform());
}

TEST_F(NativeCursorManagerAshTest, SetCursor) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->GetCursor().platform());
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->GetCursor().platform());
}

TEST_F(NativeCursorManagerAshTest, SetCursorSize) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, cursor_manager->GetCursorSize());

  cursor_manager->SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager->GetCursorSize());
}

TEST_F(NativeCursorManagerAshTest, SetDeviceScaleFactorAndRotation) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  CursorManagerTestApi test_api;
  UpdateDisplay("800x100*2");
  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("800x100/l");
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_270, test_api.GetCurrentCursorRotation());
}

TEST_F(NativeCursorManagerAshTest, RotationWithPanelOrientation) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  // The panel is portrait but its orientation is landscape.
  const gfx::Rect bounds = gfx::Rect(0, 0, 1920, 1080);
  display::ManagedDisplayInfo native_display_info =
      display::CreateDisplayInfo(display_id, bounds);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  native_display_info.set_panel_orientation(
      display::PanelOrientation::kRightUp);
  std::vector<display::ManagedDisplayInfo> display_info_list{
      native_display_info};
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  CursorManagerTestApi test_api;
  ASSERT_EQ(gfx::Size(1080, 1920),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

TEST_F(NativeCursorManagerAshTest, FractionalScale) {
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  // Cursor should use the resource scale factor.
  UpdateDisplay("800x100*1.25");
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
}

}  // namespace ash
