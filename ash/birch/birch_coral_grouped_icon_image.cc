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
// Used when the icon is close to the edge.
constexpr int kIconPaddingSmall = 4;
// Used when the icon is in the center.
constexpr int kIconPaddingMedium = 13;
// Used when the icon is far from the edge. This constant is a reflection of the
// distance between the left/top bound and the icon farthest away from the
// left/top bound.
constexpr int kIconPaddingLarge =
    kBackgroundSize - kIconSize - kIconPaddingSmall;
constexpr int kExtraNumberLabelRadius = 8;
constexpr int kExtraNumberLabelSize = kExtraNumberLabelRadius * 2;
constexpr int kExtraNumberLabelSpacing = 1;

}  // namespace

CoralGroupedIconImage::CoralGroupedIconImage(
    const std::vector<gfx::ImageSkia>& icon_images,
    int extra_number,
    const ui::ColorProvider* color_provider)
    : gfx::CanvasImageSource(gfx::Size(kBackgroundSize, kBackgroundSize)),
      icon_images_(icon_images),
      extra_number_(extra_number),
      color_provider_(color_provider) {}

CoralGroupedIconImage::~CoralGroupedIconImage() = default;

// static
ui::ImageModel CoralGroupedIconImage::DrawCoralGroupedIconImage(
    const std::vector<gfx::ImageSkia>& icons_images,
    int extra_number) {
  auto image_generator = base::BindRepeating(
      [](const std::vector<gfx::ImageSkia>& icon_images, const int extra_number,
         const ui::ColorProvider* color_provider) -> gfx::ImageSkia {
        return gfx::CanvasImageSource::MakeImageSkia<CoralGroupedIconImage>(
            icon_images, extra_number, color_provider);
      },
      icons_images, extra_number);

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

  if (icon_count == 1) {
    // Draw the only center icon image.
    canvas->DrawImageInt(icon_images_[0], kIconPaddingMedium,
                         kIconPaddingMedium);
    return;
  }

  if (icon_count == 2) {
    // Draw the center-left icon image.
    canvas->DrawImageInt(icon_images_[0], kIconPaddingSmall,
                         kIconPaddingMedium);
    // Draw the center-right icon image.
    canvas->DrawImageInt(icon_images_[1], kIconPaddingLarge,
                         kIconPaddingMedium);
    return;
  }

  if (icon_count == 3 && !extra_number_) {
    // Draw the top-left icon image.
    canvas->DrawImageInt(icon_images_[0], kIconPaddingSmall, kIconPaddingSmall);
    // Draw the top-right icon image.
    canvas->DrawImageInt(icon_images_[1], kIconPaddingLarge, kIconPaddingSmall);
    // Draw the bottom-center icon image.
    canvas->DrawImageInt(icon_images_[2], kIconPaddingMedium,
                         kIconPaddingLarge);
    return;
  }

  CHECK_GE(icon_count, 3u);
  // Draw the top-left icon image.
  canvas->DrawImageInt(icon_images_[0], kIconPaddingSmall, kIconPaddingSmall);
  // Draw the top-right icon image.
  canvas->DrawImageInt(icon_images_[1], kIconPaddingLarge, kIconPaddingSmall);
  // Draw the bottom-left icon image.
  canvas->DrawImageInt(icon_images_[2], kIconPaddingSmall, kIconPaddingLarge);

  if (icon_count == 4) {
    // Only draw the bottom-right icon image.
    canvas->DrawImageInt(icon_images_[3], kIconPaddingLarge, kIconPaddingLarge);
    return;
  }

  // Draw the bottom-right extra number label circular background.
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
      base::NumberToString16(extra_number_), font_list,
      color_provider_->GetColor(cros_tokens::kCrosSysOnPrimaryContainer),
      string_bounds, gfx::Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace ash
