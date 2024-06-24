// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ui {
class Layer;
}

namespace ash {

class AppListViewDelegate;
class AppListSearchView;
class SearchBoxView;
class SearchResultPageDialogController;

// The search results page for the app list bubble / clamshell launcher.
// Contains a scrolling list of search results. Does not include the search box,
// which is owned by a parent view.
class ASH_EXPORT AppListBubbleSearchPage : public views::View {
  METADATA_HEADER(AppListBubbleSearchPage, views::View)

 public:
  AppListBubbleSearchPage(AppListViewDelegate* view_delegate,
                          SearchResultPageDialogController* dialog_controller,
                          SearchBoxView* search_box_view);
  AppListBubbleSearchPage(const AppListBubbleSearchPage&) = delete;
  AppListBubbleSearchPage& operator=(const AppListBubbleSearchPage&) = delete;
  ~AppListBubbleSearchPage() override;

  // Starts the animation for showing this page, coming from another page.
  void AnimateShowPage();

  // Starts the animation for hiding this page, going to another page.
  void AnimateHidePage();

  // Aborts all layer animations.
  void AbortAllAnimations();

  AppListSearchView* search_view() { return search_view_; }

  // Which layer animates is an implementation detail.
  ui::Layer* GetPageAnimationLayerForTest();

 private:
  // Owned by view hierarchy.
  raw_ptr<AppListSearchView> search_view_ = nullptr;

  base::WeakPtrFactory<AppListBubbleSearchPage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
