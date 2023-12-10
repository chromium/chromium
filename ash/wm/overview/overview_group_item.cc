// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_group_item.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_group_container_view.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_util.h"
#include "base/check_op.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Insets values for the individual overview items hosted by the overview group
// item.
constexpr gfx::InsetsF kLeftItemBoundsInsets =
    gfx::InsetsF::TLBR(/*top=*/0, /*left=*/0, /*bottom=*/0, /*right=*/1);
constexpr gfx::InsetsF kRightItemBoundsInsets =
    gfx::InsetsF::TLBR(/*top=*/0, /*left=*/1, /*bottom=*/0, /*right=*/0);

float CalculateSnapRatioForPrimarySnappedWindow(
    const aura::Window* primary_window,
    const aura::Window* secondary_window) {
  const auto primary_window_bounds_in_screen =
      primary_window->GetBoundsInScreen();
  const auto secondary_window_bounds_in_screen =
      secondary_window->GetBoundsInScreen();
  // TODO(http://b/309539997): Calculate differently for portrait screen
  // orientation.
  return static_cast<float>(primary_window_bounds_in_screen.width()) /
         (primary_window_bounds_in_screen.width() +
          secondary_window_bounds_in_screen.width());
}

// TODO(http://b/309539997): Calculate differently for portrait screen
// orientation.
gfx::RectF CalculateScreenBoundsForWindow(const gfx::RectF& union_bounds,
                                          const float primary_snap_ratio,
                                          const bool is_primary) {
  const float snap_ratio =
      is_primary ? primary_snap_ratio : (1 - primary_snap_ratio);
  const gfx::PointF origin =
      is_primary ? union_bounds.origin()
                 : gfx::PointF(union_bounds.origin().x() +
                                   union_bounds.width() * primary_snap_ratio,
                               union_bounds.origin().y());
  gfx::RectF bounds(origin, gfx::SizeF(union_bounds.width() * snap_ratio,
                                       union_bounds.height()));
  bounds.Inset(is_primary ? kLeftItemBoundsInsets : kRightItemBoundsInsets);
  return bounds;
}

}  // namespace

OverviewGroupItem::OverviewGroupItem(const Windows& windows,
                                     OverviewSession* overview_session,
                                     OverviewGrid* overview_grid)
    : OverviewItemBase(overview_session,
                       overview_grid,
                       overview_grid->root_window()) {
  CreateItemWidget();

  CHECK_EQ(windows.size(), 2u);

  const aura::Window* topmost_window = window_util::GetTopMostWindow(windows);
  OverviewItem* bottom_item = nullptr;
  for (auto* window : windows) {
    // Create the overview items hosted by `this`, which will be the delegate to
    // handle the window destroying if the overview representation for the
    // window is hosted by `this`. We also need to explicitly disable the shadow
    // to be installed on individual overview item hosted by `this` as the
    // group-level shadow will be installed instead.
    std::unique_ptr<OverviewItem> overview_item =
        std::make_unique<OverviewItem>(window, overview_session_,
                                       overview_grid_,
                                       /*destruction_delegate=*/this,
                                       /*event_handler_delegate=*/this,
                                       /*eligible_for_shadow_config=*/false);
    if (window != topmost_window) {
      bottom_item = overview_item.get();
    }
    overview_items_.push_back(std::move(overview_item));
  }

  // Explicitly stack the window of the group item widget below the item widget
  // whose window is lower in stacking order so that the `OverviewItemView` will
  // be able to receive the events.
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(
      widget_window, bottom_item->item_widget()->GetNativeWindow());
}

OverviewGroupItem::~OverviewGroupItem() = default;

aura::Window* OverviewGroupItem::GetWindow() {
  // TODO(michelefan): `GetWindow()` will be replaced by `GetWindows()` in a
  // follow-up cl.
  CHECK_LE(overview_items_.size(), 2u);
  return overview_items_.empty() ? nullptr : overview_items_[0]->GetWindow();
}

std::vector<aura::Window*> OverviewGroupItem::GetWindows() {
  std::vector<aura::Window*> windows;
  for (const auto& item : overview_items_) {
    windows.push_back(item->GetWindow());
  }

  return windows;
}

bool OverviewGroupItem::HasVisibleOnAllDesksWindow() {
  for (const auto& item : overview_items_) {
    if (item->HasVisibleOnAllDesksWindow()) {
      return true;
    }
  }

  return false;
}

bool OverviewGroupItem::Contains(const aura::Window* target) const {
  for (const auto& item : overview_items_) {
    if (item->Contains(target)) {
      return true;
    }
  }

  return false;
}

OverviewItem* OverviewGroupItem::GetLeafItemForWindow(aura::Window* window) {
  for (const auto& item : overview_items_) {
    if (item->GetWindow() == window) {
      return item.get();
    }
  }

  return nullptr;
}

void OverviewGroupItem::RestoreWindow(bool reset_transform, bool animate) {
  for (const auto& item : overview_items_) {
    item->RestoreWindow(reset_transform, animate);
  }
}

void OverviewGroupItem::SetBounds(const gfx::RectF& target_bounds,
                                  OverviewAnimationType animation_type) {
  // Run at the exit of this function to `UpdateRoundedCornersAndShadow()`.
  base::ScopedClosureRunner exit_runner(base::BindOnce(
      [](base::WeakPtr<OverviewGroupItem> overview_group_item) {
        if (overview_group_item) {
          overview_group_item->UpdateRoundedCornersAndShadow();
        }
      },
      weak_ptr_factory_.GetWeakPtr()));

  target_bounds_ = target_bounds;

  const int size = overview_items_.size();
  if (size == 1) {
    return overview_items_[0]->SetBounds(target_bounds, animation_type);
  }

  CHECK_EQ(size, 2);
  item_widget_->SetBounds(gfx::ToRoundedRect(target_bounds));

  // TODO(http://b/309539997): Set bounds differently based on the screen
  // orientation.
  const float primary_window_snap_ratio =
      CalculateSnapRatioForPrimarySnappedWindow(
          overview_items_[0]->GetWindow(), overview_items_[1]->GetWindow());

  gfx::RectF sub_bounds1 =
      CalculateScreenBoundsForWindow(target_bounds, primary_window_snap_ratio,
                                     /*is_primary=*/true);
  sub_bounds1.Inset(kLeftItemBoundsInsets);
  gfx::RectF sub_bounds2 =
      CalculateScreenBoundsForWindow(target_bounds, primary_window_snap_ratio,
                                     /*is_primary=*/false);
  sub_bounds2.Inset(kRightItemBoundsInsets);
  overview_items_[0]->SetBounds(sub_bounds1, animation_type);
  overview_items_[1]->SetBounds(sub_bounds2, animation_type);
}

gfx::Transform OverviewGroupItem::ComputeTargetTransform(
    const gfx::RectF& target_bounds) {
  return gfx::Transform();
}

gfx::RectF OverviewGroupItem::GetWindowsUnionScreenBounds() const {
  gfx::RectF target_bounds;
  for (const auto& item : overview_items_) {
    target_bounds.Union(item->GetWindowsUnionScreenBounds());
  }

  return target_bounds;
}

gfx::RectF OverviewGroupItem::GetTargetBoundsWithInsets() const {
  gfx::RectF target_bounds_with_insets = target_bounds_;
  target_bounds_with_insets.Inset(gfx::InsetsF::TLBR(kHeaderHeightDp, 0, 0, 0));
  return target_bounds_with_insets;
}

gfx::RectF OverviewGroupItem::GetTransformedBounds() const {
  // TODO(michelefan): This is a temporary placeholder for the transformed
  // bounds calculation, which needs to be updated when we start working on the
  // actual implementation of this function.
  CHECK_GE(overview_items_.size(), 1u);
  CHECK_LE(overview_items_.size(), 2u);
  return overview_items_[0]->GetTransformedBounds();
}

float OverviewGroupItem::GetItemScale(int height) {
  // TODO(michelefan): This is a temporary placeholder for the item scale
  // calculation, which should be updated when we implement the overview for
  // vertical split screen.
  CHECK(!overview_items_.empty());
  return overview_items_[0]->GetItemScale(height);
}

void OverviewGroupItem::ScaleUpSelectedItem(
    OverviewAnimationType animation_type) {}

void OverviewGroupItem::EnsureVisible() {}

std::vector<OverviewFocusableView*> OverviewGroupItem::GetFocusableViews()
    const {
  std::vector<OverviewFocusableView*> focusable_views;
  for (const auto& overview_item : overview_items_) {
    if (auto* overview_item_view = overview_item->overview_item_view()) {
      focusable_views.push_back(overview_item_view);
    }
  }
  return focusable_views;
}

views::View* OverviewGroupItem::GetBackDropView() const {
  return overview_group_container_view_;
}

void OverviewGroupItem::UpdateRoundedCornersAndShadow() {
  for (const auto& overview_item : overview_items_) {
    overview_item->UpdateRoundedCorners();
  }

  RefreshShadowVisuals(/*shadow_visible=*/true);
}

void OverviewGroupItem::SetOpacity(float opacity) {}

float OverviewGroupItem::GetOpacity() const {
  // TODO(michelefan): This is a temporary placeholder value. The opacity
  // settings will be handled in a separate task.
  return 1.f;
}

void OverviewGroupItem::PrepareForOverview() {
  prepared_for_overview_ = true;
}

void OverviewGroupItem::OnStartingAnimationComplete() {
  for (const auto& item : overview_items_) {
    item->OnStartingAnimationComplete();
  }
}

void OverviewGroupItem::HideForSavedDeskLibrary(bool animate) {}

void OverviewGroupItem::RevertHideForSavedDeskLibrary(bool animate) {}

void OverviewGroupItem::CloseWindows() {
  for (const auto& overview_item : overview_items_) {
    overview_item->CloseWindows();
  }
}

void OverviewGroupItem::Restack() {}

void OverviewGroupItem::StartDrag() {
  for (const auto& item : overview_items_) {
    item->StartDrag();
  }
}

void OverviewGroupItem::OnOverviewItemDragStarted(OverviewItemBase* item) {}

void OverviewGroupItem::OnOverviewItemDragEnded(bool snap) {
}

void OverviewGroupItem::OnOverviewItemContinuousScroll(
    const gfx::Transform& target_transform,
    float scroll_ratio) {}

void OverviewGroupItem::SetVisibleDuringItemDragging(bool visible,
                                                     bool animate) {}

void OverviewGroupItem::UpdateCannotSnapWarningVisibility(bool animate) {}

void OverviewGroupItem::HideCannotSnapWarning(bool animate) {}

void OverviewGroupItem::OnMovingItemToAnotherDesk() {
  is_moving_to_another_desk_ = true;

  for (const auto& overview_item : overview_items_) {
    overview_item->OnMovingItemToAnotherDesk();
  }
}

void OverviewGroupItem::UpdateMirrorsForDragging(bool is_touch_dragging) {}

void OverviewGroupItem::DestroyMirrorsForDragging() {}

void OverviewGroupItem::Shutdown() {}

void OverviewGroupItem::AnimateAndCloseItem(bool up) {}

void OverviewGroupItem::StopWidgetAnimation() {}

OverviewGridWindowFillMode OverviewGroupItem::GetWindowDimensionsType() const {
  // This return value assumes that the snap group represented by `this` will
  // occupy the entire work space. So it's mostly likely that the window
  // dimension type will be normal.
  // TODO(michelefan): Consider the corner cases when the work space has
  // abnormal dimension ratios.
  return OverviewGridWindowFillMode::kNormal;
}

void OverviewGroupItem::UpdateWindowDimensionsType() {}

gfx::Point OverviewGroupItem::GetMagnifierFocusPointInScreen() const {
  return overview_group_container_view_->GetBoundsInScreen().CenterPoint();
}

const gfx::RoundedCornersF OverviewGroupItem::GetRoundedCorners() const {
  // TODO(michelefan): Return a different set of rounded corners for vertical
  // split screen.
  const gfx::RoundedCornersF& front_rounded_corners =
      overview_items_.front()->GetRoundedCorners();
  const gfx::RoundedCornersF& back_rounded_corners =
      overview_items_.back()->GetRoundedCorners();
  return gfx::RoundedCornersF(
      front_rounded_corners.upper_left(), back_rounded_corners.upper_right(),
      back_rounded_corners.lower_right(), front_rounded_corners.lower_left());
}

void OverviewGroupItem::OnOverviewItemWindowDestroying(
    OverviewItem* overview_item,
    bool reposition) {
  // We use 2-step removal to ensure that the `overview_item` gets removed from
  // the vector before been destroyed so that all the overview items in
  // `overview_items_` are valid.
  auto iter = base::ranges::find_if(overview_items_,
                                    base::MatchesUniquePtr(overview_item));
  auto to_be_removed = std::move(*iter);
  overview_items_.erase(iter);
  to_be_removed.reset();

  if (overview_items_.empty()) {
    overview_grid_->RemoveItem(this, /*item_destroying=*/true, reposition);
    return;
  }

  overview_grid_->PositionWindows(/*animate=*/false);
  for (const auto& item : overview_items_) {
    if (item && item.get() != overview_item) {
      OverviewItemView* item_view = item->overview_item_view();
      item_view->ResetRoundedCorners();
      item_view->RefreshItemVisuals();
    }
  }
}

void OverviewGroupItem::HandleDragEvent(const gfx::PointF& location_in_screen) {
  if (IsDragItem()) {
    overview_session_->Drag(this, location_in_screen);
  }
}

void OverviewGroupItem::CreateItemWidget() {
  TRACE_EVENT0("ui", "OverviewGroupItem::CreateItemWidget");

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(CreateOverviewItemWidgetParams(
      desks_util::GetActiveDeskContainerForRoot(overview_grid_->root_window()),
      "OverviewGroupItemWidget", /*accept_events=*/true));

  ConfigureTheShadow();

  overview_group_container_view_ = item_widget_->SetContentsView(
      std::make_unique<OverviewGroupContainerView>(this));
  item_widget_->Show();
  item_widget_->GetLayer()->SetMasksToBounds(/*masks_to_bounds=*/false);
}

}  // namespace ash
