// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include <memory>

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kFocusRingCornerRadius = 20;

// Returns the rounded corners of the preview view scaled by the given value of
// `scale` for the preview view with given source `window` if allowed to `show`.
// If the preview view is completely inside the rounded bounds of `backdrop`, no
// need to round its corners.
gfx::RoundedCornersF GetRoundedCornersForPreviewView(
    aura::Window* window,
    views::View* backdrop,
    const gfx::Rect& preview_bounds_in_screen,
    float scale,
    bool show,
    std::optional<gfx::RoundedCornersF> preview_view_rounded_corners) {
  if (!show) {
    return gfx::RoundedCornersF();
  }

  if (!window_util::ShouldRoundThumbnailWindow(
          backdrop, gfx::RectF(preview_bounds_in_screen))) {
    return gfx::RoundedCornersF();
  }

  if (preview_view_rounded_corners.has_value()) {
    // TODO(b/294294344): Return a different set of rounded corners if it is
    // for vertical split view.
    const auto raw_value = preview_view_rounded_corners.value();
    return gfx::RoundedCornersF(raw_value.upper_left(), raw_value.upper_right(),
                                raw_value.lower_right(),
                                raw_value.lower_left());
  }

  return gfx::RoundedCornersF(0, 0, kWindowMiniViewCornerRadius / scale,
                              kWindowMiniViewCornerRadius / scale);
}

}  // namespace

WindowMiniViewBase::~WindowMiniViewBase() = default;

void WindowMiniViewBase::SetRoundedCornersRadius(
    const gfx::RoundedCornersF& exposed_rounded_corners) {
  exposed_rounded_corners_ = exposed_rounded_corners;
  header_view_rounded_corners_ =
      gfx::RoundedCornersF(exposed_rounded_corners.upper_left(),
                           exposed_rounded_corners.upper_right(),
                           /*lower_right=*/0,
                           /*lower_left=*/0);
  preview_view_rounded_corners_ =
      gfx::RoundedCornersF(/*upper_left=*/0, /*upper_right=*/0,
                           exposed_rounded_corners.upper_right(),
                           exposed_rounded_corners.lower_left());
}

void WindowMiniViewBase::UpdateFocusState(bool focus) {
  if (is_focused_ == focus) {
    return;
  }

  is_focused_ = focus;
  views::FocusRing::Get(this)->SchedulePaint();
}

WindowMiniViewBase::WindowMiniViewBase() = default;

BEGIN_METADATA(WindowMiniViewBase)
END_METADATA

WindowMiniView::~WindowMiniView() = default;

void WindowMiniView::SetSelectedWindowForFocus(aura::Window* window) {
  CHECK_EQ(window, source_window_);
  UpdateFocusState(/*focus=*/true);
}

void WindowMiniView::ClearFocusSelection() {
  UpdateFocusState(/*focus=*/false);
}

void WindowMiniView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible) {
    return;
  }

  if (!backdrop_view_) {
    // Always put the backdrop view under other children.
    backdrop_view_ = AddChildViewAt(std::make_unique<views::View>(), 0);
    backdrop_view_->SetPaintToLayer();
    backdrop_view_->SetBackground(
        views::CreateThemedSolidBackground(cros_tokens::kCrosSysScrim));

    ui::Layer* layer = backdrop_view_->layer();
    layer->SetFillsBoundsOpaquely(false);
    layer->SetRoundedCornerRadius(gfx::RoundedCornersF(
        0.f, 0.f, kWindowMiniViewCornerRadius, kWindowMiniViewCornerRadius));
    layer->SetIsFastRoundedCorner(true);
    backdrop_view_->SetCanProcessEventsWithinSubtree(false);
    DeprecatedLayoutImmediately();
  }

  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::RefreshPreviewRoundedCorners(bool show) {
  if (!preview_view_) {
    return;
  }

  ui::Layer* layer = preview_view_->layer();
  CHECK(layer);

  layer->SetRoundedCornerRadius(GetRoundedCornersForPreviewView(
      source_window_, backdrop_view_, preview_view_->GetBoundsInScreen(),
      layer->transform().To2dScale().x(), show, preview_view_rounded_corners_));
  if (!chromeos::features::IsRoundedWindowsEnabled()) {
    layer->SetIsFastRoundedCorner(true);
  }
}

void WindowMiniView::RefreshHeaderViewRoundedCorners() {
  if (!header_view_) {
    return;
  }

  const auto header_rounded_corners =
      header_view_->GetHeaderRoundedCorners(source_window_);
  if (header_view_rounded_corners_.has_value()) {
    if (header_rounded_corners == header_view_rounded_corners_.value()) {
      return;
    }
    header_view_->SetHeaderViewRoundedCornerRadius(
        header_view_rounded_corners_.value());
  } else if (header_rounded_corners ==
             gfx::RoundedCornersF(kWindowMiniViewHeaderCornerRadius,
                                  kWindowMiniViewHeaderCornerRadius, 0, 0)) {
    return;
  }

  header_view_->RefreshHeaderViewRoundedCorners();
}

void WindowMiniView::RefreshFocusRingVisuals() {
  views::HighlightPathGenerator::Install(this, GenerateFocusRingPath());
}

void WindowMiniView::ResetRoundedCorners() {
  if (header_view_rounded_corners_.has_value()) {
    header_view_->ResetRoundedCorners();
  }

  header_view_rounded_corners_.reset();
  preview_view_rounded_corners_.reset();
}

bool WindowMiniView::Contains(aura::Window* window) const {
  return source_window_ == window;
}

aura::Window* WindowMiniView::GetWindowAtPoint(
    const gfx::Point& screen_point) const {
  return GetBoundsInScreen().Contains(screen_point) ? source_window_ : nullptr;
}

void WindowMiniView::SetShowPreview(bool show) {
  if (show == !!preview_view_) {
    return;
  }

  if (!show) {
    RemoveChildViewT(preview_view_.get());
    preview_view_ = nullptr;
    return;
  }

  if (!source_window_) {
    return;
  }

  preview_view_ =
      AddChildView(std::make_unique<WindowPreviewView>(source_window_));
  preview_view_->SetPaintToLayer();
  preview_view_->layer()->SetFillsBoundsOpaquely(false);

  // TODO(crbug.com/1522471): Consider redesigning `WindowCycleItemView` to
  // cancel Layer rounded corners.
  //
  // The derived class `WindowCycleItemView` of `WindowMiniView` will create
  // `backdrop_view_` in the Layout method so that we can enter
  // `WindowMiniView::RefreshPreviewRoundedCorners` in the first layout to
  // cancel the rounded corners for the Layer of the view. This is a very subtle
  // logic. If this DeprecatedLayoutImmediately() call is not here, we will lose
  // the opportunity to adjust the Layer fillet. Maybe we should refactor here.
  DeprecatedLayoutImmediately();

  // The preferred size of `WindowMiniView` is tied to the presence or absence
  // of `preview_view_`. Although we have performed Layout above, this will not
  // invalidate the cache in `LayoutManagerBase`. We need to actively call cache
  // invalidation.
  PreferredSizeChanged();
}

int WindowMiniView::TryRemovingChildItem(aura::Window* destroying_window) {
  return 0;
}

gfx::RoundedCornersF WindowMiniView::GetRoundedCorners() const {
  if (!header_view_ || !preview_view_) {
    return gfx::RoundedCornersF();
  }

  const gfx::RoundedCornersF header_rounded_corners =
      header_view_->GetHeaderRoundedCorners(source_window_);
  const gfx::RoundedCornersF preview_rounded_corners =
      preview_view_->layer()->rounded_corner_radii();
  return gfx::RoundedCornersF(header_rounded_corners.upper_left(),
                              header_rounded_corners.upper_right(),
                              preview_rounded_corners.lower_right(),
                              preview_rounded_corners.lower_left());
}

gfx::Rect WindowMiniView::GetHeaderBounds() const {
  gfx::Rect header_bounds = GetContentsBounds();
  header_bounds.set_height(kWindowMiniViewHeaderHeight);
  return header_bounds;
}

gfx::Size WindowMiniView::GetPreviewViewSize() const {
  DCHECK(preview_view_);
  return preview_view_->GetPreferredSize();
}

WindowMiniView::WindowMiniView(aura::Window* source_window)
    : source_window_(source_window) {
  InstallFocusRing();
  window_observation_.Observe(source_window);
  header_view_ = AddChildView(std::make_unique<WindowMiniViewHeaderView>(this));
}

gfx::Rect WindowMiniView::GetContentAreaBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(gfx::Insets::TLBR(kWindowMiniViewHeaderHeight, 0, 0, 0));
  return bounds;
}

void WindowMiniView::Layout(PassKey) {
  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  if (backdrop_view_) {
    backdrop_view_->SetBoundsRect(content_area_bounds);
  }

  if (preview_view_) {
    gfx::Rect preview_bounds = content_area_bounds;
    preview_bounds.ClampToCenteredSize(GetPreviewViewSize());
    preview_view_->SetBoundsRect(preview_bounds);
  }

  header_view_->SetBoundsRect(GetHeaderBounds());
  LayoutSuperclass<views::View>(this);
}

void WindowMiniView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // This may be called after `OnWindowDestroying`. `this` should be destroyed
  // shortly by the owner (OverviewItem/WindowCycleView) but there may be a
  // small window where `source_window_` is null. Speculative fix for
  // https://crbug.com/1274775.
  if (!source_window_) {
    return;
  }

  node_data->role = ax::mojom::Role::kWindow;
  node_data->SetName(wm::GetTransientRoot(source_window_)->GetTitle());
}

void WindowMiniView::OnWindowPropertyChanged(aura::Window* window,
                                             const void* key,
                                             intptr_t old) {
  // Update the icon if it changes in the middle of an overview or alt tab
  // session (due to device scale factor change or other).
  if (key != aura::client::kAppIconKey && key != aura::client::kWindowIconKey) {
    return;
  }

  header_view_->UpdateIconView(source_window_);
}

void WindowMiniView::OnWindowDestroying(aura::Window* window) {
  if (window != source_window_) {
    return;
  }

  window_observation_.Reset();
  source_window_ = nullptr;
  SetShowPreview(false);
}

void WindowMiniView::OnWindowTitleChanged(aura::Window* window) {
  header_view_->UpdateTitleLabel(window);
}

void WindowMiniView::InstallFocusRing() {
  RefreshFocusRingVisuals();
  views::FocusRing::Install(this);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<WindowMiniView>(view);
        CHECK(v);
        return v->is_focused_;
      }));
}

std::unique_ptr<views::HighlightPathGenerator>
WindowMiniView::GenerateFocusRingPath() {
  if (exposed_rounded_corners_) {
    const float upper_left = exposed_rounded_corners_->upper_left() == 0
                                 ? 0
                                 : kFocusRingCornerRadius;
    const float upper_right = exposed_rounded_corners_->upper_right() == 0
                                  ? 0
                                  : kFocusRingCornerRadius;
    const float lower_right = exposed_rounded_corners_->lower_right() == 0
                                  ? 0
                                  : kFocusRingCornerRadius;
    const float lower_left = exposed_rounded_corners_->lower_left() == 0
                                 ? 0
                                 : kFocusRingCornerRadius;
    return std::make_unique<views::RoundRectHighlightPathGenerator>(
        gfx::Insets(kWindowMiniViewFocusRingHaloInset),
        gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left));
  }

  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      gfx::Insets(kWindowMiniViewFocusRingHaloInset),
      gfx::RoundedCornersF(kFocusRingCornerRadius));
}

BEGIN_METADATA(WindowMiniView)
END_METADATA

}  // namespace ash
