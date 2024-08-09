// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/hardware_renderer.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/aw_gl_surface.h"
#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/display_webview.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/overlay_processor_webview.h"
#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"
#include "android_webview/browser/gfx/task_queue_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/frame_interval_decider.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {
namespace {

BASE_FEATURE(kWebViewUseOutputSurfaceClipRect,
             "WebViewUseOutputSurfaceClipRect",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDrawAndSwapInjectLatency,
             "DrawAndSwapInjectLatency",
             base::FEATURE_DISABLED_BY_DEFAULT);

class ScopedAcquireExternalContext {
 public:
  ScopedAcquireExternalContext(gpu::SharedContextState* state,
                               gl::GLSurface* surface,
                               bool is_angle)
      : state_(state), surface_(surface), is_angle_(is_angle) {
    if (is_angle_) {
      // When using ANGLE, need to make sure ANGLE's internals are in sync
      // with the external context.

      // If the context has changed, make sure it gets current now.
      if (!state_->context()->IsCurrent(surface_)) {
        state_->MakeCurrent(surface_);
      }

      eglAcquireExternalContextANGLE(state_->display()->GetDisplay(),
                                     surface_->GetHandle());
    } else {
      // When not using ANGLE, fake context and surface are used, so the
      // MakeCurrent calls are cheap.
      state_->MakeCurrent(surface_);
    }
  }
  ~ScopedAcquireExternalContext() {
    if (is_angle_) {
      eglReleaseExternalContextANGLE(state_->display()->GetDisplay());
    } else {
      state_->ReleaseCurrent(surface_);
    }
  }

 private:
  const raw_ptr<gpu::SharedContextState> state_;
  raw_ptr<gl::GLSurface> surface_;
  const bool is_angle_;
};

void MoveCopyRequests(CopyOutputRequestQueue* from,
                      CopyOutputRequestQueue* to) {
  std::move(from->begin(), from->end(), std::back_inserter(*to));
  from->clear();
}

viz::BeginFrameArgs NewerBeginFrameArgs(const viz::BeginFrameArgs& args1,
                                        const viz::BeginFrameArgs& args2) {
  return args1.frame_id.IsNextInSequenceTo(args2.frame_id) ? args1 : args2;
}

enum WebViewDrawAndSubmissionType : uint8_t {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kNoInvalidateNoSubmissionSameParams = 0,
  kNoInvalidateNoSubmissionDifferentParams = 1,
  kNoInvalidateSubmittedFrameSameParams = 2,
  kNoInvalidateSubmittedFrameDifferentParams = 3,
  kInvalidateNoSubmissionSameParams = 4,
  kInvalidateNoSubmissionDifferentParams = 5,
  kInvalidateSubmittedFrameSameParams = 6,
  kInvalidateSubmittedFrameDifferentParams = 7,
  kMaxValue = kInvalidateSubmittedFrameDifferentParams
};

WebViewDrawAndSubmissionType GetDrawAndSubmissionType(bool invalidated,
                                                      bool submitted_frame,
                                                      bool params_changed) {
  if (invalidated) {
    if (submitted_frame) {
      return params_changed ? kInvalidateSubmittedFrameDifferentParams
                            : kInvalidateSubmittedFrameSameParams;
    } else {
      return params_changed ? kInvalidateNoSubmissionDifferentParams
                            : kInvalidateNoSubmissionSameParams;
    }
  } else {
    if (submitted_frame) {
      return params_changed ? kNoInvalidateSubmittedFrameDifferentParams
                            : kNoInvalidateSubmittedFrameSameParams;
    } else {
      return params_changed ? kNoInvalidateNoSubmissionDifferentParams
                            : kNoInvalidateNoSubmissionSameParams;
    }
  }
}

}  // namespace

class HardwareRenderer::OnViz : public viz::DisplayClient {
 public:
  OnViz(OutputSurfaceProviderWebView* output_surface_provider,
        const scoped_refptr<RootFrameSink>& root_frame_sink);

  OnViz(const OnViz&) = delete;
  OnViz& operator=(const OnViz&) = delete;

  ~OnViz() override;

  void DrawAndSwapOnViz(const gfx::Size& viewport,
                        const gfx::Rect& clip,
                        const gfx::Transform& transform,
                        const viz::SurfaceId& child_id,
                        float device_scale_factor,
                        const gfx::ColorSpace& color_space,
                        bool overlays_enabled_by_hwui,
                        ChildFrame* child_frame);
  void PostDrawOnViz(viz::FrameTimingDetailsMap* timing_details,
                     std::vector<pid_t>* rendering_thread_ids,
                     base::TimeDelta* preferred_frame_interval);
  void RemoveOverlaysOnViz();
  void MarkAllowContextLossOnViz();

  OverlayProcessorWebView* overlay_processor() {
    return overlay_processor_webview_;
  }

  // viz::DisplayClient overrides.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      viz::AggregatedRenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void DisplayAddChildWindowToBrowser(
      gpu::SurfaceHandle child_window) override {}
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
  std::unique_ptr<viz::BeginFrameSource> stub_begin_frame_source_;
  std::unique_ptr<DisplayWebView> display_;

  std::unique_ptr<viz::HitTestAggregator> hit_test_aggregator_;
  viz::SurfaceId child_surface_id_;
  const bool viz_frame_submission_;
  const bool use_new_invalidate_heuristic_;
  bool expect_context_loss_ = false;

  // Initialized in ctor and never changes, so it's safe to access from both
  // threads. Can be null, if overlays are disabled.
  raw_ptr<OverlayProcessorWebView> overlay_processor_webview_ = nullptr;

  base::PlatformThreadId browser_io_thread_id_ = base::kInvalidThreadId;

  base::TimeDelta preferred_frame_interval_;

  THREAD_CHECKER(viz_thread_checker_);
};

HardwareRenderer::OnViz::OnViz(
    OutputSurfaceProviderWebView* output_surface_provider,
    const scoped_refptr<RootFrameSink>& root_frame_sink)
    : without_gpu_(root_frame_sink),
      frame_sink_id_(without_gpu_->root_frame_sink_id()),
      viz_frame_submission_(::features::IsUsingVizFrameSubmissionForWebView()),
      use_new_invalidate_heuristic_(
          ::features::UseWebViewNewInvalidateHeuristic()) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);

  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
      display_controller = output_surface_provider->CreateDisplayController();
  std::unique_ptr<viz::OutputSurface> output_surface =
      output_surface_provider->CreateOutputSurface(display_controller.get());

  stub_begin_frame_source_ = std::make_unique<viz::StubBeginFrameSource>();

  display_ = DisplayWebView::Create(
      output_surface_provider->renderer_settings(),
      output_surface_provider->debug_settings(), frame_sink_id_,
      std::move(display_controller), std::move(output_surface),
      GetFrameSinkManager(), without_gpu_.get());
  display_->Initialize(this, GetFrameSinkManager()->surface_manager());
  overlay_processor_webview_ = display_->overlay_processor();

  display_->SetVisible(true);
  display_->DisableGPUAccessByDefault();

  if (viz::FrameIntervalDecider* decider = display_->frame_interval_decider()) {
    viz::FrameIntervalDecider::Settings settings;
    std::vector<std::unique_ptr<viz::FrameIntervalMatcher>> matchers;
    matchers.push_back(std::make_unique<viz::InputBoostMatcher>());
    matchers.push_back(std::make_unique<viz::OnlyVideoMatcher>());
    matchers.push_back(std::make_unique<viz::OnlyAnimatingImageMatcher>());

    // Raw `self` pointer is safe because this owns viz::Display which owns
    // viz::FrameIntervalDecider. So this pointer is guaranteed to be valid for
    // the lifetime of viz::FrameIntervalDecider.
    settings.result_callback = base::BindRepeating(
        [](HardwareRenderer::OnViz* self,
           viz::FrameIntervalDecider::Result result,
           viz::FrameIntervalMatcherType matcher_type) {
          self->preferred_frame_interval_ = absl::visit(
              base::Overloaded(
                  [](viz::FrameIntervalDecider::FrameIntervalClass
                         frame_interval_class) {
                    // Zero currently is interpreted by WebView as no opinion,
                    // which allows system to use its default heuristics.
                    return base::Milliseconds(0);
                  },
                  [](base::TimeDelta interval) { return interval; }),
              result);
        },
        this);
    decider->UpdateSettings(std::move(settings), std::move(matchers));
  }
}

HardwareRenderer::OnViz::~OnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  if (child_surface_id_.is_valid()) {
    without_gpu_->EvictChildSurface(child_surface_id_);
  }

  if (root_local_surface_id_.is_valid()) {
    without_gpu_->EvictRootSurface(root_local_surface_id_);
  }

  GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
}

void HardwareRenderer::OnViz::DrawAndSwapOnViz(
    const gfx::Size& viewport,
    const gfx::Rect& clip,
    const gfx::Transform& transform,
    const viz::SurfaceId& child_id,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool overlays_enabled_by_hwui,
    ChildFrame* child_frame) {
  TRACE_EVENT1("android_webview", "HardwareRenderer::DrawAndSwap", "child_id",
               child_id.ToString());
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(child_id.is_valid());
  DCHECK(child_frame);

  if (child_frame->frame) {
    DCHECK(!viz_frame_submission_);
    DCHECK(!child_frame->rendered);
    // Browser thread is trusted, and can be saved straight away.
    // Renderer threads are not trusted, and need to go through verification
    // in SubmitChildCompositorFrame before being reported to the ADPF session.
    browser_io_thread_id_ = child_frame->browser_io_thread_id;
    without_gpu_->SubmitChildCompositorFrame(child_frame);
  }

  gfx::Size frame_size = without_gpu_->GetChildFrameSize();

  if (!child_frame->copy_requests.empty()) {
    viz::FrameSinkManagerImpl* manager = GetFrameSinkManager();
    CopyOutputRequestQueue requests;
    requests.swap(child_frame->copy_requests);
    for (auto& copy_request : requests) {
      manager->RequestCopyOfOutput(child_id, std::move(copy_request),
                                   /*capture_exact_surface_id=*/false);
    }
  }

  if (overlay_processor_webview_) {
    overlay_processor_webview_->SetOverlaysEnabledByHWUI(
        overlays_enabled_by_hwui);
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

  const bool use_output_surface_clip_rect =
      base::FeatureList::IsEnabled(kWebViewUseOutputSurfaceClipRect);

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_to_target_transform = transform;
  quad_state->quad_layer_rect = gfx::Rect(frame_size);
  quad_state->visible_quad_layer_rect = gfx::Rect(frame_size);
  quad_state->opacity = 1.f;

  // We don't need to clip render pass if we apply clip on the viz::Display
  // level.
  if (!use_output_surface_clip_rect) {
    quad_state->clip_rect = clip;
  }

  viz::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                       gfx::Rect(quad_state->quad_layer_rect),
                       viz::SurfaceRange(std::nullopt, child_id),
                       SkColors::kWhite,
                       /*stretch_content_to_fill_bounds=*/false);

  viz::CompositorFrame frame;
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.render_pass_list.push_back(std::move(render_pass));
  frame.metadata.device_scale_factor = device_scale_factor;

  if (child_surface_id_ != child_id) {
    if (child_surface_id_.frame_sink_id() != child_id.frame_sink_id()) {
      hit_test_aggregator_ = std::make_unique<viz::HitTestAggregator>(
          GetFrameSinkManager()->hit_test_manager(), GetFrameSinkManager(),
          display_.get(), child_id.frame_sink_id());
    }
    child_surface_id_ = child_id;
    GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
  }

  {
    std::vector<viz::SurfaceRange> child_ranges;
    child_ranges.emplace_back(child_surface_id_);
    frame.metadata.referenced_surfaces = std::move(child_ranges);
  }

  const auto& local_surface_id =
      without_gpu_->SubmitRootCompositorFrame(std::move(frame));

  if (use_new_invalidate_heuristic_) {
    auto root_surface_id =
        viz::SurfaceId(without_gpu_->root_frame_sink_id(), local_surface_id);

    const auto& current_frame_id = child_frame->begin_frame_args.frame_id;
    const auto& root_frame_sink_id = root_surface_id.frame_sink_id();
    const auto& child_frame_sink_id = child_surface_id_.frame_sink_id();

    // Each OnDraw on UI we get new ChildFrame. Without OnDraw we can't modify
    // contents of the webview or it will break HWUI damage tracking, so only
    // commit if the frame is new.
    const bool commit_child_frames = !child_frame->rendered;

    base::flat_set<viz::SurfaceId> manual_surfaces;
    auto commit_predicate = [&](const viz::SurfaceId& surface_id,
                                const viz::BeginFrameId& frame_id) {
      const bool is_root_surface =
          surface_id.frame_sink_id() == root_frame_sink_id;
      const bool is_main_renderer_surface =
          surface_id.frame_sink_id() == child_frame_sink_id;

      // If we have uncommitted main renderer frame, `commit_child_frames`
      // must be true.
      CHECK(!is_main_renderer_surface || commit_child_frames);

      if (!commit_child_frames) {
        // Commit only root frame, all child surfaces can be committed only
        // if we did have Draw on UI thread.
        return is_root_surface;
      }

      // Always commit frame from different begin frame sources, because we
      // can't order with them.
      if (frame_id.source_id != current_frame_id.source_id) {
        // We always should have single source_id except for the manual
        // acks.
        DCHECK_EQ(frame_id.source_id, viz::BeginFrameArgs::kManualSourceId);

        // For manual acks commit only one frame at time to avoid excessive
        // frame drops.
        auto [_, inserted] = manual_surfaces.insert(surface_id);
        return inserted;
      }

      // Commit all frames that are older than current one.
      if (frame_id.sequence_number < current_frame_id.sequence_number) {
        return true;
      }

      // All clients except main renderer and root surface are frame behind.
      const bool is_frame_behind =
          !is_main_renderer_surface && !is_root_surface;

      // If this surface is not frame behind, commit it for current frame
      // too.
      if (!is_frame_behind &&
          frame_id.sequence_number == current_frame_id.sequence_number) {
        return true;
      }

      return false;
    };

    GetFrameSinkManager()->surface_manager()->CommitFramesInRangeRecursively(
        viz::SurfaceRange(root_surface_id), commit_predicate);
  }

  if (root_local_surface_id_ != local_surface_id) {
    root_local_surface_id_ = local_surface_id;
    display_->SetLocalSurfaceId(local_surface_id, device_scale_factor);
  }

  display_->Resize(viewport);

  if (use_output_surface_clip_rect) {
    display_->SetOutputSurfaceClipRect(clip);
  }

  auto now = base::TimeTicks::Now();
  display_->DrawAndSwap({now, now});

  child_frame->rendered = true;
  without_gpu_->SetContainedSurfaces(display_->GetContainedSurfaceIds());
}

void HardwareRenderer::OnViz::PostDrawOnViz(
    viz::FrameTimingDetailsMap* timing_details,
    std::vector<pid_t>* rendering_thread_ids,
    base::TimeDelta* preferred_frame_interval) {
  *timing_details = without_gpu_->TakeChildFrameTimingDetailsMap();

  auto renderer_thread_ids = without_gpu_->GetChildFrameRendererThreadIds();
  *rendering_thread_ids = std::vector<pid_t>(renderer_thread_ids.begin(),
                                             renderer_thread_ids.end());

  auto gpu_thread_ids =
      VizCompositorThreadRunnerWebView::GetInstance()->GetThreadIds();
  std::copy(gpu_thread_ids.begin(), gpu_thread_ids.end(),
            std::back_inserter(*rendering_thread_ids));

  if (browser_io_thread_id_ != base::kInvalidThreadId) {
    rendering_thread_ids->push_back(browser_io_thread_id_);
  }

  *preferred_frame_interval = preferred_frame_interval_;
}

void HardwareRenderer::OnViz::RemoveOverlaysOnViz() {
  if (overlay_processor_webview_) {
    overlay_processor_webview_->RemoveOverlays();
  }
}

void HardwareRenderer::OnViz::MarkAllowContextLossOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  expect_context_loss_ = true;
}

viz::FrameSinkManagerImpl* HardwareRenderer::OnViz::GetFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  return VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager();
}

void HardwareRenderer::OnViz::DisplayOutputSurfaceLost() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  if (!expect_context_loss_) {
    // Android WebView does not handle real context loss.
    LOG(FATAL) << "Render thread context loss";
  }
}

void HardwareRenderer::OnViz::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    viz::AggregatedRenderPassList* render_passes) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  hit_test_aggregator_->Aggregate(child_surface_id_);
}

base::TimeDelta
HardwareRenderer::OnViz::GetPreferredFrameIntervalForFrameSinkId(
    const viz::FrameSinkId& id,
    viz::mojom::CompositorFrameSinkType* type) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  return GetFrameSinkManager()->GetPreferredFrameIntervalForFrameSinkId(id,
                                                                        type);
}

// static
ChildFrameQueue HardwareRenderer::WaitAndPruneFrameQueue(
    ChildFrameQueue* child_frames_ptr) {
  ChildFrameQueue& child_frames = *child_frames_ptr;
  ChildFrameQueue pruned_frames;
  if (child_frames.empty()) {
    return pruned_frames;
  }

  // First find the last non-empty frame.
  int remaining_frame_index = -1;
  for (size_t i = 0; i < child_frames.size(); ++i) {
    auto& child_frame = *child_frames[i];
    child_frame.WaitOnFutureIfNeeded();
    if (child_frame.frame) {
      remaining_frame_index = i;
    }
  }
  // If all empty, keep the last one.
  if (remaining_frame_index < 0) {
    remaining_frame_index = child_frames.size() - 1;
  }

  // Prune end.
  while (child_frames.size() > static_cast<size_t>(remaining_frame_index + 1)) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.back());
    child_frames.pop_back();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames[remaining_frame_index]->copy_requests);

    // If we're dropping frames at the end, we need update begin frame args.
    child_frames[remaining_frame_index]->begin_frame_args = NewerBeginFrameArgs(
        child_frames[remaining_frame_index]->begin_frame_args,
        frame->begin_frame_args);
    // We shouldn't get rendered frames here.
    DCHECK(!frame->frame);
    DCHECK(!frame->rendered);
  }
  DCHECK_EQ(static_cast<size_t>(remaining_frame_index),
            child_frames.size() - 1);

  // Prune front.
  while (child_frames.size() > 1) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.front());
    child_frames.pop_front();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames.back()->copy_requests);
    // We shouldn't drop newer frames.
    DCHECK(!frame->begin_frame_args.frame_id.IsNextInSequenceTo(
        child_frames.back()->begin_frame_args.frame_id));
    // We shouldn't get rendered frames here.
    DCHECK(!frame->rendered);
    if (frame->frame) {
      pruned_frames.emplace_back(std::move(frame));
    }
  }
  return pruned_frames;
}

bool HardwareRendererDrawParams::operator==(
    const HardwareRendererDrawParams& other) const {
  return clip_left == other.clip_left && clip_top == other.clip_top &&
         clip_right == other.clip_right && clip_bottom == other.clip_bottom &&
         width == other.width && height == other.height &&
         color_space == other.color_space &&
         !memcmp(transform, other.transform, sizeof(transform));
}

bool HardwareRendererDrawParams::operator!=(
    const HardwareRendererDrawParams& other) const {
  return !(*this == other);
}

HardwareRenderer::HardwareRenderer(RenderThreadManager* state,
                                   RootFrameSinkGetter root_frame_sink_getter,
                                   AwVulkanContextProvider* context_provider)
    : render_thread_manager_(state),
      last_egl_context_(eglGetCurrentContext()),
      output_surface_provider_(context_provider),
      report_rendering_threads_(
          base::FeatureList::IsEnabled(::features::kWebViewEnableADPF)) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRenderer::InitializeOnViz, base::Unretained(this),
                     std::move(root_frame_sink_getter)));
}

void HardwareRenderer::InitializeOnViz(
    RootFrameSinkGetter root_frame_sink_getter) {
  scoped_refptr<RootFrameSink> root_frame_sink =
      std::move(root_frame_sink_getter).Run();
  if (root_frame_sink) {
    on_viz_ = std::make_unique<OnViz>(&output_surface_provider_,
                                      std::move(root_frame_sink));
  }
}

HardwareRenderer::~HardwareRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  // Do not crash for context loss during destruction. It's possible functor is
  // being destroyed due to an already-detected lost context.
  MarkAllowContextLoss();
  output_surface_provider_.shared_context_state()->MakeCurrent(nullptr);
  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::DoNothingWithBoundArgs(std::move(on_viz_)));

  // Reset draw constraints.
  if (child_frame_) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        ParentCompositorDrawConstraints(), child_frame_->frame_sink_id,
        viz::FrameTimingDetailsMap(), 0u, preferred_frame_interval_);
  }
  for (auto& child_frame : child_frame_queue_) {
    child_frame->WaitOnFutureIfNeeded();
    ReturnChildFrame(std::move(child_frame));
  }
}

bool HardwareRenderer::IsUsingVulkan() const {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(output_surface_provider_.shared_context_state());
  return output_surface_provider_.shared_context_state()->GrContextIsVulkan();
}

bool HardwareRenderer::IsUsingANGLEOverGL() const {
  return !IsUsingVulkan() && gl::GLSurfaceEGL::GetGLDisplayEGL()
                                 ->IsANGLEExternalContextAndSurfaceSupported();
}

void HardwareRenderer::DrawAndSwap(
    const HardwareRendererDrawParams& params,
    const OverlaysParams& overlays_params,
    ReportRenderingThreadsCallback report_rendering_threads_callback) {
  TRACE_EVENT1("android_webview", "HardwareRenderer::Draw", "vulkan",
               IsUsingVulkan());

  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

  if (base::FeatureList::IsEnabled(kDrawAndSwapInjectLatency)) {
    usleep(1000);
  }

  // Ensure that the context is synced from external and synced back before
  // returning. This is only necessary when using ANGLE to keep its internals
  // synced with the external context
  ScopedAcquireExternalContext scoped_acquire(
      output_surface_provider_.shared_context_state().get(),
      output_surface_provider_.gl_surface().get(), IsUsingANGLEOverGL());

  viz::FrameTimingDetailsMap timing_details;

  gfx::Transform transform = gfx::Transform::ColMajorF(params.transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(params.width, params.height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints =
      ParentCompositorDrawConstraints(viewport, transform);
  bool need_to_update_draw_constraints =
      !child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_);

  if (child_frame_) {
    viz::SurfaceId child_surface_id = child_frame_->GetSurfaceId();
    if (child_surface_id.is_valid() && child_surface_id != surface_id_) {
      surface_id_ = child_surface_id;
      device_scale_factor_ = child_frame_->device_scale_factor;
    }
  }

  if (!surface_id_.is_valid()) {
    if (need_to_update_draw_constraints) {
      // FrameSinkId is used only for FrameTimingDetails and we want to update
      // only draw constraints here.
      // TODO(vasilyt): Move frame timing details delivery over to
      // RootFrameSink.
      render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
          draw_constraints, viz::FrameSinkId(), viz::FrameTimingDetailsMap(), 0,
          preferred_frame_interval_);
    }
    return;
  }

  gfx::Rect clip(params.clip_left, params.clip_top,
                 params.clip_right - params.clip_left,
                 params.clip_bottom - params.clip_top);

  output_surface_provider_.gl_surface()->RecalculateClipAndTransform(
      &viewport, &clip, &transform);

  // Reset Skia's state if not using ANGLE. For ANGLE, it is in general not
  // necessary as ANGLE will restore GL context state for us as long as Chrome
  // hasn't mucked with it outside ANGLE's knowledge. There is only one case
  // where Chrome does so: the complex clip case. That case is rare and
  // we can't know a priori whether we are going to hit it for this frame.
  // Hence, rather than resetting Skia state on every frame for ANGLE, we
  // instead detect whether this frame has hit the complex clip case at the end
  // of this function and reset the GR context as needed for ANGLE there.
  if (!gl::GLSurfaceEGL::GetGLDisplayEGL()
           ->IsANGLEExternalContextAndSurfaceSupported()) {
    DCHECK(output_surface_provider_.shared_context_state());
    output_surface_provider_.shared_context_state()
        ->PessimisticallyResetGrContext();
  }

  std::optional<OverlayProcessorWebView::ScopedSurfaceControlAvailable>
      allow_surface_control;

  auto* overlay_processor = on_viz_->overlay_processor();
  const bool can_use_overlays =
      overlays_params.overlays_mode == OverlaysParams::Mode::Enabled &&
      !output_surface_provider_.gl_surface()->IsDrawingToFBO();
  if (can_use_overlays && overlay_processor) {
    DCHECK(overlays_params.get_surface_control);
    allow_surface_control.emplace(overlay_processor,
                                  overlays_params.get_surface_control);
  }

  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRenderer::OnViz::DrawAndSwapOnViz,
                     base::Unretained(on_viz_.get()), viewport, clip, transform,
                     surface_id_, device_scale_factor_, params.color_space,
                     can_use_overlays, child_frame_.get()));

  MergeTransactionIfNeeded(overlays_params.merge_transaction);

  output_surface_provider_.gl_surface()->MaybeDidPresent(
      gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                0 /* flags */));

  // Implement proper damage tracking, then deliver FrameTimingDetails
  // through the common begin frame path.
  std::vector<pid_t> rendering_thread_ids;
  base::TimeDelta preferred_frame_interval;
  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRenderer::OnViz::PostDrawOnViz,
                     base::Unretained(on_viz_.get()), &timing_details,
                     &rendering_thread_ids, &preferred_frame_interval));
  if (report_rendering_threads_ && report_rendering_threads_callback) {
    std::move(report_rendering_threads_callback)
        .Run(rendering_thread_ids.data(), rendering_thread_ids.size());
  }

  bool frame_interval_changed =
      preferred_frame_interval_ != preferred_frame_interval;
  preferred_frame_interval_ = preferred_frame_interval;

  if (need_to_update_draw_constraints || !timing_details.empty() ||
      frame_interval_changed) {
    // |frame_token| will be reported through the FrameSinkManager so we pass 0
    // here.
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, child_frame_->frame_sink_id,
        std::move(timing_details), 0, preferred_frame_interval_);
  }

  // If using ANGLE we have not reset Skia's state at the beginning of the draw,
  // as in general ANGLE will take care of saving/restoring GL state. However,
  // it is necessary to reset Skia's state in the complex clip case, as Chrome
  // mucks with GL state outside of ANGLE's knowledge in handling this case. We
  // need to do this check at the end of the frame as it is only at this point
  // that we know whether this frame hit the complex clip case. (For non-ANGLE
  // we need to reset Skia's state at the beginning of each draw in any case, so
  // doing it here would be redundant).
  if (gl::GLSurfaceEGL::GetGLDisplayEGL()
          ->IsANGLEExternalContextAndSurfaceSupported() &&
      output_surface_provider_.gl_surface()->IsDrawingToFBO()) {
    DCHECK(output_surface_provider_.shared_context_state());
    output_surface_provider_.shared_context_state()
        ->PessimisticallyResetGrContext();
  }
}

void HardwareRenderer::RemoveOverlays(
    OverlaysParams::MergeTransactionFn merge_transaction) {
  VizCompositorThreadRunnerWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&HardwareRenderer::OnViz::RemoveOverlaysOnViz,
                     base::Unretained(on_viz_.get())));

  MergeTransactionIfNeeded(merge_transaction);
}

void HardwareRenderer::MergeTransactionIfNeeded(
    OverlaysParams::MergeTransactionFn merge_transaction) {
  auto* overlay_processor = on_viz_->overlay_processor();
  if (overlay_processor) {
    auto transaction = overlay_processor->TakeSurfaceTransactionOnRT();
    if (transaction) {
      DCHECK(merge_transaction);
      merge_transaction(transaction->GetTransaction());
    }
  }
}

void HardwareRenderer::AbandonContext() {
  MarkAllowContextLoss();
  output_surface_provider_.shared_context_state()->MarkContextLost(
      gpu::error::ContextLostReason::kUnknown);
}

void HardwareRenderer::MarkAllowContextLoss() {
  if (on_viz_) {
    VizCompositorThreadRunnerWebView::GetInstance()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&HardwareRenderer::OnViz::MarkAllowContextLossOnViz,
                       base::Unretained(on_viz_.get())));
  }
  output_surface_provider_.MarkAllowContextLoss();
}

void HardwareRenderer::CommitFrame() {
  TRACE_EVENT0("android_webview", "CommitFrame");
  scroll_offset_ = render_thread_manager_->GetScrollOffsetOnRT();
  ChildFrameQueue child_frames = render_thread_manager_->PassFramesOnRT();
  // |child_frames| should have at most one non-empty frame, and one current
  // and unwaited frame, in that order.
  DCHECK_LE(child_frames.size(), 2u);
  if (child_frames.empty()) {
    return;
  }
  // Insert all except last, ie current frame.
  while (child_frames.size() > 1u) {
    child_frame_queue_.emplace_back(std::move(child_frames.front()));
    child_frames.pop_front();
  }
  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_)) {
    ReturnChildFrame(std::move(pruned_frame));
  }
  DCHECK_LE(child_frame_queue_.size(), 1u);
  child_frame_queue_.emplace_back(std::move(child_frames.front()));
}

void HardwareRenderer::ReportDrawMetric(
    const HardwareRendererDrawParams& params) {
  const bool params_changed = last_draw_params_ == params;

  auto type = GetDrawAndSubmissionType(
      did_invalidate_, did_submit_compositor_frame_, params_changed);
  UMA_HISTOGRAM_ENUMERATION("Android.WebView.Gfx.HardwareDrawType", type);

  last_draw_params_ = params;
  did_invalidate_ = false;
  did_submit_compositor_frame_ = false;
}

void HardwareRenderer::Draw(
    const HardwareRendererDrawParams& params,
    const OverlaysParams& overlays_params,
    ReportRenderingThreadsCallback report_rendering_threads_callback) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::Draw");

  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_)) {
    ReturnChildFrame(std::move(pruned_frame));
  }
  DCHECK_LE(child_frame_queue_.size(), 1u);
  if (!child_frame_queue_.empty()) {
    child_frame_ = std::move(child_frame_queue_.front());
    child_frame_queue_.clear();

    did_invalidate_ = child_frame_->did_invalidate;
    did_submit_compositor_frame_ = !!child_frame_->frame;
  }
  // 0u is not a valid frame_sink_id, but can happen when renderer did not
  // produce a frame. Keep the existing id in that case.
  if (child_frame_ && child_frame_->layer_tree_frame_sink_id > 0u) {
    last_committed_layer_tree_frame_sink_id_ =
        child_frame_->layer_tree_frame_sink_id;
  }

  ReportDrawMetric(params);

  if (last_egl_context_ && !IsUsingVulkan() && !IsUsingANGLEOverGL()) {
    // We need to watch if the current Android context has changed and enforce a
    // clean-up in the compositor.  This is only necessary for the validating
    // command decoder.
    EGLContext current_context = eglGetCurrentContext();
    DCHECK(current_context) << "Draw called without EGLContext";

    // TODO(boliu): Handle context loss.
    if (last_egl_context_ != current_context) {
      DLOG(WARNING) << "EGLContextChanged";
    }
  }

  DrawAndSwap(params, overlays_params,
              std::move(report_rendering_threads_callback));
}

void HardwareRenderer::ReturnChildFrame(
    std::unique_ptr<ChildFrame> child_frame) {
  if (!child_frame || !child_frame->frame) {
    return;
  }

  std::vector<viz::ReturnedResource> resources_to_return =
      viz::TransferableResource::ReturnResources(
          child_frame->frame->resource_list);

  // The child frame's frame_sink_id is not necessarily same as
  // |child_frame_sink_id_|.
  ReturnResourcesToCompositor(std::move(resources_to_return),
                              child_frame->frame_sink_id,
                              child_frame->layer_tree_frame_sink_id);
}

void HardwareRenderer::ReturnResourcesToCompositor(
    std::vector<viz::ReturnedResource> resources,
    const viz::FrameSinkId& frame_sink_id,
    uint32_t layer_tree_frame_sink_id) {
  render_thread_manager_->InsertReturnedResourcesOnRT(
      std::move(resources), frame_sink_id, layer_tree_frame_sink_id);
}

void HardwareRenderer::SetChildFrameForTesting(
    std::unique_ptr<ChildFrame> child_frame) {
  child_frame_ = std::move(child_frame);
}

}  // namespace android_webview
