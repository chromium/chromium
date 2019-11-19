// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORK_AREA_INSETS_H_
#define ASH_WM_WORK_AREA_INSETS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ash {
class RootWindowController;

// Insets of the work area associated with this root window.
// Work area is the space left for user after removing areas occupied by
// persistent system elements like shelf, keyboard and accessibility widgets.
// Work area is assocciated with a single root window, because the configuration
// of persistent elements can be different for different screens.
// WorkAreaInsets class caches information about persistent system elements to
// avoid frequent recalculations. It gathers work area related computations and
// provides single interface to query work area details.
class ASH_EXPORT WorkAreaInsets : public KeyboardControllerObserver {
 public:
  // Returns work area parameters associated with the given |window|.
  static WorkAreaInsets* ForWindow(const aura::Window* window);

  explicit WorkAreaInsets(RootWindowController* root_window_controller);
  ~WorkAreaInsets() override;

  // Returns cached height of the accessibility panel in DIPs for this root
  // window.
  int accessibility_panel_height() const { return accessibility_panel_height_; }

  // Returns cached height of the docked magnifier in DIPs for this root
  // window.
  int docked_magnifier_height() const { return docked_magnifier_height_; }

  // Returns cached user work area bounds in screen coordinates DIPs for this
  // root window.
  const gfx::Rect& user_work_area_bounds() const {
    return user_work_area_bounds_;
  }

  // Returns cached user work area insets in DIPs for this root window.
  const gfx::Insets& user_work_area_insets() const {
    return user_work_area_insets_;
  }

  // Returns accessibility insets in DIPs.
  gfx::Insets GetAccessibilityInsets() const;

  // Returns bounds of the stable work area (work area when the shelf is
  // visible) in screen coordinates DIPs.
  gfx::Rect ComputeStableWorkArea() const;

  // Returns whether keyboard is shown for this root window.
  // TODO(agawronska): It would be nice to contain all keyboard related
  // functionality in this class and remove this method.
  bool IsKeyboardShown() const;

  // Sets height of the accessibility panel in DIPs for this root window.
  // Shell observers will be notified that accessibility insets changed.
  void SetDockedMagnifierHeight(int height);

  // Sets height of the docked magnifier in DIPs for this root window.
  // Shell observers will be notified that accessibility insets changed.
  void SetAccessibilityPanelHeight(int height);

  // Sets bounds (in window coordinates) and insets of the shelf for this root
  // window. |bounds| and |insets| are passed separately, because insets depend
  // on shelf visibility and can be different than calculated from bounds.
  void SetShelfBoundsAndInsets(const gfx::Rect& bounds,
                               const gfx::Insets& insets);

  // KeyboardControllerObserver:
  void OnKeyboardAppearanceChanged(
      const KeyboardStateDescriptor& state) override;
  void OnKeyboardVisibilityChanged(bool is_visible) override;

 private:
  // Updates cached values of work area bounds and insets.
  void UpdateWorkArea();

  // RootWindowController associated with this work area.
  RootWindowController* const root_window_controller_ = nullptr;

  // Cached bounds of user work area in screen coordinates DIPs.
  gfx::Rect user_work_area_bounds_;

  // Cached insets of user work area in DIPs.
  gfx::Insets user_work_area_insets_;

  // Cached occluded bounds of the keyboard in window coordinates. It needs to
  // be removed from the available work area. See
  // ui/keyboard/keyboard_controller_observer.h for details.
  gfx::Rect keyboard_occluded_bounds_;

  // Cached displaced bounds of the keyboard in window coordinates.
  // See ui/keyboard/keyboard_controller_observer.h for details.
  gfx::Rect keyboard_displaced_bounds_;

  // Cached bounds of the shelf in window coordinates in DIPs.
  gfx::Rect shelf_bounds_;

  // Cached insets of the shelf in DIPs.
  gfx::Insets shelf_insets_;

  // Cached height of the docked magnifier in DIPs at the top of the screen.
  // It needs to be removed from the available work area.
  int docked_magnifier_height_ = 0;

  // Cached height of the accessibility panel in DIPs at the top of the
  // screen. It needs to be removed from the available work area.
  int accessibility_panel_height_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WorkAreaInsets);
};

}  // namespace ash

#endif  // ASH_WM_WORK_AREA_INSETS_H_
