// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_RASTER_BUFFER_PROVIDER_H_

#include <stddef.h>
#include <memory>
#include <vector>

#include "cc/raster/raster_buffer.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/tile_task.h"
#include "cc/resources/resource_pool.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
}

namespace cc {

class CC_EXPORT RasterBufferProvider {
 public:
  RasterBufferProvider();
  virtual ~RasterBufferProvider();

  // Utility function that will create a temporary bitmap and copy pixels to
  // |memory| when necessary. The |canvas_bitmap_rect| is the rect of the bitmap
  // being played back in the pixel space of the source, ie a rect in the source
  // that will cover the resulting |memory|. The |canvas_playback_rect| can be a
  // smaller contained rect inside the |canvas_bitmap_rect| if the |memory| is
  // already partially complete, and only the subrect needs to be played back.
  // Set |gpu_compositing| to true if the compositor is using gpu, as we respect
  // the format more accurately, vs in software compositing where the format is
  // a placeholder for the skia native format.
  static void PlaybackToMemory(
      void* memory,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      size_t stride,
      const RasterSource* raster_source,
      const gfx::Rect& canvas_bitmap_rect,
      const gfx::Rect& canvas_playback_rect,
      const gfx::AxisTransform2d& transform,
      const gfx::ColorSpace& target_color_space,
      bool gpu_compositing,
      const RasterSource::PlaybackSettings& playback_settings);

  // Acquire raster buffer.
  virtual std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) = 0;

  // Returns the format to use for the tiles.
  virtual viz::SharedImageFormat GetFormat() const = 0;

  // Determines if the resource is premultiplied.
  virtual bool IsResourcePremultiplied() const = 0;

  // Determine if the RasterBufferProvider can handle partial raster into
  // the Resource provided in AcquireBufferForRaster.
  virtual bool CanPartialRasterIntoProvidedResource() const = 0;

  // Returns true if the indicated resource is ready to draw.
  virtual bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) = 0;

  // Calls the provided |callback| when the provided |resources| are ready to
  // draw. Returns a callback ID which can be used to track this callback.
  // Will return 0 if no callback is needed (resources are already ready to
  // draw). The caller may optionally pass the ID of a pending callback to
  // avoid creating a new callback unnecessarily. If the caller does not
  // have a pending callback, 0 should be passed for |pending_callback_id|.
  virtual uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) = 0;

  // Sets an event, guaranteed to live past this object's lifetime, that is
  // signalled when the TileManger is cancelling tasks. Subclasses can use
  // this as an argument to GpuMemoryBufferManager::CreateGpuMemoryBuffer to
  // avoid deadlocks when TileManager is cancelling tasks.
  virtual void SetShutdownEvent(base::WaitableEvent* shutdown_event) {}

  // Shutdown for doing cleanup.
  virtual void Shutdown() = 0;

  // Must be called when the contents of a buffer acquired for raster are
  // updated. This should be done after all GPU commands for the raster work
  // have been issued.
  void NotifyWorkSubmitted() { needs_flush_ = true; }

 protected:
  void FlushIfNeeded();

  // Flush pending work from writing the content of the RasterBuffer, so that
  // queries to tell if the backing is ready to draw from will get the right
  // answer. This should be done before calling IsResourceReadyToDraw() or
  // SetReadyToDrawCallback().
  virtual void Flush() = 0;

 private:
  bool needs_flush_ = false;
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_BUFFER_PROVIDER_H_
