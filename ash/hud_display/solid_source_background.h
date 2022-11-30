// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_SOLID_SOURCE_BACKGROUND_H_
#define ASH_HUD_DISPLAY_SOLID_SOURCE_BACKGROUND_H_

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/views/background.h"

namespace gfx {
class Canvas;
}

namespace views {
class View;
}

namespace ash {
namespace hud_display {

// Basically views::SolidBackground with SkBlendMode::kSrc paint mode.
class SolidSourceBackground : public views::Background {
 public:
  // Background will have top rounded corners with |top_rounding_radius|.
  SolidSourceBackground(SkColor color, SkScalar top_rounding_radius);

  SolidSourceBackground(const SolidSourceBackground&) = delete;
  SolidSourceBackground& operator=(const SolidSourceBackground&) = delete;

  ~SolidSourceBackground() override = default;

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  SkScalar top_rounding_radius_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_SOLID_SOURCE_BACKGROUND_H_
