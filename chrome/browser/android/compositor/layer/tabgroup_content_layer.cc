// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tabgroup_content_layer.h"

#include <vector>

#include "cc/layers/layer.h"
#include "chrome/browser/android/compositor/layer/tabgroup_tab_content_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// row x column of the tab group content grid is 2 x 2.
const int THUMBNAIL_ROWS = 2;
const int THUMBNAIL_COLS = THUMBNAIL_ROWS;
const float DESIRED_SCALE = 0.475;
const float GAP_SCALE = 0.05;

// static
scoped_refptr<TabGroupContentLayer> TabGroupContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new TabGroupContentLayer(tab_content_manager));
}

void TabGroupContentLayer::SetProperties(
    int id,
    const std::vector<int>& tab_ids,
    bool can_use_live_layer,
    float static_to_view_blend,
    bool should_override_content_alpha,
    float content_alpha_override,
    float saturation,
    bool should_clip,
    const gfx::Rect& clip,
    ui::NinePatchResource* inner_shadow_resource,
    float inner_shadow_alpha) {
  if (group_tab_content_layers_.size() == 0) {
    for (int i = 0; i < 4; i++) {
      group_tab_content_layers_.emplace_back(
          TabGroupTabContentLayer::Create(tab_content_manager_));
      layer_->AddChild(group_tab_content_layers_.back()->layer());
    }
  }

  const gfx::RectF border_inner_shadow_padding(
      inner_shadow_resource->padding());

  const gfx::Size border_inner_shadow_padding_size(
      inner_shadow_resource->size().width() -
          border_inner_shadow_padding.width(),
      inner_shadow_resource->size().height() -
          border_inner_shadow_padding.height());

  int size = tab_ids.size();
  for (int i = 0; i < size; i++) {
    group_tab_content_layers_[i]->SetProperties(
        tab_ids[i], can_use_live_layer, static_to_view_blend,
        should_override_content_alpha, content_alpha_override, saturation,
        should_clip, clip, inner_shadow_resource, tab_ids, inner_shadow_alpha);

    gfx::Transform transform;
    transform.Scale(DESIRED_SCALE, DESIRED_SCALE);
    group_tab_content_layers_[i]->layer()->SetTransform(transform);

    int height_offset_factor = i / THUMBNAIL_ROWS;
    int width_offset_factor = i % THUMBNAIL_COLS;

    float position_x =
        clip.x() +
        width_offset_factor *
            (clip.width() * DESIRED_SCALE + clip.width() * GAP_SCALE -
             border_inner_shadow_padding_size.width());

    float position_y =
        clip.y() +
        height_offset_factor *
            (clip.height() * DESIRED_SCALE + clip.height() * GAP_SCALE -
             border_inner_shadow_padding_size.height());
    gfx::PointF position(position_x, position_y);
    group_tab_content_layers_[i]->layer()->SetPosition(position);
  }
}

TabGroupContentLayer::TabGroupContentLayer(
    TabContentManager* tab_content_manager)
    : ContentLayer(tab_content_manager) {}

TabGroupContentLayer::~TabGroupContentLayer() {}

}  //  namespace android
