// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_tab_title.h"

#include <android/bitmap.h>

#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/decoration_icon_title.h"
#include "components/viz/common/features.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace android {

DecorationTabTitle::DecorationTabTitle(ui::ResourceManager* resource_manager,
                                       int title_resource_id,
                                       int icon_resource_id,
                                       int spinner_resource_id,
                                       int spinner_incognito_resource_id,
                                       int fade_width,
                                       int icon_start_padding,
                                       int icon_end_padding,
                                       bool is_incognito,
                                       bool is_rtl,
                                       bool show_bubble,
                                       int bubble_inner_dimension,
                                       int bubble_outer_dimension,
                                       int bubble_offset,
                                       int bubble_inner_tint,
                                       int bubble_outer_tint)
    : DecorationIconTitle(resource_manager,
                          title_resource_id,
                          icon_resource_id,
                          fade_width,
                          icon_start_padding,
                          icon_end_padding,
                          is_incognito,
                          is_rtl),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id),
      show_bubble_(show_bubble),
      bubble_inner_dimension_(bubble_inner_dimension),
      bubble_outer_dimension_(bubble_outer_dimension),
      bubble_offset_(bubble_offset),
      bubble_inner_tint_(bubble_inner_tint),
      bubble_outer_tint_(bubble_outer_tint) {}

DecorationTabTitle::~DecorationTabTitle() = default;

void DecorationTabTitle::SetUIResourceIds() {
  DecorationTitle::SetUIResourceIds();

  if (!is_loading_) {
    handleIconResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP);
  } else if (spinner_resource_id_ != 0 && spinner_incognito_resource_id_ != 0) {
    int resource_id =
        is_incognito_ ? spinner_incognito_resource_id_ : spinner_resource_id_;

    ui::Resource* spinner_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);

    if (spinner_resource) {
      layer_icon_->SetUIResourceId(spinner_resource->ui_resource()->id());
    }

    // Rotate about the center of the layer.
    layer_icon_->SetTransformOrigin(
        gfx::PointF(icon_size_.width() / 2, icon_size_.height() / 2));
  }
  size_ = DecorationIconTitle::calculateSize(icon_size_.width());
}

void DecorationTabTitle::SetIsLoading(bool is_loading) {
  if (is_loading != is_loading_) {
    is_loading_ = is_loading;
    icon_needs_refresh_ = true;
    SetUIResourceIds();
  }
}

void DecorationTabTitle::SetSpinnerRotation(float rotation) {
  if (!is_loading_) {
    return;
  }
  float diff = rotation - spinner_rotation_;
  spinner_rotation_ = rotation;
  if (diff != 0) {
    transform_->RotateAboutZAxis(diff);
  }
  layer_icon_->SetTransform(*transform_.get());
}

void DecorationTabTitle::SetShowBubble(bool show_bubble) {
  if (show_bubble != show_bubble_) {
    show_bubble_ = show_bubble;
  }
}

scoped_refptr<cc::slim::SolidColorLayer>
DecorationTabTitle::CreateTabBubbleCircle(int size, int tint) {
  scoped_refptr<cc::slim::SolidColorLayer> circle_layer =
      cc::slim::SolidColorLayer::Create();
  circle_layer->SetBounds(gfx::Size(size, size));
  circle_layer->SetBackgroundColor(SkColor4f::FromColor(tint));
  circle_layer->SetOpacity(1.0f);
  circle_layer->SetRoundedCorner(
      gfx::RoundedCornersF(size / 2.0f, size / 2.0f, size / 2.0f, size / 2.0f));
  return circle_layer;
}

void DecorationTabTitle::CreateTabBubble() {
  // Create tab bubbler outer circle.
  tab_bubble_outer_circle_layer_ =
      CreateTabBubbleCircle(bubble_outer_dimension_, bubble_outer_tint_);

  // Create tab bubbler inner circle.
  tab_bubble_inner_circle_layer_ =
      CreateTabBubbleCircle(bubble_inner_dimension_, bubble_inner_tint_);

  // Add inner bubble as a child of outer bubble.
  float offset = (bubble_outer_dimension_ - bubble_inner_dimension_) / 2.0f;
  tab_bubble_outer_circle_layer_->AddChild(tab_bubble_inner_circle_layer_);
  tab_bubble_inner_circle_layer_->SetPosition((gfx::PointF(offset, offset)));
}

void DecorationTabTitle::CreateAndShowTabBubble(gfx::PointF position) {
  if (!tab_bubble_outer_circle_layer_) {
    CreateTabBubble();
  }
  layer_->AddChild(tab_bubble_outer_circle_layer_);
  tab_bubble_outer_circle_layer_->SetPosition(position);
  tab_bubble_outer_circle_layer_->SetIsDrawable(true);
  tab_bubble_outer_circle_layer_->SetOpacity(1.0f);
  tab_bubble_inner_circle_layer_->SetIsDrawable(true);
  tab_bubble_inner_circle_layer_->SetOpacity(1.0f);
}

void DecorationTabTitle::HideTabBubble() {
  if (!tab_bubble_outer_circle_layer_) {
    return;
  }

  // Ensure the bubble is not visible and removed from its parent.
  tab_bubble_outer_circle_layer_->SetHideLayerAndSubtree(true);
}

void DecorationTabTitle::Update(int title_resource_id,
                                int icon_resource_id,
                                int fade_width,
                                int icon_start_padding,
                                int icon_end_padding,
                                bool is_incognito,
                                bool is_rtl,
                                bool show_bubble) {
  DecorationIconTitle::Update(title_resource_id, icon_resource_id, fade_width,
                              icon_start_padding, icon_end_padding,
                              is_incognito, is_rtl);
  show_bubble_ = show_bubble;
}

void DecorationTabTitle::SetShouldHideTitleText(bool should_hide_title_text) {
  DecorationIconTitle::SetShouldHideTitleText(should_hide_title_text);
}

void DecorationTabTitle::SetShouldHideIcon(bool should_hide_icon) {
  DecorationIconTitle::SetShouldHideIcon(should_hide_icon);
}

void DecorationTabTitle::setBounds(const gfx::Size& bounds) {
  // Place tab favicon.
  DecorationIconTitle::setBounds(bounds);

  // Place or hide tab bubble if applicable.
  if (show_bubble_ && layer_icon_) {
    float bubble_x =
        l10n_util::IsLayoutRtl()
            ? icon_position_.x() - bubble_outer_dimension_ + bubble_offset_
            : icon_position_.x() + icon_size_.width() - bubble_offset_;
    float bubble_y = icon_position_.y() + icon_size_.height() - bubble_offset_;
    CreateAndShowTabBubble(gfx::PointF(bubble_x, bubble_y));
  } else {
    HideTabBubble();
  }
}

}  // namespace android
