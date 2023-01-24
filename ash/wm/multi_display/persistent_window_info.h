// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_
#define ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Describes the information that each window needs to carry for persistent
// window placement in multi-displays or screen rotation scenario.
struct ASH_EXPORT PersistentWindowInfo {
  // `is_landscape_before_rotation` indicates the screen orientation before
  // screen rotation happens. This is used to help restore window bounds in
  // screen rotation scenario.
  PersistentWindowInfo(aura::Window* window, bool is_landscape_before_rotation);
  PersistentWindowInfo(const PersistentWindowInfo& other);
  ~PersistentWindowInfo();

  // Persistent window bounds in screen coordinates.
  gfx::Rect window_bounds_in_screen;

  // Indicates the display to restore to in multi-displays scenario or the
  // display on which screen rotation happens.
  int64_t display_id;

  // Indicates last display bounds for |display_id| in screen coordinates.
  gfx::Rect display_bounds_in_screen;

  // True if it is in landscape orientation before screen orientation happens.
  // Note, this is only meaningful in the screen rotation scenario.
  bool is_landscape;

  // Stores the restore bounds in screen coordinates if they exist.
  absl::optional<gfx::Rect> restore_bounds_in_screen;
};

}  // namespace ash

#endif  // ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_INFO_H_
