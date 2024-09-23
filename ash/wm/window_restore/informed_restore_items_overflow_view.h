// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_OVERFLOW_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_OVERFLOW_VIEW_H_

#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// An alternative to `InformedRestoreItemView` when there are more than four
// windows in `apps` and the remaining information needs to be condensed.
class ASH_EXPORT InformedRestoreItemsOverflowView
    : public views::BoxLayoutView {
  METADATA_HEADER(InformedRestoreItemsOverflowView, views::BoxLayoutView)

 public:
  explicit InformedRestoreItemsOverflowView(
      const InformedRestoreContentsData::AppsInfos& apps_infos);

  InformedRestoreItemsOverflowView(const InformedRestoreItemsOverflowView&) =
      delete;
  InformedRestoreItemsOverflowView& operator=(
      const InformedRestoreItemsOverflowView&) = delete;
  ~InformedRestoreItemsOverflowView() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_OVERFLOW_VIEW_H_
