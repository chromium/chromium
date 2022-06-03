// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <random>
#include <vector>

#include "base/time/time.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/raster_query_queue.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gpu {
namespace raster {
class RasterInterface;
}  // namespace raster
}  // namespace gpu

namespace viz {
class ContextProvider;
class RasterContextProvider;
}  // namespace viz

namespace cc {

class CC_EXPORT GpuRasterBufferProvider : public RasterBufferProvider {
 public:
  static constexpr float kRasterMetricProbability = 0.01;
  GpuRasterBufferProvider(
      viz::ContextProvider* compositor_context_provider,
      viz::RasterContextProvider* worker_context_provider,
      bool use_gpu_memory_buffer_resources,
      viz::ResourceFormat tile_format,
      const gfx::Size& max_tile_size,
      bool unpremultiply_and_dither_low_bit_depth_tiles,
      bool enable_oop_rasterization,
      RasterQueryQueue* const pending_raster_queries,
      float raster_metric_probability = kRasterMetricProbability);
  GpuRasterBufferProvider(const GpuRasterBufferProvider&) = delete;
  ~GpuRasterBufferProvider() override;

  GpuRasterBufferProvider& operator=(const GpuRasterBufferProvider&) = delete;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;

  gpu::SyncToken PlaybackOnWorkerThread(
      gpu::Mailbox* mailbox,
      GLenum texture_target,
      bool texture_is_overlay_candidate,
      const gpu::SyncToken& sync_token,
      const gfx::Size& resource_size,
      viz::ResourceFormat resource_format,
      const gfx::ColorSpace& color_space,
      bool resource_has_previous_content,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings,
      const GURL& url,
      base::TimeTicks raster_buffer_creation_time,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates);

 private:
  class GpuRasterBacking;

  class RasterBufferImpl : public RasterBuffer {
   public:
    RasterBufferImpl(GpuRasterBufferProvider* client,
                     const ResourcePool::InUsePoolResource& in_use_resource,
                     GpuRasterBacking* backing,
                     bool resource_has_previous_content,
                     bool depends_on_at_raster_decodes,
                     bool depends_on_hardware_accelerated_jpeg_candidates,
                     bool depends_on_hardware_accelerated_webp_candidates);
    RasterBufferImpl(const RasterBufferImpl&) = delete;
    ~RasterBufferImpl() override;

    RasterBufferImpl& operator=(const RasterBufferImpl&) = delete;

    // Overridden from RasterBuffer:
    void Playback(const RasterSource* raster_source,
                  const gfx::Rect& raster_full_rect,
                  const gfx::Rect& raster_dirty_rect,
                  uint64_t new_content_id,
                  const gfx::AxisTransform2d& transform,
                  const RasterSource::PlaybackSettings& playback_settings,
                  const GURL& url) override;
    bool SupportsBackgroundThreadPriority() const override;

   private:
    // These fields may only be used on the compositor thread.
    GpuRasterBufferProvider* const client_;
    GpuRasterBacking* backing_;

    // These fields are for use on the worker thread.
    const gfx::Size resource_size_;
    const viz::ResourceFormat resource_format_;
    const gfx::ColorSpace color_space_;
    const bool resource_has_previous_content_;
    const bool depends_on_at_raster_decodes_;
    const bool depends_on_hardware_accelerated_jpeg_candidates_;
    const bool depends_on_hardware_accelerated_webp_candidates_;
    const gpu::SyncToken before_raster_sync_token_;
    const GLenum texture_target_;
    const bool texture_is_overlay_candidate_;

    gpu::Mailbox mailbox_;
    // A SyncToken to be returned from the worker thread, and waited on before
    // using the rastered resource.
    gpu::SyncToken after_raster_sync_token_;

    base::TimeTicks creation_time_;
  };

  bool ShouldUnpremultiplyAndDitherResource(viz::ResourceFormat format) const;
  gpu::SyncToken PlaybackOnWorkerThreadInternal(
      gpu::Mailbox* mailbox,
      GLenum texture_target,
      bool texture_is_overlay_candidate,
      const gpu::SyncToken& sync_token,
      const gfx::Size& resource_size,
      viz::ResourceFormat resource_format,
      const gfx::ColorSpace& color_space,
      bool resource_has_previous_content,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings,
      const GURL& url,
      bool depends_on_at_raster_decodes,
      RasterQuery* query);

  viz::ContextProvider* const compositor_context_provider_;
  viz::RasterContextProvider* const worker_context_provider_;
  const bool use_gpu_memory_buffer_resources_;
  const viz::ResourceFormat tile_format_;
  const gfx::Size max_tile_size_;
  const bool enable_oop_rasterization_;

  RasterQueryQueue* const pending_raster_queries_;

  // Accessed with the worker context lock acquired.
  std::mt19937 random_generator_;
  std::bernoulli_distribution bernoulli_distribution_;
};

}  // namespace cc

#endif  // CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
