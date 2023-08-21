// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

// Renders "Showing X out of Y" label and "See all" button. Used in classroom
// and tasks bubbles.
class ASH_EXPORT GlanceablesListFooterView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesListFooterView);

  GlanceablesListFooterView(const std::u16string& see_all_accessible_name,
                            base::RepeatingClosure on_see_all_pressed);
  GlanceablesListFooterView(const GlanceablesListFooterView&) = delete;
  GlanceablesListFooterView& operator=(const GlanceablesListFooterView&) =
      delete;
  ~GlanceablesListFooterView() override = default;

  // Updates `items_count_label_`.
  // `visible_items_count` - number of items visible/rendered in a list.
  // `total_items_count`   - total number of items returned from API.
  void UpdateItemsCount(size_t visible_items_count, size_t total_items_count);

  views::Label* items_count_label() { return items_count_label_; }
  views::LabelButton* see_all_button() const { return see_all_button_; }

 private:
  raw_ptr<views::Label> items_count_label_ = nullptr;
  raw_ptr<views::LabelButton> see_all_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_
