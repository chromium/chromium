// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_mask_layer_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace cc {

FakeMaskLayerImpl::FakeMaskLayerImpl(LayerTreeImpl* tree_impl,
                                     int id,
                                     scoped_refptr<RasterSource> raster_source)
    : PictureLayerImpl(tree_impl, id) {
  SetBounds(raster_source->size());
  Region region;
  UpdateRasterSource(raster_source, &region);
}

std::unique_ptr<FakeMaskLayerImpl> FakeMaskLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<RasterSource> raster_source) {
  return base::WrapUnique(new FakeMaskLayerImpl(tree_impl, id, raster_source));
}

void FakeMaskLayerImpl::GetContentsResourceId(viz::ResourceId* resource_id,
                                              gfx::Size* resource_size,
                                              gfx::SizeF* mask_uv_size) const {
  *resource_id = viz::kInvalidResourceId;
  *resource_size = resource_size_;
  *mask_uv_size = gfx::SizeF(1.0f, 1.0f);
}

}  // namespace cc
