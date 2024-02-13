// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/views/picker_category_type.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerSectionView;

class ASH_EXPORT PickerZeroStateView : public views::View {
  METADATA_HEADER(PickerZeroStateView, views::View)

 public:
  // Indicates the user has selected a category.
  using SelectCategoryCallback =
      base::RepeatingCallback<void(PickerCategory category)>;

  explicit PickerZeroStateView(int picker_view_width,
                               SelectCategoryCallback select_category_callback);
  PickerZeroStateView(const PickerZeroStateView&) = delete;
  PickerZeroStateView& operator=(const PickerZeroStateView&) = delete;
  ~PickerZeroStateView() override;

  std::map<PickerCategoryType, raw_ptr<PickerSectionView>>
  section_views_for_testing() const {
    return section_views_;
  }

 private:
  // Gets or creates the section to contain `category`.
  PickerSectionView* GetOrCreateSectionView(PickerCategory category);

  // Width of the containing PickerView.
  int picker_view_width_ = 0;

  // The views for each section of categories.
  std::map<PickerCategoryType, raw_ptr<PickerSectionView>> section_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_H_
