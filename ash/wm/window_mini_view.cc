// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include <memory>
#include <utility>

#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/window_preview_view.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// Values of the backdrop.
constexpr int kBackdropBorderRoundingDp = 4;

constexpr int kFocusRingCornerRadius = 20;

constexpr gfx::Insets kHeaderInsets = gfx::Insets::TLBR(0, 10, 0, 10);

std::u16string GetWindowTitle(aura::Window* window) {
  aura::Window* transient_root = wm::GetTransientRoot(window);
  const std::u16string* overview_title =
      transient_root->GetProperty(chromeos::kWindowOverviewTitleKey);
  return (overview_title && !overview_title->empty())
             ? *overview_title
             : transient_root->GetTitle();
}

}  // namespace

WindowMiniView::~WindowMiniView() = default;

constexpr gfx::Size WindowMiniView::kIconSize;
constexpr int WindowMiniView::kHeaderPaddingDp;

void WindowMiniView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible) {
    return;
  }

  if (!backdrop_view_) {
    // Always put the backdrop view under other children.
    backdrop_view_ = AddChildViewAt(std::make_unique<views::View>(), 0);
    backdrop_view_->SetPaintToLayer();
    backdrop_view_->SetBackground(views::CreateThemedSolidBackground(
        chromeos::features::IsJellyrollEnabled()
            ? cros_tokens::kCrosSysScrim
            : static_cast<ui::ColorId>(
                  kColorAshControlBackgroundColorInactive)));

    ui::Layer* layer = backdrop_view_->layer();
    layer->SetFillsBoundsOpaquely(false);

    const gfx::RoundedCornersF rounded_corder_radius =
        chromeos::features::IsJellyrollEnabled()
            ? gfx::RoundedCornersF(0, 0, kWindowMiniViewCornerRadius,
                                   kWindowMiniViewCornerRadius)
            : gfx::RoundedCornersF(kBackdropBorderRoundingDp);

    layer->SetRoundedCornerRadius(rounded_corder_radius);
    layer->SetIsFastRoundedCorner(true);
    backdrop_view_->SetCanProcessEventsWithinSubtree(false);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::SetShowPreview(bool show) {
  if (show == !!preview_view_) {
    return;
  }

  if (!show) {
    RemoveChildView(preview_view_);
    delete preview_view_;
    preview_view_ = nullptr;
    return;
  }

  if (!source_window_) {
    return;
  }

  preview_view_ = AddChildView(std::make_unique<WindowPreviewView>(
      source_window_,
      /*trilinear_filtering_on_init=*/false));
  preview_view_->SetPaintToLayer();
  preview_view_->layer()->SetFillsBoundsOpaquely(false);
  Layout();
}

void WindowMiniView::UpdatePreviewRoundedCorners(bool show) {
  if (!preview_view()) {
    return;
  }

  ui::Layer* layer = preview_view()->layer();
  DCHECK(layer);
  const float scale = layer->transform().To2dScale().x();
  const float rounding = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kLow);
  gfx::RoundedCornersF radii;

  if (!show) {
    radii = gfx::RoundedCornersF();
  } else {
    if (chromeos::features::IsJellyrollEnabled()) {
      // Corner radius is applied to the previw view only if the
      // `backdrop_view_` is not visible.
      if (backdrop_view_ && backdrop_view_->GetVisible()) {
        radii = gfx::RoundedCornersF();
      } else {
        radii = gfx::RoundedCornersF(0, 0, kWindowMiniViewCornerRadius / scale,
                                     kWindowMiniViewCornerRadius / scale);
      }
    } else {
      radii = gfx::RoundedCornersF(rounding / scale);
    }
  }

  layer->SetRoundedCornerRadius(radii);
  layer->SetIsFastRoundedCorner(true);
}

void WindowMiniView::UpdateFocusState(bool focus) {
  if (is_focused_ == focus) {
    return;
  }

  is_focused_ = focus;
  views::FocusRing::Get(this)->SchedulePaint();
}

gfx::Rect WindowMiniView::GetHeaderBounds() const {
  gfx::Rect header_bounds = GetContentsBounds();
  header_bounds.set_height(kHeaderHeightDp);
  return header_bounds;
}

gfx::Size WindowMiniView::GetPreviewViewSize() const {
  DCHECK(preview_view_);
  return preview_view_->GetPreferredSize();
}

WindowMiniView::WindowMiniView(aura::Window* source_window, int border_inset)
    : source_window_(source_window) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(conniekxu|sammiequon): Remove the border once the calculation method
  // for the bounds of the OverviewItemView is redone.
  SetBorder(views::CreateEmptyBorder(gfx::Insets(border_inset)));

  window_observation_.Observe(source_window);

  header_view_ = AddChildView(std::make_unique<views::View>());
  header_view_->SetPaintToLayer();
  header_view_->layer()->SetFillsBoundsOpaquely(false);

  gfx::Insets header_insets(0);
  if (chromeos::features::IsJellyrollEnabled()) {
    header_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        chromeos::features::IsJellyrollEnabled()
            ? cros_tokens::kCrosSysHeader
            : static_cast<ui::ColorId>(kColorAshShieldAndBase80),
        /*top_radius=*/kWindowMiniViewCornerRadius,
        /*bottom_radius=*/0, /*for_border_thickness=*/0));
    header_insets = kHeaderInsets;
  }

  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, header_insets,
          kHeaderPaddingDp));

  title_label_ = header_view_->AddChildView(
      std::make_unique<views::Label>(GetWindowTitle(source_window_)));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  layout->SetFlexForView(title_label_, 1);

  // In order to show the focus ring out of the content view, `border_inset`
  // needs to be counted when setting the insets for the focus ring.
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset + border_inset),
      chromeos::features::IsJellyrollEnabled() ? kFocusRingCornerRadius
                                               : kBackdropBorderRoundingDp);
  views::FocusRing::Install(this);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHasFocusPredicate(
      [&](views::View* view) { return is_focused_; });
}

void WindowMiniView::UpdateIconView() {
  DCHECK(source_window_);
  aura::Window* transient_root = wm::GetTransientRoot(source_window_);
  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically larger.
  gfx::ImageSkia* icon = transient_root->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty()) {
    icon = transient_root->GetProperty(aura::client::kWindowIconKey);
  }
  if (!icon) {
    return;
  }

  if (!icon_view_) {
    icon_view_ =
        header_view_->AddChildViewAt(std::make_unique<views::ImageView>(), 0);
  }

  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
}

gfx::Rect WindowMiniView::GetContentAreaBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(gfx::Insets::TLBR(kHeaderHeightDp, 0, 0, 0));
  return bounds;
}

void WindowMiniView::Layout() {
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
  views::View::Layout();
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

void WindowMiniView::OnThemeChanged() {
  views::View::OnThemeChanged();
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

void WindowMiniView::OnWindowPropertyChanged(aura::Window* window,
                                             const void* key,
                                             intptr_t old) {
  // Update the icon if it changes in the middle of an overview or alt tab
  // session (due to device scale factor change or other).
  if (key != aura::client::kAppIconKey && key != aura::client::kWindowIconKey) {
    return;
  }

  UpdateIconView();
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
  title_label_->SetText(GetWindowTitle(window));
}

BEGIN_METADATA(WindowMiniView, views::View)
END_METADATA

}  // namespace ash
