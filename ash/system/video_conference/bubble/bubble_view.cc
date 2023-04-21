// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash::video_conference {

BubbleView::BubbleView(const InitParams& init_params,
                       VideoConferenceTrayController* controller)
    : TrayBubbleView(init_params), controller_(controller) {
  SetID(BubbleViewID::kMainBubbleView);

  // Add a `FlexLayout` for the entire view.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  // `ReturnToAppPanel` resides in the top-level layout and isn't part of the
  // scrollable area (that can't be added until the `BubbleView` officially has
  // a parent widget).
  AddChildView(std::make_unique<ReturnToAppPanel>());
}

void BubbleView::AddedToWidget() {
  // Create the `views::ScrollView` to house the effects sections. This has to
  // be done here because `BubbleDialogDelegate::GetBubbleBounds` requires a
  // parent widget, which isn't officially assigned until after the call to
  // `ShowBubble` in `VideoConferenceTray::ToggleBubble`.
  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetAllowKeyboardScrolling(false);
  scroll_view->SetBackgroundColor(absl::nullopt);

  // TODO(b/262930924): Use the correct max_height.
  scroll_view->ClipHeightTo(/*min_height=*/0, /*max_height=*/300);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  views::BoxLayoutView* layout_view =
      scroll_view->SetContents(std::make_unique<views::BoxLayoutView>());
  layout_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  layout_view->SetInsideBorderInsets(gfx::Insets::TLBR(16, 12, 16, 12));
  layout_view->SetBetweenChildSpacing(16);

  // Make the effects sections children of the `views::FlexLayoutView`, so that
  // they scroll (if more effects are present than can fit in the available
  // height).
  if (controller_->effects_manager().HasToggleEffects()) {
    layout_view->AddChildView(std::make_unique<ToggleEffectsView>(controller_));
  }
  if (controller_->effects_manager().HasSetValueEffects()) {
    layout_view->AddChildView(
        std::make_unique<SetValueEffectsView>(controller_));
  }
}

void BubbleView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
  SizeToContents();
}

bool BubbleView::CanActivate() const {
  return true;
}

}  // namespace ash::video_conference
