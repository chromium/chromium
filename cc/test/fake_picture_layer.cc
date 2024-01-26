// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer.h"

#include <utility>

#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"

namespace cc {

FakePictureLayer::FakePictureLayer(ContentLayerClient* client)
    : PictureLayer(client) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::~FakePictureLayer() = default;

std::unique_ptr<LayerImpl> FakePictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  auto layer_impl = FakePictureLayerImpl::Create(tree_impl, id());

  if (!fixed_tile_size_.IsEmpty())
    layer_impl->set_fixed_tile_size(fixed_tile_size_);

  return layer_impl;
}

bool FakePictureLayer::Update() {
  bool updated = PictureLayer::Update();
  update_count_++;
  return updated || always_update_resources_;
}

bool FakePictureLayer::RequiresSetNeedsDisplayOnHdrHeadroomChange() const {
  return reraster_on_hdr_change_;
}

scoped_refptr<RasterSource> FakePictureLayer::CreateRasterSource() const {
  if (playback_allowed_event_) {
    return FakeRasterSource::CreateFromRecordingSourceWithWaitable(
        GetRecordingSourceForTesting(), playback_allowed_event_);
  }
  return PictureLayer::CreateRasterSource();
}

}  // namespace cc
