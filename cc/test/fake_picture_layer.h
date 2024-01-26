// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_H_
#define CC_TEST_FAKE_PICTURE_LAYER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "cc/layers/picture_layer.h"

namespace base {
class WaitableEvent;
}

namespace cc {

class RasterSource;

class FakePictureLayer : public PictureLayer {
 public:
  static scoped_refptr<FakePictureLayer> Create(ContentLayerClient* client) {
    return base::WrapRefCounted(new FakePictureLayer(client));
  }

  // Layer implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  bool Update() override;
  bool RequiresSetNeedsDisplayOnHdrHeadroomChange() const override;

  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  void set_reraster_on_hdr_change(bool reraster_on_hdr_change) {
    reraster_on_hdr_change_ = reraster_on_hdr_change;
  }

  void set_fixed_tile_size(gfx::Size fixed_tile_size) {
    fixed_tile_size_ = fixed_tile_size;
  }

  void set_playback_allowed_event(base::WaitableEvent* event) {
    playback_allowed_event_ = event;
  }

 private:
  explicit FakePictureLayer(ContentLayerClient* client);
  ~FakePictureLayer() override;

  scoped_refptr<RasterSource> CreateRasterSource() const override;

  int update_count_ = 0;
  bool always_update_resources_ = false;
  bool reraster_on_hdr_change_ = false;

  gfx::Size fixed_tile_size_;
  raw_ptr<base::WaitableEvent> playback_allowed_event_ = nullptr;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_H_
