// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_
#define ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_

#include "ash/display/mouse_warp_controller.h"

#include <stdint.h>

#include <map>
#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Point;
}

namespace ash {

// A MouseWarpController used in unified display mode.
class ASH_EXPORT UnifiedMouseWarpController : public MouseWarpController {
 public:
  UnifiedMouseWarpController();

  UnifiedMouseWarpController(const UnifiedMouseWarpController&) = delete;
  UnifiedMouseWarpController& operator=(const UnifiedMouseWarpController&) =
      delete;

  ~UnifiedMouseWarpController() override;

  // MouseWarpController:
  bool WarpMouseCursor(ui::MouseEvent* event) override;
  void SetEnabled(bool enabled) override;

 private:
  friend class AshTestBase;
  friend class DisplayManagerTestApi;
  friend class UnifiedMouseWarpControllerTest;

  void ComputeBounds();

  // Warps the mouse cursor to an alternate root window when the
  // mouse location in |event|, hits the edge of the event target's root and
  // the mouse cursor is considered to be in an alternate display.
  // If |update_mouse_location_now| is true, the mouse location is updated
  // synchronously.
  // Returns true if the cursor was moved.
  bool WarpMouseCursorInNativeCoords(int64_t source_display,
                                     const gfx::Point& point_in_native,
                                     const gfx::Point& point_in_screen,
                                     bool update_mouse_location_now);

  void update_location_for_test() { update_location_for_test_ = true; }

  struct DisplayEdge {
    DisplayEdge(int64_t source_id,
                int64_t target_id,
                const gfx::Rect& edge_bounds)
        : source_display_id(source_id),
          target_display_id(target_id),
          edge_native_bounds_in_source_display(edge_bounds) {}

    // The ID of the display where the cursor is now.
    int64_t source_display_id;

    // The ID of the display with which there's an edge and the cursor would
    // move to if it resides in that edge.
    int64_t target_display_id;

    // The native bounds of the edge between the source and target displays
    // which is part of the source display.
    gfx::Rect edge_native_bounds_in_source_display;
  };

  // Maps a display by its ID to all the boundary edges that reside in it with
  // the surrounding displays.
  std::map<int64_t, std::vector<DisplayEdge>> displays_edges_map_;

  bool update_location_for_test_;

  // True if the edge boundaries between displays (where mouse cursor should
  // warp) have been computed.
  bool display_boundaries_computed_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_
