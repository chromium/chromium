// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/pine_app_image_view.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_item_view.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/i18n/number_formatting.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

constexpr gfx::Insets kIconRowInsets =
    gfx::Insets::TLBR(pine::kPreviewContainerRadius + 4, 4, 4, 4);
constexpr int kIconRowHeight = pine::kScreenshotIconRowIconSize +
                               kIconRowInsets.top() + kIconRowInsets.bottom();

// Returns the preferred size of the icon row. `child_number` indicates the
// number of children while `one_browser_window` indicates it is only one
// browser window opened, which means the favicons of the tabs will be shown
// instead.
gfx::Size GetPreferredSizeOfTheRow(int child_number, bool one_browser_window) {
  int width = child_number * pine::kScreenshotIconRowIconSize +
              kIconRowInsets.left() + kIconRowInsets.right() +
              pine::kPreviewContainerRadius;
  if (one_browser_window) {
    width += 2 * pine::kScreenshotIconRowChildSpacing +
             (child_number - 2) * pine::kScreenshotFaviconSpacing +
             views::Separator::kThickness;
  } else {
    width += (child_number - 1) * pine::kScreenshotIconRowChildSpacing;
  }
  return gfx::Size(width, kIconRowHeight);
}

}  // namespace

PineScreenshotIconRowView::PineScreenshotIconRowView(
    const PineContentsData::AppsInfos& apps_infos) {
  SetID(pine::kScreenshotIconRowViewID);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(pine::kScreenshotIconRowChildSpacing);
  SetInsideBorderInsets(kIconRowInsets);
  SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBaseOpaque));

  const int elements_size = static_cast<int>(apps_infos.size());
  const bool one_browser_window =
      elements_size == 1 && IsBrowserAppId(apps_infos[0].app_id);

  // If there is only one browser window, show the browser icon and its tabs
  // favicons inside the icon row view.
  if (one_browser_window) {
    AddChildView(std::make_unique<PineItemView>(apps_infos[0],
                                                /*inside_screenshot=*/true));
  } else {
    const bool exceed_max_elements =
        elements_size > pine::kScreenshotIconRowMaxElements;
    // If there are more than `kScreenshotIconRowMaxElements` number of windows,
    // show `kScreenshotIconRowMaxElements - 1` number of icons and save the
    // last spot in the row to count the remaining windows.
    const int num_icon = exceed_max_elements
                             ? pine::kScreenshotIconRowMaxElements - 1
                             : elements_size;

    for (int i = 0; i < num_icon; i++) {
      auto image_view = std::make_unique<PineAppImageView>(
          apps_infos[i].app_id, PineAppImageView::Type::kScreenshot,
          base::DoNothing());
      image_view->SetID(pine::kScreenshotImageViewID);
      AddChildView(std::move(image_view));
    }
    if (exceed_max_elements) {
      auto* count_label = AddChildView(
          views::Builder<views::Label>()
              .SetText(u"+" + base::FormatNumber(elements_size - num_icon))
              .SetPreferredSize(pine::kScreenshotIconRowImageViewSize)
              .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  cros_tokens::kCrosSysPrimaryContainer,
                  pine::kScreenshotIconRowIconSize / 2.0))
              .Build());
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                            *count_label);
    }
  }
  int child_number =
      std::min(pine::kScreenshotIconRowMaxElements,
               one_browser_window ? static_cast<int>(apps_infos[0].tab_count)
                                  : elements_size);
  // Add the browser icon when there is only one browser window opened.
  if (one_browser_window) {
    child_number++;
  }
  SetPreferredSize(GetPreferredSizeOfTheRow(child_number, one_browser_window));
}

PineScreenshotIconRowView::~PineScreenshotIconRowView() = default;

void PineScreenshotIconRowView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  const gfx::Size preferred_size = GetPreferredSize();
  const int width = preferred_size.width();
  const int height = preferred_size.height();

  const auto top_left = SkPoint::Make(0.f, 0.f);
  const auto bottom_left = SkPoint::Make(0.f, kIconRowHeight);
  const auto bottom_right = SkPoint::Make(width, height);

  const int cutout_curve1_end_x = pine::kPreviewContainerRadius;
  const int cutout_curve1_end_y = pine::kPreviewContainerRadius;

  const int cutout_curve2_end_x = width - pine::kPreviewContainerRadius;
  const int cutout_curve2_end_y = 2 * pine::kPreviewContainerRadius;

  auto clip_path =
      SkPath()
          // Start from the top-left point.
          .moveTo(top_left)
          // Draw the first concave arc at the top-left and a horizontal line
          // connecting it to the top-right rounded corner.
          .arcTo(SkPoint::Make(0, cutout_curve1_end_y),
                 SkPoint::Make(cutout_curve1_end_x, cutout_curve1_end_y),
                 pine::kPreviewContainerRadius)
          // Draw the top-right rounded corner and a vertical line connecting
          // it to the bottom-right concave arc.
          .arcTo(SkPoint::Make(cutout_curve2_end_x, cutout_curve1_end_y),
                 SkPoint::Make(cutout_curve2_end_x, cutout_curve2_end_y),
                 pine::kPreviewContainerRadius)
          // Draw the bottom-right concave arc and a horizontal line
          // connecting it to the bottom-left rounded corner.
          .arcTo(SkPoint::Make(cutout_curve2_end_x, kIconRowHeight),
                 bottom_right, pine::kPreviewContainerRadius)
          // Draw the bottom-left rounded corner and the vertical line
          // connecting it to the top-left point.
          .arcTo(bottom_left, top_left, pine::kPreviewContainerRadius)
          .close();
  SetClipPath(clip_path);
}

BEGIN_METADATA(PineScreenshotIconRowView)
END_METADATA

}  // namespace ash
