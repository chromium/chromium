// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_group_item.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/style/rounded_label_widget.h"
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
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "base/check_op.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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

  const aura::Window* topmost_window = window_util::GetTopMostWindow(windows);
  OverviewItem* bottom_item = nullptr;
  for (aura::Window* window : windows) {
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

void OverviewGroupItem::SetOpacity(float opacity) {
  OverviewItemBase::SetOpacity(opacity);

  for (const auto& overview_item : overview_items_) {
    overview_item->SetOpacity(opacity);
  }
}

aura::Window::Windows OverviewGroupItem::GetWindowsForHomeGesture() {
  aura::Window::Windows windows = OverviewItemBase::GetWindowsForHomeGesture();

  for (const auto& overview_item : overview_items_) {
    aura::Window::Windows item_windows =
        overview_item->GetWindowsForHomeGesture();
    windows.insert(windows.end(), item_windows.begin(), item_windows.end());
  }

  return windows;
}

void OverviewGroupItem::HideForSavedDeskLibrary(bool animate) {
  for (const auto& item : overview_items_) {
    item->HideForSavedDeskLibrary(animate);
  }

  OverviewItemBase::HideForSavedDeskLibrary(animate);
}

void OverviewGroupItem::RevertHideForSavedDeskLibrary(bool animate) {
  for (const auto& item : overview_items_) {
    item->RevertHideForSavedDeskLibrary(animate);
  }

  OverviewItemBase::RevertHideForSavedDeskLibrary(animate);
}

void OverviewGroupItem::UpdateMirrorsForDragging(bool is_touch_dragging) {
  // TODO(http://b/339516036): Revisit whether we should update mirror for the
  // group's `item_widget_` after the blue background issue is resolved.
  for (const auto& overview_item : overview_items_) {
    overview_item->UpdateMirrorsForDragging(is_touch_dragging);
  }
}

void OverviewGroupItem::DestroyMirrorsForDragging() {
  // TODO(http://b/339516036): Revisit whether we should destroy mirror for the
  // group's `item_widget_` after the blue background issue is resolved.
  for (const auto& overview_item : overview_items_) {
    overview_item->DestroyMirrorsForDragging();
  }
}

aura::Window* OverviewGroupItem::GetWindow() {
  // TODO(michelefan): `GetWindow()` will be replaced by `GetWindows()` in a
  // follow-up cl.
  CHECK_LE(overview_items_.size(), 2u);
  return overview_items_.empty() ? nullptr : overview_items_[0]->GetWindow();
}

std::vector<raw_ptr<aura::Window, VectorExperimental>>
OverviewGroupItem::GetWindows() {
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows;
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
  // TODO(dcheng): This can probably just capture `this`.
  absl::Cleanup exit_runner = [overview_group_item =
                                   weak_ptr_factory_.GetWeakPtr()] {
    CHECK(overview_group_item);
    overview_group_item->UpdateRoundedCornersAndShadow();
  };

  target_bounds_ = target_bounds;

  const int size = overview_items_.size();
  auto& item0 = overview_items_[0];
  if (size == 1) {
    return item0->SetBounds(target_bounds, animation_type);
  }

  CHECK_EQ(size, 2);
  auto& item1 = overview_items_[1];

  aura::Window* item0_window = item0->GetWindow();
  aura::Window* item1_window = item1->GetWindow();
  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(item0_window)
                                  .work_area();
  const bool is_horizontal = IsLayoutHorizontal(item0_window);
  item_widget_->SetBounds(gfx::ToRoundedRect(target_bounds));

  if (is_horizontal) {
    // Calculate the ratio that reflects how much the windows' widths should be
    // scaled to fit within `target_bounds`.
    const float ratio =
        static_cast<float>(target_bounds.width()) / work_area.width();
    auto item0_bounds = target_bounds;
    item0_bounds.set_width(ratio * item0_window->bounds().width());
    item0->SetBounds(item0_bounds, animation_type);

    const auto item1_width = ratio * item1_window->bounds().width();
    auto item1_bounds = target_bounds;
    item1_bounds.set_width(item1_width);
    item1_bounds.set_x(target_bounds.right() - item1_width);
    item1->SetBounds(item1_bounds, animation_type);

    return;
  }

  const float ratio =
      static_cast<float>(target_bounds.height()) / work_area.height();
  auto item0_bounds = target_bounds;
  item0_bounds.set_height(ratio * item0_window->bounds().height());
  item0->SetBounds(item0_bounds, animation_type);

  const auto item1_height = ratio * item1_window->bounds().height();
  auto item1_bounds = target_bounds;
  item1_bounds.set_height(item1_height);
  item1_bounds.set_y(target_bounds.bottom() - item1_height);
  item1->SetBounds(item1_bounds, animation_type);
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
  target_bounds_with_insets.Inset(
      gfx::InsetsF::TLBR(kWindowMiniViewHeaderHeight, 0, 0, 0));
  return target_bounds_with_insets;
}

gfx::RectF OverviewGroupItem::GetTransformedBounds() const {
  return GetWindowsUnionScreenBounds();
}

float OverviewGroupItem::GetItemScale(int height) {
  CHECK(!overview_items_.empty());

  // Calculate the scaling factor for the group item:
  //
  // For horizontal window layout, the title and item height remain consistent
  // across all items in the group. The larger of the two windows'
  // `kTopViewInset` properties is applied for the calculation.
  //
  //     +--------------------++--------------------+
  //     | Window 0 Header    || Window 1 Header    |
  //     +--------------------++--------------------|
  //     |                    ||                    |
  //     | Window 0 Preview   || Window 1 Preview   |
  //     |                    ||                    |
  //     +--------------------++--------------------+
  //
  // In a vertical window layout, double the fixed header height
  // (`kWindowMiniViewHeaderHeight`) and apply the sum of both windows'
  // `kTopViewInset` properties for the calculation.
  //
  //     +--------------------+
  //     | Window 0 Header    |
  //     +--------------------+
  //     | Window 0 Preview   |
  //     |                    |
  //     +--------------------+
  //     +--------------------+
  //     | Window 1 Header    |
  //     +--------------------+
  //     | Window 1 Preview   |
  //     |                    |
  //     +--------------------+

  const bool is_layout_horizontal =
      IsLayoutHorizontal(overview_items_[0]->GetWindow());

  int top_inset = 0;
  for (const auto& overview_item : overview_items_) {
    const int item_top_inset = overview_item->GetTopInset();
    if (is_layout_horizontal) {
      top_inset = std::max(item_top_inset, top_inset);
    } else {
      top_inset += item_top_inset;
    }
  }

  return ScopedOverviewTransformWindow::GetItemScale(
      /*source_height=*/GetWindowsUnionScreenBounds().height(),
      /*target_height=*/height,
      /*top_view_inset=*/top_inset,
      /*title_height=*/
      is_layout_horizontal ? kWindowMiniViewHeaderHeight
                           : 2 * kWindowMiniViewHeaderHeight);
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

bool OverviewGroupItem::ShouldHaveShadow() const {
  return overview_items_.size() > 1u;
}

void OverviewGroupItem::UpdateRoundedCornersAndShadow() {
  for (const auto& overview_item : overview_items_) {
    overview_item->UpdateRoundedCorners();
  }

  RefreshShadowVisuals(/*shadow_visible=*/true);
}

float OverviewGroupItem::GetOpacity() const {
  // TODO(michelefan): This is a temporary placeholder value. The opacity
  // settings will be handled in a separate task.
  return 1.f;
}

void OverviewGroupItem::PrepareForOverview() {
  prepared_for_overview_ = true;
}

void OverviewGroupItem::SetShouldUseSpawnAnimation(bool value) {
  for (const auto& item : overview_items_) {
    item->SetShouldUseSpawnAnimation(value);
  }

  should_use_spawn_animation_ = value;
}

void OverviewGroupItem::OnStartingAnimationComplete() {
  for (const auto& item : overview_items_) {
    item->OnStartingAnimationComplete();
  }
}

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

void OverviewGroupItem::OnOverviewItemDragStarted() {
  for (const auto& item : overview_items_) {
    item->OnOverviewItemDragStarted();
  }
}

void OverviewGroupItem::OnOverviewItemDragEnded(bool snap) {
  for (const auto& item : overview_items_) {
    item->OnOverviewItemDragEnded(snap);
  }
}

void OverviewGroupItem::OnOverviewItemContinuousScroll(
    const gfx::Transform& target_transform,
    float scroll_ratio) {}

void OverviewGroupItem::UpdateCannotSnapWarningVisibility(bool animate) {}

void OverviewGroupItem::HideCannotSnapWarning(bool animate) {}

void OverviewGroupItem::OnMovingItemToAnotherDesk() {
  is_moving_to_another_desk_ = true;

  for (const auto& overview_item : overview_items_) {
    overview_item->OnMovingItemToAnotherDesk();
  }
}

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
  auto& item0 = overview_items_.front();
  const gfx::RoundedCornersF& primary_rounded_corners =
      item0->GetRoundedCorners();
  const gfx::RoundedCornersF& secondary_rounded_corners =
      overview_items_.back()->GetRoundedCorners();
  return IsLayoutHorizontal(item0->GetWindow())
             ? gfx::RoundedCornersF(primary_rounded_corners.upper_left(),
                                    secondary_rounded_corners.upper_right(),
                                    secondary_rounded_corners.lower_right(),
                                    primary_rounded_corners.lower_left())
             : gfx::RoundedCornersF(primary_rounded_corners.upper_left(),
                                    primary_rounded_corners.upper_right(),
                                    secondary_rounded_corners.lower_right(),
                                    secondary_rounded_corners.lower_left());
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

  for (const auto& item : overview_items_) {
    if (item && item.get() != overview_item) {
      // Remove the group-level shadow and apply it on the window-level to
      // ensure that the shadow bounds get updated properly.
      item->set_eligible_for_shadow_config(/*eligible_for_shadow_config=*/true);

      OverviewItemView* item_view = item->overview_item_view();
      item_view->ResetRoundedCorners();
    }
  }

  overview_grid_->PositionWindows(/*animate=*/true);
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
      "OverviewGroupItemWidget", /*accept_events=*/false));

  CreateShadow();

  overview_group_container_view_ = item_widget_->SetContentsView(
      std::make_unique<OverviewGroupContainerView>(this));
  item_widget_->Show();
  item_widget_->GetLayer()->SetMasksToBounds(/*masks_to_bounds=*/false);
}

}  // namespace ash
