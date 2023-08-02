// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_item_view.h"

#include <algorithm>
#include <memory>

#include "ash/shell.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
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

void WindowCycleItemView::ShowPreview() {
  DCHECK(!preview_view());

  header_view()->UpdateIconView(source_window());
  SetShowPreview(/*show=*/true);
  UpdatePreviewRoundedCorners(/*show=*/true);
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

BEGIN_METADATA(WindowCycleItemView, WindowMiniView)
END_METADATA

GroupContainerView::GroupContainerView() {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetAccessibleName(u"Group container view");

  // TODO(michelefan@): Orientation should correspond to the window layout.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kInsideContainerBorderInset, kBetweenCycleItemsSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

GroupContainerView::~GroupContainerView() = default;

BEGIN_METADATA(GroupContainerView, FocusableView)
END_METADATA

}  // namespace ash
