// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_
#define ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_

#include <stdint.h>

#include <optional>

#include "ash/ash_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Describes the information that each window needs to carry for persistent
// window placement in multi-displays or screen rotation scenario.
class ASH_EXPORT PersistentWindowInfo {
 public:
  // `is_landscape_before_rotation` indicates the screen orientation before
  // screen rotation happens. This is used to help restore window bounds in
  // screen rotation scenario. The `given_restore_bounds_in_parent` will be
  // ignored if it is empty.
  PersistentWindowInfo(aura::Window* window,
                       bool is_landscape_before_rotation,
                       const gfx::Rect& restore_bounds_in_parent);
  PersistentWindowInfo(const PersistentWindowInfo&) = delete;
  PersistentWindowInfo& operator=(const PersistentWindowInfo&) = delete;
  ~PersistentWindowInfo();

  gfx::Rect window_bounds_in_screen() const { return window_bounds_in_screen_; }
  int64_t display_id() const { return display_id_; }
  gfx::Vector2d display_offset_from_origin_in_screen() const {
    return display_offset_from_origin_in_screen_;
  }
  gfx::Size display_size_in_pixel() const { return display_size_in_pixel_; }
  bool is_landscape() const { return is_landscape_; }
  gfx::Rect restore_bounds_in_parent() const {
    return restore_bounds_in_parent_.value_or(gfx::Rect());
  }

  int64_t display_id_after_removal() const { return display_id_after_removal_; }
  void set_display_id_after_removal(int64_t id) {
    display_id_after_removal_ = id;
  }

 private:
  // Persistent window bounds in screen coordinates.
  gfx::Rect window_bounds_in_screen_;

  // Indicates the display to restore to in multi-displays scenario or the
  // display on which screen rotation happens.
  int64_t display_id_;

  // Indicates the window's display id after display removal happens. This can
  // be used to compare with `display_id` to see whether the window has been
  // moved to another display. As we can not tell this from whether window has
  // been re-parented to a different root window on display removal.
  int64_t display_id_after_removal_ = display::kInvalidDisplayId;

  // Indicates last display's origin in the screen coordinate.
  gfx::Vector2d display_offset_from_origin_in_screen_;

  // Indicates last display's size in pixel.
  gfx::Size display_size_in_pixel_;

  // True if it is in landscape orientation before screen orientation happens.
  // Note, this is only meaningful in the screen rotation scenario.
  bool is_landscape_;

  // Stores the restore bounds in its parent coordinates if they exist.
  std::optional<gfx::Rect> restore_bounds_in_parent_;
};

}  // namespace ash

#endif  // ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_
