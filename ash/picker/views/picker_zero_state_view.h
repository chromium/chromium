// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class PickerCapsNudgeView;
class PickerItemView;
class PickerSectionListView;
class PickerSectionView;

class ASH_EXPORT PickerZeroStateView : public PickerPageView {
  METADATA_HEADER(PickerZeroStateView, PickerPageView)

 public:
  // Indicates the user has selected a category.
  using SelectCategoryCallback =
      base::RepeatingCallback<void(PickerCategory category)>;

  explicit PickerZeroStateView(int picker_view_width,
                               SelectCategoryCallback select_category_callback);
  PickerZeroStateView(const PickerZeroStateView&) = delete;
  PickerZeroStateView& operator=(const PickerZeroStateView&) = delete;
  ~PickerZeroStateView() override;

  // PickerPageView:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;

  std::map<PickerCategoryType, raw_ptr<PickerSectionView>>
  section_views_for_testing() const {
    return section_views_;
  }

  PickerCapsNudgeView* CapsNudgeViewForTesting() const {
    return caps_nudge_view_;
  }

 private:
  void ClearCapsNudge();

  // Gets or creates the section to contain `category`.
  PickerSectionView* GetOrCreateSectionView(PickerCategory category);

  void SetPseudoFocusedItem(PickerItemView* item);

  void ScrollPseudoFocusedItemToVisible();

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // Used to track the section view for each category type.
  std::map<PickerCategoryType, raw_ptr<PickerSectionView>> section_views_;

  raw_ptr<PickerCapsNudgeView> caps_nudge_view_;
  // The currently pseudo focused item, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<PickerItemView> pseudo_focused_item_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
