// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_

#include "base/guid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class BoxLayoutView;
class Label;
class Textfield;
}  // namespace views

namespace ash {

class DesksTemplatesDeleteButton;
class DesksTemplatesIconView;
class DeskTemplate;

// A view that represents each individual template item in the desks templates
// grid.
class DesksTemplatesItemView : public views::Button {
 public:
  METADATA_HEADER(DesksTemplatesItemView);

  explicit DesksTemplatesItemView(DeskTemplate* desk_template);
  DesksTemplatesItemView(const DesksTemplatesItemView&) = delete;
  DesksTemplatesItemView& operator=(const DesksTemplatesItemView&) = delete;
  ~DesksTemplatesItemView() override;

  // The maximum number of icons that can be displayed.
  static constexpr int kMaxIcons = 4;

  // Updates the visibility state of the delete button depending on whether this
  // view is mouse hovered, or if switch access is enabled.
  void UpdateDeleteButtonVisibility();

  // views::View:
  void Layout() override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  // Given a vector of pairs, where the first entry is an icon's identifier and
  // the second entry is its count, create views for them.
  void SetIcons(
      const std::vector<std::pair<std::string, int>>& identifiers_and_counts);

  void OnDeleteButtonPressed();

  void OnGridItemPressed();

  // Owned by the views hierarchy.
  views::Textfield* name_view_ = nullptr;
  views::Label* time_view_ = nullptr;
  views::BoxLayoutView* preview_view_ = nullptr;
  DesksTemplatesDeleteButton* delete_button_ = nullptr;

  // A vector of the `DesksTemplatesIconView`s stored in `preview_view_`. They
  // are owned by the views hierarchy but store pointers to them here as well.
  // The last element of `icon_views_` is always an `DesksTemplatesIconView`
  // used for storing the overflow count of icons. Not every View in this vector
  // is visible.
  std::vector<DesksTemplatesIconView*> icon_views_;

  // We force showing the delete button when `this` is long pressed or tapped
  // using touch gestures.
  bool force_show_delete_button_ = false;

  // The desk template's unique identifier.
  const base::GUID uuid_;
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesItemView, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
