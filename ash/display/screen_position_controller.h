// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_
#define ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/screen_position_client.h"

namespace ash {

class ASH_EXPORT ScreenPositionController
    : public aura::client::ScreenPositionClient {
 public:
  // Finds the root window at |location| in |window|'s coordinates
  // from given |root_windows| and returns the found root window and
  // location in that root window's coordinates. The function usually
  // returns |window->GetRootWindow()|, but if the mouse pointer is
  // moved outside the |window|'s root while the mouse is captured, it
  // returns the other root window.
  static void ConvertHostPointToRelativeToRootWindow(
      aura::Window* root_window,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          root_windows,
      gfx::Point* point_in_host,
      aura::Window** target_window);

  ScreenPositionController() {}

  ScreenPositionController(const ScreenPositionController&) = delete;
  ScreenPositionController& operator=(const ScreenPositionController&) = delete;

  ~ScreenPositionController() override {}

  // aura::client::ScreenPositionClient overrides:
  void ConvertPointToScreen(const aura::Window* window,
                            gfx::PointF* point) override;
  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::PointF* point) override;
  void ConvertHostPointToScreen(aura::Window* window,
                                gfx::Point* point) override;
  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override;

 protected:
  // aura::client::ScreenPositionClient:
  gfx::Point GetRootWindowOriginInScreen(
      const aura::Window* root_window) override;
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_
