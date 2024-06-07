// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}

namespace ash {

class PickerAssetFetcher;
class PickerClipboardProvider;
class PickerSearchResult;
class PickerSectionListView;
class PickerSectionView;
class PickerZeroStateViewDelegate;

class ASH_EXPORT PickerZeroStateView : public PickerPageView {
  METADATA_HEADER(PickerZeroStateView, PickerPageView)

 public:
  explicit PickerZeroStateView(
      PickerZeroStateViewDelegate* delegate,
      base::span<const PickerCategory> available_categories,
      base::span<const PickerCategory> recent_results_categories,
      int picker_view_width,
      PickerAssetFetcher* asset_fetcher);
  PickerZeroStateView(const PickerZeroStateView&) = delete;
  PickerZeroStateView& operator=(const PickerZeroStateView&) = delete;
  ~PickerZeroStateView() override;

  // PickerPageView:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  bool AdvancePseudoFocus(PseudoFocusDirection direction) override;
  bool GainPseudoFocus(PseudoFocusDirection direction) override;
  void LosePseudoFocus() override;

  std::map<PickerCategoryType, raw_ptr<PickerSectionView>>
  category_section_views_for_testing() const {
    return category_section_views_;
  }

  PickerSectionView& PrimarySectionForTesting() const {
    return *primary_section_view_;
  }

 private:
  void OnCategorySelected(PickerCategory category);
  void OnResultSelected(const PickerSearchResult& result);

  // Gets or creates the category type section to contain `category`.
  PickerSectionView* GetOrCreateSectionView(PickerCategory category);

  void SetPseudoFocusedView(views::View* view);

  void ScrollPseudoFocusedViewToVisible();

  // Moves pseudo focus to the top item, or does nothing if this zero state view
  // is not currently handling pseudo focus.
  void MovePseudoFocusToTopIfNeeded();

  void OnFetchRecentResults(std::vector<PickerSearchResult> result);

  void OnFetchZeroStateEditorResults(PickerCategory category,
                                     std::vector<PickerSearchResult> result);

  raw_ptr<PickerZeroStateViewDelegate> delegate_;
  PickerPreviewBubbleController preview_controller_;

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // The primary section is a titleless section that is shown first.
  // It contains items such as zero-state suggestions.
  raw_ptr<PickerSectionView> primary_section_view_ = nullptr;

  // Below the primary section, there is a set of sections for each category
  // type.
  std::map<PickerCategoryType, raw_ptr<PickerSectionView>>
      category_section_views_;

  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;

  std::unique_ptr<PickerClipboardProvider> clipboard_provider_;

  base::WeakPtrFactory<PickerZeroStateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
