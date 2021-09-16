// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class ImageView;
class StyledLabel;
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
}  // namespace test

class AppListViewDelegate;
class MaskedImageView;
class SearchResult;
class SearchResultListView;

// SearchResultView displays a SearchResult.
class ASH_EXPORT SearchResultView : public SearchResultBaseView,
                                    public views::ContextMenuController,
                                    public SearchResultActionsViewDelegate {
 public:
  enum class SearchResultViewType {
    // The default vew type used for the majority of search results.
    kDefault,
    // The classic view type continues support for pre-BubbleView launcher's
    // search UI.
    kClassic,
    // Inline Answer views are used to directly answer questions posed by the
    // search query.
    kInlineAnswer,
  };

  // Internal class name.
  static const char kViewClassName[];

  SearchResultView(SearchResultListView* list_view,
                   AppListViewDelegate* view_delegate,
                   SearchResultViewType view_type);

  SearchResultView(const SearchResultView&) = delete;
  SearchResultView& operator=(const SearchResultView&) = delete;

  ~SearchResultView() override;

  // Sets/gets SearchResult displayed by this view.
  void OnResultChanged() override;

  void SetSearchResultViewType(SearchResultViewType type) { view_type_ = type; }

 private:
  friend class test::SearchResultListViewTest;
  friend class SearchResultListView;

  int PreferredHeight() const;
  int PrimaryTextHeight() const;
  int SecondaryTextHeight() const;

  void UpdateTitleText();
  void UpdateDetailsText();

  void StyleTitleLabel();
  void StyleDetailsLabel();

  // Callback for query suggstion removal confirmation.
  void OnQueryRemovalAccepted(bool accepted);

  // views::View overrides:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnThemeChanged() override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Bound by ShowContextMenuForViewImpl().
  void OnGetContextMenu(views::View* source,
                        const gfx::Point& point,
                        ui::MenuSourceType source_type,
                        std::unique_ptr<ui::SimpleMenuModel> menu_model);

  // SearchResultObserver overrides:
  void OnMetadataChanged() override;

  void OnButtonPressed(const ui::Event& event);

  void SetIconImage(const gfx::ImageSkia& source,
                    views::ImageView* const icon,
                    const gfx::Size& size);

  // SearchResultActionsViewDelegate overrides:
  void OnSearchResultActionActivated(size_t index) override;
  bool IsSearchResultHoveredOrSelected() override;

  // Invoked when the context menu closes.
  void OnMenuClosed();

  // Whether this result has a rich image icon.
  bool IsRichImage() const;

  // Parent list view. Owned by views hierarchy.
  SearchResultListView* list_view_;

  AppListViewDelegate* view_delegate_;

  MaskedImageView* icon_ = nullptr;              // Owned by views hierarchy.
  views::ImageView* badge_icon_ = nullptr;       // Owned by views hierarchy.
  views::StyledLabel* title_label_ = nullptr;    // Owned by view hierarchy.
  views::StyledLabel* details_label_ = nullptr;  // Owned by view hierarchy.
  views::Label* separator_label_ = nullptr;      // Owned by view hierarchy.

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  // Whether the removal confirmation dialog is invoked by long press touch.
  bool confirm_remove_by_long_press_ = false;

  SearchResultViewType view_type_;

  base::WeakPtrFactory<SearchResultView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
