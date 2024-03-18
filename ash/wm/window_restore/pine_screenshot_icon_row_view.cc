// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "base/i18n/number_formatting.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Constants for the icon row inside the screenshot preview.
constexpr int kIconRowRadius = 12;
constexpr int kIconRowChildSpacing = 4;
constexpr gfx::Insets kIconRowInsets =
    gfx::Insets::TLBR(kIconRowRadius + 4, 4, 4, 4);
constexpr int kIconRowIconSize = 20;
constexpr int kIconRowHeight =
    kIconRowIconSize + kIconRowInsets.top() + kIconRowInsets.bottom();

}  // namespace

PineScreenshotIconRowView::PineScreenshotIconRowView(
    const PineContentsData::AppsInfos& apps_infos) {
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(kIconRowChildSpacing);
  SetInsideBorderInsets(kIconRowInsets);
  SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBaseOpaque));

  const int elements_size = static_cast<int>(apps_infos.size());
  const int child_num =
      std::min(elements_size, pine::kScreenshotIconRowMaxElements);
  const int row_width =
      child_num * kIconRowIconSize + (child_num - 1) * kIconRowChildSpacing +
      kIconRowInsets.left() + kIconRowInsets.right() + kIconRowRadius;
  SetPreferredSize(gfx::Size(row_width, kIconRowHeight));

  const bool exceed_max_elements =
      elements_size > pine::kScreenshotIconRowMaxElements;
  // If there are more than `kScreenshotIconRowMaxElements` number of windows,
  // show `kScreenshotIconRowMaxElements - 1` number of icons and save the last
  // spot in the row to count the remaining windows.
  const int num_icon = exceed_max_elements
                           ? pine::kScreenshotIconRowMaxElements - 1
                           : elements_size;

  for (int i = 0; i < num_icon; i++) {
    views::ImageView* image_view = AddChildView(
        views::Builder<views::ImageView>()
            .SetHorizontalAlignment(views::ImageView::Alignment::kCenter)
            .SetVerticalAlignment(views::ImageView::Alignment::kCenter)
            .SetPreferredSize(gfx::Size(kIconRowIconSize, kIconRowIconSize))
            .SetImageSize(gfx::Size(kIconRowIconSize, kIconRowIconSize))
            .Build());
    image_view_map_[i] = image_view;

    Shell::Get()->saved_desk_delegate()->GetIconForAppId(
        apps_infos[i].app_id, kIconRowIconSize,
        base::BindOnce(&PineScreenshotIconRowView::SetIconForIndex,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  if (exceed_max_elements) {
    auto* count_label = AddChildView(
        views::Builder<views::Label>()
            .SetText(u"+" + base::FormatNumber(elements_size - num_icon))
            .SetPreferredSize(gfx::Size(kIconRowIconSize, kIconRowIconSize))
            .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
            .SetBackground(views::CreateThemedRoundedRectBackground(
                cros_tokens::kCrosSysPrimaryContainer, kIconRowIconSize / 2.0))
            .Build());
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                          *count_label);
  }
}

PineScreenshotIconRowView::~PineScreenshotIconRowView() = default;

void PineScreenshotIconRowView::SetIconForIndex(int index,
                                                const gfx::ImageSkia& icon) {
  views::ImageView* image_view = image_view_map_[index];
  CHECK(image_view);
  image_view->SetImage(ui::ImageModel::FromImageSkia(icon));
}

void PineScreenshotIconRowView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  const gfx::Size preferred_size = GetPreferredSize();
  const int width = preferred_size.width();
  const int height = preferred_size.height();

  const auto top_left = SkPoint::Make(0.f, 0.f);
  const auto bottom_left = SkPoint::Make(0.f, kIconRowHeight);
  const auto bottom_right = SkPoint::Make(width, height);

  const int cutout_curve1_end_x = kIconRowRadius;
  const int cutout_curve1_end_y = kIconRowRadius;

  const int cutout_curve2_end_x = width - kIconRowRadius;
  const int cutout_curve2_end_y = 2 * kIconRowRadius;

  auto clip_path =
      SkPathBuilder()
          // Start from the top-left point.
          .moveTo(top_left)
          // Draw the first concave arc at the top-left and a horizontal line
          // connecting it to the top-right rounded corner.
          .arcTo(SkPoint::Make(0, cutout_curve1_end_y),
                 SkPoint::Make(cutout_curve1_end_x, cutout_curve1_end_y),
                 kIconRowRadius)
          // Draw the top-right rounded corner and a vertical line connecting
          // it to the bottom-right concave arc.
          .arcTo(SkPoint::Make(cutout_curve2_end_x, cutout_curve1_end_y),
                 SkPoint::Make(cutout_curve2_end_x, cutout_curve2_end_y),
                 kIconRowRadius)
          // Draw the bottom-right concave arc and a horizontal line
          // connecting it to the bottom-left rounded corner.
          .arcTo(SkPoint::Make(cutout_curve2_end_x, kIconRowHeight),
                 bottom_right, kIconRowRadius)
          // Draw the bottom-left rounded corner and the vertical line
          // connecting it to the top-left point.
          .arcTo(bottom_left, top_left, kIconRowRadius)
          .close()
          .detach();
  SetClipPath(clip_path);
}

BEGIN_METADATA(PineScreenshotIconRowView)
END_METADATA

}  // namespace ash
