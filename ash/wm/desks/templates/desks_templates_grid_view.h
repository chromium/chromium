// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// A view that acts as the content view of the desks templates widget.
// TODO(sammiequon): Add details and ASCII.
class DesksTemplatesGridView : public views::View {
 public:
  METADATA_HEADER(DesksTemplatesGridView);

  DesksTemplatesGridView();
  DesksTemplatesGridView(const DesksTemplatesGridView&) = delete;
  DesksTemplatesGridView& operator=(const DesksTemplatesGridView&) = delete;
  ~DesksTemplatesGridView() override;

  // Creates and returns the widget that contains the DesksTemplatesGridView in
  // overview mode. This does not show the widget.
  // TODO(sammiequon): We might want this view to be part of the DesksWidget
  // depending on the animations.
  static views::UniqueWidgetPtr CreateDesksTemplatesGridWidget(
      aura::Window* root,
      const gfx::Rect& grid_bounds);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
