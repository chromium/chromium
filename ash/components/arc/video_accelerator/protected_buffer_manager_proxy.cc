// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/protected_buffer_manager_proxy.h"

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
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

  // This ScopedFDPair dance is chromeos-specific.
  base::subtle::ScopedFDPair fd_pair = region.PassPlatformHandle();
  std::move(callback).Run(mojo::WrapPlatformFile(std::move(fd_pair.fd)));
}

void GpuArcProtectedBufferManagerProxy::GetProtectedSharedMemoryFromHandle(
    mojo::ScopedHandle dummy_handle,
    GetProtectedSharedMemoryFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));

  auto region_platform_handle =
      protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
          std::move(unwrapped_fd));
  if (!region_platform_handle.IsValid())
    return std::move(callback).Run(mojo::ScopedSharedBufferHandle());

  base::UnsafeSharedMemoryRegion unsafe_shared_memory_region =
      base::UnsafeSharedMemoryRegion::Deserialize(
          std::move(region_platform_handle));
  CHECK(unsafe_shared_memory_region.IsValid());
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
    return std::move(callback).Run(absl::nullopt);
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
