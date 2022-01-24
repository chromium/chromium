// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer.h"

#include "cc/layers/video_layer_impl.h"

namespace cc {

scoped_refptr<VideoLayer> VideoLayer::Create(
    VideoFrameProvider* provider,
    media::VideoTransformation transform) {
  return base::WrapRefCounted(new VideoLayer(provider, transform));
}

VideoLayer::VideoLayer(VideoFrameProvider* provider,
                       media::VideoTransformation transform)
    : provider_(provider), transform_(transform) {
  SetMayContainVideo(true);
  DCHECK(provider_);
}

VideoLayer::~VideoLayer() = default;

std::unique_ptr<LayerImpl> VideoLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return VideoLayerImpl::Create(tree_impl, id(), provider_, transform_);
}

bool VideoLayer::Update() {
  bool updated = Layer::Update();

  // Video layer doesn't update any resources from the main thread side,
  // but repaint rects need to be sent to the VideoLayerImpl via commit.
  //
  // This is the inefficient legacy redraw path for videos.  It's better to
  // communicate this directly to the VideoLayerImpl.
  updated |= !update_rect().IsEmpty();

  return updated;
}

void VideoLayer::StopUsingProvider() {
  DCHECK(IsMutationAllowed());
  provider_ = nullptr;
}

}  // namespace cc
