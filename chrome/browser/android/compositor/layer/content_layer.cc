// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

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

static bool DoesLayerDrawContent(const cc::slim::Layer* layer) {
  if (!layer) {
    return false;
  }

  // If the subtree is hidden, then any layers in this tree will not be drawn.
  if (layer->hide_layer_and_subtree()) {
    return false;
  }

  if (layer->opacity() == 0.0f) {
    return false;
  }

  if (layer->draws_content()) {
    return true;
  }

  for (const auto& child : layer->children()) {
    if (DoesLayerDrawContent(child.get())) {
      return true;
    }
  }
  return false;
}

void ContentLayer::SetProperties(int id,
                                 bool can_use_live_layer,
                                 bool can_use_thumbnail) {
  scoped_refptr<cc::slim::Layer> live_layer =
      tab_content_manager_->GetLiveLayer(id);
  if (live_layer) {
    live_layer->SetHideLayerAndSubtree(!can_use_live_layer);
  }

  layer_->RemoveAllChildren();

  if (live_layer.get()) {
    live_layer->SetMasksToBounds(false);
    live_layer->SetBounds(gfx::Size());
    // Don't override the opacity for layers internal to the WebContents.
    live_layer->SetOpacity(1.0f);

    layer_->AddChild(live_layer);
  }

  if (can_use_thumbnail && !DoesLayerDrawContent(live_layer.get())) {
    ThumbnailLayer* static_layer = tab_content_manager_->GetStaticLayer(id);
    if (static_layer) {
      static_layer->layer()->SetIsDrawable(true);
      static_layer->layer()->SetOpacity(1.0f);
      layer_->AddChild(static_layer->layer());
    }
  }
}

scoped_refptr<cc::slim::Layer> ContentLayer::layer() {
  return layer_;
}

ContentLayer::ContentLayer(TabContentManager* tab_content_manager)
    : layer_(cc::slim::Layer::Create()),
      tab_content_manager_(tab_content_manager) {}

ContentLayer::~ContentLayer() = default;

}  //  namespace android
