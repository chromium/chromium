// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_

#include "base/guid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Label;
class Textfield;
}  // namespace views

namespace ash {

class DesksTemplatesDeleteButton;
class DeskTemplate;

// A view that represents each individual template item in the desks templates
// grid.
class DesksTemplatesItemView : public views::View {
 public:
  METADATA_HEADER(DesksTemplatesItemView);

  explicit DesksTemplatesItemView(DeskTemplate* desk_template);
  DesksTemplatesItemView(const DesksTemplatesItemView&) = delete;
  DesksTemplatesItemView& operator=(const DesksTemplatesItemView&) = delete;
  ~DesksTemplatesItemView() override;

  // Updates the visibility state of the delete button depending on whether this
  // view is mouse hovered, or if switch access is enabled.
  void UpdateDeleteButtonVisibility();

  // views::View:
  void Layout() override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  // TODO(richui): Pass a list of icons as the parameter.
  void SetIcons();

  void OnDeleteButtonPressed();

  // Owned by the views hierarchy.
  views::Textfield* name_view_ = nullptr;
  views::Label* time_view_ = nullptr;
  views::BoxLayoutView* preview_view_ = nullptr;
  DesksTemplatesDeleteButton* delete_button_ = nullptr;

  // We force showing the delete button when `this` is long pressed or tapped
  // using touch gestures.
  bool force_show_delete_button_ = false;

  // The desk template's unique identifier.
  const base::GUID uuid_;
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesItemView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
