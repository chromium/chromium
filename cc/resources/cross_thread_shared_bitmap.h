// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_CROSS_THREAD_SHARED_BITMAP_H_
#define CC_RESOURCES_CROSS_THREAD_SHARED_BITMAP_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// This class holds ownership of a base::ReadOnlySharedMemoryRegion and its
// base::WritableSharedMemoryMapping for use as a composited resource, and is
// refcounted in order to share ownership with the LayerTreeHost, via
// TextureLayer, which needs access to the base::ReadOnlySharedMemory from
// the compositor thread. Because all the fields exposed are const, they can
// be used from any thread without conflict, as they only read existing states.
class CC_EXPORT CrossThreadSharedBitmap
    : public base::RefCountedThreadSafe<CrossThreadSharedBitmap> {
 public:
  CrossThreadSharedBitmap(const viz::SharedBitmapId& id,
                          base::MappedReadOnlyRegion shm,
                          const gfx::Size& size,
                          viz::ResourceFormat format);

  const viz::SharedBitmapId& id() const { return id_; }
  const base::ReadOnlySharedMemoryRegion& shared_region() const {
    return region_;
  }
  void* memory() const { return mapping_.memory(); }
  const gfx::Size& size() const { return size_; }
  viz::ResourceFormat format() const { return format_; }

 private:
  friend base::RefCountedThreadSafe<CrossThreadSharedBitmap>;

  ~CrossThreadSharedBitmap();

  const viz::SharedBitmapId id_;
  const base::ReadOnlySharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  const gfx::Size size_;
  const viz::ResourceFormat format_;
};

}  // namespace cc

#endif  // CC_RESOURCES_CROSS_THREAD_SHARED_BITMAP_H_
