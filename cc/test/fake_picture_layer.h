// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_H_
#define CC_TEST_FAKE_PICTURE_LAYER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/recording_source.h"

namespace cc {
class FakePictureLayer : public PictureLayer {
 public:
  static scoped_refptr<FakePictureLayer> Create(ContentLayerClient* client) {
    return base::WrapRefCounted(new FakePictureLayer(client));
  }

  static scoped_refptr<FakePictureLayer> CreateWithRecordingSource(
      ContentLayerClient* client,
      std::unique_ptr<RecordingSource> source) {
    return base::WrapRefCounted(
        new FakePictureLayer(client, std::move(source)));
  }

  // Layer implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  bool Update() override;

  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  void set_fixed_tile_size(gfx::Size fixed_tile_size) {
    fixed_tile_size_ = fixed_tile_size;
  }

 private:
  explicit FakePictureLayer(ContentLayerClient* client);
  FakePictureLayer(ContentLayerClient* client,
                   std::unique_ptr<RecordingSource> source);
  ~FakePictureLayer() override;

  int update_count_ = 0;
  bool always_update_resources_ = false;

  gfx::Size fixed_tile_size_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_H_
