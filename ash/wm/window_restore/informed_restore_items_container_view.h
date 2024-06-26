// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_CONTAINER_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_CONTAINER_VIEW_H_

#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// The right side contents (in LTR) of the `InformedRestoreContentsView`. It is
// a vertical list of `InformedRestoreItemView`s, with each view representing an
// app. Shows a maximum of `kMaxItems` items.
class InformedRestoreItemsContainerView : public views::BoxLayoutView {
  METADATA_HEADER(InformedRestoreItemsContainerView, views::BoxLayoutView)

 public:
  explicit InformedRestoreItemsContainerView(
      const InformedRestoreContentsData::AppsInfos& apps_infos);
  InformedRestoreItemsContainerView(const InformedRestoreItemsContainerView&) =
      delete;
  InformedRestoreItemsContainerView& operator=(
      const InformedRestoreItemsContainerView&) = delete;
  ~InformedRestoreItemsContainerView() override = default;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEMS_CONTAINER_VIEW_H_
