// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_item_view.h"

#include <algorithm>
#include <memory>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Spacing between the `WindowCycleItemView`s hosted by the container view.
constexpr int kBetweenCycleItemsSpacing = 4;

// Fixed preview height to windows in portrait-oriented snap layouts.
constexpr int kFixedPreviewHeightForVerticalSnapGroupDp =
    (WindowCycleItemView::kFixedPreviewHeightDp - kWindowMiniViewHeaderHeight -
     kBetweenCycleItemsSpacing) /
    2;

// The border padding value of the container view.
constexpr auto kInsideContainerBorderInset = gfx::Insets(2);

// Returns true if the given `window` belongs to a snap group with a vertical
// split layout.
bool IsWindowInVerticalSnapGroup(const aura::Window* window) {
  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window);
        snap_group && !snap_group->IsSnapGroupLayoutHorizontal()) {
      return true;
    }
  }

  return false;
}

// Calculates fixed preview height for `window`. In vertical snap groups,
// applies `kFixedPreviewHeightForVerticalSnapGroupDp` to maintain equal height
// with other item preview views.
int GetPreviewFixedHeight(const aura::Window* window) {
  return IsWindowInVerticalSnapGroup(window)
             ? kFixedPreviewHeightForVerticalSnapGroupDp
             : WindowCycleItemView::kFixedPreviewHeightDp;
}

}  // namespace

WindowCycleItemView::WindowCycleItemView(aura::Window* window)
    : WindowMiniView(window, /*use_custom_focus_predicate=*/true),
      window_cycle_controller_(Shell::Get()->window_cycle_controller()) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);

  // The parent of these views is not drawn due to its size, so we need to need
  // to make this a layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

WindowCycleItemView::~WindowCycleItemView() = default;

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
  const int preview_view_height = GetPreviewFixedHeight(source_window());
  const int max_preview_width = 2 * preview_view_height;
  if (preview_pref_size.width() > max_preview_width ||
      preview_pref_size.height() > kFixedPreviewHeightDp) {
    const float scale = std::min(
        max_preview_width / static_cast<float>(preview_pref_size.width()),
        kFixedPreviewHeightDp / static_cast<float>(preview_pref_size.height()));
    preview_pref_size =
        gfx::ScaleToRoundedSize(preview_pref_size, scale, scale);
  }

  return preview_pref_size;
}

void WindowCycleItemView::Layout(PassKey) {
  LayoutSuperclass<WindowMiniView>(this);

  if (!preview_view())
    return;

  // Show the backdrop if the preview view does not take up all the bounds
  // allocated for it.
  gfx::Rect preview_max_bounds = GetContentsBounds();
  preview_max_bounds.Subtract(GetHeaderBounds());
  const gfx::Rect preview_area_bounds = preview_view()->bounds();
  SetBackdropVisibility(preview_max_bounds.size() !=
                        preview_area_bounds.size());

  if (!chromeos::features::IsRoundedWindowsEnabled()) {
    return;
  }

  if (!layer_tree_synchronizer_) {
    layer_tree_synchronizer_ = std::make_unique<ScopedLayerTreeSynchronizer>(
        layer(), /*restore_tree=*/false);
  }

  // In order to draw the final result without requiring the rendering of
  // surfaces, the rounded corners bounds of the layer tree, that is rooted at
  // WindowCycleItemView, are synchronized.
  // Since the rounded corners of the WindowPreviewView layer may overlap with
  // those of the mirrored window (as well as its mirrored transient windows),
  // and the overlapping corners might have different radii, the use of render
  // surfaces would be necessary. However, by matching (synchronizing) the
  // radii, the need for render surfaces is eliminated.
  layer_tree_synchronizer_->SynchronizeRoundedCorners(
      layer(),
      gfx::RRectF(gfx::RectF(preview_max_bounds),
                  window_util::GetMiniWindowRoundedCorners(
                      source_window(), /*include_header_rounding=*/false)));
}

gfx::Size WindowCycleItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Previews can range in width from half to double of
  // |kFixedPreviewHeightDp|. Padding will be added to the
  // sides to achieve this if the preview is too narrow.
  gfx::Size preview_size = GetPreviewViewSize();

  const int preview_height = GetPreviewFixedHeight(source_window());

  // All previews are the same height (this may add padding on top and
  // bottom).
  preview_size.set_height(preview_height);

  // Previews should never be narrower than half or wider than double their
  // fixed height.
  const int min_preview_width = preview_height / 2;
  const int max_preview_width = preview_height * 2;
  preview_size.set_width(
      std::clamp(preview_size.width(), min_preview_width, max_preview_width));

  const int margin = GetInsets().width();
  preview_size.Enlarge(margin, margin + kWindowMiniViewHeaderHeight);
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
  RefreshPreviewRoundedCorners();
  RefreshFocusRingVisuals();
}

BEGIN_METADATA(WindowCycleItemView)
END_METADATA

GroupContainerCycleView::GroupContainerCycleView(SnapGroup* snap_group)
    : is_layout_horizontal_(snap_group->IsSnapGroupLayoutHorizontal()) {
  mini_views_.push_back(AddChildView(std::make_unique<WindowCycleItemView>(
      snap_group->GetPhysicallyLeftOrTopWindow())));
  mini_views_.push_back(AddChildView(std::make_unique<WindowCycleItemView>(
      snap_group->GetPhysicallyRightOrBottomWindow())));
  SetShowPreview(/*show=*/true);
  RefreshItemVisuals();

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          is_layout_horizontal_ ? views::BoxLayout::Orientation::kHorizontal
                                : views::BoxLayout::Orientation::kVertical,
          kInsideContainerBorderInset, kBetweenCycleItemsSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetDescription(
      l10n_util::GetStringUTF16(IDS_ASH_SNAP_GROUP_WINDOW_CYCLE_DESCRIPTION));
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
  for (WindowCycleItemView* mini_view : mini_views_) {
    if (auto* window = mini_view->GetWindowAtPoint(screen_point)) {
      return window;
    }
  }
  return nullptr;
}

void GroupContainerCycleView::SetShowPreview(bool show) {
  for (WindowCycleItemView* mini_view : mini_views_) {
    mini_view->SetShowPreview(show);
  }
}

void GroupContainerCycleView::RefreshItemVisuals() {
  if (mini_views_.size() == 2u) {
    mini_views_[0]->SetRoundedCornersRadius(
        window_util::GetMiniWindowRoundedCorners(
            mini_views_[0]->source_window(), /*include_header_rounding=*/true));
    mini_views_[1]->SetRoundedCornersRadius(
        window_util::GetMiniWindowRoundedCorners(
            mini_views_[1]->source_window(),
            /*include_header_rounding=*/true));
  }

  for (WindowCycleItemView* mini_view : mini_views_) {
    mini_view->RefreshItemVisuals();
  }
}

int GroupContainerCycleView::TryRemovingChildItem(
    aura::Window* destroying_window) {
  for (auto it = mini_views_.begin(); it != mini_views_.end();) {
    // Explicitly reset the current visuals so that the default rounded
    // corners i.e. rounded corners on four corners will be applied on the
    // remaining item.
    (*it)->ResetRoundedCorners();
    if ((*it)->Contains(destroying_window)) {
      RemoveChildViewT(*it);
      it = mini_views_.erase(it);
    } else {
      ++it;
    }
  }

  RefreshItemVisuals();
  return mini_views_.size();
}

void GroupContainerCycleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  for (WindowCycleItemView* mini_view : mini_views_) {
    if (mini_view->is_mini_view_focused()) {
      mini_view->GetViewAccessibility().GetAccessibleNodeData(node_data);
      break;
    }
  }
}

gfx::RoundedCornersF GroupContainerCycleView::GetRoundedCorners() const {
  if (mini_views_.empty()) {
    return gfx::RoundedCornersF();
  }

  if (mini_views_.size() == 1u) {
    return mini_views_[0]->GetRoundedCorners();
  }

  CHECK_EQ(mini_views_.size(), 2u);

  // For horizontal window layout, the left corners (`upper_left` and
  // `lower_left`) will depend on the primary snapped window, and likewise for
  // the right corners.
  // For vertical window layout, the top corners (`upper_left` and
  // `upper_right`) will depend on the primary snapped window, and likewise for
  // the bottom corners.
  const float upper_left = mini_views_[0]->GetRoundedCorners().upper_left();
  const float upper_right =
      is_layout_horizontal_ ? mini_views_[1]->GetRoundedCorners().upper_right()
                            : mini_views_[0]->GetRoundedCorners().upper_right();
  const float lower_right = mini_views_[1]->GetRoundedCorners().lower_right();
  const float lower_left =
      is_layout_horizontal_ ? mini_views_[0]->GetRoundedCorners().lower_left()
                            : mini_views_[1]->GetRoundedCorners().lower_left();
  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

void GroupContainerCycleView::SetSelectedWindowForFocus(aura::Window* window) {
  const bool old_is_first_focus_selection_request =
      is_first_focus_selection_request_;
  is_first_focus_selection_request_ = false;

  if (mini_views_.size() == 1u) {
    mini_views_[0]->UpdateFocusState(/*focus=*/true);
    return;
  }

  CHECK_EQ(mini_views_.size(), 2u);
  // If `this` is the first item in the cycle list with secondary snapped window
  // focused, cycle the primary snapped window first.
  if (old_is_first_focus_selection_request &&
      window_util::GetActiveWindow() == mini_views_[1]->source_window()) {
    mini_views_[0]->UpdateFocusState(/*focus=*/true);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  } else {
    // For normal use case, follow the window cycle order and `UpdateFocusState`
    // on the cycle item that contains the target window.
    for (WindowCycleItemView* mini_view : mini_views_) {
      if (mini_view->Contains(window)) {
        mini_view->UpdateFocusState(/*focus=*/true);
        NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
        break;
      }
    }
  }
}

void GroupContainerCycleView::ClearFocusSelection() {
  for (WindowCycleItemView* mini_view : mini_views_) {
    mini_view->UpdateFocusState(/*focus=*/false);
  }
}

BEGIN_METADATA(GroupContainerCycleView)
END_METADATA

}  // namespace ash
