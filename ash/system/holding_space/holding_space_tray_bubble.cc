// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_bubble.h"

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/holding_space/pinned_files_container.h"
#include "ash/system/holding_space/recent_files_container.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Finds all visible `HoldingSpaceItem`s in `parent`'s view hierarchy.
void FindVisibleHoldingSpaceItems(
    views::View* parent,
    std::vector<const HoldingSpaceItem*>* result) {
  for (views::View* view : parent->children()) {
    if (view->GetVisible() && HoldingSpaceItemView::IsInstance(view))
      result->push_back(HoldingSpaceItemView::Cast(view)->item());
    FindVisibleHoldingSpaceItems(view, result);
  }
}

// Records the time from first availability to first entry into holding space.
void RecordTimeFromFirstAvailabilityToFirstEntry(PrefService* prefs) {
  base::Time time_of_first_availability =
      holding_space_prefs::GetTimeOfFirstAvailability(prefs).value();
  base::Time time_of_first_entry =
      holding_space_prefs::GetTimeOfFirstEntry(prefs).value();
  holding_space_metrics::RecordTimeFromFirstAvailabilityToFirstEntry(
      time_of_first_entry - time_of_first_availability);
}

// Sets up the layer for the specified `view`.
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

// HoldingSpaceBubbleContainer -------------------------------------------------

class HoldingSpaceBubbleContainer : public views::View {
 public:
  HoldingSpaceBubbleContainer() {
    layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kHoldingSpaceContainerSpacing));
  }

  void SetFlexForChild(views::View* child, int flex) {
    layout_->SetFlexForView(child, flex);
  }

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  views::BoxLayout* layout_ = nullptr;
};

}  // namespace

// HoldingSpaceTrayBubble ------------------------------------------------------

HoldingSpaceTrayBubble::HoldingSpaceTrayBubble(
    HoldingSpaceTray* holding_space_tray,
    bool show_by_click)
    : holding_space_tray_(holding_space_tray) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = holding_space_tray;
  init_params.parent_window = holding_space_tray->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect =
      holding_space_tray->shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = holding_space_tray->shelf()->alignment();
  init_params.preferred_width = kHoldingSpaceBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.show_by_click = show_by_click;
  init_params.has_shadow = false;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->SetMaxHeight(CalculateMaxHeight());

  HoldingSpaceBubbleContainer* bubble_container = bubble_view->AddChildView(
      std::make_unique<HoldingSpaceBubbleContainer>());

  // Add pinned files container.
  pinned_files_container_ = bubble_container->AddChildView(
      std::make_unique<PinnedFilesContainer>(&delegate_));
  bubble_container->SetFlexForChild(pinned_files_container_, 1);
  SetupViewLayer(pinned_files_container_);

  // Add recent files container.
  recent_files_container_ = bubble_container->AddChildView(
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

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Mark when holding space was first entered. If this is not the first entry
  // into holding space, this will no-op. If this is the first entry, record the
  // amount of time from first availability to first entry into holding space.
  if (holding_space_prefs::MarkTimeOfFirstEntry(prefs))
    RecordTimeFromFirstAvailabilityToFirstEntry(prefs);

  // Record visible holding space items.
  std::vector<const HoldingSpaceItem*> visible_items;
  FindVisibleHoldingSpaceItems(bubble_view, &visible_items);
  holding_space_metrics::RecordItemCounts(visible_items);
}

HoldingSpaceTrayBubble::~HoldingSpaceTrayBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
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
  const WorkAreaInsets* work_area = WorkAreaInsets::ForWindow(
      holding_space_tray_->shelf()->GetWindow()->GetRootWindow());

  const int bottom =
      holding_space_tray_->shelf()->IsHorizontalAlignment()
          ? holding_space_tray_->shelf()->GetShelfBoundsInScreen().y()
          : work_area->user_work_area_bounds().bottom();

  const int free_space_height_above_anchor =
      bottom - work_area->user_work_area_bounds().y();

  const gfx::Insets insets = GetTrayBubbleInsets();
  const int bubble_vertical_margin = insets.top() + insets.bottom();

  return free_space_height_above_anchor - bubble_vertical_margin;
}

}  // namespace ash
