// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"

#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle) {
  if (!handle.is_valid()) {
    VLOGF(1) << "Handle is invalid";
    return base::ScopedFD();
  }

  base::ScopedPlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK)
    VLOGF(1) << "UnwrapPlatformFile failed: " << mojo_result;
  return platform_file;
}

std::vector<base::ScopedFD> DuplicateFD(base::ScopedFD fd, size_t num_fds) {
  if (!fd.is_valid()) {
    VLOGF(1) << "Input fd is not valid";
    return {};
  }

  std::vector<base::ScopedFD> fds(num_fds);
  fds[0] = std::move(fd);
  for (size_t i = 1; i < num_fds; ++i) {
    base::ScopedFD dup_fd(HANDLE_EINTR(dup(fds[0].get())));
    if (!dup_fd.is_valid()) {
      VLOGF(1) << "Failed to duplicate fd";
      return {};
    }
    fds[i] = std::move(dup_fd);
  }

  return fds;
}

std::optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    uint64_t modifier,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<VideoFramePlane>& planes) {
  std::vector<media::ColorPlaneLayout> color_planes;
  for (size_t i = 0; i < planes.size(); ++i) {
    int32_t stride = base::checked_cast<int32_t>(planes[i].stride);
    size_t offset = base::checked_cast<size_t>(planes[i].offset);
    size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<size_t> current_size =
        base::CheckMul(stride, plane_height);
    if (!current_size.IsValid()) {
      VLOGF(1) << "Invalid stride/height";
      return std::nullopt;
    }

    color_planes.emplace_back(stride, offset, current_size.ValueOrDie());
  }

  return CreateGpuMemoryBufferHandle(pixel_format, modifier, coded_size,
                                     std::move(scoped_fds), color_planes);
}

std::optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    uint64_t modifier,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<media::ColorPlaneLayout>& planes) {
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (planes.size() != num_planes || planes.size() == 0) {
    VLOGF(1) << "Invalid number of dmabuf planes passed: " << planes.size()
             << ", expected: " << num_planes;
    return std::nullopt;
  }
  if (scoped_fds.size() != num_planes) {
    VLOGF(1) << "Invalid number of fds passed: " << scoped_fds.size()
             << ", expected: " << num_planes;
    return std::nullopt;
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.modifier = modifier;
  for (size_t i = 0; i < num_planes; ++i) {
    // NOTE: planes[i].stride and planes[i].offset both are int32_t. stride and
    // offset in NativePixmapPlane are uint32_t and uint64_t, respectively.
    if (!base::IsValueInRangeForNumericType<uint32_t>(planes[i].stride)) {
      VLOGF(1) << "Invalid stride";
      return std::nullopt;
    }
    if (!base::IsValueInRangeForNumericType<uint64_t>(planes[i].offset)) {
      VLOGF(1) << "Invalid offset";
      return std::nullopt;
    }
    uint32_t stride = base::checked_cast<uint32_t>(planes[i].stride);
    uint64_t offset = base::checked_cast<uint64_t>(planes[i].offset);
    uint64_t size = base::checked_cast<uint64_t>(planes[i].size);
    gmb_handle.native_pixmap_handle.planes.emplace_back(
        stride, offset, size, std::move(scoped_fds[i]));
  }

  if (!media::VerifyGpuMemoryBufferHandle(pixel_format, coded_size, gmb_handle))
    return std::nullopt;

  return gmb_handle;
}

base::ScopedFD CreateTempFileForTesting(const std::string& data) {
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  if (!base::WriteFile(path, data)) {
    VLOGF(1) << "Cannot write the whole data into file.";
    return base::ScopedFD();
  }

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    VLOGF(1) << "Failed to create file.";
    return base::ScopedFD();
  }

  return base::ScopedFD(file.TakePlatformFile());
}

bool IsBufferSecure(ProtectedBufferManager* protected_buffer_manager,
                    const base::ScopedFD& fd) {
  DCHECK(protected_buffer_manager);
  // If we can get the corresponding protected buffer from the protected buffer
  // manager we can consider the buffer as secure.
  base::ScopedFD dup_fd(HANDLE_EINTR(dup(fd.get())));
  return dup_fd.is_valid() &&
         protected_buffer_manager
             ->GetProtectedSharedMemoryRegionFor(std::move(dup_fd))
             .IsValid();
}

}  // namespace arc
