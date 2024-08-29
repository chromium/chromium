// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_

#include "ui/views/controls/scroll_view.h"

namespace ash {

// A selection view that allows users to pick which tabs and apps they want to
// move to a new desk. It's a scroll view that contains many
// `TabAppSelectionItemView`'s representing tabs and apps.
// TODO(http://b/361326120): Add the experimental features view.
// TODO(http://b/361326120): Replace hardcoded values.
// TODO(http://b/361326120): Localize.
class TabAppSelectionView : public views::ScrollView {
  METADATA_HEADER(TabAppSelectionView, views::ScrollView)

 public:
  TabAppSelectionView();
  TabAppSelectionView(const TabAppSelectionView&) = delete;
  TabAppSelectionView& operator=(const TabAppSelectionView&) = delete;
  ~TabAppSelectionView() override;

  void OnCloseButtonPressed(views::View* sender);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_
