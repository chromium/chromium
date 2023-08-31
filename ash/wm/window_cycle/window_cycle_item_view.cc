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

// Spacing between the child `WindowCycleItemView`s of the container view.
constexpr int kBetweenCycleItemsSpacing = 2;

}  // namespace

WindowCycleItemView::WindowCycleItemView(aura::Window* window)
    : WindowMiniView(window) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);
}

void WindowCycleItemView::OnMouseEntered(const ui::MouseEvent& event) {
  Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
}

bool WindowCycleItemView::OnMousePressed(const ui::MouseEvent& event) {
  Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
  Shell::Get()->window_cycle_controller()->CompleteCycling();
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
    Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    return true;
  }

  return View::HandleAccessibleAction(action_data);
}

void WindowCycleItemView::RefreshItemVisuals() {
  header_view()->UpdateIconView(source_window());
  SetShowPreview(/*show=*/true);
  RefreshHeaderViewRoundedCorners();
  RefreshPreviewRoundedCorners(/*show=*/true);
}

BEGIN_METADATA(WindowCycleItemView, WindowMiniView)
END_METADATA

GroupContainerCycleView::GroupContainerCycleView(SnapGroup* snap_group) {
  mini_view1_ = AddChildView(
      std::make_unique<WindowCycleItemView>(snap_group->window1()));
  mini_view2_ = AddChildView(
      std::make_unique<WindowCycleItemView>(snap_group->window2()));
  RefreshItemVisuals();

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(michelefan@): Orientation should correspond to the window layout.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kInsideContainerBorderInset, kBetweenCycleItemsSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

GroupContainerCycleView::~GroupContainerCycleView() = default;

bool GroupContainerCycleView::Contains(aura::Window* window) const {
  return (mini_view1_ && mini_view1_->Contains(window)) ||
         (mini_view2_ && mini_view2_->Contains(window));
}

aura::Window* GroupContainerCycleView::GetWindowAtPoint(
    const gfx::Point& screen_point) const {
  for (auto mini_view : {mini_view1_, mini_view2_}) {
    if (!mini_view) {
      continue;
    }
    if (auto* window = mini_view->GetWindowAtPoint(screen_point)) {
      return window;
    }
  }
  return nullptr;
}

void GroupContainerCycleView::RefreshItemVisuals() {
  if (mini_view1_ && mini_view2_) {
    mini_view1_->SetRoundedCornersRadius(gfx::RoundedCornersF(
        /*upper_left=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*upper_right=*/0, /*lower_right=*/0,
        /*lower_left=*/WindowMiniView::kWindowMiniViewCornerRadius));
    mini_view2_->SetRoundedCornersRadius(gfx::RoundedCornersF(
        /*upper_left=*/0,
        /*upper_right=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*lower_right=*/WindowMiniView::kWindowMiniViewCornerRadius,
        /*lower_left=*/0));
  }

  for (auto mini_view : {mini_view1_, mini_view2_}) {
    if (mini_view) {
      mini_view->RefreshItemVisuals();
    }
  }
}

int GroupContainerCycleView::TryRemovingChildItem(
    aura::Window* destroying_window) {
  std::vector<raw_ptr<WindowCycleItemView>*> mini_views_ptrs = {&mini_view1_,
                                                                &mini_view2_};
  for (auto* mini_view_ptr : mini_views_ptrs) {
    if (auto& mini_view = *mini_view_ptr; mini_view) {
      // Explicitly reset the current visuals.
      mini_view->ResetRoundedCorners();
      if (mini_view->Contains(destroying_window)) {
        auto* temp = mini_view.get();
        // Explicitly reset the `mini_view` to avoid dangling pointer detection.
        mini_view = nullptr;
        RemoveChildViewT(temp);
      }
    }
  }

  RefreshItemVisuals();

  return base::ranges::count_if(
      mini_views_ptrs,
      [](raw_ptr<WindowCycleItemView>* ptr) { return *ptr != nullptr; });
}

void GroupContainerCycleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kGroup;
  // TODO(b/297062026): Update the string after been finalized by consulting
  // with a11y team.
  node_data->SetName(u"Group container view");
}

gfx::RoundedCornersF GroupContainerCycleView::GetRoundedCorners() const {
  if (!mini_view1_ && !mini_view2_) {
    return gfx::RoundedCornersF();
  }

  // In normal use cases, the left corners (`upper_left` and `lower_left`) will
  // depend on the primary snapped window, and likewise for the right corners.
  // However, if one window gets destructed leaving only one mini view hosted by
  // this. All the rounded corners have to be from the remaining mini view.
  // TODO(b/294294344): for vertical split view, the upper corners will depend
  // on the primary snapped window and likewise for the lower corners.
  const float upper_left = mini_view1_
                               ? mini_view1_->GetRoundedCorners().upper_left()
                               : mini_view2_->GetRoundedCorners().upper_left();
  const float upper_right =
      mini_view2_ ? mini_view2_->GetRoundedCorners().upper_right()
                  : mini_view1_->GetRoundedCorners().upper_right();
  const float lower_right = mini_view2_
                                ? mini_view2_->GetRoundedCorners().lower_right()
                                : mini_view1_->GetRoundedCorners().lower_left();
  const float lower_left = mini_view1_
                               ? mini_view1_->GetRoundedCorners().lower_left()
                               : mini_view2_->GetRoundedCorners().lower_right();
  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

BEGIN_METADATA(GroupContainerCycleView, WindowMiniViewBase)
END_METADATA

}  // namespace ash
