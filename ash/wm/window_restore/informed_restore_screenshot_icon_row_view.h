// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_SCREENSHOT_ICON_ROW_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_SCREENSHOT_ICON_ROW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// TODO(hewer): There are some duplicates among this class,
// InformedRestoreItemView, InformedRestoreItemsOverflowView and
// InformedRestoreItemsContainerView. Especially the logic to get the app icons
// and tab favicon. Refactoring to eliminate the duplicate after finishing all
// the functionality.
//
// The view holds a row of icons resides at the bottom-left of the screenshot
// preview.
class ASH_EXPORT InformedRestoreScreenshotIconRowView
    : public views::BoxLayoutView {
  METADATA_HEADER(InformedRestoreScreenshotIconRowView, views::BoxLayoutView)

 public:
  explicit InformedRestoreScreenshotIconRowView(
      const InformedRestoreContentsData::AppsInfos& apps_infos);
  InformedRestoreScreenshotIconRowView(
      const InformedRestoreScreenshotIconRowView&) = delete;
  InformedRestoreScreenshotIconRowView& operator=(
      const InformedRestoreScreenshotIconRowView&) = delete;
  ~InformedRestoreScreenshotIconRowView() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_SCREENSHOT_ICON_ROW_VIEW_H_
