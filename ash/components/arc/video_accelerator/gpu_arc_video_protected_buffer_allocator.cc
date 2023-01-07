// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"

#include <limits>
#include <utility>

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/protected_buffer_allocator.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/format_utils.h"
#include "media/gpu/macros.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace arc {
// static
std::unique_ptr<GpuArcVideoProtectedBufferAllocator>
GpuArcVideoProtectedBufferAllocator::Create(
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager) {
  auto protected_buffer_allocator =
      protected_buffer_manager->CreateProtectedBufferAllocator(
          protected_buffer_manager);
  if (!protected_buffer_allocator)
    return nullptr;
  return base::WrapUnique(new GpuArcVideoProtectedBufferAllocator(
      std::move(protected_buffer_allocator)));
}

GpuArcVideoProtectedBufferAllocator::GpuArcVideoProtectedBufferAllocator(
    std::unique_ptr<ProtectedBufferAllocator> protected_buffer_allocator)
    : protected_buffer_allocator_(std::move(protected_buffer_allocator)) {}

GpuArcVideoProtectedBufferAllocator::~GpuArcVideoProtectedBufferAllocator() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOGF(2);
}

void GpuArcVideoProtectedBufferAllocator::AllocateProtectedSharedMemory(
    mojo::ScopedHandle handle_fd,
    uint64_t size,
    AllocateProtectedSharedMemoryCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle_fd));
  if (!fd.is_valid()) {
    std::move(callback).Run(false);
    return;
  }
  if (!base::IsValueInRangeForNumericType<size_t>(size)) {
    VLOGF(1) << "size is too large to fit in size_t"
             << ", size=" << size
             << ", size_t max=" << std::numeric_limits<size_t>::max();
    std::move(callback).Run(false);
    return;
  }
  VLOGF(2) << "size=" << size;
  std::move(callback).Run(
      protected_buffer_allocator_->AllocateProtectedSharedMemory(
          std::move(fd), static_cast<size_t>(size)));
}

void GpuArcVideoProtectedBufferAllocator::AllocateProtectedNativePixmap(
    mojo::ScopedHandle handle_fd,
    mojom::HalPixelFormat format,
    const gfx::Size& picture_size,
    AllocateProtectedNativePixmapCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle_fd));
  if (!fd.is_valid()) {
    std::move(callback).Run(false);
    return;
  }
  media::VideoPixelFormat pixel_format;
  switch (format) {
    case arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12:
      pixel_format = media::PIXEL_FORMAT_YV12;
      break;
    case arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_NV12:
      pixel_format = media::PIXEL_FORMAT_NV12;
      break;
    default:
      VLOGF(1) << "Unsupported format: " << format;
      std::move(callback).Run(false);
      return;
  }
  VLOGF(2) << "format=" << media::VideoPixelFormatToString(pixel_format)
           << ", picture_size=" << picture_size.ToString();

  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(
      protected_buffer_allocator_->AllocateProtectedNativePixmap(
          std::move(fd), *buffer_format, picture_size));
  return;
}

void GpuArcVideoProtectedBufferAllocator::ReleaseProtectedBuffer(
    mojo::ScopedHandle handle_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle_fd));
  if (!fd.is_valid())
    return;
  VLOGF(2);
  protected_buffer_allocator_->ReleaseProtectedBuffer(std::move(fd));
}
}  // namespace arc
