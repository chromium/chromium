// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_grouped_icon_image.h"

#include "cc/paint/paint_flags.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kBackgroundRadius = 20;
constexpr int kBackgroundSize = kBackgroundRadius * 2;
constexpr int kIconSize = 14;
constexpr int kIconCornerSpacing = 4;
// Radius for the extra number of tabs label.
constexpr int kExtraNumberLabelRadius = 8;
constexpr int kExtraNumberLabelSize = kExtraNumberLabelRadius * 2;
constexpr int kExtraNumberLabelSpacing = 1;

}  // namespace

CoralGroupedIconImage::CoralGroupedIconImage(
    const std::vector<gfx::ImageSkia>& icon_images,
    const int extra_tabs_number,
    const ui::ColorProvider* color_provider)
    : gfx::CanvasImageSource(gfx::Size(kBackgroundSize, kBackgroundSize)),
      icon_images_(icon_images),
      extra_tabs_number_(extra_tabs_number),
      color_provider_(color_provider) {}

CoralGroupedIconImage::~CoralGroupedIconImage() = default;

// static
ui::ImageModel CoralGroupedIconImage::DrawCoralGroupedIconImage(
    const std::vector<gfx::ImageSkia>& icons_images,
    int extra_tabs_number) {
  auto image_generator = base::BindRepeating(
      [](const std::vector<gfx::ImageSkia>& icon_images,
         const int extra_tabs_number,
         const ui::ColorProvider* color_provider) -> gfx::ImageSkia {
        return gfx::CanvasImageSource::MakeImageSkia<CoralGroupedIconImage>(
            icon_images, extra_tabs_number, color_provider);
      },
      /*icon_images=*/icons_images,
      /*extra_number=*/extra_tabs_number);

  return ui::ImageModel::FromImageGenerator(
      image_generator, gfx::Size(kBackgroundSize, kBackgroundSize));
}

void CoralGroupedIconImage::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // Draw the parent circular background.
  flags.setColor(color_provider_->GetColor(cros_tokens::kCrosSysSystemOnBase));
  canvas->DrawCircle(gfx::Point(kBackgroundRadius, kBackgroundRadius),
                     kBackgroundRadius, flags);

  const size_t icon_count = icon_images_.size();

  // TODO(owenzhang): Replace with correct icon count handling.
  if (icon_count == 1) {
  } else if (icon_count == 2) {
  } else if (icon_count == 3) {
  } else if (icon_count == 4) {
  } else if (icon_count > 4) {
    const int first_column_left = kIconCornerSpacing;
    const int second_column_left =
        kBackgroundSize - kIconSize - kIconCornerSpacing;
    const int first_row_top = kIconCornerSpacing;
    const int second_row_top = kBackgroundSize - kIconSize - kIconCornerSpacing;

    // Draw the top-left icon image.
    canvas->DrawImageInt(icon_images_[0], first_column_left, first_row_top);

    // Draw the top-right icon image.
    canvas->DrawImageInt(icon_images_[1], second_column_left, first_row_top);

    // Draw the bottom-left icon image.
    canvas->DrawImageInt(icon_images_[2], first_column_left, second_row_top);

    // Draw the bottom-right extra tab label circular background.
    flags.setColor(
        color_provider_->GetColor(cros_tokens::kCrosSysPrimaryContainer));
    int icon_four_midpoint =
        kBackgroundRadius + kExtraNumberLabelSpacing + kExtraNumberLabelRadius;
    canvas->DrawCircle(gfx::Point(icon_four_midpoint, icon_four_midpoint),
                       kExtraNumberLabelRadius, flags);

    // Draw the extra number of icons label.
    const auto string_bounds = gfx::Rect(
        icon_four_midpoint - kExtraNumberLabelRadius,
        icon_four_midpoint - kExtraNumberLabelRadius + /*label_y_offset=*/1,
        kExtraNumberLabelSize, kExtraNumberLabelSize);
    gfx::FontList font_list({"Google Sans"}, gfx::Font::NORMAL, 10,
                            gfx::Font::Weight::NORMAL);
    canvas->DrawStringRectWithFlags(
        base::NumberToString16(extra_tabs_number_), font_list,
        color_provider_->GetColor(cros_tokens::kCrosSysOnPrimaryContainer),
        string_bounds, gfx::Canvas::TEXT_ALIGN_CENTER);
  }
}

}  // namespace ash
