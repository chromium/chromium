// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"

#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<ThumbnailLayer> ThumbnailLayer::Create() {
  return base::WrapRefCounted(new ThumbnailLayer());
}

void ThumbnailLayer::SetThumbnail(thumbnail::Thumbnail* thumbnail) {
  layer_->SetUIResourceId(thumbnail->ui_resource_id());
  UpdateSizes(thumbnail->scaled_content_size());
}

void ThumbnailLayer::AddSelfToParentOrReplaceAt(
    scoped_refptr<cc::slim::Layer> parent,
    size_t index) {
  if (index >= parent->children().size()) {
    parent->AddChild(layer_);
  } else if (parent->children()[index]->id() != layer_->id()) {
    parent->ReplaceChild(parent->children()[index].get(), layer_);
  }
}

scoped_refptr<cc::slim::Layer> ThumbnailLayer::layer() {
  return layer_;
}

ThumbnailLayer::ThumbnailLayer() : layer_(cc::slim::UIResourceLayer::Create()) {
  layer_->SetIsDrawable(true);
}

ThumbnailLayer::~ThumbnailLayer() = default;

void ThumbnailLayer::UpdateSizes(const gfx::SizeF& content_size) {
  if (content_size != content_size_) {
    content_size_ = content_size;
    layer_->SetBounds(gfx::Size(content_size_.width(), content_size_.height()));
  }
}

}  // namespace android
