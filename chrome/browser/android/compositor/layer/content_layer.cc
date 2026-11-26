// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include <cstddef>

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
                                 bool can_use_thumbnail,
                                 scoped_refptr<cc::slim::Layer> live_layer) {
  if (live_layer) {
    live_layer->SetHideLayerAndSubtree(false);
    live_layer->SetMasksToBounds(false);
    live_layer->SetBounds(gfx::Size());
    // Don't override the opacity for layers internal to the WebContents.
    live_layer->SetOpacity(1.0f);
  }

  scoped_refptr<cc::slim::Layer> static_layer;
  if (can_use_thumbnail && !DoesLayerDrawContent(live_layer.get())) {
    if (ThumbnailLayer* thumbnail_layer =
            tab_content_manager_->GetStaticLayer(id)) {
      static_layer = thumbnail_layer->layer();
      static_layer->SetIsDrawable(true);
      static_layer->SetOpacity(1.0f);
    }
  }

  // `layer_` only ever hosts `live_layer` and/or `static_layer` (in that
  // order). Remove any stale children first, then insert missing ones in order
  // so unchanged children are not detached and re-attached across frames.
  const auto& children = layer_->children();
  for (size_t i = children.size(); i-- > 0;) {
    // Retain a ref across RemoveFromParent() in case `layer_->children()` holds
    // the last reference to `child`.
    scoped_refptr<cc::slim::Layer> child = children[i];
    if (child != live_layer && child != static_layer) {
      child->RemoveFromParent();
    }
  }

  size_t next_index = 0;
  for (const auto& desired : {live_layer, static_layer}) {
    if (!desired) {
      continue;
    }
    if (next_index >= children.size() || children[next_index] != desired) {
      layer_->InsertChild(desired, next_index);
    }
    ++next_index;
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
