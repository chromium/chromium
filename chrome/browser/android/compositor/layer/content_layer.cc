// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include "base/lazy_instance.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/paint/filter_operations.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<ContentLayer> ContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new ContentLayer(tab_content_manager));
}

static void SetOpacityOnLeaf(scoped_refptr<cc::Layer> layer, float alpha) {
  const cc::LayerList& children = layer->children();
  if (children.size() > 0) {
    layer->SetOpacity(1.0f);
    for (uint i = 0; i < children.size(); ++i)
      SetOpacityOnLeaf(children[i], alpha);
  } else {
    layer->SetOpacity(alpha);
  }
}

static cc::Layer* GetDrawsContentLeaf(scoped_refptr<cc::Layer> layer) {
  if (!layer.get())
    return nullptr;

  // If the subtree is hidden, then any layers in this tree will not be drawn.
  if (layer->hide_layer_and_subtree())
    return nullptr;

  if (layer->opacity() == 0.0f)
    return nullptr;

  if (layer->DrawsContent())
    return layer.get();

  const cc::LayerList& children = layer->children();
  for (unsigned i = 0; i < children.size(); i++) {
    cc::Layer* leaf = GetDrawsContentLeaf(children[i]);
    if (leaf)
      return leaf;
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
  scoped_refptr<cc::Layer> live_layer = tab_content_manager_->GetLiveLayer(id);
  if (live_layer)
    live_layer->SetHideLayerAndSubtree(!can_use_live_layer);
  bool live_layer_draws = GetDrawsContentLeaf(live_layer);

  float content_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  float static_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  if (live_layer_draws)
    static_opacity = static_to_view_blend;

  const cc::LayerList& layer_children = layer_->children();
  for (unsigned i = 0; i < layer_children.size(); i++)
    layer_children[i]->RemoveFromParent();

  if (live_layer.get()) {
    live_layer->SetMasksToBounds(should_clip);
    live_layer->SetBounds(clip.size());
    SetOpacityOnLeaf(live_layer, content_opacity);

    layer_->AddChild(live_layer);
  }

  if (static_opacity > 0) {
    scoped_refptr<ThumbnailLayer> static_layer =
        tab_content_manager_->GetOrCreateStaticLayer(id, !live_layer_draws);
    if (static_layer.get()) {
      static_layer->layer()->SetIsDrawable(true);
      if (should_clip)
        static_layer->Clip(clip);
      else
        static_layer->ClearClip();
      SetOpacityOnLeaf(static_layer->layer(), static_opacity);

      cc::FilterOperations static_filter_operations;
      if (saturation < 1.0f) {
        static_filter_operations.Append(
            cc::FilterOperation::CreateSaturateFilter(saturation));
      }
      static_layer->layer()->SetFilters(static_filter_operations);

      layer_->AddChild(static_layer->layer());
    }
  }
}

gfx::Size ContentLayer::ComputeSize(int id) const {
  gfx::Size size;

  scoped_refptr<cc::Layer> live_layer = tab_content_manager_->GetLiveLayer(id);
  cc::Layer* leaf_that_draws = GetDrawsContentLeaf(live_layer);
  if (leaf_that_draws)
    size.SetToMax(leaf_that_draws->bounds());

  scoped_refptr<ThumbnailLayer> static_layer =
      tab_content_manager_->GetStaticLayer(id);
  if (static_layer.get() && GetDrawsContentLeaf(static_layer->layer()))
    size.SetToMax(static_layer->layer()->bounds());

  return size;
}

scoped_refptr<cc::Layer> ContentLayer::layer() {
  return layer_;
}

ContentLayer::ContentLayer(TabContentManager* tab_content_manager)
    : layer_(cc::Layer::Create()),
      tab_content_manager_(tab_content_manager) {}

ContentLayer::~ContentLayer() {
}

}  //  namespace android
