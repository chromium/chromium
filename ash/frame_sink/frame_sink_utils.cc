// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_utils.h"

#include <string_view>

#include "base/check.h"
#include "base/logging.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace ash::frame_sink_utils {
namespace {

constexpr viz::SharedImageFormat kSharedImageFormat =
    SK_B32_SHIFT ? viz::SinglePlaneFormat::kRGBA_8888
                 : viz::SinglePlaneFormat::kBGRA_8888;

}  // namespace

scoped_refptr<viz::RasterContextProvider> GetContextProvider() {
  return aura::Env::GetInstance()
      ->context_factory()
      ->SharedMainThreadRasterContextProvider();
}

cc::ResourcePool::InUsePoolResource AcquirePooledResource(
    const gfx::Size& size,
    bool is_overlay_candidate,
    cc::ResourcePool& resource_pool,
    gpu::SharedImageInterface* sii,
    std::string_view debug_tag) {
  DCHECK(sii);
  cc::ResourcePool::InUsePoolResource resource = resource_pool.AcquireResource(
      size, kSharedImageFormat, gfx::ColorSpace());
  if (!resource) {
    return resource;
  }

  if (!resource.backing()) {
    auto backing = std::make_unique<cc::ResourcePool::Backing>(
        resource.size(), resource.format(), resource.color_space());

    gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (is_overlay_candidate &&
        sii->GetCapabilities().supports_scanout_shared_images) {
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }

    if (!backing->CreateSharedImage(sii, usage, debug_tag,
                                    gfx::BufferUsage::SCANOUT_CPU_READ_WRITE)) {
      LOG(ERROR) << "Failed to create MappableSharedImage";
      resource_pool.ReleaseResource(std::move(resource));
      return cc::ResourcePool::InUsePoolResource();
    }

    resource.set_backing(std::move(backing));
  }

  return resource;
}

void PrepareToExportResource(
    cc::ResourcePool::InUsePoolResource& resource,
    cc::ResourcePool& resource_pool,
    viz::ClientResourceProvider& client_resource_provider,
    gpu::SharedImageInterface* sii,
    viz::CompositorFrame& frame) {
  resource.backing()->mailbox_sync_token =
      resource.backing()->shared_image()->BackingWasExternallyUpdated(
          resource.backing()->returned_sync_token);

  resource_pool.PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kUI);

  client_resource_provider.PrepareSendToParent(
      {resource.resource_id_for_export()}, &frame.resource_list, sii);
}

}  // namespace ash::frame_sink_utils
