// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_items_container_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_item_view.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {
constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);
constexpr int kItemsContainerRounding = 15;
}  // namespace

PineItemsContainerView::PineItemsContainerView(
    const PineContentsData::AppsInfos& apps_infos) {
  const int elements = static_cast<int>(apps_infos.size());
  CHECK_GT(elements, 0);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kItemsContainerRounding));
  SetBetweenChildSpacing(pine::kItemsContainerChildSpacing);
  SetInsideBorderInsets(kItemsContainerInsets);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  for (int i = 0; i < elements; ++i) {
    const PineContentsData::AppInfo& app_info = apps_infos[i];
    // If there are more than four elements, we will need to save the last
    // space for the overflow view to condense the remaining info.
    if (elements > pine::kMaxItems && i >= pine::kOverflowMinThreshold) {
      overflow_view_for_testing_ =
          AddChildView(std::make_unique<PineItemsOverflowView>(apps_infos));
      break;
    }

    AddChildView(
        std::make_unique<PineItemView>(app_info, /*inside_screenshot=*/false));
  }
}

BEGIN_METADATA(PineItemsContainerView)
END_METADATA

}  // namespace ash
