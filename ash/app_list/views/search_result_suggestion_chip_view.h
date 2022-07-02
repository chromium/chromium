// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_

#include <memory>

#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AppListViewDelegate;

// A chip view that displays a search result.
class ASH_EXPORT SearchResultSuggestionChipView : public SearchResultBaseView {
 public:
  explicit SearchResultSuggestionChipView(AppListViewDelegate* view_delegate);
  SearchResultSuggestionChipView(const SearchResultSuggestionChipView&) =
      delete;
  SearchResultSuggestionChipView& operator=(
      const SearchResultSuggestionChipView&) = delete;
  ~SearchResultSuggestionChipView() override;

  // Enables background blur for folder icon if |enabled| is true.
  void SetBackgroundBlurEnabled(bool enabled);

  void OnResultChanged() override;

  // SearchResultObserver:
  void OnMetadataChanged() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnThemeChanged() override;

  // ui::LayerOwner:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  void SetIcon(const gfx::ImageSkia& icon);

  void SetText(const std::u16string& text);
  const std::u16string& GetText() const;

 private:
  // Updates the suggestion chip view's title and icon.
  void UpdateSuggestionChipView();

  void InitLayout();

  void OnButtonPressed(const ui::Event& event);

  // Sets rounded corners for the layer with |corner_radius| to clip the chip.
  void SetRoundedCornersForLayer(float corner_radius);

  // The color of the enabled focus ring. Stored so we can swap between this and
  // transparent.
  const ui::ColorId focus_ring_color_;

  AppListViewDelegate* const view_delegate_;  // Owned by AppListView.

  views::ImageView* icon_view_ = nullptr;  // Owned by view hierarchy.
  views::Label* text_view_ = nullptr;      // Owned by view hierarchy.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  base::WeakPtrFactory<SearchResultSuggestionChipView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_
