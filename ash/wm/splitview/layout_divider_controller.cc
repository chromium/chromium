// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/screen_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/gfx/range/range.h"

namespace ash {

gfx::Range LayoutDividerController::GetDividerPositionAllowedRange() const {
  aura::Window::Windows windows = GetLayoutWindows();
  if (windows.size() == 0u) {
    return gfx::Range();
  }

  aura::Window* root_window = windows.at(0)->GetRootWindow();

  aura::Window* primary_window;
  aura::Window* secondary_window;
  for (auto window : windows) {
    if (WindowState::Get(window)->GetStateType() ==
        chromeos::WindowStateType::kPrimarySnapped) {
      primary_window = window;
    } else if (WindowState::Get(window)->GetStateType() ==
               chromeos::WindowStateType::kSecondarySnapped) {
      secondary_window = window;
    }
  }

  const bool is_horizontal = IsLayoutHorizontal(root_window);
  const int primary_window_minimum_length =
      GetMinimumWindowLength(primary_window, is_horizontal);
  const int secondary_window_minimum_length =
      GetMinimumWindowLength(secondary_window, is_horizontal);
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  return gfx::Range(primary_window_minimum_length,
                    is_horizontal ? (work_area_bounds_in_screen.width() -
                                     secondary_window_minimum_length -
                                     kSplitviewDividerShortSideLength)
                                  : (work_area_bounds_in_screen.height() -
                                     secondary_window_minimum_length -
                                     kSplitviewDividerShortSideLength));
}

}  // namespace ash
