// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
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
  METADATA_HEADER(GlanceablesListFooterView, views::FlexLayoutView)

 public:
  using GlanceablesContext = GlanceablesTimeManagementBubbleView::Context;

  explicit GlanceablesListFooterView(base::RepeatingClosure on_see_all_pressed);
  GlanceablesListFooterView(const GlanceablesListFooterView&) = delete;
  GlanceablesListFooterView& operator=(const GlanceablesListFooterView&) =
      delete;
  ~GlanceablesListFooterView() override = default;

  void SetTitleText(const std::u16string& title_text);
  void SetSeeAllAccessibleName(const std::u16string& see_all_accessible_name);

  views::Label* title_label() const { return title_label_; }
  views::LabelButton* see_all_button() const { return see_all_button_; }

 private:
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::LabelButton> see_all_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_LIST_FOOTER_VIEW_H_
