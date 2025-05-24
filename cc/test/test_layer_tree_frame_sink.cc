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
#include "cc/mojo_embedder/viz_layer_context.h"
#include "cc/test/test_client_shared_image_interface.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace cc {

class TestLayerTreeFrameSink::TestCompositorFrameSinkSupport
    : public viz::CompositorFrameSinkSupport {
 public:
  TestCompositorFrameSinkSupport(viz::mojom::CompositorFrameSinkClient* client,
                                 TestLayerTreeFrameSinkClient* test_client,
                                 TaskRunnerProvider* task_runner_provider,
                                 viz::FrameSinkManagerImpl* frame_sink_manager,
                                 const viz::FrameSinkId& frame_sink_id,
                                 bool is_root,
                                 viz::Display* display)
      : viz::CompositorFrameSinkSupport(client,
                                        frame_sink_manager,
                                        frame_sink_id,
                                        is_root),
        display_(display),
        test_client_(test_client),
        task_runner_provider_(task_runner_provider) {}
  ~TestCompositorFrameSinkSupport() override = default;

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {
    DebugScopedSetImplThread impl(task_runner_provider_);
    test_client_->DisplayReceivedCompositorFrame(frame);

    CHECK(local_surface_id.is_valid()) << "Tests should ensure a valid LSIid";

    if (last_submitted_display_size_ != frame.size_in_pixels() ||
        last_submitted_device_scale_factor_ != frame.device_scale_factor()) {
      CHECK_NE(last_submitted_local_surface_id_, local_surface_id)
          << "Tests should update LSIid when changing display size or scale";
      last_submitted_display_size_ = frame.size_in_pixels();
      last_submitted_device_scale_factor_ = frame.device_scale_factor();
    }

    last_submitted_local_surface_id_ = local_surface_id;

    // Ensure that the display's local surface ID and its size are initialized
    // (note that these calls will be no-ops if already called for this surface
    // ID/device scale factor/frame size on a previous invocation of
    // SubmitCompositorFrame()).
    display_->SetLocalSurfaceId(local_surface_id, frame.device_scale_factor());
    display_->Resize(frame.size_in_pixels());

    viz::CompositorFrameSinkSupport::SubmitCompositorFrame(
        local_surface_id, std::move(frame), hit_test_region_list, submit_time);

    if (!display_->has_scheduler()) {
      // In synchronous mode, we manually issue DrawAndSwap.
      display_->DrawAndSwap({base::TimeTicks::Now(), base::TimeTicks::Now()});
    }
  }

 private:
  raw_ptr<viz::Display> display_;
  raw_ptr<TestLayerTreeFrameSinkClient> test_client_ = nullptr;
  raw_ptr<TaskRunnerProvider> task_runner_provider_;
  gfx::Size last_submitted_display_size_;
  float last_submitted_device_scale_factor_;
  viz::LocalSurfaceId last_submitted_local_surface_id_;
};

class TestLayerTreeFrameSink::TestCompositorFrameSinkImpl
    : public viz::mojom::CompositorFrameSink {
 public:
  explicit TestCompositorFrameSinkImpl(
      viz::CompositorFrameSinkSupport* support,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver)
      : support_(support), receiver_(this, std::move(receiver)) {}
  ~TestCompositorFrameSinkImpl() override = default;

 private:
  // viz::mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override {}
  void SetWantsAnimateOnlyBeginFrames() override {}
  void SetAutoNeedsBeginFrame() override {}
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {}
  void DidNotProduceFrame(const viz::BeginFrameAck& begin_frame_ack) override {}
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override {}
  void InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType type) override {}
  void BindLayerContext(viz::mojom::PendingLayerContextPtr context,
                        bool draw_mode_is_gpu) override;
#if BUILDFLAG(IS_ANDROID)
  void SetThreads(const std::vector<viz::Thread>& threads) override {}
#endif

  raw_ptr<viz::CompositorFrameSinkSupport> support_;
  mojo::Receiver<viz::mojom::CompositorFrameSink> receiver_;
};

void TestLayerTreeFrameSink::TestCompositorFrameSinkImpl::BindLayerContext(
    viz::mojom::PendingLayerContextPtr context,
    bool draw_mode_is_gpu) {
  support_->BindLayerContext(*context, draw_mode_is_gpu);
}

static constexpr viz::FrameSinkId kLayerTreeFrameSinkId(1, 1);

TestLayerTreeFrameSink::TestLayerTreeFrameSink(
    scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
    const viz::RendererSettings& renderer_settings,
    const viz::DebugRendererSettings* const debug_settings,
    TaskRunnerProvider* task_runner_provider,
    bool synchronous_composite,
    bool disable_display_vsync,
    double refresh_rate,
    viz::BeginFrameSource* begin_frame_source)
    : LayerTreeFrameSink(
          std::move(compositor_context_provider),
          std::move(worker_context_provider),
          task_runner_provider->HasImplThread()
              ? task_runner_provider->ImplThreadTaskRunner()
              : task_runner_provider->MainThreadTaskRunner(),
          shared_image_interface
              ? base::MakeRefCounted<TestClientSharedImageInterface>(
                    shared_image_interface)
              : nullptr),
      synchronous_composite_(synchronous_composite),
      disable_display_vsync_(disable_display_vsync),
      renderer_settings_(renderer_settings),
      debug_settings_(debug_settings),
      refresh_rate_(refresh_rate),
      frame_sink_id_(kLayerTreeFrameSinkId),
      client_provided_begin_frame_source_(begin_frame_source),
      external_begin_frame_source_(this),
      task_runner_provider_(task_runner_provider),
      shared_image_interface_provider_(
          shared_image_interface
              ? std::make_unique<viz::TestSharedImageInterfaceProvider>(
                    shared_image_interface)
              : std::make_unique<viz::TestSharedImageInterfaceProvider>()) {
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

  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
  frame_sink_manager_->SetSharedImageInterfaceProviderForTest(
      shared_image_interface_provider_.get());

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

  gpu::SharedImageManager* shared_image_manager = nullptr;
  gpu::Scheduler* gpu_scheduler = nullptr;

  if (!LayerTreeFrameSink::context_provider()) {
    viz::GpuServiceImpl* gpu_service =
        viz::TestGpuServiceHolder::GetInstance()->gpu_service();
    shared_image_manager = gpu_service->shared_image_manager();
    gpu_scheduler = gpu_service->gpu_scheduler();
  }

  display_ = std::make_unique<viz::Display>(
      shared_image_manager, gpu_scheduler, renderer_settings_, debug_settings_,
      frame_sink_id_, std::move(display_controller),
      std::move(display_output_surface), std::move(overlay_processor),
      std::move(scheduler), compositor_task_runner_);

  constexpr bool is_root = true;
  support_ = std::make_unique<TestCompositorFrameSinkSupport>(
      this, test_client_, task_runner_provider_, frame_sink_manager_.get(),
      frame_sink_id_, is_root, display_.get());
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
  DebugScopedSetImplThread impl(task_runner_provider_);

  if (display_begin_frame_source_) {
    frame_sink_manager_->UnregisterBeginFrameSource(
        display_begin_frame_source_);
    display_begin_frame_source_ = nullptr;
  }
  client_->SetBeginFrameSource(nullptr);
  compositor_frame_sink_impl_.reset();
  compositor_frame_sink_remote_.reset();
  support_ = nullptr;
  display_ = nullptr;
  begin_frame_source_ = nullptr;
  frame_sink_manager_ = nullptr;
  test_client_ = nullptr;
  LayerTreeFrameSink::DetachFromClient();
}

void TestLayerTreeFrameSink::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  local_surface_id_ = local_surface_id;
  test_client_->DisplayReceivedLocalSurfaceId(local_surface_id);
}

std::unique_ptr<LayerContext> TestLayerTreeFrameSink::CreateLayerContext(
    LayerTreeHostImpl& host_impl) {
  compositor_frame_sink_impl_ = std::make_unique<TestCompositorFrameSinkImpl>(
      support_.get(),
      compositor_frame_sink_remote_.BindNewPipeAndPassReceiver());
  return std::make_unique<mojo_embedder::VizLayerContext>(
      *compositor_frame_sink_remote_.get(), host_impl);
}

void TestLayerTreeFrameSink::SubmitCompositorFrame(viz::CompositorFrame frame,
                                                   bool hit_test_data_changed) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK(frame.metadata.begin_frame_ack.frame_id.IsSequenceValid());

  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame),
                                  std::nullopt, 0);

  if (!display_->has_scheduler()) {
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
    std::vector<viz::ReturnedResource> resources) {
  if (!resources.empty()) {
    ReclaimResources(std::move(resources));
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
