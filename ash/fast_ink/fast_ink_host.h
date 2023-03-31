// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_HOST_H_
#define ASH_FAST_INK_FAST_INK_HOST_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/canvas.h"

namespace gfx {
class GpuMemoryBuffer;
struct PresentationFeedback;
}  // namespace gfx

namespace fast_ink {

// FastInkHost is used to support low-latency rendering. It supports
// 'auto-refresh' mode which provide minimum latency updates for the
// associated window. 'auto-refresh' mode will take advantage of HW overlays
// when possible and trigger continuous updates.
class FastInkHost {
 public:
  // Convert the rect in window's coordinate to the buffer's coordinate.  If the
  // window is rotated, the damaged_rect will also be rotated, for example. The
  // size is clamped by |buffer_size| to ensure it does not exceeds the buffer
  // size.
  static gfx::Rect BufferRectFromWindowRect(
      const gfx::Transform& window_to_buffer_transform,
      const gfx::Size& buffer_size,
      const gfx::Rect& damage_rect);

  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;

  // Creates a FastInkView.
  FastInkHost(aura::Window* host_window,
              const PresentationCallback& presentation_callback);
  ~FastInkHost();
  FastInkHost(const FastInkHost&) = delete;
  FastInkHost& operator=(const FastInkHost&) = delete;

  // Update content and damage rectangles for surface. |auto_refresh| should
  // be set to true if continuous updates are expected within content rectangle.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool auto_refresh);

  aura::Window* host_window() { return host_window_; }
  const gfx::Transform& window_to_buffer_transform() const {
    return window_to_buffer_transform_;
  }
  gfx::GpuMemoryBuffer* gpu_memory_buffer() { return gpu_memory_buffer_.get(); }

 private:
  class LayerTreeFrameSinkHolder;
  struct Resource;

  void SubmitCompositorFrame();
  void SubmitPendingCompositorFrame();
  void ReclaimResource(std::unique_ptr<Resource> resource);
  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

  aura::Window* host_window_;
  const PresentationCallback presentation_callback_;
  gfx::Transform window_to_buffer_transform_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  // The size of |gpu_memory_buffer_|.
  gfx::Size buffer_size_;
  // The bounds of the content to be pushed in window coordinates.
  gfx::Rect content_rect_;
  // The damage rect in window coordinates.
  gfx::Rect damage_rect_;
  // When true it keeps pushing entire buffer with hw overlay option.
  bool auto_refresh_ = false;
  bool pending_compositor_frame_ = false;
  bool pending_compositor_frame_ack_ = false;
  viz::ResourceIdGenerator id_generator_;
  // Cached resources that can be reused.
  std::vector<std::unique_ptr<Resource>> returned_resources_;
  std::unique_ptr<LayerTreeFrameSinkHolder> frame_sink_holder_;
  base::WeakPtrFactory<FastInkHost> weak_ptr_factory_{this};
};

}  // namespace fast_ink

#endif  // ASH_FAST_INK_FAST_INK_HOST_H_
