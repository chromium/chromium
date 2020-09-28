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
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window.h"
#include "ui/views/layout/box_layout.h"

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

class HoldingSpaceBubbleContainerView : public views::View {
 public:
  HoldingSpaceBubbleContainerView() {
    layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kHoldingSpaceContainerSpacing));
  }

  void SetFlexForChild(views::View* child, int flex) {
    layout_->SetFlexForView(child, flex);
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  views::BoxLayout* layout_ = nullptr;
};
}  // namespace

HoldingSpaceTrayBubble::HoldingSpaceTrayBubble(
    HoldingSpaceTray* holding_space_tray,
    bool show_by_click)
    : holding_space_tray_(holding_space_tray) {
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

  bubble_view->SetMaxHeight(CalculateMaxHeight());

  HoldingSpaceBubbleContainerView* bubble_container_view =
      bubble_view->AddChildView(
          std::make_unique<HoldingSpaceBubbleContainerView>());

  // Add pinned files container.
  pinned_files_container_ = bubble_container_view->AddChildView(
      std::make_unique<PinnedFilesContainer>(&delegate_));
  bubble_container_view->SetFlexForChild(pinned_files_container_, 1);

  SetupViewLayer(pinned_files_container_);

  // Add recent files container.
  recent_files_container_ = bubble_container_view->AddChildView(
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

int HoldingSpaceTrayBubble::CalculateMaxHeight() const {
  gfx::Rect anchor_bounds =
      holding_space_tray_->GetBubbleAnchor()->GetBoundsInScreen();
  int bottom = holding_space_tray_->shelf()->IsHorizontalAlignment()
                   ? anchor_bounds.y() - kHoldingSpaceTrayIconMainAxisMargin
                   : anchor_bounds.bottom();
  WorkAreaInsets* work_area = WorkAreaInsets::ForWindow(
      holding_space_tray_->shelf()->GetWindow()->GetRootWindow());
  int free_space_height_above_anchor =
      bottom - work_area->user_work_area_bounds().y();

  int bubble_vertical_margin = GetSecondaryBubbleInsets().bottom() * 2;

  return free_space_height_above_anchor - bubble_vertical_margin;
}

}  // namespace ash
