// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_ALLOCATOR_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_ALLOCATOR_H_

#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace arc {

// ProtectedBufferAllocator allocates/deallocates protected buffers.
// This class guarantees that the underlying protected buffers are not destroyed
// and guarantees that referring to the protected buffers via the dummy handles
// will always work and return the same buffers for the same dummy_fd for the
// lifetime of this class. Once the class is destroyed, referring via dummy
// handles becomes impossible, and the underlying buffers will be freed as soon
// as all other users release own references to them (if any).
class ProtectedBufferAllocator {
 public:
  virtual ~ProtectedBufferAllocator() = default;

  // Allocates a protected SharedMemory whose size is |size| bytes, to be
  // referred to via |dummy_fd| as the dummy handle.
  // Returns whether the allocation is successful.
  virtual bool AllocateProtectedSharedMemory(base::ScopedFD dummy_fd,
                                             size_t size) = 0;

  // Allocates a protected native pixmap with |format| and |size|, to be
  // referred to via |dummy_fd| as the dummy handle.
  // Returns whether the allocation is successful.
  virtual bool AllocateProtectedNativePixmap(base::ScopedFD dummy_fd,
                                             gfx::BufferFormat format,
                                             const gfx::Size& size) = 0;

  // Releases reference to ProtectedSharedMemory or ProtectedNativePixmap
  // referred by |dummy_fd|.
  virtual void ReleaseProtectedBuffer(base::ScopedFD dummy_fd) = 0;
};

}  // namespace arc
#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_ALLOCATOR_H_
