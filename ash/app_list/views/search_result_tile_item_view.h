// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/views/context_menu_controller.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AppListViewDelegate;
class SearchResult;

// A tile view that displays a search result. It hosts view for search result
// that has SearchResult::DisplayType DISPLAY_TILE or DISPLAY_RECOMMENDATION.
class ASH_EXPORT SearchResultTileItemView
    : public SearchResultBaseView,
      public views::ContextMenuController {
 public:
  explicit SearchResultTileItemView(AppListViewDelegate* view_delegate);

  SearchResultTileItemView(const SearchResultTileItemView&) = delete;
  SearchResultTileItemView& operator=(const SearchResultTileItemView&) = delete;

  ~SearchResultTileItemView() override;

  void OnResultChanged() override;

  // Informs the SearchResultTileItemView of its parent's background color. The
  // controls within the SearchResultTileItemView will adapt to suit the given
  // color.
  void SetParentBackgroundColor(SkColor color);

  void set_group_index_in_container_view(int index) {
    group_index_in_container_view_ = index;
  }
  int group_index_in_container_view() const {
    return group_index_in_container_view_;
  }

  // Overridden from views::Button:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void StateChanged(ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Overridden from SearchResultObserver:
  void OnMetadataChanged() override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 private:
  // Launch the result and log to various histograms.
  // |by_button_press|: True if |result_| is activated by button pressing;
  //                    otherwise |result| is activated by ENTER key pressing.
  void ActivateResult(int event_flags, bool by_button_press);

  // Bound by ShowContextMenuForViewImpl().
  void OnGetContextMenuModel(views::View* source,
                             const gfx::Point& point,
                             ui::MenuSourceType source_type,
                             std::unique_ptr<ui::SimpleMenuModel> menu_model);

  // The callback used when a menu closes.
  void OnMenuClosed();

  void OnButtonPressed(const ui::Event& event);

  void SetIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const ui::ImageModel& badge_icon,
                    bool use_badge_icon_background);
  void SetTitle(const std::u16string& title);
  void SetTitleTags(const SearchResultTags& tags);
  void SetRating(float rating);
  void SetPrice(const std::u16string& price);

  AppListMenuModelAdapter::AppListViewAppType GetAppType() const;

  // Whether the tile view is a suggested app.
  bool IsSuggestedAppTile() const;

  // Records an app being launched.
  void LogAppLaunchForSuggestedApp() const;

  void UpdateBackgroundColor();

  // Gets the bounds for the selection ring (shown when the result is selected).
  gfx::RectF GetSelectionRingBounds() const;

  // Overridden from views::View:
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  AppListViewDelegate* const view_delegate_;  // Owned by AppListView.

  views::ImageView* icon_ = nullptr;         // Owned by views hierarchy.
  views::ImageView* badge_ = nullptr;        // Owned by views hierarchy.
  views::Label* title_ = nullptr;            // Owned by views hierarchy.
  views::Label* rating_ = nullptr;           // Owned by views hierarchy.
  views::Label* price_ = nullptr;            // Owned by views hierarchy.
  views::ImageView* rating_star_ = nullptr;  // Owned by views hierarchy.

  SkColor parent_background_color_ = SK_ColorTRANSPARENT;

  // The index of the app in its display group in its container view. Currently,
  // there are three separately displayed groups for apps in launcher's
  // suggestion window: Installed apps, play store apps, play store reinstalled
  // app.
  int group_index_in_container_view_;
  const bool is_app_reinstall_recommendation_enabled_;

  // Whether the result view moved into selected state only because a context
  // menu was shown.
  bool selected_for_context_menu_ = false;

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  base::WeakPtrFactory<SearchResultTileItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
