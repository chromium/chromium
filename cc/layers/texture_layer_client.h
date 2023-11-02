// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_CLIENT_H_
#define CC_LAYERS_TEXTURE_LAYER_CLIENT_H_

#include "components/viz/common/resources/release_callback.h"

namespace viz {
struct TransferableResource;
}

namespace cc {
class SharedBitmapIdRegistrar;

class TextureLayerClient {
 public:
  // Returns true and provides a mailbox if a new frame is available.
  // Returns false if no new data is available
  // and the old mailbox is to be reused.
  virtual bool PrepareTransferableResource(
      SharedBitmapIdRegistrar* bitmap_registar,
      viz::TransferableResource* transferable_resource,
      viz::ReleaseCallback* release_callback) = 0;

 protected:
  virtual ~TextureLayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_CLIENT_H_
