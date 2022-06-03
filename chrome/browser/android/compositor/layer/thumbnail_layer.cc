// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"

#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace android {

// static
scoped_refptr<ThumbnailLayer> ThumbnailLayer::Create() {
  return base::WrapRefCounted(new ThumbnailLayer());
}

void ThumbnailLayer::SetThumbnail(Thumbnail* thumbnail) {
  layer_->SetUIResourceId(thumbnail->ui_resource_id());
  UpdateSizes(thumbnail->scaled_content_size(), thumbnail->scaled_data_size());
}

void ThumbnailLayer::Clip(const gfx::Rect& clipping) {
  last_clipping_ = clipping;
  clipped_ = true;

  gfx::Size clipped_content = gfx::Size(content_size_.width() - clipping.x(),
                                        content_size_.height() - clipping.y());
  clipped_content.SetToMin(clipping.size());
  layer_->SetBounds(clipped_content);

  layer_->SetUV(
      gfx::PointF(clipping.x() / resource_size_.width(),
                  clipping.y() / resource_size_.height()),
      gfx::PointF(
          (clipping.x() + clipped_content.width()) / resource_size_.width(),
          (clipping.y() + clipped_content.height()) / resource_size_.height()));
}

void ThumbnailLayer::ClearClip() {
  layer_->SetUV(gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f));
  layer_->SetBounds(gfx::Size(content_size_.width(), content_size_.height()));
  clipped_ = false;
}

void ThumbnailLayer::AddSelfToParentOrReplaceAt(scoped_refptr<cc::Layer> parent,
                                                size_t index) {
  if (index >= parent->children().size())
    parent->AddChild(layer_);
  else if (parent->children()[index]->id() != layer_->id())
    parent->ReplaceChild(parent->children()[index].get(), layer_);
}

scoped_refptr<cc::Layer> ThumbnailLayer::layer() {
  return layer_;
}

ThumbnailLayer::ThumbnailLayer() : layer_(cc::UIResourceLayer::Create()) {
  layer_->SetIsDrawable(true);
}

ThumbnailLayer::~ThumbnailLayer() {
}

void ThumbnailLayer::UpdateSizes(const gfx::SizeF& content_size,
                                 const gfx::SizeF& resource_size) {
  if (content_size != content_size_ || resource_size != resource_size_) {
    content_size_ = content_size;
    resource_size_ = resource_size;
    if (clipped_)
      Clip(last_clipping_);
    else
      ClearClip();
  }
}

}  // namespace android
