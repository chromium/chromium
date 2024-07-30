// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_items_overflow_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/informed_restore_app_image_view.h"
#include "ash/wm/window_restore/informed_restore_constants.h"
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

InformedRestoreItemsOverflowView::InformedRestoreItemsOverflowView(
    const InformedRestoreContentsData::AppsInfos& apps_infos) {
  const int num_elements = static_cast<int>(apps_infos.size());
  CHECK_GT(num_elements, informed_restore::kMaxItems);

  // TODO(hewer): Fix margins so the icons and text are aligned with
  // `InformedRestoreItemView` elements.
  SetBetweenChildSpacing(informed_restore::kItemChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetID(informed_restore::kOverflowViewID);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  auto add_inner_box_layout_view =
      [](views::BoxLayoutView* outer_box) -> views::BoxLayoutView* {
    views::BoxLayoutView* inner_box =
        outer_box->AddChildView(std::make_unique<views::BoxLayoutView>());
    inner_box->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    inner_box->SetBetweenChildSpacing(kOverflowIconSpacing);
    inner_box->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    inner_box->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    return inner_box;
  };

  // Create a series of `BoxLayoutView`s to represent a 1x2 row, a triangle
  // with one element on top and two on the bottom, or a 2x2 box. The triangle
  // is specific to the 3-window overflow case, and is why we prefer a
  // `BoxLayout` over a `TableLayout` to keep things uniform.
  views::BoxLayoutView* outer_box_view;
  views::BoxLayoutView* top_row_view;
  views::BoxLayoutView* bottom_row_view = nullptr;
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&outer_box_view)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              informed_restore::kIconBackgroundColorId,
              kOverflowBackgroundRounding))
          .Build());
  top_row_view = add_inner_box_layout_view(outer_box_view);
  top_row_view->SetID(informed_restore::kOverflowTopRowViewID);

  // Populate the `BoxLayoutView`s with window icons or a count of any excess
  // windows.
  for (int i = informed_restore::kOverflowMinThreshold; i < num_elements; ++i) {
    // If there are 5 or more overflow windows, save the last spot in the
    // bottom row to count the remaining windows.
    if (num_elements > kOverflowMaxElements && i >= kOverflowMaxThreshold) {
      views::Label* count_label;
      bottom_row_view->AddChildView(
          views::Builder<views::Label>()
              .CopyAddressTo(&count_label)
              // TODO(hewer): Cut off the maximum number of digits to
              // display.
              .SetText(base::FormatNumber(num_elements - kOverflowMaxThreshold))
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

    // The bottom row is needed once:
    //   - `num_elements` == `kOverflowTriangleElements` and `top_row_view` has
    //     1 element. This is to achieve the triangle layout.
    //   - `top_row_view` has 2 elements otherwise.
    if ((num_elements == kOverflowTriangleElements &&
         top_row_view->children().size() == 1u) ||
        top_row_view->children().size() == 2u) {
      if (!bottom_row_view) {
        bottom_row_view = add_inner_box_layout_view(outer_box_view);
        bottom_row_view->SetID(informed_restore::kOverflowBottomRowViewID);
      }
    }

    views::BoxLayoutView* row_view =
        bottom_row_view ? bottom_row_view : top_row_view;
    auto image_view = std::make_unique<InformedRestoreAppImageView>(
        apps_infos[i].app_id, InformedRestoreAppImageView::Type::kOverflow,
        base::DoNothing());
    image_view->SetID(informed_restore::kOverflowImageViewID);
    row_view->AddChildView(std::move(image_view));
  }

  // If there are no children in the bottom row inner box layout view, we have
  // two overflow icons. In this case, remove the between child spacing, and add
  // padding so that the size of the outer box layout will be the same as if
  // there were three or more overflow icons.
  if (bottom_row_view) {
    outer_box_view->SetBetweenChildSpacing(kOverflowIconSpacing);
  } else {
    const int padding_height =
        (informed_restore::kOverflowIconPreferredSize.height() +
         kOverflowIconSpacing) /
        2;
    outer_box_view->SetBetweenChildSpacing(0);
    outer_box_view->SetInsideBorderInsets(gfx::Insets::VH(padding_height, 0));
  }

  // Add a text label displaying the count of the remaining windows.
  views::Label* remaining_windows_label;
  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&remaining_windows_label)
                   .SetEnabledColorId(informed_restore::kItemTextColorId)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .SetText(l10n_util::GetStringFUTF16Int(
                       IDS_ASH_INFORMED_RESTORE_WINDOW_OVERFLOW_COUNT,
                       num_elements - informed_restore::kOverflowMinThreshold))
                   .Build());
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *remaining_windows_label);
  SetFlexForView(remaining_windows_label, 1);
}

InformedRestoreItemsOverflowView::~InformedRestoreItemsOverflowView() = default;

BEGIN_METADATA(InformedRestoreItemsOverflowView)
END_METADATA

}  // namespace ash
