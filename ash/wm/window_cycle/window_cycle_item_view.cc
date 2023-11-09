// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_item_view.h"

#include <algorithm>
#include <memory>

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "base/containers/cxx20_erase_vector.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The min and max width for preview size are in relation to the fixed height.
constexpr int kMinPreviewWidthDp =
    WindowCycleItemView::kFixedPreviewHeightDp / 2;
constexpr int kMaxPreviewWidthDp =
    WindowCycleItemView::kFixedPreviewHeightDp * 2;

// The border padding value of the container view.
constexpr auto kInsideContainerBorderInset = gfx::Insets(2);

// Spacing between the `WindowCycleItemView`s hosted by the container view.
constexpr int kBetweenCycleItemsSpacing = 2;

}  // namespace

WindowCycleItemView::WindowCycleItemView(aura::Window* window)
    : WindowMiniView(window),
      window_cycle_controller_(Shell::Get()->window_cycle_controller()) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);

  // The parent of these views is not drawn due to its size, so we need to need
  // to make this a layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void WindowCycleItemView::OnMouseEntered(const ui::MouseEvent& event) {
  window_cycle_controller_->SetFocusedWindow(source_window());
}

bool WindowCycleItemView::OnMousePressed(const ui::MouseEvent& event) {
  window_cycle_controller_->SetFocusedWindow(source_window());
  window_cycle_controller_->CompleteCycling();
  return true;
}

gfx::Size WindowCycleItemView::GetPreviewViewSize() const {
  // When the preview is not shown, do an estimate of the expected size.
  // |this| will not be visible anyways, and will get corrected once
  // ShowPreview() is called.
  if (!preview_view()) {
    gfx::SizeF source_size(source_window()->bounds().size());
    // Windows may have no size in tests.
    if (source_size.IsEmpty())
      return gfx::Size();
    const float aspect_ratio = source_size.width() / source_size.height();
    return gfx::Size(kFixedPreviewHeightDp * aspect_ratio,
                     kFixedPreviewHeightDp);
  }

  // Returns the size for the preview view, scaled to fit within the max
  // bounds. Scaling is always 1:1 and we only scale down, never up.
  gfx::Size preview_pref_size = preview_view()->GetPreferredSize();
  if (preview_pref_size.width() > kMaxPreviewWidthDp ||
      preview_pref_size.height() > kFixedPreviewHeightDp) {
    const float scale = std::min(
        kMaxPreviewWidthDp / static_cast<float>(preview_pref_size.width()),
        kFixedPreviewHeightDp / static_cast<float>(preview_pref_size.height()));
    preview_pref_size =
        gfx::ScaleToRoundedSize(preview_pref_size, scale, scale);
  }

  return preview_pref_size;
}

void WindowCycleItemView::Layout() {
  WindowMiniView::Layout();

  if (!preview_view())
    return;

  // Show the backdrop if the preview view does not take up all the bounds
  // allocated for it.
  gfx::Rect preview_max_bounds = GetContentsBounds();
  preview_max_bounds.Subtract(GetHeaderBounds());
  const gfx::Rect preview_area_bounds = preview_view()->bounds();
  SetBackdropVisibility(preview_max_bounds.size() !=
                        preview_area_bounds.size());
}

gfx::Size WindowCycleItemView::CalculatePreferredSize() const {
  // Previews can range in width from half to double of
  // |kFixedPreviewHeightDp|. Padding will be added to the
  // sides to achieve this if the preview is too narrow.
  gfx::Size preview_size = GetPreviewViewSize();

  // All previews are the same height (this may add padding on top and
  // bottom).
  preview_size.set_height(kFixedPreviewHeightDp);

  // Previews should never be narrower than half or wider than double their
  // fixed height.
  preview_size.set_width(
      std::clamp(preview_size.width(), kMinPreviewWidthDp, kMaxPreviewWidthDp));

  const int margin = GetInsets().width();
  preview_size.Enlarge(margin, margin + WindowMiniView::kHeaderHeightDp);
  return preview_size;
}

bool WindowCycleItemView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  // Since this class destroys itself on mouse press, and
  // View::HandleAccessibleAction calls OnEvent twice (first with a mouse press
  // event, then with a mouse release event), override the base impl from
  // triggering that behavior which leads to a UAF.
  if (action_data.action == ax::mojom::Action::kDoDefault) {
    window_cycle_controller_->SetFocusedWindow(source_window());
    window_cycle_controller_->CompleteCycling();
    return true;
  }

  return View::HandleAccessibleAction(action_data);
}

void WindowCycleItemView::RefreshItemVisuals() {
  header_view()->UpdateIconView(source_window());
  RefreshHeaderViewRoundedCorners();
  RefreshPreviewRoundedCorners(/*show=*/true);
}

BEGIN_METADATA(WindowCycleItemView, WindowMiniView)
END_METADATA

GroupContainerCycleView::GroupContainerCycleView(SnapGroup* snap_group) {
  mini_views_.push_back(AddChildView(
      std::make_unique<WindowCycleItemView>(snap_group->window1())));
  mini_views_.push_back(AddChildView(
      std::make_unique<WindowCycleItemView>(snap_group->window2())));
  SetShowPreview(/*show=*/true);
  RefreshItemVisuals();

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(michelefan): Window layout should correspond to screen orientation.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kInsideContainerBorderInset, kBetweenCycleItemsSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

GroupContainerCycleView::~GroupContainerCycleView() = default;

bool GroupContainerCycleView::Contains(aura::Window* window) const {
  return base::ranges::any_of(mini_views_,
                              [window](const WindowCycleItemView* mini_view) {
                                return mini_view->Contains(window);
                              });
}

aura::Window* GroupContainerCycleView::GetWindowAtPoint(
    const gfx::Point& screen_point) const {
  for (auto* mini_view : mini_views_) {
    if (auto* window = mini_view->GetWindowAtPoint(screen_point)) {
      return window;
    }
  }
  return nullptr;
}

void GroupContainerCycleView::SetShowPreview(bool show) {
  for (auto* mini_view : mini_views_) {
    mini_view->SetShowPreview(show);
  }
}

void GroupContainerCycleView::RefreshItemVisuals() {
  if (mini_views_.size() == 2u) {
    mini_views_[0]->SetRoundedCornersRadius(gfx::RoundedCornersF(
        /*upper_left=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*upper_right=*/0, /*lower_right=*/0,
        /*lower_left=*/WindowMiniView::kWindowMiniViewCornerRadius));
    mini_views_[1]->SetRoundedCornersRadius(gfx::RoundedCornersF(
        /*upper_left=*/0,
        /*upper_right=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*lower_right=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*lower_left=*/0));
  }

  for (auto* mini_view : mini_views_) {
    mini_view->RefreshItemVisuals();
  }
}

int GroupContainerCycleView::TryRemovingChildItem(
    aura::Window* destroying_window) {
  for (auto* mini_view : mini_views_) {
    // Explicitly reset the current visuals so that the default rounded corners
    // i.e. rounded corners on four corners will be applied on the remaining
    // item.
    mini_view->ResetRoundedCorners();
    if (mini_view->Contains(destroying_window)) {
      base::Erase(mini_views_, mini_view);
      RemoveChildViewT(mini_view);
    }
  }

  RefreshItemVisuals();
  return mini_views_.size();
}

void GroupContainerCycleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kGroup;
  // TODO(b/297062026): Update the string after been finalized by consulting
  // with a11y team.
  node_data->SetName(u"Group container view");
}

gfx::RoundedCornersF GroupContainerCycleView::GetRoundedCorners() const {
  if (mini_views_.empty()) {
    return gfx::RoundedCornersF();
  }

  if (mini_views_.size() == 1u) {
    return mini_views_[0]->GetRoundedCorners();
  }

  CHECK_EQ(mini_views_.size(), 2u);
  // The left corners (`upper_left` and `lower_left`) will depend on the primary
  // snapped window, and likewise for the right corners.
  // TODO(http://b/294294344): for vertical split view, the upper corners will
  // depend on the primary snapped window and likewise for the lower corners.
  const float upper_left = mini_views_[0]->GetRoundedCorners().upper_left();
  const float upper_right = mini_views_[1]->GetRoundedCorners().upper_right();
  const float lower_right = mini_views_[1]->GetRoundedCorners().lower_right();
  const float lower_left = mini_views_[0]->GetRoundedCorners().lower_left();
  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

void GroupContainerCycleView::UpdateFocusState(bool focus) {
  for (auto* mini_view : mini_views_) {
    mini_view->UpdateFocusState(focus);
  }
}

BEGIN_METADATA(GroupContainerCycleView, WindowMiniViewBase)
END_METADATA

}  // namespace ash
