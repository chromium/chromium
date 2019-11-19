// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_SINGLE_THREAD_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_SINGLE_THREAD_H_

#include <memory>

#include "android_webview/browser/gfx/hardware_renderer.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace viz {
class CompositorFrameSinkSupport;
class ParentLocalSurfaceIdAllocator;
}  // namespace viz

namespace android_webview {

class SurfacesInstance;

class HardwareRendererSingleThread
    : public HardwareRenderer,
      public viz::mojom::CompositorFrameSinkClient {
 public:
  explicit HardwareRendererSingleThread(RenderThreadManager* state);
  ~HardwareRendererSingleThread() override;

 private:
  // HardwareRenderer implementation.
  void DrawAndSwap(HardwareRendererDrawParams* params) override;

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  void AllocateSurface();
  void DestroySurface();

  void CreateNewCompositorFrameSinkSupport();

  const scoped_refptr<SurfacesInstance> surfaces_;

  // Information about last delegated frame.
  gfx::Size surface_size_;
  float device_scale_factor_ = 0;

  viz::FrameSinkId frame_sink_id_;
  const std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  viz::LocalSurfaceId child_id_;
  viz::FrameSinkId child_frame_sink_id_;
  uint32_t last_submitted_layer_tree_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRendererSingleThread);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_SINGLE_THREAD_H_
