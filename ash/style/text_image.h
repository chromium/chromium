// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TEXT_IMAGE_H_
#define ASH_STYLE_TEXT_IMAGE_H_

#include "ash/ash_export.h"
#include "base/third_party/icu/icu_utf.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace ash {

// Renders a unicode character as an image at a desired size.
class ASH_EXPORT TextImage : public gfx::CanvasImageSource {
 public:
  TextImage(const gfx::Size& size, base_icu::UChar32 symbol);
  ~TextImage() override;

  // Returns a `ui::ImageModel` for `symbol` of `size` in `color_id`.
  static ui::ImageModel AsImageModel(const gfx::Size& size,
                                     base_icu::UChar32 symbol,
                                     ui::ColorId color_id);

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  void set_color(SkColor color) { color_ = color; }

 private:
  const std::u16string symbol_;
  SkColor color_ = SK_ColorBLACK;
};

}  // namespace ash

#endif  // ASH_STYLE_TEXT_IMAGE_H_
