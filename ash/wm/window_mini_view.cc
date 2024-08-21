// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include <memory>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr int kFocusRingCornerRadius = 14;
constexpr int kFocusRingCornerRadiusOld = 20;
constexpr float kFocusRingThickness = 3.0f;

// Returns the rounded corners of the preview view scaled by the given value of
// `scale` for the preview view with given source `window`. If the preview view
// is completely inside the rounded bounds of `backdrop`, no need to round its
// corners.
gfx::RoundedCornersF GetRoundedCornersForPreviewView(
    aura::Window* window,
    views::View* backdrop,
    const gfx::Rect& preview_bounds_in_screen,
    float scale,
    std::optional<gfx::RoundedCornersF> preview_view_rounded_corners) {
  if (!window_util::ShouldRoundThumbnailWindow(
          backdrop, gfx::RectF(preview_bounds_in_screen))) {
    return gfx::RoundedCornersF();
  }

  if (preview_view_rounded_corners.has_value()) {
    return *preview_view_rounded_corners;
  }

  const int corner_radius = window_util::GetMiniWindowRoundedCornerRadius();
  return gfx::RoundedCornersF(0, 0, corner_radius / scale,
                              corner_radius / scale);
}

}  // namespace

WindowMiniViewBase::~WindowMiniViewBase() = default;

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

void WindowMiniView::SetRoundedCornersRadius(
    const gfx::RoundedCornersF& exposed_rounded_corners) {
  if (exposed_rounded_corners_ == exposed_rounded_corners) {
    return;
  }

  exposed_rounded_corners_ = exposed_rounded_corners;
  if (header_view_) {
    gfx::RoundedCornersF header_rounded_corners =
        gfx::RoundedCornersF(exposed_rounded_corners.upper_left(),
                             exposed_rounded_corners.upper_right(),
                             /*lower_right=*/0,
                             /*lower_left=*/0);
    header_view_->SetHeaderViewRoundedCornerRadius(header_rounded_corners);
  }

  preview_view_rounded_corners_ =
      gfx::RoundedCornersF(/*upper_left=*/0, /*upper_right=*/0,
                           exposed_rounded_corners.lower_right(),
                           exposed_rounded_corners.lower_left());
  OnRoundedCornersSet();
}

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

    layer->SetName("BackdropView");
    layer->SetFillsBoundsOpaquely(false);

    const int corner_radius = window_util::GetMiniWindowRoundedCornerRadius();
    layer->SetRoundedCornerRadius(
        gfx::RoundedCornersF(0.f, 0.f, corner_radius, corner_radius));
    layer->SetIsFastRoundedCorner(true);
    backdrop_view_->SetCanProcessEventsWithinSubtree(false);
    DeprecatedLayoutImmediately();
  }

  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::RefreshPreviewRoundedCorners() {
  if (!preview_view_) {
    return;
  }

  ui::Layer* layer = preview_view_->layer();
  CHECK(layer);

  layer->SetRoundedCornerRadius(GetRoundedCornersForPreviewView(
      source_window_, backdrop_view_, preview_view_->GetBoundsInScreen(),
      layer->transform().To2dScale().x(), preview_view_rounded_corners_));
  layer->SetIsFastRoundedCorner(true);
}

void WindowMiniView::RefreshHeaderViewRoundedCorners() {
  if (header_view_) {
    header_view_->RefreshHeaderViewRoundedCorners();
  }
}

void WindowMiniView::RefreshFocusRingVisuals() {
  views::HighlightPathGenerator::Install(this, GenerateFocusRingPath());
}

void WindowMiniView::ResetRoundedCorners() {
  if (header_view_) {
    header_view_->ResetRoundedCorners();
  }

  preview_view_rounded_corners_.reset();
  exposed_rounded_corners_.reset();
  OnRoundedCornersSet();
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
  ui::Layer* preview_layer = preview_view_->layer();
  preview_layer->SetName("PreviewView");
  preview_layer->SetFillsBoundsOpaquely(false);

  // TODO(http://b/41495434): Consider redesigning `WindowCycleItemView` to
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
      header_view_->background()->GetRoundedCornerRadii().value_or(
          gfx::RoundedCornersF());
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

WindowMiniView::WindowMiniView(aura::Window* source_window,
                               bool use_custom_focus_predicate)
    : source_window_(source_window) {
  InstallFocusRing(use_custom_focus_predicate);
  window_observation_.Observe(source_window);
  header_view_ = AddChildView(std::make_unique<WindowMiniViewHeaderView>(this));

  GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);
  UpdateAccessibleName();
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

  UpdateAccessibleIgnoredState();
}

void WindowMiniView::OnWindowTitleChanged(aura::Window* window) {
  header_view_->UpdateTitleLabel(window);
  UpdateAccessibleName();
}

void WindowMiniView::OnRoundedCornersSet() {
  RefreshHeaderViewRoundedCorners();
  RefreshPreviewRoundedCorners();
  RefreshFocusRingVisuals();
}

void WindowMiniView::InstallFocusRing(bool use_custom_predicate) {
  RefreshFocusRingVisuals();
  views::FocusRing::Install(this);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysTertiary);
  focus_ring->SetHaloThickness(kFocusRingThickness);

  if (use_custom_predicate) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<WindowMiniView>(view);
          CHECK(v);
          return v->is_focused_;
        }));
  }
}

void WindowMiniView::UpdateAccessibleIgnoredState() {
  if (source_window_) {
    GetViewAccessibility().SetIsIgnored(false);
  } else {
    // Don't expose to accessibility when `source_window_` is null.
    GetViewAccessibility().SetIsIgnored(true);
  }
}

void WindowMiniView::UpdateAccessibleName() {
  const std::u16string& accessible_name =
      wm::GetTransientRoot(source_window_)->GetTitle();
  if (accessible_name.empty()) {
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_WM_WINDOW_CYCLER_UNTITLED_WINDOW));
  } else {
    GetViewAccessibility().SetName(accessible_name);
  }
}

std::unique_ptr<views::HighlightPathGenerator>
WindowMiniView::GenerateFocusRingPath() {
  const int focus_ring_radius = chromeos::features::IsRoundedWindowsEnabled()
                                    ? kFocusRingCornerRadius
                                    : kFocusRingCornerRadiusOld;

  if (exposed_rounded_corners_) {
    const float upper_left =
        exposed_rounded_corners_->upper_left() == 0 ? 0 : focus_ring_radius;
    const float upper_right =
        exposed_rounded_corners_->upper_right() == 0 ? 0 : focus_ring_radius;
    const float lower_right =
        exposed_rounded_corners_->lower_right() == 0 ? 0 : focus_ring_radius;
    const float lower_left =
        exposed_rounded_corners_->lower_left() == 0 ? 0 : focus_ring_radius;

    gfx::Insets focus_ring_insets =
        gfx::Insets(kWindowMiniViewFocusRingHaloInset);

    // Apply reduced inset to internal edge to prevent focus ring occlusion.
    // Internal edge corners will be sharp (90 degrees).
    focus_ring_insets.set_right(
        exposed_rounded_corners_->upper_right() == 0 &&
                exposed_rounded_corners_->lower_right() == 0
            ? kWindowMiniViewFocusRingHaloInternalInset
            : kWindowMiniViewFocusRingHaloInset);
    focus_ring_insets.set_left(exposed_rounded_corners_->upper_left() == 0 &&
                                       exposed_rounded_corners_->lower_left() ==
                                           0
                                   ? kWindowMiniViewFocusRingHaloInternalInset
                                   : kWindowMiniViewFocusRingHaloInset);
    focus_ring_insets.set_bottom(
        exposed_rounded_corners_->lower_left() == 0 &&
                exposed_rounded_corners_->lower_right() == 0
            ? kWindowMiniViewFocusRingHaloInternalInset
            : kWindowMiniViewFocusRingHaloInset);
    focus_ring_insets.set_top(exposed_rounded_corners_->upper_left() == 0 &&
                                      exposed_rounded_corners_->upper_right() ==
                                          0
                                  ? kWindowMiniViewFocusRingHaloInternalInset
                                  : kWindowMiniViewFocusRingHaloInset);

    return std::make_unique<views::RoundRectHighlightPathGenerator>(
        focus_ring_insets,
        gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left));
  }

  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      gfx::Insets(kWindowMiniViewFocusRingHaloInset),
      gfx::RoundedCornersF(focus_ring_radius));
}

BEGIN_METADATA(WindowMiniView)
END_METADATA

}  // namespace ash
