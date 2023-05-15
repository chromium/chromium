// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"

namespace gfx {
class Size;
class RoundedCornersF;
}  // namespace gfx

namespace ash {

class RoundedDisplayGutter;

class ASH_EXPORT RoundedDisplayGutterFactory {
 public:
  RoundedDisplayGutterFactory() = default;

  RoundedDisplayGutterFactory(const RoundedDisplayGutterFactory&) = delete;
  RoundedDisplayGutterFactory& operator=(const RoundedDisplayGutterFactory&) =
      delete;

  ~RoundedDisplayGutterFactory() = default;

  // Creates drawable overlay gutters. An overlay gutter is considered drawable
  // if it has at least one RoundedDisplayCorners with non-zero radius.
  std::vector<std::unique_ptr<RoundedDisplayGutter>> CreateOverlayGutters(
      const gfx::Size& panel_size,
      const gfx::RoundedCornersF& panel_radii,
      bool create_vertical_gutters);
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_FACTORY_H_
