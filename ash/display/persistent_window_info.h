// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PERSISTENT_WINDOW_INFO_H_
#define ASH_DISPLAY_PERSISTENT_WINDOW_INFO_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Describes the information that each window needs to carry for persistent
// window placement in multi-displays scenario.
struct ASH_EXPORT PersistentWindowInfo {
  explicit PersistentWindowInfo(aura::Window* window);
  PersistentWindowInfo(const PersistentWindowInfo& other);
  ~PersistentWindowInfo();

  // Persistent window bounds in screen coordinates.
  gfx::Rect window_bounds_in_screen;

  // Indicates the display to restore to when available.
  int64_t display_id;

  // Indicates last display bounds for |display_id| in screen coordinates.
  gfx::Rect display_bounds_in_screen;

  // Stores the restore bounds in screen coordinates if they exist.
  absl::optional<gfx::Rect> restore_bounds_in_screen;
};

}  // namespace ash

#endif  // ASH_DISPLAY_PERSISTENT_WINDOW_INFO_H_
