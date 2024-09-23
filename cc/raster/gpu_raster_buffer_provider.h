// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/raster_query_queue.h"
#include "cc/trees/raster_capabilities.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gpu {
namespace raster {
class RasterInterface;
}  // namespace raster
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace cc {

class CC_EXPORT GpuRasterBufferProvider : public RasterBufferProvider {
 public:
  static constexpr float kRasterMetricProbability = 0.01;
  GpuRasterBufferProvider(
      viz::RasterContextProvider* compositor_context_provider,
      viz::RasterContextProvider* worker_context_provider,
      const RasterCapabilities& raster_caps,
      const gfx::Size& max_tile_size,
      bool unpremultiply_and_dither_low_bit_depth_tiles,
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
  viz::SharedImageFormat GetFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) override;
  void Shutdown() override;

 protected:
  void Flush() override;

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
    void PlaybackOnWorkerThread(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings,
        const GURL& url);

    void PlaybackOnWorkerThreadInternal(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings,
        const GURL& url,
        RasterQuery* query);

    void RasterizeSource(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& playback_rect,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings);

    // These fields may only be used on the compositor thread.
    const raw_ptr<GpuRasterBufferProvider> client_;
    raw_ptr<GpuRasterBacking> backing_;

    // These fields are for use on the worker thread.
    const gfx::Size resource_size_;
    const viz::SharedImageFormat shared_image_format_;
    const gfx::ColorSpace color_space_;
    const bool resource_has_previous_content_;
    const bool depends_on_at_raster_decodes_;
    const bool depends_on_hardware_accelerated_jpeg_candidates_;
    const bool depends_on_hardware_accelerated_webp_candidates_;
    base::TimeTicks creation_time_;
  };

  bool ShouldUnpremultiplyAndDitherResource(
      viz::SharedImageFormat format) const;

  const raw_ptr<viz::RasterContextProvider> compositor_context_provider_;
  const raw_ptr<viz::RasterContextProvider> worker_context_provider_;
  const viz::SharedImageFormat tile_format_;
  const bool tile_overlay_candidate_;
  const gfx::Size max_tile_size_;

  const raw_ptr<RasterQueryQueue, DanglingUntriaged> pending_raster_queries_;

  const double raster_metric_probability_;
  // Accessed with the worker context lock acquired.
  base::MetricsSubSampler metrics_subsampler_;
  const bool is_using_raw_draw_;
  bool is_using_dmsaa_ = false;
};

}  // namespace cc

#endif  // CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
