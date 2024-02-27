// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_ITEMS_CONTAINER_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_ITEMS_CONTAINER_VIEW_H_

#include "ash/wm/window_restore/pine_contents_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// The right side contents (in LTR) of the `PineContentsView`. It is a
// vertical list of `PineItemView`, with each view representing an app. Shows
// a maximum of `kMaxItems` items.
class PineItemsContainerView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemsContainerView, views::BoxLayoutView)

 public:
  explicit PineItemsContainerView(
      const PineContentsData::AppsInfos& apps_infos);
  PineItemsContainerView(const PineItemsContainerView&) = delete;
  PineItemsContainerView& operator=(const PineItemsContainerView&) = delete;
  ~PineItemsContainerView() override = default;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_ITEMS_CONTAINER_VIEW_H_
