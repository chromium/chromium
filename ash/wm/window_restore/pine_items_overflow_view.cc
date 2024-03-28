// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_items_overflow_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/pine_app_image_view.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kOverflowMaxElements = 7;
constexpr int kOverflowMaxThreshold = kOverflowMaxElements - 1;
constexpr int kOverflowTriangleElements = 6;
constexpr int kOverflowIconSpacing = 2;
constexpr int kOverflowBackgroundRounding = 20;
constexpr int kOverflowCountBackgroundRounding = 9;
constexpr gfx::Size kOverflowCountPreferredSize(18, 18);

}  // namespace

PineItemsOverflowView::PineItemsOverflowView(
    const PineContentsData::AppsInfos& apps_infos) {
  const int elements = static_cast<int>(apps_infos.size());
  CHECK_GT(elements, pine::kMaxItems);

  // TODO(hewer): Fix margins so the icons and text are aligned with
  // `PineItemView` elements.
  SetBetweenChildSpacing(pine::kItemChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  // Create a series of `BoxLayoutView`s to represent a 1x2 row, a triangle
  // with one element on top and two on the bottom, or a 2x2 box. The triangle
  // is specific to the 3-window overflow case, and is why we prefer a
  // `BoxLayout` over a `TableLayout` to keep things uniform.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetBetweenChildSpacing(kOverflowIconSpacing)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              pine::kIconBackgroundColor, kOverflowBackgroundRounding))
          .AddChildren(
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&top_row_view_)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetMainAxisAlignment(
                      views::BoxLayout::MainAxisAlignment::kCenter)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kStretch)
                  .SetBetweenChildSpacing(kOverflowIconSpacing),
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&bottom_row_view_)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetMainAxisAlignment(
                      views::BoxLayout::MainAxisAlignment::kCenter)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kStretch)
                  .SetBetweenChildSpacing(kOverflowIconSpacing))
          .Build());

  // Populate the `BoxLayoutView`s with window icons or a count of any excess
  // windows.
  for (int i = pine::kOverflowMinThreshold; i < elements; ++i) {
    // If there are 5 or more overflow windows, save the last spot in the
    // bottom row to count the remaining windows.
    if (elements > kOverflowMaxElements && i >= kOverflowMaxThreshold) {
      views::Label* count_label;
      bottom_row_view_->AddChildView(
          views::Builder<views::Label>()
              .CopyAddressTo(&count_label)
              // TODO(hewer): Cut off the maximum number of digits to
              // display.
              .SetText(base::FormatNumber(elements - kOverflowMaxThreshold))
              .SetPreferredSize(kOverflowCountPreferredSize)
              .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  cros_tokens::kCrosSysPrimaryContainer,
                  kOverflowCountBackgroundRounding))
              .Build());
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                            *count_label);
      break;
    }

    // Add the image view to the correct row based on the total number of
    // elements and the current index.
    views::BoxLayoutView* row_view =
        // If there are 6 elements (3 overflow elements), we will want to
        // display the overflow elements in a triangle. Thus, we will only add
        // the first element (i == 3) to the top row.
        (elements == kOverflowTriangleElements &&
         i == pine::kOverflowMinThreshold) ||
                // Otherwise, we can add the first two elements (i == 3 || i
                // == 4) to the top row, as the view will be in a 1x2 or 2x2
                // configuration.
                (elements != kOverflowTriangleElements && i <= pine::kMaxItems)
            ? top_row_view_
            : bottom_row_view_;

    row_view->AddChildView(std::make_unique<PineAppImageView>(
        apps_infos[i].app_id, PineAppImageView::Type::kOverflow));
  }

  // Add a text label displaying the count of the remaining windows.
  views::Label* remaining_windows_label;
  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&remaining_windows_label)
                   .SetEnabledColorId(pine::kPineItemTextColor)
                   .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                              pine::kItemTitleFontSize,
                                              gfx::Font::Weight::BOLD))
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .SetText(l10n_util::GetPluralStringFUTF16(
                       IDS_ASH_FOREST_WINDOW_OVERFLOW_COUNT,
                       elements - pine::kOverflowMinThreshold))
                   .Build());
  SetFlexForView(remaining_windows_label, 1);
}

PineItemsOverflowView::~PineItemsOverflowView() = default;

BEGIN_METADATA(PineItemsOverflowView)
END_METADATA

}  // namespace ash
