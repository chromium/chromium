// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/views/context_menu_controller.h"

namespace gfx {
class RenderText;
}

namespace views {
class ImageView;
class ProgressBar;
}  // namespace views

namespace app_list {
namespace test {
class SearchResultListViewTest;
}  // namespace test

class AppListViewDelegate;
class SearchResult;
class SearchResultListView;
class SearchResultActionsView;

// SearchResultView displays a SearchResult.
class APP_LIST_EXPORT SearchResultView
    : public SearchResultBaseView,
      public views::ContextMenuController,
      public SearchResultActionsViewDelegate,
      public AppListMenuModelAdapter::Delegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  explicit SearchResultView(SearchResultListView* list_view,
                            AppListViewDelegate* view_delegate);
  ~SearchResultView() override;

  // Sets/gets SearchResult displayed by this view.
  void SetResult(SearchResult* result);
  SearchResult* result() { return result_; }

  // Clears reference to SearchResult but don't schedule repaint.
  void ClearResultNoRepaint();

  // Clears the selected action.
  void ClearSelectedAction();

  // Computes the button's spoken feedback name.
  base::string16 ComputeAccessibleName() const;

  void set_is_last_result(bool is_last) { is_last_result_ = is_last; }

  // AppListMenuModelAdapter::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class app_list::test::SearchResultListViewTest;

  void UpdateTitleText();
  void UpdateDetailsText();
  void UpdateAccessibleName();

  // Creates title/details render text.
  void CreateTitleRenderText();
  void CreateDetailsRenderText();

  // views::View overrides:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // Bound by ShowContextMenuForView().
  void OnGetContextMenu(views::View* source,
                        const gfx::Point& point,
                        ui::MenuSourceType source_type,
                        std::vector<ash::mojom::MenuItemPtr> menu);

  // SearchResultObserver overrides:
  void OnMetadataChanged() override;
  void OnIsInstallingChanged() override;
  void OnPercentDownloadedChanged() override;
  void OnItemInstalled() override;

  void SetIconImage(const gfx::ImageSkia& source,
                    views::ImageView* const icon,
                    const int icon_dimension);

  // SearchResultActionsViewDelegate overrides:
  void OnSearchResultActionActivated(size_t index, int event_flags) override;

  SearchResult* result_ = nullptr;  // Owned by SearchModel::SearchResults.

  bool is_last_result_ = false;

  // Parent list view. Owned by views hierarchy.
  SearchResultListView* list_view_;

  AppListViewDelegate* view_delegate_;

  views::ImageView* icon_;        // Owned by views hierarchy.
  views::ImageView* badge_icon_;  // Owned by views hierarchy.
  std::unique_ptr<gfx::RenderText> title_text_;
  std::unique_ptr<gfx::RenderText> details_text_;
  SearchResultActionsView* actions_view_;  // Owned by the views hierarchy.
  views::ProgressBar* progress_bar_;       // Owned by views hierarchy.

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  // Whether this view is selected.
  bool selected_ = false;

  base::WeakPtrFactory<SearchResultView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
