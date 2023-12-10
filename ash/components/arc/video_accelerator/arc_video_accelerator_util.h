// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/video_accelerator/video_frame_plane.h"
#include "base/files/scoped_file.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace arc {

class ProtectedBufferManager;

// Creates ScopedFD from given mojo::ScopedHandle.
// Returns invalid ScopedFD on failure.
base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);

// Return a list of duplicated |fd|. The size of list is |num_fds|. Return an
// empty list if duplicatation fails.
std::vector<base::ScopedFD> DuplicateFD(base::ScopedFD fd, size_t num_fds);

// Return GpuMemoryBufferHandle iff |planes| are valid for a video frame located
// on |scoped_fds| and of |pixel_format| and |coded_size|. Otherwise
// returns std::nullopt.
std::optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    uint64_t modifier,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<VideoFramePlane>& planes);
std::optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    uint64_t modifier,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<media::ColorPlaneLayout>& planes);

// Create a temp file and write |data| into the file.
base::ScopedFD CreateTempFileForTesting(const std::string& data);

// Check whether the specified buffer uses secure memory.
bool IsBufferSecure(ProtectedBufferManager* protected_buffer_manager,
                    const base::ScopedFD& fd);

}  // namespace arc
#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
