// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_INIT_PARAMS_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_INIT_PARAMS_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class AshWindowTreeHostDelegate;

struct ASH_EXPORT AshWindowTreeHostInitParams {
  // Not owned.
  raw_ptr<AshWindowTreeHostDelegate> delegate = nullptr;
  // This corresponds to display::ManagedDisplayInfo::bounds_in_native.
  gfx::Rect initial_bounds;
  bool offscreen = false;
  bool mirroring_unified = false;
  int64_t display_id = 0;
  float device_scale_factor = 0.0f;
  // Compositor's memory limit in MB.
  size_t compositor_memory_limit_mb = 0;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_INIT_PARAMS_H_
