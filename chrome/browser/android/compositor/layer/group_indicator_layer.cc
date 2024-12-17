// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/group_indicator_layer.h"

#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/decoration_icon_title.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace android {

// static
scoped_refptr<GroupIndicatorLayer> GroupIndicatorLayer::Create(
    LayerTitleCache* layer_title_cache) {
  return base::WrapRefCounted(new GroupIndicatorLayer(layer_title_cache));
}

void GroupIndicatorLayer::SetProperties(int id,
                                        int tint,
                                        int bubble_tint,
                                        bool incognito,
                                        bool foreground,
                                        bool show_bubble,
                                        float x,
                                        float y,
                                        float width,
                                        float height,
                                        float title_start_padding,
                                        float title_end_padding,
                                        float corner_radius,
                                        float bottom_indicator_width,
                                        float bottom_indicator_height,
                                        float bubble_size,
                                        float tab_strip_height) {
  // Update group indicator properties.
  foreground_ = foreground;
  group_indicator_->SetPosition(gfx::PointF(x, y));
  group_indicator_->SetBounds(gfx::Size(width, height));
  group_indicator_->SetRoundedCorner(gfx::RoundedCornersF(
      corner_radius, corner_radius, corner_radius, corner_radius));
  group_indicator_->SetBackgroundColor(SkColor4f::FromColor(tint));

  // Show title if needed.
  DecorationIconTitle* title_layer = nullptr;
  // Only pull if group id is valid.
  if (layer_title_cache_ && id != -1) {
    title_layer = layer_title_cache_->GetGroupTitleLayer(id, incognito);
  }
  if (title_layer) {
    // Ensure we're using the updated title bitmap prior to accessing/updating
    // any properties.
    title_layer->SetUIResourceIds();

    float title_y = (height - title_layer->size().height()) / 2.f;
    title_layer->setBounds(
        gfx::Size(width - title_start_padding - title_end_padding, height));
    title_layer_ = title_layer->layer();
    title_layer_->SetPosition(gfx::PointF(title_start_padding, title_y));

    unsigned expected_children = 2;
    if (group_indicator_->children().size() < expected_children) {
      group_indicator_->AddChild(title_layer_);
    } else {
      group_indicator_->ReplaceChild(
          group_indicator_->children()[expected_children - 1].get(),
          title_layer_);
    }
  } else if (title_layer_.get()) {
    title_layer_->RemoveFromParent();
    title_layer_ = nullptr;
  }

  // Show notification bubble if needed.
  if (show_bubble) {
    float bubble_x = l10n_util::IsLayoutRtl()
                         ? title_end_padding
                         : width - title_end_padding - bubble_size;
    float bubble_y = (height - bubble_size) / 2.0f;
    float corner_size = bubble_size / 2.0f;

    notification_bubble_->SetIsDrawable(true);
    notification_bubble_->SetBounds(gfx::Size(bubble_size, bubble_size));
    notification_bubble_->SetPosition(gfx::PointF(bubble_x, bubble_y));
    notification_bubble_->SetBackgroundColor(SkColor4f::FromColor(bubble_tint));
    notification_bubble_->SetRoundedCorner(gfx::RoundedCornersF(
        corner_size, corner_size, corner_size, corner_size));
  } else {
    notification_bubble_->SetIsDrawable(false);
  }

  // Set bottom indicator properties.
  float bottom_indicator_x = x;
  float bottom_indicator_y = tab_strip_height - bottom_indicator_height;
  if (l10n_util::IsLayoutRtl()) {
    bottom_indicator_x -= (bottom_indicator_width - width);
  }

  // Use ceiling value to prevent height float from getting truncated, otherwise
  // it could result in bottom indicator looks thinner than intended in certain
  // screen densities.
  bottom_outline_->SetBounds(
      gfx::Size(bottom_indicator_width, ceil(bottom_indicator_height)));

  // Use the floor value to position vertically to prevent bottom indicator from
  // getting cut off in certain screen densities.
  bottom_outline_->SetPosition(
      gfx::PointF(bottom_indicator_x, floor(bottom_indicator_y)));
  bottom_outline_->SetBackgroundColor(SkColor4f::FromColor(tint));
}

bool GroupIndicatorLayer::foreground() {
  return foreground_;
}

scoped_refptr<cc::slim::Layer> GroupIndicatorLayer::layer() {
  return layer_;
}

GroupIndicatorLayer::GroupIndicatorLayer(LayerTitleCache* layer_title_cache)
    : layer_title_cache_(layer_title_cache),
      layer_(cc::slim::Layer::Create()),
      group_indicator_(cc::slim::SolidColorLayer::Create()),
      bottom_outline_(cc::slim::SolidColorLayer::Create()),
      notification_bubble_(cc::slim::SolidColorLayer::Create()),
      foreground_(false) {
  group_indicator_->SetIsDrawable(true);
  bottom_outline_->SetIsDrawable(true);
  notification_bubble_->SetIsDrawable(false);

  layer_->AddChild(group_indicator_);
  layer_->AddChild(bottom_outline_);
  group_indicator_->AddChild(notification_bubble_);
}

GroupIndicatorLayer::~GroupIndicatorLayer() {}

}  // namespace android
