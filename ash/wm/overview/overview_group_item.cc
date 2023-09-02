// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_group_item.h"

#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_group_container_view.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/window_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverviewGroupItem::OverviewGroupItem(const Windows& windows,
                                     OverviewSession* overview_session,
                                     OverviewGrid* overview_grid)
    : OverviewItemBase(overview_session,
                       overview_grid,
                       overview_grid->root_window()) {
  CreateItemWidget();

  CHECK_EQ(windows.size(), 2u);
  for (auto* window : windows) {
    // Create the overview items hosted by `this`.
    overview_items_.push_back(std::make_unique<OverviewItem>(
        window, overview_session_, overview_grid_));
  }
}

OverviewGroupItem::~OverviewGroupItem() = default;

aura::Window* OverviewGroupItem::GetWindow() {
  CHECK_GE(overview_items_.size(), 1u);
  CHECK_LE(overview_items_.size(), 2u);
  return overview_items_[0]->GetWindow();
}

std::vector<aura::Window*> OverviewGroupItem::GetWindows() {
  std::vector<aura::Window*> windows;
  for (auto& item : overview_items_) {
    windows.push_back(item->GetWindow());
  }

  return windows;
}

bool OverviewGroupItem::Contains(const aura::Window* target) const {
  for (auto& item : overview_items_) {
    if (item->Contains(target)) {
      return true;
    }
  }

  return false;
}

OverviewItem* OverviewGroupItem::GetLeafItemForWindow(aura::Window* window) {
  for (auto& item : overview_items_) {
    if (item->GetWindow() == window) {
      return item.get();
    }
  }

  return nullptr;
}

void OverviewGroupItem::RestoreWindow(bool reset_transform, bool animate) {}

void OverviewGroupItem::SetBounds(const gfx::RectF& target_bounds,
                                  OverviewAnimationType animation_type) {}

gfx::RectF OverviewGroupItem::GetTargetBoundsInScreen() const {
  gfx::RectF target_bounds;
  for (auto& item : overview_items_) {
    target_bounds.Union(item->GetTargetBoundsInScreen());
  }

  return target_bounds;
}

gfx::RectF OverviewGroupItem::GetWindowTargetBoundsWithInsets() const {
  // TODO(b/295067835): `target_bounds_` will be updated when we start working
  // on the actual implementation of `SetBounds()`.
  gfx::RectF item_target_bounds = target_bounds_;
  item_target_bounds.Inset(gfx::InsetsF::TLBR(kHeaderHeightDp, 0, 0, 0));
  return item_target_bounds;
}

gfx::RectF OverviewGroupItem::GetTransformedBounds() const {
  // TODO(michelefan): This is a temporary placeholder for the transformed
  // bounds calculation, which needs to be updated when we start working on the
  // actual implementation of this function.
  CHECK_GE(overview_items_.size(), 1u);
  CHECK_LE(overview_items_.size(), 2u);
  return overview_items_.at(0)->GetTransformedBounds();
}

float OverviewGroupItem::GetItemScale(const gfx::Size& size) {
  // TODO(michelefan): This is a temporary placeholder for the item scale
  // calculation, which needs to be updated when we start working on the actual
  // implementation of this function.
  return overview_items_.at(0)->GetItemScale(size);
}

void OverviewGroupItem::ScaleUpSelectedItem(
    OverviewAnimationType animation_type) {}

void OverviewGroupItem::EnsureVisible() {}

OverviewFocusableView* OverviewGroupItem::GetFocusableView() const {
  return overview_group_container_view_;
}

views::View* OverviewGroupItem::GetBackDropView() const {
  return overview_group_container_view_;
}

void OverviewGroupItem::UpdateRoundedCornersAndShadow() {}

void OverviewGroupItem::SetShadowBounds(
    absl::optional<gfx::RectF> bounds_in_screen) {}

void OverviewGroupItem::SetOpacity(float opacity) {}

float OverviewGroupItem::GetOpacity() const {
  // TODO(michelefan): This is a temporary placeholder value. The opacity
  // settings will be handled in a separate task.
  return 1.f;
}

void OverviewGroupItem::PrepareForOverview() {}

void OverviewGroupItem::OnStartingAnimationComplete() {}

void OverviewGroupItem::HideForSavedDeskLibrary(bool animate) {}

void OverviewGroupItem::RevertHideForSavedDeskLibrary(bool animate) {}

void OverviewGroupItem::CloseWindow() {}

void OverviewGroupItem::Restack() {}

void OverviewGroupItem::HandleMouseEvent(const ui::MouseEvent& event) {}

void OverviewGroupItem::HandleGestureEvent(ui::GestureEvent* event) {}

void OverviewGroupItem::OnFocusedViewActivated() {}

void OverviewGroupItem::OnFocusedViewClosed() {}

void OverviewGroupItem::OnOverviewItemDragStarted(OverviewItemBase* item) {}

void OverviewGroupItem::OnOverviewItemDragEnded(bool snap) {}

void OverviewGroupItem::OnOverviewItemContinuousScroll(
    const gfx::RectF& target_bouns,
    bool first_scroll,
    float scroll_ratio) {}

void OverviewGroupItem::SetVisibleDuringItemDragging(bool visible,
                                                     bool animate) {}

void OverviewGroupItem::UpdateShadowTypeForDrag(bool is_dragging) {}

void OverviewGroupItem::UpdateCannotSnapWarningVisibility(bool animate) {}

void OverviewGroupItem::HideCannotSnapWarning(bool animate) {}

void OverviewGroupItem::OnMovingItemToAnotherDesk() {}

void OverviewGroupItem::UpdateMirrorsForDragging(bool is_touch_dragging) {}

void OverviewGroupItem::DestroyMirrorsForDragging() {}

void OverviewGroupItem::Shutdown() {}

void OverviewGroupItem::AnimateAndCloseItem(bool up) {}

void OverviewGroupItem::StopWidgetAnimation() {}

OverviewGridWindowFillMode OverviewGroupItem::GetWindowDimensionsType() const {
  // This return value assumes that the snap group represented by this will
  // occupy the entire work space. So it's mostly likely that the window
  // dimension type will be normal.
  // TODO(michelefan): Consider the corner cases when the work space has
  // abnormal dimension ratios.
  return OverviewGridWindowFillMode::kNormal;
}

void OverviewGroupItem::UpdateWindowDimensionsType() {}

gfx::Point OverviewGroupItem::GetMagnifierFocusPointInScreen() const {
  return overview_group_container_view_->GetMagnifierFocusPointInScreen();
}

void OverviewGroupItem::CreateItemWidget() {
  TRACE_EVENT0("ui", "OverviewGroupItem::CreateItemWidget");

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(CreateOverviewItemWidgetParams(
      desks_util::GetActiveDeskContainerForRoot(overview_grid_->root_window()),
      "OverviewGroupItemWidget"));

  ConfigureTheShadow();

  overview_group_container_view_ = item_widget_->SetContentsView(
      std::make_unique<OverviewGroupContainerView>(this));
  item_widget_->Show();
  item_widget_->SetOpacity(
      overview_session_ && overview_session_->ShouldEnterWithoutAnimations()
          ? 1.f
          : 0.f);
  item_widget_->GetLayer()->SetMasksToBounds(false);
}

}  // namespace ash
