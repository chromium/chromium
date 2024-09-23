// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_tree_frame_sink.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace cc {

static constexpr viz::FrameSinkId kLayerTreeFrameSinkId(1, 1);

TestLayerTreeFrameSink::TestLayerTreeFrameSink(
    scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const viz::RendererSettings& renderer_settings,
    const viz::DebugRendererSettings* const debug_settings,
    TaskRunnerProvider* task_runner_provider,
    bool synchronous_composite,
    bool disable_display_vsync,
    double refresh_rate,
    viz::BeginFrameSource* begin_frame_source)
    : LayerTreeFrameSink(
          std::move(compositor_context_provider),
          worker_context_provider
              ? base::MakeRefCounted<RasterContextProviderWrapper>(
                    std::move(worker_context_provider),
                    /*dark_mode_filter=*/nullptr,
                    ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
                        /*for_renderer=*/false))
              : nullptr,
          task_runner_provider->HasImplThread()
              ? task_runner_provider->ImplThreadTaskRunner()
              : task_runner_provider->MainThreadTaskRunner(),
          gpu_memory_buffer_manager,
          /*shared_image_interface=*/nullptr),
      synchronous_composite_(synchronous_composite),
      disable_display_vsync_(disable_display_vsync),
      renderer_settings_(renderer_settings),
      debug_settings_(debug_settings),
      refresh_rate_(refresh_rate),
      frame_sink_id_(kLayerTreeFrameSinkId),
      shared_image_manager_(
          std::make_unique<gpu::SharedImageManager>(/*thread_safe=*/true)),
      sync_point_manager_(std::make_unique<gpu::SyncPointManager>()),
      gpu_scheduler_(
          std::make_unique<gpu::Scheduler>(sync_point_manager_.get())),
      parent_local_surface_id_allocator_(
          new viz::ParentLocalSurfaceIdAllocator),
      client_provided_begin_frame_source_(begin_frame_source),
      external_begin_frame_source_(this),
      task_runner_provider_(task_runner_provider) {
  parent_local_surface_id_allocator_->GenerateId();
}

TestLayerTreeFrameSink::~TestLayerTreeFrameSink() = default;

void TestLayerTreeFrameSink::SetDisplayColorSpace(
    const gfx::ColorSpace& display_color_space) {
  display_color_spaces_ = gfx::DisplayColorSpaces(display_color_space);
  if (display_)
    display_->SetDisplayColorSpaces(display_color_spaces_);
}

bool TestLayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (!LayerTreeFrameSink::BindToClient(client))
    return false;

  shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams(shared_bitmap_manager_.get()));
  frame_sink_manager_->SetSharedImageInterfaceProviderForTest(
      &shared_image_interface_provider_);

  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
      display_controller;
  std::unique_ptr<viz::OutputSurface> display_output_surface;
  const bool gpu_accelerated = context_provider();
  if (gpu_accelerated) {
    display_controller = test_client_->CreateDisplayController();
    auto output_surface =
        test_client_->CreateSkiaOutputSurface(display_controller.get());
    display_output_surface = std::move(output_surface);
  } else {
    display_output_surface = test_client_->CreateSoftwareOutputSurface();
  }

  std::unique_ptr<viz::DisplayScheduler> scheduler;
  if (!synchronous_composite_) {
    if (client_provided_begin_frame_source_) {
      display_begin_frame_source_ = client_provided_begin_frame_source_;
    } else if (disable_display_vsync_) {
      begin_frame_source_ = std::make_unique<viz::BackToBackBeginFrameSource>(
          std::make_unique<viz::DelayBasedTimeSource>(
              compositor_task_runner_.get()));
      display_begin_frame_source_ = begin_frame_source_.get();
    } else {
      begin_frame_source_ = std::make_unique<viz::DelayBasedBeginFrameSource>(
          std::make_unique<viz::DelayBasedTimeSource>(
              compositor_task_runner_.get()),
          viz::BeginFrameSource::kNotRestartableId);
      begin_frame_source_->OnUpdateVSyncParameters(
          base::TimeTicks::Now(), base::Milliseconds(1000.f / refresh_rate_));
      display_begin_frame_source_ = begin_frame_source_.get();
    }
    scheduler = std::make_unique<viz::DisplayScheduler>(
        display_begin_frame_source_, compositor_task_runner_.get(),
        display_output_surface->capabilities().pending_swap_params);
  }

  auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();
  // Normally display will need to take ownership of a
  // gpu::GpuTaskschedulerhelper in order to keep it alive to share between the
  // output surface and the overlay processor. In this case the overlay
  // processor is only a stub, and viz::TestGpuServiceHolder will keep a
  // gpu::GpuTaskSchedulerHelper alive for output surface to use, so there is no
  // need to pass in an gpu::GpuTaskSchedulerHelper here.

  display_ = std::make_unique<viz::Display>(
      shared_bitmap_manager_.get(), shared_image_manager_.get(),
      sync_point_manager_.get(), gpu_scheduler_.get(), renderer_settings_,
      debug_settings_, frame_sink_id_, std::move(display_controller),
      std::move(display_output_surface), std::move(overlay_processor),
      std::move(scheduler), compositor_task_runner_);

  constexpr bool is_root = true;
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, frame_sink_manager_.get(), frame_sink_id_, is_root);
  support_->SetWantsAnimateOnlyBeginFrames();
  client_->SetBeginFrameSource(&external_begin_frame_source_);
  if (display_begin_frame_source_) {
    frame_sink_manager_->RegisterBeginFrameSource(display_begin_frame_source_,
                                                  frame_sink_id_);
  }
  display_->Initialize(this, frame_sink_manager_->surface_manager());
  display_->renderer_for_testing()->SetEnlargePassTextureAmountForTesting(
      enlarge_pass_texture_amount_);
  display_->SetDisplayColorSpaces(display_color_spaces_);
  display_->SetVisible(true);
  return true;
}

void TestLayerTreeFrameSink::UnregisterBeginFrameSource() {
  if (display_begin_frame_source_) {
    frame_sink_manager_->UnregisterBeginFrameSource(
        display_begin_frame_source_);
    display_begin_frame_source_ = nullptr;
  }
}

void TestLayerTreeFrameSink::DetachFromClient() {
  // This acts like the |shared_bitmap_manager_| is a global object, while
  // in fact it is tied to the lifetime of this class and is destroyed below:
  // The shared_bitmap_manager_ has ownership of shared memory for each
  // SharedBitmapId that has been reported from the client. Since the client is
  // gone that memory can be freed. If we don't then it would leak.
  DebugScopedSetImplThread impl(task_runner_provider_);
  for (const auto& id : owned_bitmaps_)
    shared_bitmap_manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.clear();

  if (display_begin_frame_source_) {
    frame_sink_manager_->UnregisterBeginFrameSource(
        display_begin_frame_source_);
    display_begin_frame_source_ = nullptr;
  }
  client_->SetBeginFrameSource(nullptr);
  support_ = nullptr;
  display_ = nullptr;
  begin_frame_source_ = nullptr;
  parent_local_surface_id_allocator_ = nullptr;
  frame_sink_manager_ = nullptr;
  shared_bitmap_manager_ = nullptr;
  test_client_ = nullptr;
  LayerTreeFrameSink::DetachFromClient();
}

void TestLayerTreeFrameSink::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  test_client_->DisplayReceivedLocalSurfaceId(local_surface_id);
}

void TestLayerTreeFrameSink::SubmitCompositorFrame(viz::CompositorFrame frame,
                                                   bool hit_test_data_changed) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK(frame.metadata.begin_frame_ack.frame_id.IsSequenceValid());
  test_client_->DisplayReceivedCompositorFrame(frame);

  gfx::Size frame_size = frame.size_in_pixels();
  float device_scale_factor = frame.device_scale_factor();
  viz::LocalSurfaceId local_surface_id =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();

  if (frame_size != display_size_ ||
      device_scale_factor != device_scale_factor_) {
    parent_local_surface_id_allocator_->GenerateId();
    local_surface_id =
        parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
    display_->SetLocalSurfaceId(local_surface_id, device_scale_factor);
    display_->Resize(frame_size);
    display_size_ = frame_size;
    device_scale_factor_ = device_scale_factor;
  }

  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  if (!display_->has_scheduler()) {
    display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    // Post this to get a new stack frame so that we exit this function before
    // calling the client to tell it that it is done.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestLayerTreeFrameSink::SendCompositorFrameAckToClient,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TestLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                FrameSkippedReason reason) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  DCHECK(!ack.has_damage);
  DCHECK(ack.frame_id.IsSequenceValid());
  support_->DidNotProduceFrame(ack);
}

void TestLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  bool ok =
      shared_bitmap_manager_->ChildAllocatedSharedBitmap(region.Map(), id);
  DCHECK(ok);
  owned_bitmaps_.insert(id);
}

void TestLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  shared_bitmap_manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

void TestLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  ReclaimResources(std::move(resources));
  // In synchronous mode, we manually send acks and this method should not be
  // used.
  if (!display_->has_scheduler())
    return;
  client_->DidReceiveCompositorFrameAck();
}

void TestLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details,
    bool frame_ack,
    std::vector<viz::ReturnedResource> resources) {
  // We do not want to Ack the first OnBeginFrame. Only deliver Acks once there
  // is a valid activated surface, and we have pending frames.
  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }
  DebugScopedSetImplThread impl(task_runner_provider_);
  for (const auto& pair : timing_details)
    client_->DidPresentCompositorFrame(pair.first, pair.second);
  external_begin_frame_source_.OnBeginFrame(args);
}

void TestLayerTreeFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  client_->ReclaimResources(std::move(resources));
}

void TestLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  external_begin_frame_source_.OnSetBeginFrameSourcePaused(paused);
}

void TestLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  client_->DidLoseLayerTreeFrameSink();
}

void TestLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    viz::AggregatedRenderPassList* render_passes) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  test_client_->DisplayWillDrawAndSwap(will_draw_and_swap, render_passes);
}

void TestLayerTreeFrameSink::DisplayDidDrawAndSwap() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  test_client_->DisplayDidDrawAndSwap();
}

void TestLayerTreeFrameSink::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {}

void TestLayerTreeFrameSink::DisplayDidCompleteSwapWithSize(
    const gfx::Size& pixel_Size) {}

void TestLayerTreeFrameSink::DisplayAddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {}

void TestLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void TestLayerTreeFrameSink::SendCompositorFrameAckToClient() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  client_->DidReceiveCompositorFrameAck();
}

base::TimeDelta TestLayerTreeFrameSink::GetPreferredFrameIntervalForFrameSinkId(
    const viz::FrameSinkId& id,
    viz::mojom::CompositorFrameSinkType* type) {
  return viz::BeginFrameArgs::MinInterval();
}

}  // namespace cc
