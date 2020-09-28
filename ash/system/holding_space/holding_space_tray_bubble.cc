// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_bubble.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/holding_space/pinned_files_container.h"
#include "ash/system/holding_space/recent_files_container.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {
void SetupViewLayer(views::View* view) {
  view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);

  auto* layer = view->layer();
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF{kUnifiedTrayCornerRadius});
  layer->SetColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
  layer->SetBackgroundBlur(kUnifiedMenuBackgroundBlur);
  layer->SetFillsBoundsOpaquely(false);
  layer->SetIsFastRoundedCorner(true);
}
}  // namespace

HoldingSpaceTrayBubble::HoldingSpaceTrayBubble(
    HoldingSpaceTray* holding_space_tray,
    bool show_by_click) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = holding_space_tray;
  init_params.parent_window = holding_space_tray->GetBubbleWindowContainer();
  init_params.anchor_view = holding_space_tray->GetBubbleAnchor();
  init_params.shelf_alignment = holding_space_tray->shelf()->alignment();
  init_params.preferred_width = kHoldingSpaceBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.show_by_click = show_by_click;
  init_params.has_shadow = false;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(
      holding_space_tray->GetBubbleAnchorInsets());
  bubble_view->set_margins(GetSecondaryBubbleInsets());

  // Add pinned files container.
  pinned_files_container_ = bubble_view->AddChildView(
      std::make_unique<PinnedFilesContainer>(&delegate_));
  SetupViewLayer(pinned_files_container_);

  // Separator between the two containers, gives illusion of 2 separate bubbles.
  auto* separator =
      bubble_view->AddChildView(std::make_unique<views::Separator>());
  separator->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kHoldingSpaceContainerSpacing, 0, 0, 0)));

  recent_files_container_ = bubble_view->AddChildView(
      std::make_unique<RecentFilesContainer>(&delegate_));
  SetupViewLayer(recent_files_container_);

  // Show the bubble.
  bubble_wrapper_ = std::make_unique<TrayBubbleWrapper>(
      holding_space_tray, bubble_view, false /* is_persistent */);

  // Set bubble frame to be invisible.
  bubble_wrapper_->GetBubbleWidget()
      ->non_client_view()
      ->frame_view()
      ->SetVisible(false);
}

HoldingSpaceTrayBubble::~HoldingSpaceTrayBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
  bubble_wrapper_->GetBubbleWidget()->CloseNow();
}

void HoldingSpaceTrayBubble::AnchorUpdated() {
  bubble_wrapper_->bubble_view()->UpdateBubble();
}

TrayBubbleView* HoldingSpaceTrayBubble::GetBubbleView() {
  return bubble_wrapper_->bubble_view();
}

views::Widget* HoldingSpaceTrayBubble::GetBubbleWidget() {
  return bubble_wrapper_->GetBubbleWidget();
}

}  // namespace ash
