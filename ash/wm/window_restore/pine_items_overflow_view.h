// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_ITEMS_OVERFLOW_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_ITEMS_OVERFLOW_VIEW_H_

#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// An alternative to `PineItemView` when there are more than four windows in
// `apps` and the remaining information needs to be condensed.
class ASH_EXPORT PineItemsOverflowView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemsOverflowView, views::BoxLayoutView)

 public:
  explicit PineItemsOverflowView(
      const InformedRestoreContentsData::AppsInfos& apps_infos);

  PineItemsOverflowView(const PineItemsOverflowView&) = delete;
  PineItemsOverflowView& operator=(const PineItemsOverflowView&) = delete;
  ~PineItemsOverflowView() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_ITEMS_OVERFLOW_VIEW_H_
