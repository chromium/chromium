// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_IMPL_H_
#define CC_SLIM_FRAME_SINK_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/slim/frame_sink.h"
#include "cc/slim/scheduler.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace gpu {
class ClientSharedImage;
}

namespace cc::slim {

class FrameSinkImplClient;
class Scheduler;
class TestFrameSinkImpl;

// Slim implementation of FrameSink.
// * Owns mojo interfaces to viz and responsible for submitting frames and
//   issuing BeginFrame to client.
// * Owns context provider.
// * Listen and respond to context loss or GPU process crashes.
// * Manage uploading UIResource.
class COMPONENT_EXPORT(CC_SLIM) FrameSinkImpl
    : public FrameSink,
      public SchedulerClient,
      public viz::ContextLostObserver,
      public viz::mojom::CompositorFrameSinkClient {
 public:
  ~FrameSinkImpl() override;

  viz::RasterContextProvider* context_provider() const {
    return context_provider_.get();
  }
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);

  // Called by LayerTree. Virtual for testing.
  virtual bool BindToClient(FrameSinkImplClient* client);
  virtual void SetNeedsBeginFrame(bool needs_begin_frame);
  void MaybeCompositeNow();
  void UploadUIResource(cc::UIResourceId resource_id,
                        cc::UIResourceBitmap resource_bitmap);
  void MarkUIResourceForDeletion(cc::UIResourceId resource_id);
  viz::ResourceId GetVizResourceId(cc::UIResourceId id);
  bool IsUIResourceOpaque(cc::UIResourceId resource_id);
  gfx::Size GetUIResourceSize(cc::UIResourceId resource_id);
  viz::ClientResourceProvider* client_resource_provider() {
    return &resource_provider_;
  }
  int GetMaxTextureSize() const;

  // viz::ContextLostObserver
  void OnContextLost() override;

  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}
  void OnBeginFrame(const viz::BeginFrameArgs& begin_frame_args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override;

  // SchedulerClient:
  bool DoBeginFrame(const viz::BeginFrameArgs& begin_frame_args) override;
  void SendDidNotProduceFrame(
      const viz::BeginFrameArgs& begin_frame_args) override;

 private:
  friend class FrameSink;
  friend class TestFrameSinkImpl;

  struct UploadedUIResource {
    UploadedUIResource();
    ~UploadedUIResource();
    UploadedUIResource(const UploadedUIResource&);
    UploadedUIResource& operator=(const UploadedUIResource&);

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gfx::Size size;
    bool is_opaque = true;
    viz::ResourceId viz_resource_id;
  };

  FrameSinkImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
                    compositor_frame_sink_associated_remote,
                mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
                    client_receiver,
                scoped_refptr<viz::RasterContextProvider> context_provider,
                base::PlatformThreadId io_thread_id,
                std::unique_ptr<Scheduler> scheduler);

  using UploadedResourceMap =
      base::flat_map<cc::UIResourceId, UploadedUIResource>;
  void UIResourceReleased(cc::UIResourceId ui_resource_id,
                          const gpu::SyncToken& sync_token,
                          bool is_lost);

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const std::unique_ptr<Scheduler> scheduler_;

  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
      pending_compositor_frame_sink_associated_remote_;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
      pending_client_receiver_;

  mojo::AssociatedRemote<viz::mojom::CompositorFrameSink> frame_sink_remote_;
  // Separate from AssociatedRemote above for testing.
  raw_ptr<viz::mojom::CompositorFrameSink, DanglingUntriaged> frame_sink_ =
      nullptr;
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient> client_receiver_{this};
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  raw_ptr<FrameSinkImplClient> client_ = nullptr;
  viz::LocalSurfaceId local_surface_id_;

  UploadedResourceMap uploaded_resources_;
  viz::ClientResourceProvider resource_provider_;
  // Last `HitTestRegionList` sent to viz.
  std::optional<viz::HitTestRegionList> hit_test_region_list_;
  base::PlatformThreadId io_thread_id_;

  viz::LocalSurfaceId last_submitted_local_surface_id_;
  float last_submitted_device_scale_factor_ = 1.f;
  gfx::Size last_submitted_size_in_pixels_;

  uint32_t num_unacked_frames_ = 0u;
  bool needs_begin_frame_ = false;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_IMPL_H_
