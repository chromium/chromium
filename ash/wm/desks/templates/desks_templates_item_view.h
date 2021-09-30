// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// A view that represents each individual template item in the desks templates
// grid.
class DesksTemplatesItemView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(DesksTemplatesItemView);

  DesksTemplatesItemView();
  DesksTemplatesItemView(const DesksTemplatesItemView&) = delete;
  DesksTemplatesItemView& operator=(const DesksTemplatesItemView&) = delete;
  ~DesksTemplatesItemView() override;

 private:
  // TODO(richui): Pass a list of icons as the parameter.
  void SetIcons();

  // Owned by the views hierarchy.
  views::View* name_view_ = nullptr;
  views::View* time_view_ = nullptr;
  views::BoxLayoutView* preview_view_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesItemView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
