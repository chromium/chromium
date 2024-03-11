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

class PickerCapsNudgeView;
class PickerClipboardProvider;
class PickerListItemView;
class PickerSearchResult;
class PickerSectionListView;
class PickerSectionView;

class ASH_EXPORT PickerZeroStateView : public PickerPageView {
  METADATA_HEADER(PickerZeroStateView, PickerPageView)

 public:
  // Indicates the user has selected a category.
  using SelectCategoryCallback =
      base::RepeatingCallback<void(PickerCategory category)>;

  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::RepeatingCallback<void(const PickerSearchResult& result)>;

  explicit PickerZeroStateView(
      int picker_view_width,
      SelectCategoryCallback select_category_callback,
      SelectSearchResultCallback select_result_callback);
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

  PickerCapsNudgeView* CapsNudgeViewForTesting() const {
    return caps_nudge_view_;
  }

  PickerSectionView* SuggestedSectionForTesting() const {
    return suggested_section_view_;
  }

 private:
  void ClearCapsNudge();

  void DeleteNudge();

  // Gets or creates the section to contain `category`.
  PickerSectionView* GetOrCreateSectionView(PickerCategory category);

  void SetPseudoFocusedView(views::View* view);

  void ScrollPseudoFocusedViewToVisible();

  void OnFetchSuggestedResult(std::unique_ptr<PickerListItemView> item_view);

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // Used to track the section view for each category type.
  std::map<PickerCategoryType, raw_ptr<PickerSectionView>> section_views_;

  raw_ptr<PickerCapsNudgeView> caps_nudge_view_;
  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;

  raw_ptr<PickerSectionView> suggested_section_view_ = nullptr;
  std::unique_ptr<PickerClipboardProvider> clipboard_provider_;

  base::WeakPtrFactory<PickerZeroStateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
