// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_view.h"

#include <memory>

#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

GlanceablesView::GlanceablesView() {
  // Inside border insets are set in OnBoundsChanged() when this view is added
  // to the widget.
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

GlanceablesView::~GlanceablesView() = default;

void GlanceablesView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect local_bounds = GetLocalBounds();
  // This view fills the screen, so the margins are a fraction of the screen
  // height and width.
  const int vertical_margin = local_bounds.height() / 6;
  const int horizontal_margin = local_bounds.width() / 6;
  layout_->set_inside_border_insets(
      gfx::Insets::VH(vertical_margin, horizontal_margin));
}

}  // namespace ash
