// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_HOST_H_
#define ASH_FAST_INK_FAST_INK_HOST_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/frame_sink_host.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/canvas.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace gfx {
class Rect;
}  // namespace gfx

namespace gpu {
class ClientSharedImage;
}

namespace ash {

// FastInkHost is used to support low-latency rendering. It supports
// 'auto-refresh' mode which provide minimum latency updates for the
// associated window. 'auto-refresh' mode will take advantage of HW overlays
// when possible and trigger continuous updates.
class ASH_EXPORT FastInkHost : public FrameSinkHost {
 public:
  // Provides flicker free painting to a mappable SharedImage.
  class ScopedPaint {
   public:
    ScopedPaint(const FastInkHost* host,
                const gfx::Rect& damage_rect_in_window);

    ScopedPaint(const ScopedPaint&) = delete;
    ScopedPaint& operator=(const ScopedPaint&) = delete;

    ~ScopedPaint();

    gfx::Canvas& canvas() { return canvas_; }

   private:
    const raw_ptr<FastInkHost> host_;

    // Damage rect in the buffer coordinates.
    gfx::Rect damage_rect_;
    gfx::Canvas canvas_;
  };

  FastInkHost();

  FastInkHost(const FastInkHost&) = delete;
  FastInkHost& operator=(const FastInkHost&) = delete;

  ~FastInkHost() override;

  std::unique_ptr<FastInkHost::ScopedPaint> CreateScopedPaint(
      const gfx::Rect& damage_rect_in_window) const;

  const gfx::Transform& window_to_buffer_transform() const {
    return window_to_buffer_transform_;
  }

  gpu::ClientSharedImage* client_si_for_test() const {
    return client_shared_image_.get();
  }

  int get_pending_bitmaps_size_for_test() const {
    return pending_bitmaps_.size();
  }

  // FrameSinkHost:
  void Init(aura::Window* host_window) override;
  void InitForTesting(
      aura::Window* host_window,
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) override;

 protected:
  // FrameSinkHost:
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_update,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) override;
  void OnFirstFrameRequested() override;

 private:
  void InitBufferMetadata(aura::Window* host_window);
  void InitializeFastInkBuffer(aura::Window* host_window);
  gfx::Rect BufferRectFromWindowRect(const gfx::Rect& rect_in_window) const;
  void Draw(SkBitmap bitmap, const gfx::Rect& damage_rect);
  void DrawBitmap(SkBitmap bitmap, const gfx::Rect& damage_rect);

  gfx::Transform window_to_buffer_transform_;

  gfx::Size buffer_size_;

  struct PendingBitmap {
    SkBitmap bitmap;
    gfx::Rect damage_rect;
  };

  std::vector<PendingBitmap> pending_bitmaps_;

  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
  gpu::SyncToken sync_token_;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_HOST_H_
