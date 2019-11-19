// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FAST_INK_FAST_INK_VIEW_H_
#define ASH_COMPONENTS_FAST_INK_FAST_INK_VIEW_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class GpuMemoryBuffer;
struct PresentationFeedback;
}

namespace views {
class Widget;
}

namespace fast_ink {

// FastInkView is a view supporting low-latency rendering. The view can enter
// 'auto-refresh' mode in order to provide minimum latency updates for the
// associated widget. 'auto-refresh' mode will take advantage of HW overlays
// when possible and trigger continious updates.
class FastInkView : public views::View {
 public:
  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;

  // Creates a FastInkView filling the bounds of |root_window|.
  // If |root_window| is resized (e.g. due to a screen size change),
  // a new instance of FastInkView should be created.
  FastInkView(aura::Window* container,
              const PresentationCallback& presentation_callback);
  ~FastInkView() override;

 protected:
  // Helper class that provides flicker free painting to a GPU memory buffer.
  class ScopedPaint {
   public:
    ScopedPaint(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                const gfx::Transform& screen_to_buffer_transform,
                const gfx::Rect& rect);
    ~ScopedPaint();

    gfx::Canvas& canvas() { return canvas_; }

   private:
    gfx::GpuMemoryBuffer* const gpu_memory_buffer_;
    const gfx::Rect buffer_rect_;
    gfx::Canvas canvas_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPaint);
  };

  // Update content and damage rectangles for surface. |auto_refresh| should
  // be set to true if continuous updates are expected within content rectangle.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool auto_refresh);

  // Constants initialized in constructor.
  const PresentationCallback presentation_callback_;
  gfx::Transform screen_to_buffer_transform_;
  gfx::Size buffer_size_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

 private:
  class LayerTreeFrameSinkHolder;
  struct Resource;

  void SubmitCompositorFrame();
  void SubmitPendingCompositorFrame();
  void ReclaimResource(std::unique_ptr<Resource> resource);
  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

  std::unique_ptr<views::Widget> widget_;
  gfx::Rect content_rect_;
  gfx::Rect damage_rect_;
  bool auto_refresh_ = false;
  bool pending_compositor_frame_ = false;
  bool pending_compositor_frame_ack_ = false;
  int next_resource_id_ = 1;
  viz::FrameTokenGenerator next_frame_token_;
  std::vector<std::unique_ptr<Resource>> returned_resources_;
  std::unique_ptr<LayerTreeFrameSinkHolder> frame_sink_holder_;
  base::WeakPtrFactory<FastInkView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FastInkView);
};

}  // namespace fast_ink

#endif  // ASH_COMPONENTS_FAST_INK_FAST_INK_VIEW_H_
