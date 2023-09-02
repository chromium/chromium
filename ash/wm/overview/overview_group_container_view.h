// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_

#include "ash/wm/overview/overview_focusable_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class OverviewGroupItem;

// A view that contains individual overview item widgets that constitute the
// group item view. This class also implements `OverviewHighlightableView` so
// that this will be focused in overview.
class OverviewGroupContainerView : public views::View,
                                   public OverviewFocusableView {
 public:
  METADATA_HEADER(OverviewGroupContainerView);

  explicit OverviewGroupContainerView(OverviewGroupItem* overview_group_item);
  OverviewGroupContainerView(const OverviewGroupContainerView&) = delete;
  OverviewGroupContainerView& operator=(const OverviewGroupContainerView&) =
      delete;
  ~OverviewGroupContainerView() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  bool MaybeActivateFocusedViewOnOverviewExit(
      OverviewSession* overview_session) override;
  gfx::Point GetMagnifierFocusPointInScreen() override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;

 private:
  const raw_ptr<OverviewGroupItem> overview_group_item_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_
