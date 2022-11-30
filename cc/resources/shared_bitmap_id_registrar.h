// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SHARED_BITMAP_ID_REGISTRAR_H_
#define CC_RESOURCES_SHARED_BITMAP_ID_REGISTRAR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/shared_bitmap.h"

namespace cc {
class CrossThreadSharedBitmap;
class SharedBitmapIdRegistration;
class TextureLayer;

// An interface exposed to clients of TextureLayer for registering
// SharedBitmapIds that they will be using in viz::TransferableResources given
// to the TextureLayer. SharedBitmapId-SharedMemory pairs registered as such are
// then given to the display compositor, and the mapping between the pair is
// kept valid while the returned SharedBitmapIdRegistration is kept alive.
//
// These mappings are per-layer-tree. So if a client has multiple TextureLayers
// in the same tree, and wants to use the SharedBitmapId in more than one of
// them over time, it still only should register with a single TextureLayer. But
// if the TextureLayer is removed from the tree, they would need to be
// registered with another TextureLayer that is in each tree where they are
// being used.
class CC_EXPORT SharedBitmapIdRegistrar {
 public:
  virtual ~SharedBitmapIdRegistrar() = default;
  virtual SharedBitmapIdRegistration RegisterSharedBitmapId(
      const viz::SharedBitmapId& id,
      scoped_refptr<CrossThreadSharedBitmap> bitmap) = 0;
};

// A scoped object that maintains a mapping of SharedBitmapId to SharedMemory
// that was registered through SharedBitmapIdRegistrar for the display
// compositor. Keep this object alive while the SharedBitmapId may be used
// in viz::TransferableResources given to the TextureLayer. Typically that means
// as long as the client keeps the SharedMemory alive with a reference to the
// CrossThreadSharedBitmap, which it should keep alive at least until the
// TextureLayer calls back to the ReleaseCallback indicating the display
// compositor is no longer using the resource. When this object is destroyed, or
// assigned to, then the mapping registration will be dropped from the display
// compositor, and the SharedBitmapId will no longer be able to be used in the
// TextureLayer.
class CC_EXPORT SharedBitmapIdRegistration {
 public:
  SharedBitmapIdRegistration();
  SharedBitmapIdRegistration(const SharedBitmapIdRegistration&) = delete;
  SharedBitmapIdRegistration(SharedBitmapIdRegistration&&) noexcept;
  ~SharedBitmapIdRegistration();

  SharedBitmapIdRegistration& operator=(const SharedBitmapIdRegistration&) =
      delete;
  SharedBitmapIdRegistration& operator=(SharedBitmapIdRegistration&&) noexcept;

 private:
  // Constructed by TextureLayer only, then held by the client as long
  // as they wish to
  friend TextureLayer;
  SharedBitmapIdRegistration(base::WeakPtr<TextureLayer> layer_ptr,
                             const viz::SharedBitmapId& id);

  base::WeakPtr<TextureLayer> layer_ptr_;
  viz::SharedBitmapId id_;
};

}  // namespace cc

#endif  // CC_RESOURCES_SHARED_BITMAP_ID_REGISTRAR_H_
