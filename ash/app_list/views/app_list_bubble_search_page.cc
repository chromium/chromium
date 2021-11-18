// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include <memory>

#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AppListBubbleSearchPage::AppListBubbleSearchPage(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchBoxView* search_box_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  search_view_ = AddChildView(std::make_unique<ProductivityLauncherSearchView>(
      view_delegate, dialog_controller, search_box_view));
}

AppListBubbleSearchPage::~AppListBubbleSearchPage() = default;

}  // namespace ash
