// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// TODO(hewer): There are some duplicates among this class, PineItemView,
// PineItemsOverflowView and PineItemsContainerView. Especially the logic to get
// the app icons, tab favicon. Refactoring to eliminate the duplicate after
// finishing all the functionality.
//
// The view holds a row of icons resides at the bottom-left of the pine
// screenshot preview.
class ASH_EXPORT PineScreenshotIconRowView : public views::BoxLayoutView {
  METADATA_HEADER(PineScreenshotIconRowView, views::BoxLayoutView)

 public:
  explicit PineScreenshotIconRowView(
      const InformedRestoreContentsData::AppsInfos& apps_infos);
  PineScreenshotIconRowView(const PineScreenshotIconRowView&) = delete;
  PineScreenshotIconRowView& operator=(const PineScreenshotIconRowView&) =
      delete;
  ~PineScreenshotIconRowView() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_
