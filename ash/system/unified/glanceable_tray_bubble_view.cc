// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ui/views/controls/label.h"

namespace ash {

GlanceableTrayBubbleView::GlanceableTrayBubbleView(
    const InitParams& init_params,
    Shelf* shelf)
    : TrayBubbleView(init_params), shelf_(shelf) {}

void GlanceableTrayBubbleView::UpdateBubble() {
  // TODO(b:277268122): set real contents for glanceables view.
  if (!tasks_bubble_view_) {
    tasks_bubble_view_ = AddChildView(std::make_unique<TasksBubbleView>());
  }

  int max_height = CalculateMaxTrayBubbleHeight();
  SetMaxHeight(max_height);
  ChangeAnchorAlignment(shelf_->alignment());
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());
}

bool GlanceableTrayBubbleView::CanActivate() const {
  return true;
}

gfx::Size GlanceableTrayBubbleView::CalculatePreferredSize() const {
  // TODO(b:277268122): Scale height based on task_items_list_view_ contents.
  return gfx::Size(kRevampedTrayMenuWidth, kTasksGlanceableMinHeight);
}

}  // namespace ash
