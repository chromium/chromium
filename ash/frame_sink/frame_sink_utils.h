// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_FRAME_SINK_UTILS_H_
#define ASH_FRAME_SINK_FRAME_SINK_UTILS_H_

#include <string_view>

#include "ash/ash_export.h"
#include "base/memory/scoped_refptr.h"
#include "cc/resources/resource_pool.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class ClientResourceProvider;
class CompositorFrame;
class RasterContextProvider;
}  // namespace viz

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace ash::frame_sink_utils {

// Returns the SharedMainThreadRasterContextProvider used by frame sinks.
ASH_EXPORT scoped_refptr<viz::RasterContextProvider> GetContextProvider();

// Acquires a pooled resource of `size` from `resource_pool`. If the resource
// does not have a backing, creates a Mappable SharedImage backing with
// `SHARED_IMAGE_USAGE_DISPLAY_READ` (and `SHARED_IMAGE_USAGE_SCANOUT` if
// supported and `is_overlay_candidate` is true).
ASH_EXPORT cc::ResourcePool::InUsePoolResource AcquirePooledResource(
    const gfx::Size& size,
    bool is_overlay_candidate,
    cc::ResourcePool& resource_pool,
    gpu::SharedImageInterface* sii,
    std::string_view debug_tag);

// Synchronizes and exports `resource` into `frame->resource_list`.
ASH_EXPORT void PrepareToExportResource(
    cc::ResourcePool::InUsePoolResource& resource,
    cc::ResourcePool& resource_pool,
    viz::ClientResourceProvider& client_resource_provider,
    gpu::SharedImageInterface* sii,
    viz::CompositorFrame& frame);

}  // namespace ash::frame_sink_utils

#endif  // ASH_FRAME_SINK_FRAME_SINK_UTILS_H_
