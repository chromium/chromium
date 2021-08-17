// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "ui/views/view.h"

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

// Contains the views for the bubble version of the launcher. It looks like a
// system tray bubble. It does not derive from TrayBubbleView because it takes
// focus by default, uses a different EventHandler for closing, and isn't tied
// to the system tray area.
class ASH_EXPORT AppListBubbleView : public views::View,
                                     public SearchBoxViewDelegate {
 public:
  // Creates the bubble on the display for `root_window`.
  AppListBubbleView(AppListViewDelegate* view_delegate,
                    aura::Window* root_window);
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;

  // Returns the bounds for the bubble widget in root window coordinates.
  // TODO(jamescook): Move to AppListBubblePresenter.
  gfx::Rect GetBubbleBounds() const;

  // Handles back action if it we have a use for it besides dismissing.
  bool Back();

  // Focuses the search box text input field.
  void FocusSearchBox();

  // Returns true if the assistant page is showing.
  bool IsShowingEmbeddedAssistantUI() const;

  // Shows the assistant page.
  void ShowEmbeddedAssistantUI();

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // SearchBoxViewDelegate:
  void QueryChanged(SearchBoxViewBase* sender) override;
  void AssistantButtonPressed() override;
  void BackButtonPressed() override {}
  void CloseButtonPressed() override;
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override;
  bool CanSelectSearchResults() override;

 private:
  friend class AppListTestHelper;
  friend class AssistantTestApiImpl;

  AppListViewDelegate* const view_delegate_;
  aura::Window* const root_window_;
  SearchBoxView* search_box_view_ = nullptr;
  AppListBubbleAppsPage* apps_page_ = nullptr;
  AppListBubbleSearchPage* search_page_ = nullptr;
  AppListBubbleAssistantPage* assistant_page_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
