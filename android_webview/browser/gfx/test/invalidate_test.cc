// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/begin_frame_source_webview.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/hardware_renderer.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/root_frame_sink_proxy.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/browser/gfx/task_queue_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {
namespace {

constexpr gfx::Size kFrameSize(100, 100);
constexpr viz::FrameSinkId kRootClientSinkId(1, 1);
constexpr viz::FrameSinkId kChildClientSinkId(2, 1);

void AppendSurfaceDrawQuad(viz::CompositorRenderPass& render_pass,
                           const viz::SurfaceId& child_id) {
  viz::SharedQuadState* quad_state =
      render_pass.CreateAndAppendSharedQuadState();

  quad_state->quad_to_target_transform = gfx::Transform();
  quad_state->quad_layer_rect = gfx::Rect(kFrameSize);
  quad_state->visible_quad_layer_rect = gfx::Rect(kFrameSize);
  quad_state->clip_rect = gfx::Rect(kFrameSize);
  quad_state->opacity = 1.f;

  viz::SurfaceDrawQuad* surface_quad =
      render_pass.CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                       gfx::Rect(quad_state->quad_layer_rect),
                       viz::SurfaceRange(std::nullopt, child_id),
                       SkColors::kWhite,
                       /*stretch_content_to_fill_bounds=*/false);
}

void AppendSolidColorDrawQuad(viz::CompositorRenderPass& render_pass) {
  viz::SharedQuadState* quad_state =
      render_pass.CreateAndAppendSharedQuadState();

  quad_state->quad_to_target_transform = gfx::Transform();
  quad_state->quad_layer_rect = gfx::Rect(kFrameSize);
  quad_state->visible_quad_layer_rect = gfx::Rect(kFrameSize);
  quad_state->clip_rect = gfx::Rect(kFrameSize);
  quad_state->opacity = 1.f;

  viz::SolidColorDrawQuad* solid_color_quad =
      render_pass.CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_color_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                           gfx::Rect(quad_state->quad_layer_rect),
                           SkColors::kWhite, /*force_anti_aliasing_off=*/false);
}

class VizClient : public viz::mojom::CompositorFrameSinkClient {
 public:
  VizClient(viz::FrameSinkId frame_sink_id,
            int max_pending_frames,
            int frame_rate,
            bool use_begin_frames)
      : max_pending_frames_(max_pending_frames),
        frame_interval_(base::Seconds(1) / frame_rate),
        use_begin_frames_(use_begin_frames) {
    support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
        this,
        VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager(),
        frame_sink_id, false);

    VizCompositorThreadRunnerWebView::GetInstance()
        ->GetFrameSinkManager()
        ->RegisterFrameSinkHierarchy(kRootClientSinkId, frame_sink_id);

    local_surface_id_allocator_.GenerateId();

    if (use_begin_frames_) {
      support_->SetNeedsBeginFrame(true);
    }
  }

  ~VizClient() override {
    VizCompositorThreadRunnerWebView::GetInstance()
        ->GetFrameSinkManager()
        ->UnregisterFrameSinkHierarchy(kRootClientSinkId,
                                       support_->frame_sink_id());
  }

  void SubmitFrameIfNeeded() {
    base::TimeTicks current_frame_time;

    if (use_begin_frames_) {
      if (last_begin_frame_args_.IsValid()) {
        current_frame_time = last_begin_frame_args_.frame_time;
      }
    } else if (submitting_frames_) {
      if (stop_submitting_frames_) {
        submitting_frames_ = false;
      }
      // We don't care about this time
      static base::TimeTicks start_time = base::TimeTicks::Now();
      current_frame_time =
          start_time + submit_counter_++ * (base::Seconds(1) / 60);
    }

    if (!current_frame_time.is_null()) {
      bool need_submit = false;

      if (pending_frames_ < max_pending_frames_) {
        if (last_submitted_time_.is_null()) {
          last_submitted_time_ = current_frame_time;
          need_submit = true;
        } else if (current_frame_time >=
                   last_submitted_time_ + frame_interval_) {
          last_submitted_time_ += frame_interval_;
          need_submit = true;
        }
      }

      // If we unsubscribed from begin frames then submit frame now regardless
      // of our fps throttling.
      if (!submitting_frames_) {
        DCHECK(stop_submitting_frames_);
        DCHECK(pending_frames_ < max_pending_frames_);
        need_submit = true;
      }

      if (need_submit) {
        SubmitFrame();
      } else if (use_begin_frames_) {
        DidNotProduceFrame(viz::BeginFrameAck(last_begin_frame_args_, false));
      }
    }
    last_begin_frame_args_ = viz::BeginFrameArgs();
  }

  viz::SurfaceId GetSurfaceId() {
    return viz::SurfaceId(
        support_->frame_sink_id(),
        local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }

  viz::FrameTimingDetailsMap TakeFrameTimingDetails() {
    for (const auto& feedback : support_->TakeFrameTimingDetailsMap()) {
      DCHECK(!feedbacks_.contains(feedback.first));
      feedbacks_[feedback.first] = feedback.second;
    }

    viz::FrameTimingDetailsMap result;
    std::swap(result, feedbacks_);
    return result;
  }

  size_t frames_submitted() { return frames_submitted_; }

  void MakeNextFrameLast() { stop_submitting_frames_ = true; }

  // viz::mojom::CompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override {
    ReclaimResources(std::move(resources));

    DCHECK_GT(pending_frames_, 0);
    pending_frames_--;
  }
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& feedbacks,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override {
    if (features::IsOnBeginFrameAcksEnabled() && pending_frames_) {
      DidReceiveCompositorFrameAck(std::move(resources));
    }
    for (const auto& feedback : feedbacks) {
      DCHECK(!feedbacks_.contains(feedback.first));
      feedbacks_[feedback.first] = feedback.second;
    }

    DCHECK_GE(frame_interval_, args.interval);

    if (!submitting_frames_) {
      DidNotProduceFrame(viz::BeginFrameAck(args, false));
      return;
    }

    last_begin_frame_args_ = args;

    if (stop_submitting_frames_) {
      submitting_frames_ = false;
      support_->SetNeedsBeginFrame(false);
    }
  }
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    // No resources in this test
    DCHECK(resources.empty());
  }
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}

 private:
  void SubmitFrame() {
    pending_frames_++;
    frames_submitted_++;

    auto ack = use_begin_frames_
                   ? viz::BeginFrameAck(last_begin_frame_args_, true)
                   : viz::BeginFrameAck::CreateManualAckWithDamage();

    auto frame =
        viz::CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kFrameSize), gfx::Rect(kFrameSize))
            .SetBeginFrameAck(ack)
            .Build();
    AppendSolidColorDrawQuad(*frame.render_pass_list.back());
    frame.metadata.frame_token = frames_submitted_;
    support_->SubmitCompositorFrame(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        std::move(frame), std::nullopt);
  }

  void DidNotProduceFrame(const viz::BeginFrameAck& ack) {
    support_->DidNotProduceFrame(ack);
  }

  const int max_pending_frames_;
  const base::TimeDelta frame_interval_;
  const bool use_begin_frames_;

  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;

  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  int pending_frames_ = 0;
  size_t frames_submitted_ = 0;
  bool stop_submitting_frames_ = false;
  bool submitting_frames_ = true;

  // Used if we simulate client that doesn't use BeginFrames to submit frames.
  int submit_counter_ = 0;

  viz::BeginFrameArgs last_begin_frame_args_;
  base::TimeTicks last_submitted_time_;
  viz::FrameTimingDetailsMap feedbacks_;
};

struct PerFrameFlag {
  PerFrameFlag(uint64_t bits) : bits(bits) {}

  static PerFrameFlag AlwaysTrue() { return {static_cast<uint64_t>(-1)}; }
  static PerFrameFlag AlwaysFalse() { return {static_cast<uint64_t>(0)}; }

  bool at(int frame) const {
    DCHECK_LT(frame, 64);
    return (bits & (UINT64_C(1) << frame)) != 0;
  }

  bool IsAlways() const { return bits == static_cast<uint64_t>(-1); }

  bool IsNever() const { return bits == 0; }

  std::string ToString() {
    if (IsNever())
      return "Never";
    if (IsAlways())
      return "Always";
    return "Random";
  }

  uint64_t bits;
};

enum class AlwaysDrawType {
  // No draw happens unless we invalidate for client
  kNone,
  // Invalidate every frame (e.g if app invalidates or root client draws every
  // frame).
  kAlwaysInvalidate,
  // Invalidate only for client, but draw every frame (e.g other views updated
  // in app that intersect webview).
  kAlwaysDraw
};

enum class BeginFrameAckType {
  // Client uses BeginFrames to drive submission
  kBeginFrames,
  // Client uses CreateManualAckWithDamage() and submits frames without
  // BeginFrame
  kManual
};

class InvalidateTest
    : public testing::TestWithParam<testing::tuple<PerFrameFlag,
                                                   PerFrameFlag,
                                                   AlwaysDrawType,
                                                   BeginFrameAckType>>,
      public viz::ExternalBeginFrameSourceClient,
      public RootFrameSinkProxyClient {
 public:
  InvalidateTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {
    TaskQueueWebView::GetInstance()->ResetRenderThreadForTesting();
    begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
    root_frame_sink_proxy_ = std::make_unique<RootFrameSinkProxy>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), this,
        begin_frame_source_.get());

    root_frame_sink_proxy_->AddChildFrameSinkId(kRootClientSinkId);

    GpuServiceWebView::GetInstance();

    // For purpose of this test we don't care about RT/UI communication, so we
    // use single thread for both as we want to control timing of two threads
    // explicitly.
    render_thread_manager_ = std::make_unique<RenderThreadManager>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                  gfx::Size(100, 100));
    DCHECK(surface_);
    DCHECK(surface_->GetHandle());
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    DCHECK(context_);

    context_->MakeCurrent(surface_.get());
    render_thread_manager_->SetRootFrameSinkGetterForTesting(
        root_frame_sink_proxy_->GetRootFrameSinkCallback());
  }

  ~InvalidateTest() override {
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<VizClient> client) {
                         // `client` leaves scope.
                       },
                       std::move(client_)));
    render_thread_manager_->DestroyHardwareRendererOnRT(false, false);
    TaskQueueWebView::GetInstance()->ResetRenderThreadForTesting();
  }

  // viz::ExternalBeginFrameSourceClient
  void OnNeedsBeginFrames(bool needs_begin_frames) override {
    needs_begin_frames_ = needs_begin_frames;
    if (set_needs_begin_frames_closure_)
      std::move(set_needs_begin_frames_closure_).Run();
  }

  // RootFrameSinkProxyClient
  void Invalidate() override { did_invalidate_ = true; }

  void ReturnResourcesFromViz(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      std::vector<viz::ReturnedResource> resources) override {
    // no resources in this test
    DCHECK(resources.empty());
  }

  void OnCompositorFrameTransitionDirectiveProcessed(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) override {}

 protected:
  std::unique_ptr<ChildFrame> CreateChildFrame(
      std::unique_ptr<content::SynchronousCompositor::Frame> frame,
      const viz::BeginFrameArgs& args,
      bool invalidated) {
    auto future =
        base::MakeRefCounted<content::SynchronousCompositor::FrameFuture>();
    future->SetFrame(std::move(frame));

    auto child_frame = std::make_unique<ChildFrame>(
        future, kRootClientSinkId, kFrameSize, gfx::Transform(), false, 1.0f,
        CopyOutputRequestQueue(), /*did_invalidate=*/invalidated, args,
        /*renderer_thread_ids=*/base::flat_set<base::PlatformThreadId>(),
        /*browser_io_thread_id=*/base::kInvalidThreadId);
    return child_frame;
  }

  void DrawOnUI(std::unique_ptr<ChildFrame> frame) {
    render_thread_manager_->SetFrameOnUI(std::move(frame));
  }

  void Sync() { render_thread_manager_->CommitFrameOnRT(); }

  void DrawOnRT(const viz::BeginFrameArgs& args, bool invalidated) {
    HardwareRendererDrawParams params{.clip_left = 0,
                                      .clip_top = 0,
                                      .clip_right = 99,
                                      .clip_bottom = 99,
                                      .width = 100,
                                      .height = 100};
    params.transform[0] = 1.0f;
    params.transform[5] = 1.0f;
    params.transform[10] = 1.0f;
    params.transform[15] = 1.0f;
    params.color_space = gfx::ColorSpace::CreateSRGB();

    render_thread_manager_->DrawOnRT(/*save_restore=*/false, params,
                                     OverlaysParams(),
                                     ReportRenderingThreadsCallback());

    if (invalidated)
      last_invalidated_draw_bf_ = args;
    UpdateFrameTimingDetails();
  }

  bool BeginFrame(const viz::BeginFrameArgs& args) {
    DCHECK(!inside_begin_frame_);

    if (needs_begin_frames_) {
      inside_begin_frame_ = true;
      begin_frame_source_->OnBeginFrame(args);
      inside_begin_frame_ = false;
      root_begin_frames_count_++;
    }

    // Client could have unsubscribed from begin frames or called invalidate
    // without BF, make sure it was propagated to UI thread.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    if (did_invalidate_)
      invalidate_count_++;

    bool result = did_invalidate_;
    did_invalidate_ = false;

    return result;
  }

  void SubmitFrameIfNeeded() {
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE, base::BindOnce(&VizClient::SubmitFrameIfNeeded,
                                  base::Unretained(client_.get())));
  }

  void GetFrameTimingDetailsOnViz(viz::FrameTimingDetailsMap* timings) {
    *timings = client_->TakeFrameTimingDetails();
  }

  void UpdateFrameTimingDetails() {
    viz::FrameTimingDetailsMap timings;
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE, base::BindOnce(&InvalidateTest::GetFrameTimingDetailsOnViz,
                                  base::Unretained(this), &timings));

    for (auto& timing : timings) {
      DCHECK(!child_client_timings_.contains(timing.first));

      // We never should get ahead of hwui. If we have presentation feedback
      // newer then latest invalidated draw, then it must be failed.
      if (timing.first > last_invalidated_draw_bf_.frame_id.sequence_number) {
        LOG_IF(FATAL, !timing.second.presentation_feedback.failed())
            << "Rendered frame: " << timing.first << " ahead of "
            << last_invalidated_draw_bf_.frame_id.sequence_number;
      }
      child_client_timings_[timing.first] = timing.second;
    }
  }

  void SetUpAndDrawFirstFrameOnViz(int max_pending_frames,
                                   int frame_rate,
                                   bool use_begin_frames,
                                   viz::SurfaceId* surface_id) {
    client_ = std::make_unique<VizClient>(
        kChildClientSinkId, max_pending_frames, frame_rate, use_begin_frames);
    *surface_id = client_->GetSurfaceId();
  }

  void SetUpAndDrawFirstFrame(int max_pending_frames, int frame_rate) {
    bool use_begin_frames =
        testing::get<3>(GetParam()) == BeginFrameAckType::kBeginFrames;
    // During initialization client will request for begin frames, we need to
    // wait until that message will reach UI thread.
    base::RunLoop run_loop;
    set_needs_begin_frames_closure_ = run_loop.QuitClosure();

    viz::SurfaceId child_client_surface_id;
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE,
        base::BindOnce(&InvalidateTest::SetUpAndDrawFirstFrameOnViz,
                       base::Unretained(this), max_pending_frames, frame_rate,
                       use_begin_frames, &child_client_surface_id));

    // Wait for OnNeedsBeginFrames to be called.
    if (use_begin_frames) {
      run_loop.Run();
    }

    // Do first draw and to setup embedding.
    auto bf_args = NextBeginFrameArgs();

    auto compositor_frame =
        viz::CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kFrameSize), gfx::Rect(kFrameSize))
            .SetBeginFrameAck(viz::BeginFrameAck(bf_args, true))
            .Build();
    AppendSurfaceDrawQuad(*compositor_frame.render_pass_list.back(),
                          child_client_surface_id);
    compositor_frame.metadata.referenced_surfaces.push_back(
        viz::SurfaceRange(child_client_surface_id));

    auto frame = std::make_unique<content::SynchronousCompositor::Frame>();
    frame->layer_tree_frame_sink_id = 1;
    frame->frame =
        std::make_unique<viz::CompositorFrame>(std::move(compositor_frame));
    root_local_surface_id_allocator_.GenerateId();
    frame->local_surface_id =
        root_local_surface_id_allocator_.GetCurrentLocalSurfaceId();

    BeginFrame(bf_args);
    SubmitFrameIfNeeded();

    // First draw is implicitly invalidated.
    DrawOnUI(CreateChildFrame(std::move(frame), bf_args, /*invalidated=*/true));
    Sync();
    DrawOnRT(bf_args, /*invalidated=*/true);

    // Note, that client is always one frame behind if it uses BeginFrames, so
    // there is no client timings yet.
    ASSERT_EQ(child_client_timings_.size(), use_begin_frames ? 0u : 1u);
  }

  viz::BeginFrameArgs NextBeginFrameArgs() {
    constexpr auto frame_interval = viz::BeginFrameArgs::DefaultInterval();
    auto frame_time = begin_frame_time_;
    begin_frame_time_ += frame_interval;

    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 1, next_begin_frame_sequence_++, frame_time,
        frame_time + frame_interval, frame_interval,
        viz::BeginFrameArgs::NORMAL);
  }

  void DrawLoop(PerFrameFlag client_slow_param,
                PerFrameFlag hwui_slow_param,
                int stop_submitting_after_frames = 10000) {
    const AlwaysDrawType always_draw = testing::get<2>(GetParam());

    viz::BeginFrameArgs delayed_draw_args;
    bool delayed_draw_invalidate = false;

    for (int i = 0; i < 60; i++) {
      const bool client_fast = !client_slow_param.at(i);
      const bool hwui_fast = !hwui_slow_param.at(i);

      if (i == stop_submitting_after_frames) {
        VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
            FROM_HERE, base::BindOnce(&VizClient::MakeNextFrameLast,
                                      base::Unretained(client_.get())));
      }

      auto args = NextBeginFrameArgs();
      bool invalidate = BeginFrame(args);

      if (always_draw == AlwaysDrawType::kAlwaysInvalidate)
        invalidate = true;

      // Fast clients submit right after BF.
      if (client_fast)
        SubmitFrameIfNeeded();

      // If we didn't draw last frame, now it's time.
      if (delayed_draw_args.IsValid()) {
        DrawOnRT(delayed_draw_args, delayed_draw_invalidate);
        delayed_draw_args = viz::BeginFrameArgs();
      }

      if (invalidate) {
        DrawOnUI(CreateChildFrame(nullptr, args, /*invalidated=*/invalidate));
      }

      Sync();

      // If webview invalidated or other views requested to draw, try to draw.
      if (invalidate || always_draw == AlwaysDrawType::kAlwaysDraw) {
        // If this frame hwui is in "fast" mode (i.e RT is not a frame behind)
        // then draw a frame now.
        if (hwui_fast) {
          DrawOnRT(args, invalidate);
          DCHECK(!delayed_draw_args.IsValid());
        } else {
          delayed_draw_args = args;
          delayed_draw_invalidate = invalidate;
        }
      }

      // Slow clients submit after RT draw.
      if (!client_fast) {
        SubmitFrameIfNeeded();
      }

      UpdateFrameTimingDetails();
    }

    // Draw two more frame without client submitting to make sure all
    // frames are drawns and presentation feedback was processed.
    for (int i = 0; i < 2; i++) {
      auto args = NextBeginFrameArgs();
      bool invalidate = BeginFrame(args);

      // Finish draw if it's still pending
      if (delayed_draw_args.IsValid()) {
        DrawOnRT(delayed_draw_args, delayed_draw_invalidate);
      }

      if (invalidate) {
        DrawOnUI(CreateChildFrame(nullptr, args, /*invalidated=*/invalidate));
      }

      Sync();

      DrawOnRT(args, invalidate);
    }

    // Make sure we received all presentation feedback.
    ASSERT_EQ(client_->frames_submitted(), child_client_timings_.size());
  }

  int CountDroppedFrames() {
    int dropped_frames = 0;
    for (auto& timing : child_client_timings_) {
      if (timing.second.presentation_feedback.failed())
        dropped_frames++;
    }
    return dropped_frames;
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<RootFrameSinkProxy> root_frame_sink_proxy_;

  std::unique_ptr<RenderThreadManager> render_thread_manager_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  std::unique_ptr<VizClient> client_;
  viz::ParentLocalSurfaceIdAllocator root_local_surface_id_allocator_;

  uint64_t next_begin_frame_sequence_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  base::TimeTicks begin_frame_time_ = base::TimeTicks::Now();

  viz::FrameTimingDetailsMap child_client_timings_;
  int invalidate_count_ = 0;
  int root_begin_frames_count_ = 0;
  bool needs_begin_frames_ = false;

  bool inside_begin_frame_ = false;
  bool did_invalidate_ = false;
  viz::BeginFrameArgs last_invalidated_draw_bf_;

  base::OnceClosure set_needs_begin_frames_closure_;
};

std::string AlwaysDrawTypeToString(AlwaysDrawType type) {
  switch (type) {
    case AlwaysDrawType::kNone:
      return "";
    case AlwaysDrawType::kAlwaysInvalidate:
      return "AlwaysInvalidate";
    case AlwaysDrawType::kAlwaysDraw:
      return "AlwaysDraw";
  };
}

std::string BeginFrameAckTypeToString(BeginFrameAckType type) {
  switch (type) {
    case BeginFrameAckType::kBeginFrames:
      return "BeginFrames";
    case BeginFrameAckType::kManual:
      return "Manual";
  }
}

std::string TestParamToString(
    const testing::TestParamInfo<testing::tuple<PerFrameFlag,
                                                PerFrameFlag,
                                                AlwaysDrawType,
                                                BeginFrameAckType>>&
        param_info) {
  auto client_slow = testing::get<0>(param_info.param);
  auto hwui_slow = testing::get<1>(param_info.param);
  auto always_draw = testing::get<2>(param_info.param);
  auto begin_frame_type = testing::get<3>(param_info.param);

  return BeginFrameAckTypeToString(begin_frame_type) + "ClientSlow" +
         client_slow.ToString() + "HwuiSlow" + hwui_slow.ToString() +
         AlwaysDrawTypeToString(always_draw);
}

INSTANTIATE_TEST_SUITE_P(
    Stable,
    InvalidateTest,
    ::testing::Combine(::testing::Values(PerFrameFlag::AlwaysFalse(),
                                         PerFrameFlag::AlwaysTrue()),
                       ::testing::Values(PerFrameFlag::AlwaysFalse(),
                                         PerFrameFlag::AlwaysTrue()),
                       ::testing::Values(AlwaysDrawType::kNone,
                                         AlwaysDrawType::kAlwaysInvalidate,
                                         AlwaysDrawType::kAlwaysDraw),
                       ::testing::Values(BeginFrameAckType::kBeginFrames,
                                         BeginFrameAckType::kManual)),
    TestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Random,
    InvalidateTest,
    ::testing::Combine(::testing::Values(PerFrameFlag::AlwaysFalse()),
                       ::testing::Values(PerFrameFlag(0xAAAAAAAAAAAAAAAA)),
                       ::testing::Values(AlwaysDrawType::kNone,
                                         AlwaysDrawType::kAlwaysInvalidate,
                                         AlwaysDrawType::kAlwaysDraw),
                       ::testing::Values(BeginFrameAckType::kBeginFrames,
                                         BeginFrameAckType::kManual)),
    TestParamToString);

TEST_P(InvalidateTest, LowFpsWithMaxFrame1) {
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/30);
  DrawLoop(client_slow, hwui_slow);

  // Due to rounding error (1 / 60 * 2 < 1 / 30) we submit 29 frames instead
  // of 30. Total 30 counting frame from first draw.
  ASSERT_EQ(child_client_timings_.size(), 30u);
  EXPECT_EQ(CountDroppedFrames(), 0);
  EXPECT_LE(invalidate_count_, 31);
}

// Currently we can't reach 60fps with max pending frames 1.
TEST_P(InvalidateTest, HighFpsWithMaxFrame1) {
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/60);
  DrawLoop(client_slow, hwui_slow);

  // We should have submitted 60 frames + 1 from initial draw.
  ASSERT_EQ(child_client_timings_.size(), 61u);
  EXPECT_EQ(CountDroppedFrames(), 0);
}

TEST_P(InvalidateTest, HighFpsWithMaxFrame2) {
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/2, /*frame_rate=*/60);
  DrawLoop(client_slow, hwui_slow);

  // We should have submitted 60 frames + 1 from initial draw.
  ASSERT_EQ(child_client_timings_.size(), 61u);
  EXPECT_EQ(CountDroppedFrames(), 0);
}

TEST_P(InvalidateTest, LastFrameNotLost) {
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/30);
  DrawLoop(client_slow, hwui_slow, /*stop_submitting_after_frames=*/30);

  ASSERT_EQ(child_client_timings_.size(), 16u);
  EXPECT_EQ(CountDroppedFrames(), 0);

  // Note, that client unsubscribes at frame 30 leading to 31 BF + 4 from:
  // initial draw, one for presentation feedback delivery, one because client is
  // always behind and one because we keep BF until we don't have anything to
  // draw.
  EXPECT_LE(root_begin_frames_count_, 35);
}

TEST_P(InvalidateTest, VeryLateFrame) {
  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/60);
  client_->MakeNextFrameLast();

  // Draw until we don't need to draw anymore.
  for (int i = 0; i < 10; i++) {
    auto args = NextBeginFrameArgs();

    bool invalidate = BeginFrame(args);

    if (invalidate)
      DrawOnUI(CreateChildFrame(nullptr, args, /*invalidated=*/invalidate));
    Sync();

    if (invalidate)
      DrawOnRT(args, invalidate);
  }

  // Submit frame.
  SubmitFrameIfNeeded();

  // Client could have subscribed to begin frames, make sure it was
  // propagated to UI thread.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  for (int i = 0; i < 3; i++) {
    auto args = NextBeginFrameArgs();
    bool invalidate = BeginFrame(args);

    if (invalidate)
      DrawOnUI(CreateChildFrame(nullptr, args, /*invalidated=*/invalidate));
    Sync();

    if (invalidate)
      DrawOnRT(args, invalidate);
  }

  UpdateFrameTimingDetails();

  ASSERT_EQ(client_->frames_submitted(), child_client_timings_.size());
  ASSERT_EQ(child_client_timings_.size(), 2u);
  EXPECT_EQ(CountDroppedFrames(), 0);
}

}  // namespace
}  // namespace android_webview
