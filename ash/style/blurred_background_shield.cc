// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/blurred_background_shield.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace ash {

BlurredBackgroundShield::BlurredBackgroundShield(
    views::View* host,
    absl::variant<SkColor, ui::ColorId> color,
    float blur_sigma,
    const gfx::RoundedCornersF& rounded_corners,
    bool add_layer_to_region)
    : host_(host),
      color_(color),
      blur_sigma_(blur_sigma),
      add_layer_to_region_(add_layer_to_region) {
  host_observation_.Observe(host_);

  if (add_layer_to_region_) {
    // `AddLayerToRegion` adds the background layer as a child layer of the host
    // view's parent layer. The background layer will be positioned below the
    // host view and synchronize the the location and visibility with the host
    // view.
    background_layer_.SetBounds(gfx::Rect(host_->size()));
    host_->AddLayerToRegion(&background_layer_, views::LayerRegion::kBelow);
  } else {
    // If the layer is not added to the below region of the host, the host view
    // should owns a layer for ease of layer hierarchy arrangement. The
    // background layer should be stacked below the host layer manually.
    CHECK(host_->layer());
    if (host_->layer()->parent()) {
      StackLayerBelowHost();
    }
  }

  background_layer_.SetRoundedCornerRadius(rounded_corners);
  UpdateBackgroundColor();
}

BlurredBackgroundShield::~BlurredBackgroundShield() {
  if (add_layer_to_region_) {
    host_->RemoveLayerFromRegions(&background_layer_);
  }
}

void BlurredBackgroundShield::SetColor(SkColor color) {
  if (absl::holds_alternative<SkColor>(color_) &&
      absl::get<SkColor>(color_) == color) {
    return;
  }

  color_ = color;
  UpdateBackgroundColor();
}

void BlurredBackgroundShield::SetColorId(ui::ColorId color_id) {
  if (absl::holds_alternative<ui::ColorId>(color_) &&
      absl::get<ui::ColorId>(color_) == color_id) {
    return;
  }

  color_ = color_id;
  UpdateBackgroundColor();
}

void BlurredBackgroundShield::OnViewAddedToWidget(views::View* observed_view) {
  if (!add_layer_to_region_) {
    StackLayerBelowHost();
  }
}

void BlurredBackgroundShield::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  background_layer_.SetVisible(host_->GetVisible());
}

void BlurredBackgroundShield::OnViewLayerBoundsSet(views::View* observed_view) {
  if (auto* host_layer = host_->layer()) {
    background_layer_.SetBounds(host_layer->bounds());
  }
}

void BlurredBackgroundShield::OnViewThemeChanged(views::View* observed_view) {
  UpdateBackgroundColor();
}

void BlurredBackgroundShield::StackLayerBelowHost() {
  // If the background layer is added to the host region below, we don't have to
  // manually restack it.
  CHECK(!add_layer_to_region_);

  // Otherwise, we should manually add the layer as a child layer of the host
  // view's parent layer. In case the parent layer owns other layers, we should
  // set the background layer below the host view layer.
  auto* host_layer = host_->layer();
  CHECK(host_layer);
  auto* host_parent_layer = host_->layer()->parent();
  CHECK(host_parent_layer);
  host_parent_layer->Add(&background_layer_);
  host_parent_layer->StackBelow(&background_layer_, host_layer);
  background_layer_.SetBounds(host_layer->bounds());
}

void BlurredBackgroundShield::UpdateBackgroundColor() {
  auto* color_provider = host_->GetColorProvider();
  const SkColor background_color =
      absl::holds_alternative<SkColor>(color_)
          ? absl::get<SkColor>(color_)
          : (color_provider
                 ? color_provider->GetColor(absl::get<ui::ColorId>(color_))
                 : gfx::kPlaceholderColor);
  // Only enable the background blur if the color is translucent.
  background_layer_.SetColor(background_color);
  if (SkColorGetA(background_color) != SK_AlphaOPAQUE && blur_sigma_) {
    background_layer_.SetBackgroundBlur(blur_sigma_);
    background_layer_.SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
  } else {
    background_layer_.SetBackgroundBlur(0.0f);
  }
}

}  // namespace ash
