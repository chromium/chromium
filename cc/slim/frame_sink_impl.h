// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_IMPL_H_
#define CC_SLIM_FRAME_SINK_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/slim/frame_sink.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc::slim {

class FrameSinkImplClient;

// Slim implementation of FrameSink.
// * Owns mojo interfaces to viz and responsible for submitting frames and
//   issuing BeginFrame to client.
// * Owns ContextProvider.
// * Listen and respond to context loss or GPU process crashes.
// * Manage uploading UIResource.
class FrameSinkImpl : public FrameSink,
                      public viz::ContextLostObserver,
                      public viz::mojom::CompositorFrameSinkClient {
 public:
  ~FrameSinkImpl() override;

  viz::ContextProvider* context_provider() const {
    return context_provider_.get();
  }
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);

  // Called by LayerTree.
  bool BindToClient(FrameSinkImplClient* client);
  void SetNeedsBeginFrame(bool needs_begin_frame);
  void UploadUIResource(cc::UIResourceId resource_id,
                        cc::UIResourceBitmap resource_bitmap);
  void MarkUIResourceForDeletion(cc::UIResourceId resource_id);
  bool HasResourceToDraw(cc::UIResourceId id);
  bool IsUIResourceOpaque(cc::UIResourceId resource_id);
  gfx::Size GetUIResourceSize(cc::UIResourceId resource_id);

  // viz::ContextLostObserver
  void OnContextLost() override;

  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& begin_frame_args,
                    const viz::FrameTimingDetailsMap& timing_details) override;
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}

 private:
  friend class FrameSink;

  struct UploadedUIResource {
    UploadedUIResource();
    ~UploadedUIResource();
    UploadedUIResource(const UploadedUIResource&);
    UploadedUIResource& operator=(const UploadedUIResource&);

    gpu::Mailbox mailbox;
    gfx::Size size;
    bool is_opaque = true;
    viz::ResourceId viz_resource_id;
  };

  FrameSinkImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
                    compositor_frame_sink_associated_remote,
                mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
                    client_receiver,
                scoped_refptr<viz::ContextProvider> context_provider);

  using UploadedResourceMap =
      base::flat_map<cc::UIResourceId, UploadedUIResource>;
  void UIResourceReleased(cc::UIResourceId ui_resource_id,
                          const gpu::SyncToken& sync_token,
                          bool is_lost);

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
      pending_compositor_frame_sink_associated_remote_;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
      pending_client_receiver_;

  mojo::AssociatedRemote<viz::mojom::CompositorFrameSink> frame_sink_remote_;
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient> client_receiver_{this};
  scoped_refptr<viz::ContextProvider> context_provider_;
  raw_ptr<FrameSinkImplClient> client_ = nullptr;
  viz::LocalSurfaceId local_surface_id_;

  UploadedResourceMap uploaded_resources_;
  viz::ClientResourceProvider resource_provider_;
  // Last `HitTestRegionList` sent to viz.
  absl::optional<viz::HitTestRegionList> hit_test_region_list_;

  bool needs_begin_frame_ = false;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_IMPL_H_
