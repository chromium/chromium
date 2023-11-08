// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_app_count_icon.h"

#include "ash/style/ash_color_provider.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace ash {

namespace {

class NumberIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit NumberIconImageSource(size_t count, int size)
      : CanvasImageSource(AppIcon::GetRecommendedImageSize(size)),
        count_(count) {}

  NumberIconImageSource(const NumberIconImageSource&) = delete;
  NumberIconImageSource& operator=(const NumberIconImageSource&) = delete;

  void Draw(gfx::Canvas* canvas) override {
    float radius = size().width() / 2.0f;

    canvas->DrawStringRectWithFlags(
        base::FormatNumber(count_), GetNumberIconFontList(),
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorSecondary),
        gfx::Rect(size()),
        gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kXor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorSecondaryBackground));
    canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
  }

 private:
  size_t count_;
  const gfx::FontList& GetNumberIconFontList() {
    static gfx::FontList font_list({"Roboto"}, gfx::Font::NORMAL, 10,
                                   gfx::Font::Weight::NORMAL);
    return font_list;
  }
};

}  // namespace

AppCountIcon::AppCountIcon(const int count)
    : AppIcon(gfx::Image(
                  gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(
                      count,
                      AppIcon::kSizeSmall)),
              AppIcon::kSizeSmall) {}

BEGIN_METADATA(AppCountIcon)
END_METADATA

}  // namespace ash
