// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_MASK_LAYER_IMPL_H_
#define CC_TEST_FAKE_MASK_LAYER_IMPL_H_

#include "cc/layers/picture_layer_impl.h"
#include "cc/raster/raster_source.h"

namespace cc {

class FakeMaskLayerImpl : public PictureLayerImpl {
 public:
  static std::unique_ptr<FakeMaskLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<RasterSource> raster_source);

  void GetContentsResourceId(viz::ResourceId* resource_id,
                             gfx::Size* resource_size,
                             gfx::SizeF* mask_uv_size) const override;
  void set_resource_size(gfx::Size resource_size) {
    resource_size_ = resource_size;
  }

 private:
  FakeMaskLayerImpl(LayerTreeImpl* tree_impl,
                    int id,
                    scoped_refptr<RasterSource> raster_source);

  gfx::Size resource_size_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_MASK_LAYER_IMPL_H_
