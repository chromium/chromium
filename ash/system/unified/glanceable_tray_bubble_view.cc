// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/classroom_bubble_view.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

GlanceableTrayBubbleView::GlanceableTrayBubbleView(
    const InitParams& init_params,
    Shelf* shelf)
    : TrayBubbleView(init_params), shelf_(shelf) {}

void GlanceableTrayBubbleView::UpdateBubble() {
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->layer()->SetIsFastRoundedCorner(true);

  // TODO(b:286941809): Setting rounded corners here, can break the background
  // blur applied to child bubble views.
  scroll_view_->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(24));

  auto child_glanceable_container = std::make_unique<views::FlexLayoutView>();
  child_glanceable_container->SetOrientation(
      views::LayoutOrientation::kVertical);

  // TODO(b:277268122): set real contents for glanceables view.
  if (!tasks_bubble_view_) {
    tasks_bubble_view_ = child_glanceable_container->AddChildView(
        std::make_unique<TasksBubbleView>());
  }

  // TODO(b:283370562): only add teacher/student classroom glanceables when
  // the user is enrolled in courses.
  if (!classroom_bubble_view_) {
    classroom_bubble_view_ = child_glanceable_container->AddChildView(
        std::make_unique<ClassroomBubbleView>());
    // Add spacing between the classroom bubble and the previous bubble.
    classroom_bubble_view_->SetProperty(views::kMarginsKey,
                                        gfx::Insets::TLBR(8, 0, 0, 0));
  }

  scroll_view_->SetContents(std::move(child_glanceable_container));

  int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  ChangeAnchorAlignment(shelf_->alignment());
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());
}

bool GlanceableTrayBubbleView::CanActivate() const {
  return true;
}

}  // namespace ash
