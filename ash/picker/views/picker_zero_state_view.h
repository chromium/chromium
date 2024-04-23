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
      bool show_suggested_results,
      int picker_view_width);
  PickerZeroStateView(const PickerZeroStateView&) = delete;
  PickerZeroStateView& operator=(const PickerZeroStateView&) = delete;
  ~PickerZeroStateView() override;

  // PickerPageView:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  void AdvancePseudoFocus(PseudoFocusDirection direction) override;

  std::map<PickerCategoryType, raw_ptr<PickerSectionView>>
  section_views_for_testing() const {
    return section_views_;
  }

  PickerSectionView* SuggestedSectionForTesting() const {
    return suggested_section_view_;
  }

 private:
  void OnCategorySelected(PickerCategory category);
  void OnSuggestedResultSelected(const PickerSearchResult& result);

  // Gets or creates the section to contain `category`.
  PickerSectionView* GetOrCreateSectionView(PickerCategory category);

  void SetPseudoFocusedView(views::View* view);

  void ScrollPseudoFocusedViewToVisible();

  void OnFetchSuggestedResults(std::vector<PickerSearchResult> result);

  void OnFetchZeroStateEditorResults(PickerCategory category,
                                     std::vector<PickerSearchResult> result);

  raw_ptr<PickerZeroStateViewDelegate> delegate_;

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // Used to track the section view for each category type.
  std::map<PickerCategoryType, raw_ptr<PickerSectionView>> section_views_;

  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;

  raw_ptr<PickerSectionView> suggested_section_view_ = nullptr;
  std::unique_ptr<PickerClipboardProvider> clipboard_provider_;

  base::WeakPtrFactory<PickerZeroStateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
