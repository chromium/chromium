// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class AppListBubbleAppsPage;
class AppListBubbleAssistantPage;
class AppListBubbleSearchPage;
class AppListViewDelegate;
class SearchBoxView;
enum class ShelfAlignment;

// Contains the views for the bubble version of the launcher.
class ASH_EXPORT AppListBubbleView : public views::BubbleDialogDelegateView,
                                     public SearchBoxViewDelegate {
 public:
  // Creates the bubble on the display for `root_window`. Anchors the bubble to
  // a corner of the screen based on `shelf_alignment`.
  AppListBubbleView(AppListViewDelegate* view_delegate,
                    aura::Window* root_window,
                    ShelfAlignment shelf_alignment);
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;

  // Focuses the search box text input field.
  void FocusSearchBox();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // SearchBoxViewDelegate:
  void QueryChanged(SearchBoxViewBase* sender) override;
  void AssistantButtonPressed() override;
  void BackButtonPressed() override {}
  void CloseButtonPressed() override;
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void SearchBoxFocusChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override;
  bool CanSelectSearchResults() override;

 private:
  friend class AppListTestHelper;

  AppListViewDelegate* const view_delegate_;
  SearchBoxView* search_box_view_ = nullptr;
  AppListBubbleAppsPage* apps_page_ = nullptr;
  AppListBubbleSearchPage* search_page_ = nullptr;
  AppListBubbleAssistantPage* assistant_page_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
