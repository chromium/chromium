// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include <vector>

#include "base/lazy_instance.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<ContentLayer> ContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new ContentLayer(tab_content_manager));
}

static void SetOpacityOnLeaf(scoped_refptr<cc::slim::Layer> layer,
                             float alpha) {
  const auto& children = layer->children();
  if (!children.empty()) {
    layer->SetOpacity(1.0f);
    for (const auto& child : children) {
      SetOpacityOnLeaf(child, alpha);
    }
  } else {
    layer->SetOpacity(alpha);
  }
}

static cc::slim::Layer* GetDrawsContentLeaf(
    scoped_refptr<cc::slim::Layer> layer) {
  if (!layer.get()) {
    return nullptr;
  }

  // If the subtree is hidden, then any layers in this tree will not be drawn.
  if (layer->hide_layer_and_subtree()) {
    return nullptr;
  }

  if (layer->opacity() == 0.0f) {
    return nullptr;
  }

  if (layer->draws_content()) {
    return layer.get();
  }

  const auto& children = layer->children();
  for (const auto& child : children) {
    cc::slim::Layer* leaf = GetDrawsContentLeaf(child);
    if (leaf) {
      return leaf;
    }
  }
  return nullptr;
}

void ContentLayer::SetProperties(int id,
                                 bool can_use_live_layer,
                                 float static_to_view_blend,
                                 bool should_override_content_alpha,
                                 float content_alpha_override,
                                 float saturation,
                                 bool should_clip,
                                 const gfx::Rect& clip) {
  scoped_refptr<cc::slim::Layer> live_layer =
      tab_content_manager_->GetLiveLayer(id);
  if (live_layer) {
    live_layer->SetHideLayerAndSubtree(!can_use_live_layer);
  }
  bool live_layer_draws = GetDrawsContentLeaf(live_layer);

  float content_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  float static_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  if (live_layer_draws) {
    static_opacity = static_to_view_blend;
  }

  layer_->RemoveAllChildren();

  if (live_layer.get()) {
    live_layer->SetMasksToBounds(should_clip);
    live_layer->SetBounds(clip.size());
    // Don't override the opacity for layers internal to the WebContents.
    live_layer->SetOpacity(content_opacity);

    layer_->AddChild(live_layer);
  }

  if (static_opacity > 0) {
    ThumbnailLayer* static_layer = tab_content_manager_->GetStaticLayer(id);
    if (static_layer) {
      static_layer->layer()->SetIsDrawable(true);
      if (should_clip) {
        static_layer->Clip(clip);
      } else {
        static_layer->ClearClip();
      }
      // TOOD(liuwilliam): The opacity should only need to be set on the static
      // layer, instead of all its children. The recursive setting was a
      // workaround for some old CC bug.
      SetOpacityOnLeaf(static_layer->layer(), static_opacity);

      std::vector<cc::slim::Filter> filters;
      if (saturation < 1.0f) {
        filters.push_back(cc::slim::Filter::CreateSaturation(saturation));
      }
      static_layer->layer()->SetFilters(std::move(filters));
      layer_->AddChild(static_layer->layer());
    }
  }
}

gfx::Size ContentLayer::ComputeSize(int id) const {
  gfx::Size size;

  scoped_refptr<cc::slim::Layer> live_layer =
      tab_content_manager_->GetLiveLayer(id);
  cc::slim::Layer* leaf_that_draws = GetDrawsContentLeaf(live_layer);
  if (leaf_that_draws) {
    size.SetToMax(leaf_that_draws->bounds());
  }

  ThumbnailLayer* static_layer = tab_content_manager_->GetStaticLayer(id);
  if (static_layer && GetDrawsContentLeaf(static_layer->layer())) {
    size.SetToMax(static_layer->layer()->bounds());
  }

  return size;
}

scoped_refptr<cc::slim::Layer> ContentLayer::layer() {
  return layer_;
}

ContentLayer::ContentLayer(TabContentManager* tab_content_manager)
    : layer_(cc::slim::Layer::Create()),
      tab_content_manager_(tab_content_manager) {}

ContentLayer::~ContentLayer() = default;

}  //  namespace android
