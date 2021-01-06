// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/hardware_renderer_viz.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/aw_gl_surface.h"
#include "android_webview/browser/gfx/aw_render_thread_context_provider.h"
#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"
#include "android_webview/browser/gfx/surfaces_instance.h"
#include "android_webview/browser/gfx/task_queue_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

class HardwareRendererViz::OnViz : public viz::DisplayClient {
 public:
  OnViz(OutputSurfaceProviderWebView* output_surface_provider,
        const scoped_refptr<RootFrameSink>& root_frame_sink);
  ~OnViz() override;

  void DrawAndSwapOnViz(const gfx::Size& viewport,
                        const gfx::Rect& clip,
                        const gfx::Transform& transform,
                        const viz::SurfaceId& child_id,
                        float device_scale_factor,
                        const gfx::ColorSpace& color_space,
                        ChildFrame* child_frame);
  void PostDrawOnViz(viz::FrameTimingDetailsMap* timing_details);

  // viz::DisplayClient overrides.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      viz::AggregatedRenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const viz::FrameSinkId& id,
      viz::mojom::CompositorFrameSinkType* type) override;

 private:
  viz::FrameSinkManagerImpl* GetFrameSinkManager();

  scoped_refptr<RootFrameSink> without_gpu_;

  const viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId root_local_surface_id_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  std::unique_ptr<viz::BeginFrameSource> stub_begin_frame_source_;
  std::unique_ptr<viz::Display> display_;

  std::unique_ptr<viz::HitTestAggregator> hit_test_aggregator_;
  viz::SurfaceId child_surface_id_;
  viz::FrameTokenGenerator next_frame_token_;
  gfx::Size surface_size_;
  const bool viz_frame_submission_;

  THREAD_CHECKER(viz_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(OnViz);
};

HardwareRendererViz::OnViz::OnViz(
    OutputSurfaceProviderWebView* output_surface_provider,
    const scoped_refptr<RootFrameSink>& root_frame_sink)
    : without_gpu_(root_frame_sink),
      frame_sink_id_(without_gpu_->root_frame_sink_id()),
      viz_frame_submission_(features::IsUsingVizFrameSubmissionForWebView()) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);

  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
      display_controller = output_surface_provider->CreateDisplayController();
  std::unique_ptr<viz::OutputSurface> output_surface =
      output_surface_provider->CreateOutputSurface(display_controller.get());

  stub_begin_frame_source_ = std::make_unique<viz::StubBeginFrameSource>();
  auto scheduler =
      std::make_unique<DisplaySchedulerWebView>(without_gpu_.get());
  auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();

  // Android WebView has no overlay processor, and does not need to share
  // gpu_task_scheduler, so it is passed in as nullptr.
  // TODO(weiliangc): Android WebView should support overlays. Change initialize
  // order to make this happen.
  display_ = std::make_unique<viz::Display>(
      nullptr /* shared_bitmap_manager */,
      output_surface_provider->renderer_settings(),
      output_surface_provider->debug_settings(), frame_sink_id_,
      std::move(display_controller), std::move(output_surface),
      std::move(overlay_processor), std::move(scheduler),
      nullptr /* current_task_runner */);
  display_->Initialize(this, GetFrameSinkManager()->surface_manager(),
                       output_surface_provider->enable_shared_image());

  display_->SetVisible(true);
  display_->DisableGPUAccessByDefault();
}

HardwareRendererViz::OnViz::~OnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
}

void HardwareRendererViz::OnViz::DrawAndSwapOnViz(
    const gfx::Size& viewport,
    const gfx::Rect& clip,
    const gfx::Transform& transform,
    const viz::SurfaceId& child_id,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    ChildFrame* child_frame) {
  TRACE_EVENT1("android_webview", "HardwareRendererViz::DrawAndSwap",
               "child_id", child_id.ToString());
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(child_id.is_valid());
  DCHECK(child_frame);

  if (child_frame->frame) {
    DCHECK(!viz_frame_submission_);
    without_gpu_->SubmitChildCompositorFrame(child_frame);
  }

  gfx::Size frame_size = without_gpu_->GetChildFrameSize();

  if (!child_frame->copy_requests.empty()) {
    viz::FrameSinkManagerImpl* manager = GetFrameSinkManager();
    CopyOutputRequestQueue requests;
    requests.swap(child_frame->copy_requests);
    for (auto& copy_request : requests) {
      manager->RequestCopyOfOutput(child_id, std::move(copy_request));
    }
  }

  gfx::DisplayColorSpaces display_color_spaces(
      color_space.IsValid() ? color_space : gfx::ColorSpace::CreateSRGB());
  display_->SetDisplayColorSpaces(display_color_spaces);

  // Create a frame with a single SurfaceDrawQuad referencing the child
  // Surface and transformed using the given transform.
  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(viewport), clip,
                      gfx::Transform());
  render_pass->has_transparent_background = false;

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_to_target_transform = transform;
  quad_state->quad_layer_rect = gfx::Rect(frame_size);
  quad_state->visible_quad_layer_rect = gfx::Rect(frame_size);
  quad_state->clip_rect = clip;
  quad_state->is_clipped = true;
  quad_state->opacity = 1.f;

  viz::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                       gfx::Rect(quad_state->quad_layer_rect),
                       viz::SurfaceRange(base::nullopt, child_id),
                       SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false);

  viz::CompositorFrame frame;
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.render_pass_list.push_back(std::move(render_pass));
  frame.metadata.device_scale_factor = device_scale_factor;
  frame.metadata.frame_token = ++next_frame_token_;

  if (!root_local_surface_id_.is_valid() || viewport != surface_size_ ||
      child_surface_id_ != child_id) {
    parent_local_surface_id_allocator_.GenerateId();
    root_local_surface_id_ =
        parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    surface_size_ = viewport;
    display_->SetLocalSurfaceId(root_local_surface_id_, device_scale_factor);

    if (child_surface_id_ != child_id) {
      if (child_surface_id_.frame_sink_id() != child_id.frame_sink_id()) {
        hit_test_aggregator_ = std::make_unique<viz::HitTestAggregator>(
            GetFrameSinkManager()->hit_test_manager(), GetFrameSinkManager(),
            display_.get(), child_id.frame_sink_id());
      }
      child_surface_id_ = child_id;
      GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
    }
  }

  {
    std::vector<viz::SurfaceRange> child_ranges;
    child_ranges.emplace_back(child_surface_id_);
    frame.metadata.referenced_surfaces = std::move(child_ranges);
  }

  without_gpu_->support()->SubmitCompositorFrame(root_local_surface_id_,
                                                 std::move(frame));
  display_->Resize(viewport);
  display_->DrawAndSwap(base::TimeTicks::Now());
}

void HardwareRendererViz::OnViz::PostDrawOnViz(
    viz::FrameTimingDetailsMap* timing_details) {
  *timing_details = without_gpu_->TakeChildFrameTimingDetailsMap();
}

viz::FrameSinkManagerImpl* HardwareRendererViz::OnViz::GetFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  return VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager();
}

void HardwareRendererViz::OnViz::DisplayOutputSurfaceLost() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  // Android WebView does not handle context loss.
  LOG(FATAL) << "Render thread context loss";
}

void HardwareRendererViz::OnViz::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    viz::AggregatedRenderPassList* render_passes) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  hit_test_aggregator_->Aggregate(child_surface_id_, render_passes);
}

base::TimeDelta
HardwareRendererViz::OnViz::GetPreferredFrameIntervalForFrameSinkId(
    const viz::FrameSinkId& id,
    viz::mojom::CompositorFrameSinkType* type) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  return GetFrameSinkManager()->GetPreferredFrameIntervalForFrameSinkId(id,
                                                                        type);
}

HardwareRendererViz::HardwareRendererViz(
    RenderThreadManager* state,
    RootFrameSinkGetter root_frame_sink_getter,
    AwVulkanContextProvider* context_provider)
    : HardwareRenderer(state), output_surface_provider_(context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(output_surface_provider_.renderer_settings().use_skia_renderer);

  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRendererViz::InitializeOnViz,
                     base::Unretained(this),
                     std::move(root_frame_sink_getter)));
}

void HardwareRendererViz::InitializeOnViz(
    RootFrameSinkGetter root_frame_sink_getter) {
  scoped_refptr<RootFrameSink> root_frame_sink =
      std::move(root_frame_sink_getter).Run();
  if (root_frame_sink) {
    on_viz_ = std::make_unique<OnViz>(&output_surface_provider_,
                                      std::move(root_frame_sink));
  }
}

HardwareRendererViz::~HardwareRendererViz() {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRendererViz::DestroyOnViz,
                     base::Unretained(this)));
}

void HardwareRendererViz::DestroyOnViz() {
  on_viz_ = nullptr;
}

bool HardwareRendererViz::IsUsingVulkan() const {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(output_surface_provider_.shared_context_state());
  return output_surface_provider_.shared_context_state()->GrContextIsVulkan();
}

void HardwareRendererViz::DrawAndSwap(const HardwareRendererDrawParams& params,
                                      const OverlaysParams& overlays_params) {
  TRACE_EVENT1("android_webview", "HardwareRendererViz::Draw", "vulkan",
               IsUsingVulkan());
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

  viz::FrameTimingDetailsMap timing_details;

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(params.transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(params.width, params.height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints =
      ParentCompositorDrawConstraints(viewport, transform);
  bool need_to_update_draw_constraints =
      !child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_);

  if (!child_frame_)
    return;

  viz::SurfaceId child_surface_id = child_frame_->GetSurfaceId();
  if (child_surface_id.is_valid() && child_surface_id != surface_id_) {
    surface_id_ = child_surface_id;
    device_scale_factor_ = child_frame_->device_scale_factor;
  }

  if (!surface_id_.is_valid())
    return;

  gfx::Rect clip(params.clip_left, params.clip_top,
                 params.clip_right - params.clip_left,
                 params.clip_bottom - params.clip_top);

  output_surface_provider_.gl_surface()->RecalculateClipAndTransform(
      &viewport, &clip, &transform);

  DCHECK(output_surface_provider_.shared_context_state());
  output_surface_provider_.shared_context_state()
      ->PessimisticallyResetGrContext();

  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRendererViz::OnViz::DrawAndSwapOnViz,
                     base::Unretained(on_viz_.get()), viewport, clip, transform,
                     surface_id_, device_scale_factor_, params.color_space,
                     child_frame_.get()));

  output_surface_provider_.gl_surface()->MaybeDidPresent(
      gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                0 /* flags */));

  // Implement proper damage tracking, then deliver FrameTimingDetails
  // through the common begin frame path.
  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRendererViz::OnViz::PostDrawOnViz,
                     base::Unretained(on_viz_.get()), &timing_details));

  if (need_to_update_draw_constraints || !timing_details.empty()) {
    // |frame_token| will be reported through the FrameSinkManager so we pass 0
    // here.
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, child_frame_->frame_sink_id,
        std::move(timing_details), 0);
  }
}

void HardwareRendererViz::RemoveOverlays(
    OverlaysParams::MergeTransactionFn merge_transaction) {
  NOTIMPLEMENTED();
}

}  // namespace android_webview
