// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/begin_frame_source_webview.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/hardware_renderer_viz.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/root_frame_sink_proxy.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
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
                       viz::SurfaceRange(absl::nullopt, child_id),
                       SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false);
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
                           SK_ColorWHITE, /*force_anti_aliasing_off=*/false);
}

class VizClient : public viz::mojom::CompositorFrameSinkClient {
 public:
  VizClient(viz::FrameSinkId frame_sink_id,
            int max_pending_frames,
            int frame_rate)
      : max_pending_frames_(max_pending_frames),
        frame_interval_(base::Seconds(1) / frame_rate) {
    support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
        this,
        VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager(),
        frame_sink_id, false);

    VizCompositorThreadRunnerWebView::GetInstance()
        ->GetFrameSinkManager()
        ->RegisterFrameSinkHierarchy(kRootClientSinkId, frame_sink_id);

    local_surface_id_allocator_.GenerateId();
    // We always need begin frames.
    support_->SetNeedsBeginFrame(true);
  }

  ~VizClient() override {
    VizCompositorThreadRunnerWebView::GetInstance()
        ->GetFrameSinkManager()
        ->UnregisterFrameSinkHierarchy(kRootClientSinkId,
                                       support_->frame_sink_id());
  }

  void SubmitFrameIfNeeded() {
    if (last_begin_frame_args_.IsValid()) {
      bool need_submit = false;

      if (pending_frames_ < max_pending_frames_) {
        if (last_submitted_time_.is_null()) {
          last_submitted_time_ = last_begin_frame_args_.frame_time;
          need_submit = true;
        } else if (last_begin_frame_args_.frame_time >=
                   last_submitted_time_ + frame_interval_) {
          last_submitted_time_ += frame_interval_;
          need_submit = true;
        }
      }

      if (need_submit)
        SubmitFrame(viz::BeginFrameAck(last_begin_frame_args_, true));
      else
        DidNotProduceFrame(viz::BeginFrameAck(last_begin_frame_args_, false));
    }
    last_begin_frame_args_ = viz::BeginFrameArgs();
  }

  viz::SurfaceId GetSurfaceId() {
    return viz::SurfaceId(
        support_->frame_sink_id(),
        local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }

  viz::FrameTimingDetailsMap TakeFrameTimingDetails() {
    return support_->TakeFrameTimingDetailsMap();
  }

  size_t frames_submitted() { return frames_submitted_; }

  // viz::mojom::CompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override {
    ReclaimResources(std::move(resources));

    DCHECK_GT(pending_frames_, 0);
    pending_frames_--;
  }
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& feedbacks) override {
    // We explicitly handle feedbacks after draw.
    DCHECK(feedbacks.empty());
    DCHECK_GE(frame_interval_, args.interval);

    last_begin_frame_args_ = args;
  }
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    // No resources in this test
    DCHECK(resources.empty());
  }
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}

 private:
  void SubmitFrame(const viz::BeginFrameAck& ack) {
    pending_frames_++;
    frames_submitted_++;

    auto frame =
        viz::CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kFrameSize), gfx::Rect(kFrameSize))
            .SetBeginFrameAck(viz::BeginFrameAck(last_begin_frame_args_, true))
            .Build();
    AppendSolidColorDrawQuad(*frame.render_pass_list.back());
    frame.metadata.frame_token = ack.frame_id.sequence_number;
    support_->SubmitCompositorFrame(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        std::move(frame), absl::nullopt);
  }

  void DidNotProduceFrame(const viz::BeginFrameAck& ack) {
    support_->DidNotProduceFrame(ack);
  }

  const int max_pending_frames_;
  const base::TimeDelta frame_interval_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;

  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  int pending_frames_ = 0;
  size_t frames_submitted_ = 0;

  viz::BeginFrameArgs last_begin_frame_args_;
  base::TimeTicks last_submitted_time_;
};

struct PerFrameFlag {
  PerFrameFlag(uint64_t bits) : bits(bits) {}

  static PerFrameFlag AlwaysTrue() { return {static_cast<uint64_t>(-1)}; }
  static PerFrameFlag AlwaysFalse() { return {static_cast<uint64_t>(0)}; }

  bool at(int frame) const {
    DCHECK_LT(frame, 64);
    return bits & (1 << frame);
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

class InvalidateTest : public testing::TestWithParam<
                           testing::tuple<PerFrameFlag, PerFrameFlag, bool>>,
                       public viz::ExternalBeginFrameSourceClient,
                       public RootFrameSinkProxyClient {
 public:
  InvalidateTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {
    begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
    root_frame_sink_proxy_ = std::make_unique<RootFrameSinkProxy>(
        base::ThreadTaskRunnerHandle::Get(), this, begin_frame_source_.get());

    root_frame_sink_proxy_->AddChildFrameSinkId(kRootClientSinkId);

    GpuServiceWebView::GetInstance();

    // For purpose of this test we don't care about RT/UI communication, so we
    // use single thread for both as we want to control timing of two threads
    // explicitly.
    render_thread_manager_ = std::make_unique<RenderThreadManager>(
        base::ThreadTaskRunnerHandle::Get());

    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(100, 100));
    DCHECK(surface_);
    DCHECK(surface_->GetHandle());
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    DCHECK(context_);

    context_->MakeCurrent(surface_.get());
    hardware_renderer_ = std::make_unique<HardwareRendererViz>(
        render_thread_manager_.get(),
        root_frame_sink_proxy_->GetRootFrameSinkCallback(), nullptr);
  }

  ~InvalidateTest() override {
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<VizClient> client) {
                         // `client` leaves scope.
                       },
                       std::move(client_)));
  }

  // viz::ExternalBeginFrameSourceClient
  void OnNeedsBeginFrames(bool needs_begin_frames) override {
    if (set_needs_begin_frames_closure_)
      std::move(set_needs_begin_frames_closure_).Run();
  }

  // RootFrameSinkProxyClient
  void Invalidate() override {
    DCHECK(inside_begin_frame_);
    did_invalidate_ = true;
  }

  void ReturnResourcesFromViz(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      std::vector<viz::ReturnedResource> resources) override {
    // no resources in this test
    DCHECK(resources.empty());
  }

 protected:
  void Draw(const viz::BeginFrameArgs& args, bool invalidated) {
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

    ScopedAppGLStateRestore state_restore(ScopedAppGLStateRestore::MODE_DRAW,
                                          false);
    hardware_renderer_->DrawAndSwap(params, OverlaysParams());
    if (invalidated)
      last_invalidated_draw_bf_ = args;
    UpdateFrameTimingDetails();
  }

  bool BeginFrame(const viz::BeginFrameArgs& args) {
    DCHECK(!inside_begin_frame_);
    did_invalidate_ = false;
    inside_begin_frame_ = true;
    begin_frame_source_->OnBeginFrame(args);
    inside_begin_frame_ = false;

    return did_invalidate_;
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
        ASSERT_TRUE(timing.second.presentation_feedback.failed());
      }
      child_client_timings_[timing.first] = timing.second;
    }
  }

  void SetUpAndDrawFirstFrameOnViz(int max_pending_frames,
                                   int frame_rate,
                                   viz::SurfaceId* surface_id) {
    client_ = std::make_unique<VizClient>(kChildClientSinkId,
                                          max_pending_frames, frame_rate);
    *surface_id = client_->GetSurfaceId();
  }

  void SetUpAndDrawFirstFrame(int max_pending_frames, int frame_rate) {
    // During initialization client will request for begin frames, we need to
    // wait until that message will reach UI thread.
    base::RunLoop run_loop;
    set_needs_begin_frames_closure_ = run_loop.QuitClosure();

    viz::SurfaceId child_client_surface_id;
    VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
        FROM_HERE, base::BindOnce(&InvalidateTest::SetUpAndDrawFirstFrameOnViz,
                                  base::Unretained(this), max_pending_frames,
                                  frame_rate, &child_client_surface_id));

    // Wait for OnNeedsBeginFrames to be called.
    run_loop.Run();

    // Do first draw and to setup embedding.
    auto bf_args = NextBeginFrameArgs();

    auto compositor_frame =
        viz::CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kFrameSize), gfx::Rect(kFrameSize))
            .SetBeginFrameAck(viz::BeginFrameAck(bf_args, true))
            .Build();
    AppendSurfaceDrawQuad(*compositor_frame.render_pass_list.back(),
                          child_client_surface_id);

    auto frame = std::make_unique<content::SynchronousCompositor::Frame>();
    frame->layer_tree_frame_sink_id = 1;
    frame->frame =
        std::make_unique<viz::CompositorFrame>(std::move(compositor_frame));
    root_local_surface_id_allocator_.GenerateId();
    frame->local_surface_id =
        root_local_surface_id_allocator_.GetCurrentLocalSurfaceId();

    auto future =
        base::MakeRefCounted<content::SynchronousCompositor::FrameFuture>();
    future->SetFrame(std::move(frame));

    auto child_frame = std::make_unique<ChildFrame>(
        future, kRootClientSinkId, kFrameSize, gfx::Transform(), false, 1.0f,
        CopyOutputRequestQueue(), /*did_invalidate=*/true);
    child_frame->WaitOnFutureIfNeeded();
    hardware_renderer_->SetChildFrameForTesting(std::move(child_frame));

    BeginFrame(bf_args);
    SubmitFrameIfNeeded();
    // First draw is implicitly invalidated.
    Draw(bf_args, /*invalidated=*/true);

    // This draw must always succeed.
    ASSERT_EQ(child_client_timings_.size(), 1u);
    ASSERT_FALSE(
        child_client_timings_.begin()->second.presentation_feedback.failed());

    // Clear the map, so this draw doesn't count later.
    child_client_timings_.clear();
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

  void DrawLoop(PerFrameFlag client_slow_param, PerFrameFlag hwui_slow_param) {
    const bool always_draw = testing::get<2>(GetParam());

    viz::BeginFrameArgs delayed_draw_args;
    bool delayed_draw_invalidate = false;

    for (int i = 0; i < 60; i++) {
      const bool client_fast = !client_slow_param.at(i);
      const bool hwui_fast = !hwui_slow_param.at(i);

      auto args = NextBeginFrameArgs();
      bool invalidate = BeginFrame(args);

      // Fast clients submit right after BF.
      if (client_fast)
        SubmitFrameIfNeeded();

      // If we didn't draw last frame, now it's time.
      if (delayed_draw_args.IsValid()) {
        Draw(delayed_draw_args, delayed_draw_invalidate);
        delayed_draw_args = viz::BeginFrameArgs();
      }

      // If webview invalidated or other views requested to draw, try to draw.
      if (invalidate || always_draw) {
        // If this frame hwui is in "fast" mode (i.e RT is not a frame behind)
        // then draw a frame now.
        if (hwui_fast) {
          Draw(args, invalidate);
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

    // Draw one more frame without client submitting to make sure all
    // presentation feedback was processed.
    auto args = NextBeginFrameArgs();
    bool invalidate = BeginFrame(args);

    // Finish draw if it's still pending
    if (delayed_draw_args.IsValid()) {
      Draw(delayed_draw_args, delayed_draw_invalidate);
    }
    Draw(args, invalidate);

    // Make sure we received all presentation feedback, ignoring first frame
    // submitted during init.
    ASSERT_EQ(client_->frames_submitted() - 1, child_client_timings_.size());
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
  std::unique_ptr<HardwareRendererViz> hardware_renderer_;

  std::unique_ptr<VizClient> client_;
  viz::ParentLocalSurfaceIdAllocator root_local_surface_id_allocator_;

  uint64_t next_begin_frame_sequence_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  base::TimeTicks begin_frame_time_ = base::TimeTicks::Now();

  viz::FrameTimingDetailsMap child_client_timings_;

  bool inside_begin_frame_ = false;
  bool did_invalidate_ = false;
  viz::BeginFrameArgs last_invalidated_draw_bf_;

  base::OnceClosure set_needs_begin_frames_closure_;
};

std::string TestParamToString(
    const testing::TestParamInfo<
        testing::tuple<PerFrameFlag, PerFrameFlag, bool>>& param_info) {
  auto client_slow = testing::get<0>(param_info.param);
  auto hwui_slow = testing::get<1>(param_info.param);
  auto draw_always = testing::get<2>(param_info.param);

  return "ClientSlow" + client_slow.ToString() + "HwuiSlow" +
         hwui_slow.ToString() + (draw_always ? "AlwaysDraw" : "");
}

INSTANTIATE_TEST_SUITE_P(
    Stable,
    InvalidateTest,
    ::testing::Combine(::testing::Values(PerFrameFlag::AlwaysFalse(),
                                         PerFrameFlag::AlwaysTrue()),
                       ::testing::Values(PerFrameFlag::AlwaysFalse(),
                                         PerFrameFlag::AlwaysTrue()),
                       ::testing::Bool()),
    TestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Random,
    InvalidateTest,
    ::testing::Combine(::testing::Values(PerFrameFlag::AlwaysFalse()),
                       ::testing::Values(PerFrameFlag(0xAAAAAAAAAAAAAAAA)),
                       ::testing::Bool()),
    TestParamToString);

TEST_P(InvalidateTest, LowFpsWithMaxFrame1) {
  const bool always_draw = testing::get<2>(GetParam());
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  // Always draw case is broken because viz runs ahead of hwui.
  if (always_draw) {
    GTEST_SKIP();
  }

  // If client is faster than hwui, it runs ahead of hwui.
  if (client_slow.IsNever() && !hwui_slow.IsNever()) {
    GTEST_SKIP();
  }

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/30);
  DrawLoop(client_slow, hwui_slow);

  // Due to rounding error (1 / 60 * 2 < 1 / 30) we submit 39 frames instead
  // of 30.
  ASSERT_EQ(child_client_timings_.size(), 29u);
  EXPECT_EQ(CountDroppedFrames(), 0);
}

// Currently we can't reach 60fps with max pending frames 1.
TEST_P(InvalidateTest, DISABLED_HighFpsWithMaxFrame1) {
  const bool always_draw = testing::get<2>(GetParam());
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  // Always draw case is broken because viz runs ahead of hwui.
  if (always_draw) {
    GTEST_SKIP();
  }

  // If client is faster than hwui, it runs ahead of hwui.
  if (client_slow.IsNever() && !hwui_slow.IsNever()) {
    GTEST_SKIP();
  }

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/1, /*frame_rate=*/60);
  DrawLoop(client_slow, hwui_slow);

  ASSERT_EQ(child_client_timings_.size(), 60u);
  EXPECT_EQ(CountDroppedFrames(), 0);
}

// Currently we can't reach 60fps with max pending frames 1.
// Test is failing on Lollipop Phone Tester (crbug.com/1234442).
TEST_P(InvalidateTest, DISABLED_HighFpsWithMaxFrame2) {
  const bool always_draw = testing::get<2>(GetParam());
  auto client_slow = testing::get<0>(GetParam());
  auto hwui_slow = testing::get<1>(GetParam());

  // Always draw case is broken because viz runs ahead of hwui.
  if (always_draw) {
    GTEST_SKIP();
  }

  // If client is faster than hwui, it runs ahead of hwui.
  if (client_slow.IsNever() && !hwui_slow.IsNever()) {
    GTEST_SKIP();
  }

  SetUpAndDrawFirstFrame(/*max_pending_frames=*/2, /*frame_rate=*/60);
  DrawLoop(client_slow, hwui_slow);

  ASSERT_EQ(child_client_timings_.size(), 60u);

  // Except the case when client is slower than hwui we currently drop first
  // frame always.
  EXPECT_LE(CountDroppedFrames(), 1);
}

}  // namespace
}  // namespace android_webview
