// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tabgroup_tab_content_layer.h"

#include <vector>

#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<TabGroupTabContentLayer> TabGroupTabContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new TabGroupTabContentLayer(tab_content_manager));
}

void TabGroupTabContentLayer::SetProperties(
    int id,
    bool can_use_live_layer,
    float static_to_view_blend,
    bool should_override_content_alpha,
    float content_alpha_override,
    float saturation,
    bool should_clip,
    const gfx::Rect& clip,
    ui::NinePatchResource* border_inner_shadow_resource,
    const std::vector<int>& tab_ids,
    float border_inner_shadow_alpha) {
  content_->SetProperties(id, can_use_live_layer, static_to_view_blend,
                          should_override_content_alpha, content_alpha_override,
                          saturation, should_clip, clip);

  setBorderProperties(border_inner_shadow_resource, clip,
                      border_inner_shadow_alpha);

  layer_->SetBounds(front_border_inner_shadow_->bounds());
}

scoped_refptr<cc::slim::Layer> TabGroupTabContentLayer::layer() {
  return layer_;
}

TabGroupTabContentLayer::TabGroupTabContentLayer(
    TabContentManager* tab_content_manager)
    : layer_(cc::slim::Layer::Create()),
      content_(ContentLayer::Create(tab_content_manager)),
      front_border_inner_shadow_(cc::slim::NinePatchLayer::Create()) {
  layer_->AddChild(content_->layer());
  layer_->AddChild(front_border_inner_shadow_);

  front_border_inner_shadow_->SetIsDrawable(true);
}

TabGroupTabContentLayer::~TabGroupTabContentLayer() {}

void TabGroupTabContentLayer::setBorderProperties(
    ui::NinePatchResource* border_inner_shadow_resource,
    const gfx::Rect& clip,
    float border_inner_shadow_alpha) {
  // precalculate helper values
  const gfx::RectF border_inner_shadow_padding(
      border_inner_shadow_resource->padding());

  const gfx::Size border_inner_shadow_padding_size(
      border_inner_shadow_resource->size().width() -
          border_inner_shadow_padding.width(),
      border_inner_shadow_resource->size().height() -
          border_inner_shadow_padding.height());

  gfx::Size border_inner_shadow_size(clip.size());
  border_inner_shadow_size.Enlarge(border_inner_shadow_padding_size.width(),
                                   border_inner_shadow_padding_size.height());

  front_border_inner_shadow_->SetUIResourceId(
      border_inner_shadow_resource->ui_resource()->id());
  front_border_inner_shadow_->SetAperture(
      border_inner_shadow_resource->aperture());
  front_border_inner_shadow_->SetBorder(
      border_inner_shadow_resource->Border(border_inner_shadow_size));

  gfx::PointF border_inner_shadow_position(
      ScalePoint(border_inner_shadow_padding.origin(), -1));

  front_border_inner_shadow_->SetPosition(border_inner_shadow_position);
  front_border_inner_shadow_->SetBounds(border_inner_shadow_size);
  front_border_inner_shadow_->SetOpacity(border_inner_shadow_alpha);
}

}  //  namespace android
