// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_items_container_view.h"

#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/informed_restore_item_view.h"
#include "ash/wm/window_restore/informed_restore_items_overflow_view.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

InformedRestoreItemsContainerView::InformedRestoreItemsContainerView(
    const InformedRestoreContentsData::AppsInfos& apps_infos) {
  const int elements = static_cast<int>(apps_infos.size());
  CHECK_GT(elements, 0);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated,
      informed_restore::kPreviewContainerRadius));
  SetBetweenChildSpacing(informed_restore::kItemsContainerChildSpacing);
  SetInsideBorderInsets(informed_restore::kItemsContainerInsets);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  for (int i = 0; i < elements; ++i) {
    const InformedRestoreContentsData::AppInfo& app_info = apps_infos[i];
    // If there are more than four elements, we will need to save the last
    // space for the overflow view to condense the remaining info.
    if (elements > informed_restore::kMaxItems &&
        i >= informed_restore::kOverflowMinThreshold) {
      AddChildView(std::make_unique<InformedRestoreItemsOverflowView>(
          apps_infos));
      break;
    }

    AddChildView(
        std::make_unique<InformedRestoreItemView>(
            app_info, /*inside_screenshot=*/false));
  }
}

BEGIN_METADATA(InformedRestoreItemsContainerView)
END_METADATA

}  // namespace ash
