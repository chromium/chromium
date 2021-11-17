// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class ProductivityLauncherSearchView;
class SearchBoxView;
class SearchResultPageDialogController;

// The search results page for the app list bubble / clamshell launcher.
// Contains a scrolling list of search results. Does not include the search box,
// which is owned by a parent view.
class ASH_EXPORT AppListBubbleSearchPage : public views::View {
 public:
  AppListBubbleSearchPage(AppListViewDelegate* view_delegate,
                          SearchResultPageDialogController* dialog_controller,
                          SearchBoxView* search_box_view);
  AppListBubbleSearchPage(const AppListBubbleSearchPage&) = delete;
  AppListBubbleSearchPage& operator=(const AppListBubbleSearchPage&) = delete;
  ~AppListBubbleSearchPage() override;

  ProductivityLauncherSearchView* search_view() { return search_view_; }

 private:
  // Owned by view hierarchy.
  ProductivityLauncherSearchView* search_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
