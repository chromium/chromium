// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/media_controls_chip_view.h"

#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kMediaControlsChipContainerRadius = 8;
constexpr gfx::Insets kMediaControlsChipViewPadding(8, 16, 11, 16);
constexpr gfx::Insets kMediaControlsChipContainerPadding(8);
constexpr int kMediaControlsChipSpacing = 16;
}  // namespace

MediaControlsChipView::MediaControlsChipView()
    : artwork_view_(new views::ImageView),
      title_artist_view_(new views::View),
      title_label_(new views::Label),
      artist_label_(new views::Label) {
  auto* container = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kMediaControlsChipViewPadding + kMediaControlsChipContainerPadding,
      kMediaControlsChipSpacing));
  container->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(artwork_view_);

  auto* title_artist_container =
      title_artist_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  title_artist_container->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  title_artist_container->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  title_artist_view_->AddChildView(title_label_);

  artist_label_->SetAutoColorReadabilityEnabled(false);
  artist_label_->SetSubpixelRenderingEnabled(false);
  artist_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  title_artist_view_->AddChildView(artist_label_);

  AddChildView(title_artist_view_);
}

MediaControlsChipView::~MediaControlsChipView() {}

void MediaControlsChipView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(kMediaControlsChipViewPadding);
  canvas->DrawRoundRect(bounds, kMediaControlsChipContainerRadius, flags);
  views::View::OnPaintBackground(canvas);
}

void MediaControlsChipView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  SetVisible(expanded_amount > 0.0);
  InvalidateLayout();
  // TODO(leandre): add animation and opacity when collapsing/expanding the
  // tray.
}

const char* MediaControlsChipView::GetClassName() const {
  return "MediaControlsChipView";
}

}  // namespace ash
