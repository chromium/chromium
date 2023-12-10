// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/protected_buffer_manager_proxy.h"

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

GpuArcProtectedBufferManagerProxy::GpuArcProtectedBufferManagerProxy(
    scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager)
    : protected_buffer_manager_(std::move(protected_buffer_manager)) {
  DCHECK(protected_buffer_manager_);
}

GpuArcProtectedBufferManagerProxy::~GpuArcProtectedBufferManagerProxy() {}

void GpuArcProtectedBufferManagerProxy::
    DeprecatedGetProtectedSharedMemoryFromHandle(
        mojo::ScopedHandle dummy_handle,
        DeprecatedGetProtectedSharedMemoryFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));

  auto region = protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
      std::move(unwrapped_fd));
  if (!region.IsValid()) {
    // Note: this will just cause the remote endpoint to reject the message with
    // VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE, but we don't have another way
    // to indicate that we couldn't find the protected shared memory region.
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  // Due to POSIX limitations, the shmem platform handle consists of a pair of
  // a writable FD and a read-only FD. Since GetProtectedSharedMemoryRegionFor()
  // returns a base::UnsafeSharedMemoryRegion, only the writable FD should be
  // present.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region));
  base::subtle::ScopedFDPair fd_pair = platform_region.PassPlatformHandle();
  std::move(callback).Run(mojo::WrapPlatformFile(std::move(fd_pair.fd)));
}

void GpuArcProtectedBufferManagerProxy::GetProtectedSharedMemoryFromHandle(
    mojo::ScopedHandle dummy_handle,
    GetProtectedSharedMemoryFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));

  base::UnsafeSharedMemoryRegion unsafe_shared_memory_region =
      protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
          std::move(unwrapped_fd));
  if (!unsafe_shared_memory_region.IsValid())
    return std::move(callback).Run(mojo::ScopedSharedBufferHandle());

  std::move(callback).Run(mojo::WrapUnsafeSharedMemoryRegion(
      std::move(unsafe_shared_memory_region)));
}

void GpuArcProtectedBufferManagerProxy::
    GetProtectedNativePixmapHandleFromHandle(
        mojo::ScopedHandle dummy_handle,
        GetProtectedNativePixmapHandleFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));
  gfx::NativePixmapHandle native_pixmap_handle =
      protected_buffer_manager_->GetProtectedNativePixmapHandleFor(
          std::move(unwrapped_fd));
  if (native_pixmap_handle.planes.empty())
    return std::move(callback).Run(std::nullopt);
  std::move(callback).Run(std::move(native_pixmap_handle));
}

void GpuArcProtectedBufferManagerProxy::IsProtectedNativePixmapHandle(
    mojo::ScopedHandle dummy_handle,
    IsProtectedNativePixmapHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));
  std::move(callback).Run(
      protected_buffer_manager_->IsProtectedNativePixmapHandle(
          std::move(unwrapped_fd)));
}

}  // namespace arc
