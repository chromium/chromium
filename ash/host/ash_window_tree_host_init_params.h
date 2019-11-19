// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_
#define ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class AshWindowTreeHostMirroringDelegate;

struct ASH_EXPORT AshWindowTreeHostInitParams {
  // Not owned.
  AshWindowTreeHostMirroringDelegate* mirroring_delegate = nullptr;
  // This corresponds to display::ManagedDisplayInfo::bounds_in_native.
  gfx::Rect initial_bounds;
  bool offscreen = false;
  bool mirroring_unified = false;
  int64_t display_id = 0;
  float device_scale_factor = 0.0f;
};

}  // namespace ash

#endif  // ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_
