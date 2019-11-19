// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize{24, 24};

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// TODO(sammiequon): Combine this with the duplicate in overview.
constexpr int kHeaderHeightDp = 40;

// Values of the backdrop.
constexpr int kBackdropRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

}  // namespace

WindowMiniView::~WindowMiniView() = default;

void WindowMiniView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible)
    return;

  if (!backdrop_view_) {
    backdrop_view_ = new RoundedRectView(kBackdropRoundingDp, kBackdropColor);
    backdrop_view_->set_can_process_events_within_subtree(false);
    AddChildViewOf(this, backdrop_view_);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::SetShowPreview(bool show) {
  if (show == !!preview_view_)
    return;

  if (!show) {
    RemoveChildView(preview_view_);
    delete preview_view_;
    preview_view_ = nullptr;
    return;
  }

  if (!source_window_)
    return;

  preview_view_ = new WindowPreviewView(source_window_,
                                        /*trilinear_filtering_on_init=*/false);
  AddChildViewOf(this, preview_view_);
  Layout();
}

int WindowMiniView::GetMargin() const {
  return 0;
}

gfx::Rect WindowMiniView::GetHeaderBounds() const {
  return gfx::Rect(GetLocalBounds().width(), kHeaderHeightDp);
}

gfx::Size WindowMiniView::GetPreviewViewSize() const {
  DCHECK(preview_view_);
  return preview_view_->GetPreferredSize();
}

WindowMiniView::WindowMiniView(aura::Window* source_window,
                               bool views_should_paint_to_layers)
    : source_window_(source_window),
      views_should_paint_to_layers_(views_should_paint_to_layers) {
  window_observer_.Add(source_window);

  header_view_ = new views::View();
  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kHorizontalLabelPaddingDp));
  AddChildViewOf(this, header_view_);

  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically larger.
  gfx::ImageSkia* icon = source_window->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = source_window->GetProperty(aura::client::kWindowIconKey);
  if (icon && !icon->size().IsEmpty()) {
    image_view_ = new views::ImageView();
    image_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
    image_view_->SetSize(kIconSize);
    AddChildViewOf(header_view_, image_view_);
  }

  title_label_ = new views::Label(source_window->GetTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kLabelColor);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  AddChildViewOf(header_view_, title_label_);
  layout->SetFlexForView(title_label_, 1);
}

void WindowMiniView::AddChildViewOf(views::View* parent, views::View* child) {
  parent->AddChildView(child);
  if (views_should_paint_to_layers_) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
  }
}

void WindowMiniView::Layout() {
  const int margin = GetMargin();
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(margin, margin);

  if (backdrop_view_) {
    gfx::Rect backdrop_bounds = bounds;
    backdrop_bounds.Inset(0, kHeaderHeightDp, 0, 0);
    backdrop_view_->SetBoundsRect(backdrop_bounds);
  }

  if (preview_view_) {
    gfx::Rect preview_bounds = bounds;
    preview_bounds.Inset(0, kHeaderHeightDp, 0, 0);
    preview_bounds.ClampToCenteredSize(GetPreviewViewSize());
    preview_view_->SetBoundsRect(preview_bounds);
  }

  header_view_->SetBoundsRect(GetHeaderBounds());
}

void WindowMiniView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kWindow;
  node_data->SetName(title_label_->GetText());
}

void WindowMiniView::OnWindowDestroying(aura::Window* window) {
  if (window != source_window_)
    return;

  window_observer_.RemoveAll();
  source_window_ = nullptr;
  SetShowPreview(false);
}

void WindowMiniView::OnWindowTitleChanged(aura::Window* window) {
  title_label_->SetText(window->GetTitle());
}

}  // namespace ash
