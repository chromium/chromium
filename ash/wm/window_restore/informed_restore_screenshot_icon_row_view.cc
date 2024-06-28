// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_screenshot_icon_row_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/informed_restore_app_image_view.h"
#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/informed_restore_item_view.h"
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
    gfx::Insets::TLBR(informed_restore::kPreviewContainerRadius + 4, 4, 4, 4);
constexpr int kIconRowHeight = informed_restore::kScreenshotIconRowIconSize +
                               kIconRowInsets.top() + kIconRowInsets.bottom();

// Returns the preferred size of the icon row. `child_number` indicates the
// number of children while `one_browser_window` indicates it is only one
// browser window opened, which means the favicons of the tabs will be shown
// instead.
gfx::Size GetPreferredSizeOfTheRow(int child_number, bool one_browser_window) {
  int width = child_number * informed_restore::kScreenshotIconRowIconSize +
              kIconRowInsets.left() + kIconRowInsets.right() +
              informed_restore::kPreviewContainerRadius;
  if (one_browser_window) {
    width += 2 * informed_restore::kScreenshotIconRowChildSpacing +
             (child_number - 2) * informed_restore::kScreenshotFaviconSpacing +
             views::Separator::kThickness;
  } else {
    width +=
        (child_number - 1) * informed_restore::kScreenshotIconRowChildSpacing;
  }
  return gfx::Size(width, kIconRowHeight);
}

}  // namespace

InformedRestoreScreenshotIconRowView::InformedRestoreScreenshotIconRowView(
    const InformedRestoreContentsData::AppsInfos& apps_infos) {
  SetID(informed_restore::kScreenshotIconRowViewID);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(informed_restore::kScreenshotIconRowChildSpacing);
  SetInsideBorderInsets(kIconRowInsets);
  SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBaseOpaque));
  // Do not flip this view in RTL, since the cutout in
  // `InformedRestoreContentsView` is not flipped.
  SetMirrored(false);

  const int elements_size = static_cast<int>(apps_infos.size());
  const bool one_browser_window =
      elements_size == 1 && IsBrowserAppId(apps_infos[0].app_id);

  // If there is only one browser window, show the browser icon and its tabs
  // favicons inside the icon row view.
  if (one_browser_window) {
    AddChildView(std::make_unique<InformedRestoreItemView>(apps_infos[0],
                                                /*inside_screenshot=*/true));
  } else {
    const bool exceed_max_elements =
        elements_size > informed_restore::kScreenshotIconRowMaxElements;
    // If there are more than `kScreenshotIconRowMaxElements` number of windows,
    // show `kScreenshotIconRowMaxElements - 1` number of icons and save the
    // last spot in the row to count the remaining windows.
    const int num_icon =
        exceed_max_elements
            ? informed_restore::kScreenshotIconRowMaxElements - 1
            : elements_size;

    for (int i = 0; i < num_icon; i++) {
      auto image_view = std::make_unique<InformedRestoreAppImageView>(
          apps_infos[i].app_id, InformedRestoreAppImageView::Type::kScreenshot,
          base::DoNothing());
      image_view->SetID(informed_restore::kScreenshotImageViewID);
      AddChildView(std::move(image_view));
    }
    if (exceed_max_elements) {
      auto* count_label = AddChildView(
          views::Builder<views::Label>()
              .SetText(u"+" + base::FormatNumber(elements_size - num_icon))
              .SetPreferredSize(
                  informed_restore::kScreenshotIconRowImageViewSize)
              .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  cros_tokens::kCrosSysPrimaryContainer,
                  informed_restore::kScreenshotIconRowIconSize / 2.0))
              .Build());
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                            *count_label);
    }
  }
  int child_number =
      std::min(informed_restore::kScreenshotIconRowMaxElements,
               one_browser_window ? static_cast<int>(apps_infos[0].tab_count)
                                  : elements_size);
  // Add the browser icon when there is only one browser window opened.
  if (one_browser_window) {
    child_number++;
  }
  SetPreferredSize(GetPreferredSizeOfTheRow(child_number, one_browser_window));
}

InformedRestoreScreenshotIconRowView::~InformedRestoreScreenshotIconRowView()
    = default;

BEGIN_METADATA(InformedRestoreScreenshotIconRowView)
END_METADATA

}  // namespace ash
