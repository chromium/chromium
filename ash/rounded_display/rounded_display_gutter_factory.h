// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class RoundedDisplayGutter;

class ASH_EXPORT RoundedDisplayGutterFactory {
 public:
  RoundedDisplayGutterFactory() = default;

  RoundedDisplayGutterFactory(const RoundedDisplayGutterFactory&) = delete;
  RoundedDisplayGutterFactory& operator=(const RoundedDisplayGutterFactory&) =
      delete;

  ~RoundedDisplayGutterFactory() = default;

  std::vector<std::unique_ptr<RoundedDisplayGutter>> CreateOverlayGutters(
      const gfx::Size& display_panel_size,
      const gfx::RoundedCornersF& display_radii,
      bool create_vertical_gutters);

  std::vector<std::unique_ptr<RoundedDisplayGutter>> CreateNonOverlayGutters(
      const gfx::Size& display_panel_size,
      const gfx::RoundedCornersF& display_radii);
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_
