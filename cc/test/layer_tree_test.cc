// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
#include "cc/animation/single_keyframe_effect_animation.h"
#include "cc/animation/timing_function.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/compositor_timing_history.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_compositor_frame_reporting_controller.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_switches.h"

namespace cc {
namespace {

class SynchronousLayerTreeFrameSink : public TestLayerTreeFrameSink {
 public:
  SynchronousLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const viz::RendererSettings& renderer_settings,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      double refresh_rate,
      viz::BeginFrameSource* begin_frame_source,
      bool use_software_renderer)
      : TestLayerTreeFrameSink(std::move(compositor_context_provider),
                               std::move(worker_context_provider),
                               gpu_memory_buffer_manager,
                               renderer_settings,
                               task_runner,
                               false,
                               false,
                               refresh_rate,
                               begin_frame_source),
        use_software_renderer_(use_software_renderer),
        task_runner_(std::move(task_runner)) {}
  ~SynchronousLayerTreeFrameSink() override = default;

  void set_viewport(const gfx::Rect& viewport) { viewport_ = viewport; }

  bool BindToClient(LayerTreeFrameSinkClient* client) override {
    if (!TestLayerTreeFrameSink::BindToClient(client))
      return false;
    client_ = client;
    return true;
  }
  void DetachFromClient() override {
    client_ = nullptr;
    weak_factory_.InvalidateWeakPtrs();
    TestLayerTreeFrameSink::DetachFromClient();
  }
  void Invalidate(bool needs_draw) override {
    if (frame_request_pending_)
      return;
    frame_request_pending_ = true;
    InvalidateIfPossible();
  }
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed,
                             bool show_hit_test_borders) override {
    frame_ack_pending_ = true;
    TestLayerTreeFrameSink::SubmitCompositorFrame(
        std::move(frame), hit_test_data_changed, show_hit_test_borders);
  }
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override {
    DCHECK(frame_ack_pending_);
    frame_ack_pending_ = false;
    TestLayerTreeFrameSink::DidReceiveCompositorFrameAck(resources);
    InvalidateIfPossible();
  }

 private:
  void InvalidateIfPossible() {
    if (!frame_request_pending_ || frame_ack_pending_)
      return;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SynchronousLayerTreeFrameSink::DispatchInvalidation,
                       weak_factory_.GetWeakPtr()));
  }
  void DispatchInvalidation() {
    frame_request_pending_ = false;
    client_->OnDraw(gfx::Transform(SkMatrix::I()), viewport_,
                    use_software_renderer_, false);
  }

  bool frame_request_pending_ = false;
  bool frame_ack_pending_ = false;
  LayerTreeFrameSinkClient* client_ = nullptr;
  gfx::Rect viewport_;
  const bool use_software_renderer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<SynchronousLayerTreeFrameSink> weak_factory_{this};
};

}  // namespace

// Adapts LayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class LayerTreeHostImplForTesting : public LayerTreeHostImpl {
 public:
  static std::unique_ptr<LayerTreeHostImplForTesting> Create(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      LayerTreeHostSchedulingClient* scheduling_client,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner) {
    return base::WrapUnique(new LayerTreeHostImplForTesting(
        test_hooks, settings, host_impl_client, scheduling_client,
        task_runner_provider, task_graph_runner, stats_instrumentation,
        std::move(image_worker_task_runner)));
  }

 protected:
  LayerTreeHostImplForTesting(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      LayerTreeHostSchedulingClient* scheduling_client,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner)
      : LayerTreeHostImpl(settings,
                          host_impl_client,
                          task_runner_provider,
                          stats_instrumentation,
                          task_graph_runner,
                          AnimationHost::CreateForTesting(ThreadInstance::IMPL),
                          0,
                          std::move(image_worker_task_runner),
                          scheduling_client),
        test_hooks_(test_hooks) {}

  std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider() override {
    return test_hooks_->CreateRasterBufferProvider(this);
  }

  bool WillBeginImplFrame(const viz::BeginFrameArgs& args) override {
    bool has_damage = LayerTreeHostImpl::WillBeginImplFrame(args);
    test_hooks_->WillBeginImplFrameOnThread(this, args);
    return has_damage;
  }

  void DidFinishImplFrame() override {
    LayerTreeHostImpl::DidFinishImplFrame();
    test_hooks_->DidFinishImplFrameOnThread(this);
  }

  void WillSendBeginMainFrame() override {
    LayerTreeHostImpl::WillSendBeginMainFrame();
    test_hooks_->WillSendBeginMainFrameOnThread(this);
  }

  void DidSendBeginMainFrame(const viz::BeginFrameArgs& args) override {
    LayerTreeHostImpl::DidSendBeginMainFrame(args);
    test_hooks_->DidSendBeginMainFrameOnThread(this);
  }

  void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      const viz::BeginFrameArgs& args) override {
    LayerTreeHostImpl::BeginMainFrameAborted(reason, std::move(swap_promises),
                                             args);
    test_hooks_->BeginMainFrameAbortedOnThread(this, reason);
  }

  void ReadyToCommit() override {
    LayerTreeHostImpl::ReadyToCommit();
    test_hooks_->ReadyToCommitOnThread(this);
  }

  void BeginCommit() override {
    LayerTreeHostImpl::BeginCommit();
    test_hooks_->BeginCommitOnThread(this);
  }

  void CommitComplete() override {
    test_hooks_->WillCommitCompleteOnThread(this);
    LayerTreeHostImpl::CommitComplete();
    test_hooks_->CommitCompleteOnThread(this);
  }

  bool PrepareTiles() override {
    test_hooks_->WillPrepareTilesOnThread(this);
    return LayerTreeHostImpl::PrepareTiles();
  }

  DrawResult PrepareToDraw(FrameData* frame) override {
    test_hooks_->WillPrepareToDrawOnThread(this);
    DrawResult draw_result = LayerTreeHostImpl::PrepareToDraw(frame);
    return test_hooks_->PrepareToDrawOnThread(this, frame, draw_result);
  }

  bool DrawLayers(FrameData* frame) override {
    bool r = LayerTreeHostImpl::DrawLayers(frame);
    test_hooks_->DrawLayersOnThread(this);
    return r;
  }

  void NotifyReadyToActivate() override {
    if (block_notify_ready_to_activate_for_testing_) {
      notify_ready_to_activate_was_blocked_ = true;
    } else {
      test_hooks_->WillNotifyReadyToActivateOnThread(this);
      LayerTreeHostImpl::NotifyReadyToActivate();
      test_hooks_->NotifyReadyToActivateOnThread(this);
    }
  }

  void NotifyReadyToDraw() override {
    LayerTreeHostImpl::NotifyReadyToDraw();
    test_hooks_->NotifyReadyToDrawOnThread(this);
  }

  void NotifyAllTileTasksCompleted() override {
    LayerTreeHostImpl::NotifyAllTileTasksCompleted();
    test_hooks_->NotifyAllTileTasksCompleted(this);
  }

  void BlockNotifyReadyToActivateForTesting(bool block) override {
    CHECK(task_runner_provider()->ImplThreadTaskRunner())
        << "Not supported for single-threaded mode.";
    block_notify_ready_to_activate_for_testing_ = block;
    if (!block && notify_ready_to_activate_was_blocked_) {
      task_runner_provider_->ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&LayerTreeHostImplForTesting::NotifyReadyToActivate,
                         base::Unretained(this)));
      notify_ready_to_activate_was_blocked_ = false;
    }
  }

  void BlockImplSideInvalidationRequestsForTesting(bool block) override {
    block_impl_side_invalidation_ = block;
    if (!block_impl_side_invalidation_ && impl_side_invalidation_was_blocked_) {
      RequestImplSideInvalidationForCheckerImagedTiles();
      impl_side_invalidation_was_blocked_ = false;
    }
  }

  void ActivateSyncTree() override {
    test_hooks_->WillActivateTreeOnThread(this);
    LayerTreeHostImpl::ActivateSyncTree();
    DCHECK(!pending_tree());
    test_hooks_->DidActivateTreeOnThread(this);
  }

  bool InitializeFrameSink(LayerTreeFrameSink* layer_tree_frame_sink) override {
    bool success =
        LayerTreeHostImpl::InitializeFrameSink(layer_tree_frame_sink);
    test_hooks_->InitializedRendererOnThread(this, success);
    return success;
  }

  void SetVisible(bool visible) override {
    LayerTreeHostImpl::SetVisible(visible);
    test_hooks_->DidSetVisibleOnImplTree(this, visible);
  }

  bool AnimateLayers(base::TimeTicks monotonic_time,
                     bool is_active_tree) override {
    test_hooks_->WillAnimateLayers(this, monotonic_time);
    bool result =
        LayerTreeHostImpl::AnimateLayers(monotonic_time, is_active_tree);
    test_hooks_->AnimateLayers(this, monotonic_time);
    return result;
  }

  void UpdateAnimationState(bool start_ready_animations) override {
    LayerTreeHostImpl::UpdateAnimationState(start_ready_animations);
    bool has_unfinished_animation = false;
    for (const auto& it : animation_host()->ticking_animations_for_testing()) {
      if (it.get()->TickingKeyframeModelsCount()) {
        has_unfinished_animation = true;
        break;
      }
    }
    test_hooks_->UpdateAnimationState(this, has_unfinished_animation);
  }

  void NotifyTileStateChanged(const Tile* tile) override {
    LayerTreeHostImpl::NotifyTileStateChanged(tile);
    test_hooks_->NotifyTileStateChangedOnThread(this, tile);
  }

  void InvalidateContentOnImplSide() override {
    LayerTreeHostImpl::InvalidateContentOnImplSide();
    test_hooks_->DidInvalidateContentOnImplSide(this);
  }

  void InvalidateLayerTreeFrameSink(bool needs_redraw) override {
    LayerTreeHostImpl::InvalidateLayerTreeFrameSink(needs_redraw);
    test_hooks_->DidInvalidateLayerTreeFrameSink(this);
  }

  void RequestImplSideInvalidationForCheckerImagedTiles() override {
    test_hooks_->DidReceiveImplSideInvalidationRequest(this);
    if (block_impl_side_invalidation_) {
      impl_side_invalidation_was_blocked_ = true;
      return;
    }

    impl_side_invalidation_was_blocked_ = false;
    LayerTreeHostImpl::RequestImplSideInvalidationForCheckerImagedTiles();
    test_hooks_->DidRequestImplSideInvalidation(this);
  }

  void DidReceiveCompositorFrameAck() override {
    test_hooks_->WillReceiveCompositorFrameAckOnThread(this);
    LayerTreeHostImpl::DidReceiveCompositorFrameAck();
    test_hooks_->DidReceiveCompositorFrameAckOnThread(this);
  }

  void DidPresentCompositorFrame(
      uint32_t presentation_token,
      const viz::FrameTimingDetails& details) override {
    LayerTreeHostImpl::DidPresentCompositorFrame(presentation_token, details);
    test_hooks_->DidReceivePresentationTimeOnThread(
        this, presentation_token, details.presentation_feedback);
  }
  AnimationHost* animation_host() const {
    return static_cast<AnimationHost*>(mutator_host());
  }

 private:
  TestHooks* test_hooks_;
  bool block_notify_ready_to_activate_for_testing_ = false;
  bool notify_ready_to_activate_was_blocked_ = false;

  bool block_impl_side_invalidation_ = false;
  bool impl_side_invalidation_was_blocked_ = false;
};

// Implementation of LayerTreeHost callback interface.
class LayerTreeHostClientForTesting : public LayerTreeHostClient,
                                      public LayerTreeHostSchedulingClient,
                                      public LayerTreeHostSingleThreadClient {
 public:
  static std::unique_ptr<LayerTreeHostClientForTesting> Create(
      TestHooks* test_hooks) {
    return base::WrapUnique(new LayerTreeHostClientForTesting(test_hooks));
  }
  ~LayerTreeHostClientForTesting() override = default;

  void WillBeginMainFrame() override { test_hooks_->WillBeginMainFrame(); }

  void DidBeginMainFrame() override { test_hooks_->DidBeginMainFrame(); }

  void WillUpdateLayers() override {}
  void DidUpdateLayers() override {}

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    test_hooks_->BeginMainFrame(args);
  }

  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(bool) override {}

  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(base::TimeTicks) override {}
  std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() override {
    return nullptr;
  }

  void UpdateLayerTreeHost() override { test_hooks_->UpdateLayerTreeHost(); }

  void ApplyViewportChanges(const ApplyViewportChangesArgs& args) override {
    test_hooks_->ApplyViewportChanges(args);
  }

  void RecordManipulationTypeCounts(ManipulationInfo info) override {}

  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      ElementId scroll_latched_element_id) override {}

  void SendScrollEndEventFromImplSide(
      ElementId scroll_latched_element_id) override {}

  void RequestNewLayerTreeFrameSink() override {
    test_hooks_->RequestNewLayerTreeFrameSink();
  }

  void DidInitializeLayerTreeFrameSink() override {
    test_hooks_->DidInitializeLayerTreeFrameSink();
  }

  void DidFailToInitializeLayerTreeFrameSink() override {
    test_hooks_->DidFailToInitializeLayerTreeFrameSink();
    RequestNewLayerTreeFrameSink();
  }

  void WillCommit() override { test_hooks_->WillCommit(); }

  void DidCommit() override { test_hooks_->DidCommit(); }

  void DidCommitAndDrawFrame() override {
    test_hooks_->DidCommitAndDrawFrame();
  }

  void DidReceiveCompositorFrameAck() override {
    test_hooks_->DidReceiveCompositorFrameAck();
  }

  void DidScheduleBeginMainFrame() override {
    test_hooks_->DidScheduleBeginMainFrame();
  }
  void DidRunBeginMainFrame() override { test_hooks_->DidRunBeginMainFrame(); }

  void DidSubmitCompositorFrame() override {}
  void DidLoseLayerTreeFrameSink() override {}
  void RequestScheduleComposite() override { test_hooks_->ScheduleComposite(); }
  void DidCompletePageScaleAnimation() override {}
  void BeginMainFrameNotExpectedSoon() override {
    test_hooks_->BeginMainFrameNotExpectedSoon();
  }
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override {}

 private:
  explicit LayerTreeHostClientForTesting(TestHooks* test_hooks)
      : test_hooks_(test_hooks) {}

  TestHooks* test_hooks_;
};

// Adapts LayerTreeHost for test. Injects LayerTreeHostImplForTesting.
class LayerTreeHostForTesting : public LayerTreeHost {
 public:
  static std::unique_ptr<LayerTreeHostForTesting> Create(
      TestHooks* test_hooks,
      CompositorMode mode,
      LayerTreeHostClient* client,
      LayerTreeHostSchedulingClient* scheduling_client,
      LayerTreeHostSingleThreadClient* single_thread_client,
      TaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      MutatorHost* mutator_host) {
    LayerTreeHost::InitParams params;
    params.client = client;
    params.scheduling_client = scheduling_client;
    params.task_graph_runner = task_graph_runner;
    params.settings = &settings;
    params.mutator_host = mutator_host;
    params.image_worker_task_runner = std::move(image_worker_task_runner);
    params.ukm_recorder_factory = std::make_unique<TestUkmRecorderFactory>();

    auto layer_tree_host = base::WrapUnique(
        new LayerTreeHostForTesting(test_hooks, std::move(params), mode));
    std::unique_ptr<TaskRunnerProvider> task_runner_provider =
        TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
    std::unique_ptr<Proxy> proxy;
    switch (mode) {
      case CompositorMode::SINGLE_THREADED:
        proxy = SingleThreadProxy::Create(layer_tree_host.get(),
                                          single_thread_client,
                                          task_runner_provider.get());
        break;
      case CompositorMode::THREADED:
        DCHECK(impl_task_runner.get());
        proxy = std::make_unique<ProxyMain>(layer_tree_host.get(),
                                            task_runner_provider.get());
        break;
    }
    layer_tree_host->InitializeForTesting(std::move(task_runner_provider),
                                          std::move(proxy));
    return layer_tree_host;
  }

  std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* host_impl_client) override {
    std::unique_ptr<LayerTreeHostImpl> host_impl =
        LayerTreeHostImplForTesting::Create(
            test_hooks_, GetSettings(), host_impl_client, scheduling_client(),
            GetTaskRunnerProvider(), task_graph_runner(),
            rendering_stats_instrumentation(), image_worker_task_runner_);

    host_impl->InitializeUkm(ukm_recorder_factory_->CreateRecorder());
    input_handler_weak_ptr_ = host_impl->AsWeakPtr();
    return host_impl;
  }

  void SetNeedsCommit() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsCommit();
  }

  void SetNeedsUpdateLayers() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsUpdateLayers();
  }

  void set_test_started(bool started) { test_started_ = started; }

 private:
  LayerTreeHostForTesting(TestHooks* test_hooks,
                          LayerTreeHost::InitParams params,
                          CompositorMode mode)
      : LayerTreeHost(std::move(params), mode), test_hooks_(test_hooks) {}

  TestHooks* test_hooks_;
  bool test_started_ = false;
};

class LayerTreeTestLayerTreeFrameSinkClient
    : public TestLayerTreeFrameSinkClient {
 public:
  explicit LayerTreeTestLayerTreeFrameSinkClient(TestHooks* hooks)
      : hooks_(hooks) {}

  // TestLayerTreeFrameSinkClient implementation.
  std::unique_ptr<viz::SkiaOutputSurface> CreateDisplaySkiaOutputSurface()
      override {
    return hooks_->CreateDisplaySkiaOutputSurfaceOnThread();
  }

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return hooks_->CreateDisplayOutputSurfaceOnThread(
        std::move(compositor_context_provider));
  }
  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {
    hooks_->DisplayReceivedLocalSurfaceIdOnThread(local_surface_id);
  }
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {
    hooks_->DisplayReceivedCompositorFrameOnThread(frame);
  }
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              viz::RenderPassList* render_passes) override {
    hooks_->DisplayWillDrawAndSwapOnThread(will_draw_and_swap, *render_passes);
  }
  void DisplayDidDrawAndSwap() override {
    hooks_->DisplayDidDrawAndSwapOnThread();
  }

 private:
  TestHooks* hooks_;
};

LayerTreeTest::LayerTreeTest()
    : initial_root_bounds_(1, 1),
      layer_tree_frame_sink_client_(
          new LayerTreeTestLayerTreeFrameSinkClient(this)) {
  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();

  // Tests should timeout quickly unless --cc-layer-tree-test-no-timeout was
  // specified (for running in a debugger).
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kCCLayerTreeTestNoTimeout))
#if defined(THREAD_SANITIZER)
    // SwiftShader is a multi-threaded renderer and TSAN takes a lot longer to
    // run tests when using SwiftShader
    timeout_seconds_ = 35;
#elif defined(OS_WIN) && defined(_DEBUG)
    // Debug builds on Windows are much slower than on other platforms, possibly
    // because Windows uses separate debug versions of the C Run-Time Library
    // for debug builds, whereas other platforms use the same system libraries
    // for debug and release builds.
    timeout_seconds_ = 25;
#elif defined(MEMORY_SANITIZER)
    // MSAN is slower than uninstrumented code
    timeout_seconds_ = 20;
#elif BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
    BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC) || BUILDFLAG(CFI_ENFORCEMENT_TRAP)
    // CFI is slow as well.
    timeout_seconds_ = 20;
#elif defined(ADDRESS_SANITIZER) || defined(_DEBUG) || defined(USE_OZONE)
    // ASAN and Debug builds are slower than release builds, as expected
    // Ozone builds also go through a slower path than regular Linux builds
    timeout_seconds_ = 30;
#else
    timeout_seconds_ = 10;
#endif
  if (command_line->HasSwitch(switches::kCCLayerTreeTestLongTimeout))
    timeout_seconds_ = 5 * 60;
}

LayerTreeTest::~LayerTreeTest() {
  if (animation_host_)
    animation_host_->SetMutatorHostClient(nullptr);
}

gfx::Vector2dF LayerTreeTest::ScrollDelta(LayerImpl* layer_impl) {
  gfx::ScrollOffset delta = layer_impl->layer_tree_impl()
                                ->property_trees()
                                ->scroll_tree.GetScrollOffsetDeltaForTesting(
                                    layer_impl->element_id());
  return gfx::Vector2dF(delta.x(), delta.y());
}

void LayerTreeTest::EndTest() {
  {
    base::AutoLock hold(test_ended_lock_);
    if (ended_)
      return;
    ended_ = true;
  }

  // For the case where we EndTest during BeginTest(), set a flag to indicate
  // that the test should end the second BeginTest regains control.
  if (beginning_) {
    end_when_begin_returns_ = true;
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
  }
}

void LayerTreeTest::EndTestAfterDelayMs(int delay_milliseconds) {
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::EndTest, main_thread_weak_ptr_),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void LayerTreeTest::PostAddNoDamageAnimationToMainThread(
    SingleKeyframeEffectAnimation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddNoDamageAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 1.0));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThread(
    SingleKeyframeEffectAnimation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LayerTreeTest::DispatchAddOpacityAnimation, main_thread_weak_ptr_,
          base::Unretained(animation_to_receive_animation), 0.000004));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThreadInstantly(
    SingleKeyframeEffectAnimation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddOpacityAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 0.0));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThreadDelayed(
    SingleKeyframeEffectAnimation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddOpacityAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 1.0));
}

void LayerTreeTest::PostSetLocalSurfaceIdAllocationToMainThread(
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchSetLocalSurfaceIdAllocation,
                     main_thread_weak_ptr_, local_surface_id_allocation));
}

void LayerTreeTest::PostRequestNewLocalSurfaceIdToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchRequestNewLocalSurfaceId,
                     main_thread_weak_ptr_));
}

void LayerTreeTest::PostGetDeferMainFrameUpdateToMainThread(
    std::unique_ptr<ScopedDeferMainFrameUpdate>*
        scoped_defer_main_frame_update) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchGetDeferMainFrameUpdate,
                     main_thread_weak_ptr_,
                     base::Unretained(scoped_defer_main_frame_update)));
}

void LayerTreeTest::PostReturnDeferMainFrameUpdateToMainThread(
    std::unique_ptr<ScopedDeferMainFrameUpdate>
        scoped_defer_main_frame_update) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchReturnDeferMainFrameUpdate,
                     main_thread_weak_ptr_,
                     std::move(scoped_defer_main_frame_update)));
}

void LayerTreeTest::PostSetNeedsCommitToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchSetNeedsCommit,
                                main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsUpdateLayersToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchSetNeedsUpdateLayers,
                                main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchSetNeedsRedraw,
                                main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawRectToMainThread(
    const gfx::Rect& damage_rect) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchSetNeedsRedrawRect,
                                main_thread_weak_ptr_, damage_rect));
}

void LayerTreeTest::PostSetVisibleToMainThread(bool visible) {
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&LayerTreeTest::DispatchSetVisible,
                                             main_thread_weak_ptr_, visible));
}

void LayerTreeTest::PostSetNeedsCommitWithForcedRedrawToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchSetNeedsCommitWithForcedRedraw,
                     main_thread_weak_ptr_));
}

void LayerTreeTest::PostCompositeImmediatelyToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchCompositeImmediately,
                                main_thread_weak_ptr_));
}

void LayerTreeTest::PostNextCommitWaitsForActivationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchNextCommitWaitsForActivation,
                     main_thread_weak_ptr_));
}

std::unique_ptr<LayerTreeFrameSink>
LayerTreeTest::ReleaseLayerTreeFrameSinkOnLayerTreeHost() {
  return layer_tree_host_->ReleaseLayerTreeFrameSink();
}

void LayerTreeTest::SetVisibleOnLayerTreeHost(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void LayerTreeTest::WillBeginTest() {
  SetVisibleOnLayerTreeHost(true);
}

void LayerTreeTest::DoBeginTest() {
  client_ = LayerTreeHostClientForTesting::Create(this);

  DCHECK(!impl_thread_ || impl_thread_->task_runner().get());

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner =
      impl_thread_ ? impl_thread_->task_runner() : nullptr;
  LayerTreeHostSchedulingClient* scheduling_client =
      impl_thread_ ? client_.get() : nullptr;

  animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  layer_tree_host_ = LayerTreeHostForTesting::Create(
      this, mode_, client_.get(), scheduling_client, client_.get(),
      task_graph_runner_.get(), settings_, main_task_runner, impl_task_runner,
      image_worker_->task_runner(), animation_host_.get());
  ASSERT_TRUE(layer_tree_host_);

  main_task_runner_ =
      layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner();
  impl_task_runner_ =
      layer_tree_host_->GetTaskRunnerProvider()->ImplThreadTaskRunner();
  if (!impl_task_runner_) {
    // For tests, if there's no impl thread, make things easier by just giving
    // the main thread task runner.
    impl_task_runner_ = main_task_runner_;
  }

  if (timeout_seconds_) {
    timeout_.Reset(
        base::BindOnce(&LayerTreeTest::Timeout, base::Unretained(this)));
    main_task_runner_->PostDelayedTask(
        FROM_HERE, timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds_));
  }

  started_ = true;
  beginning_ = true;
  SetupTree();
  WillBeginTest();
  if (!skip_allocate_initial_local_surface_id_)
    GenerateNewLocalSurfaceId();
  BeginTest();
  if (!skip_allocate_initial_local_surface_id_) {
    PostSetLocalSurfaceIdAllocationToMainThread(
        GetCurrentLocalSurfaceIdAllocation());
  }
  beginning_ = false;
  if (end_when_begin_returns_)
    RealEndTest();

  // Allow commits to happen once BeginTest() has had a chance to post tasks
  // so that those tasks will happen before the first commit.
  if (layer_tree_host_) {
    static_cast<LayerTreeHostForTesting*>(layer_tree_host_.get())
        ->set_test_started(true);
  }
}

void LayerTreeTest::SkipAllocateInitialLocalSurfaceId() {
  skip_allocate_initial_local_surface_id_ = true;
}

const viz::LocalSurfaceIdAllocation&
LayerTreeTest::GetCurrentLocalSurfaceIdAllocation() const {
  return allocator_.GetCurrentLocalSurfaceIdAllocation();
}

void LayerTreeTest::GenerateNewLocalSurfaceId() {
  allocator_.GenerateId();
}

void LayerTreeTest::SetupTree() {
  if (!layer_tree_host()->root_layer()) {
    layer_tree_host()->SetRootLayer(Layer::Create());
    layer_tree_host()->root_layer()->SetBounds(initial_root_bounds_);
  }

  Layer* root_layer = layer_tree_host()->root_layer();
  gfx::Size root_bounds = root_layer->bounds();
  gfx::Size device_root_bounds =
      gfx::ScaleToCeiledSize(root_bounds, initial_device_scale_factor_);
  layer_tree_host()->SetViewportRectAndScale(gfx::Rect(device_root_bounds),
                                             initial_device_scale_factor_,
                                             viz::LocalSurfaceIdAllocation());
  root_layer->SetIsDrawable(true);
  root_layer->SetHitTestable(true);
  layer_tree_host()->SetElementIdsForTesting();

  if (layer_tree_host()->IsUsingLayerLists())
    SetupRootProperties(root_layer);
}

void LayerTreeTest::Timeout() {
  timed_out_ = true;
  EndTest();
}

void LayerTreeTest::RealEndTest() {
  // TODO(mithro): Make this method only end when not inside an impl frame.
  bool main_frame_will_happen =
      layer_tree_host_
          ? layer_tree_host_->proxy()->MainFrameWillHappenForTesting()
          : false;

  if (main_frame_will_happen && !timed_out_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
    return;
  }

  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void LayerTreeTest::DispatchAddNoDamageAnimation(
    SingleKeyframeEffectAnimation* animation_to_receive_animation,
    double animation_duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (animation_to_receive_animation) {
    AddOpacityTransitionToAnimation(animation_to_receive_animation,
                                    animation_duration, 0, 0, true);
  }
}

void LayerTreeTest::DispatchAddOpacityAnimation(
    SingleKeyframeEffectAnimation* animation_to_receive_animation,
    double animation_duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (animation_to_receive_animation) {
    AddOpacityTransitionToAnimation(animation_to_receive_animation,
                                    animation_duration, 0, 0.5, true);
  }
}

void LayerTreeTest::DispatchSetLocalSurfaceIdAllocation(
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_) {
    layer_tree_host_->SetLocalSurfaceIdAllocationFromParent(
        local_surface_id_allocation);
  }
}

void LayerTreeTest::DispatchRequestNewLocalSurfaceId() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->RequestNewLocalSurfaceId();
}

void LayerTreeTest::DispatchGetDeferMainFrameUpdate(
    std::unique_ptr<ScopedDeferMainFrameUpdate>*
        scoped_defer_main_frame_update) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    *scoped_defer_main_frame_update = layer_tree_host_->DeferMainFrameUpdate();
}

void LayerTreeTest::DispatchReturnDeferMainFrameUpdate(
    std::unique_ptr<ScopedDeferMainFrameUpdate>
        scoped_defer_main_frame_update) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Just let |scoped_defer_main_frame_update| go out of scope.
}

void LayerTreeTest::DispatchSetNeedsCommit() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommit();
}

void LayerTreeTest::DispatchSetNeedsUpdateLayers() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTreeTest::DispatchSetNeedsRedraw() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    DispatchSetNeedsRedrawRect(layer_tree_host_->device_viewport_rect());
}

void LayerTreeTest::DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

void LayerTreeTest::DispatchSetVisible(bool visible) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    SetVisibleOnLayerTreeHost(visible);
}

void LayerTreeTest::DispatchSetNeedsCommitWithForcedRedraw() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommitWithForcedRedraw();
}

void LayerTreeTest::DispatchCompositeImmediately() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->Composite(base::TimeTicks::Now(), true);
}

void LayerTreeTest::DispatchNextCommitWaitsForActivation() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitWaitsForActivation();
}

void LayerTreeTest::RunTest(CompositorMode mode) {
  mode_ = mode;
  if (mode_ == CompositorMode::THREADED) {
    impl_thread_.reset(new base::Thread("Compositor"));
    ASSERT_TRUE(impl_thread_->Start());
  }

  image_worker_ = std::make_unique<base::Thread>("ImageWorker");
  ASSERT_TRUE(image_worker_->Start());

  gpu_memory_buffer_manager_ =
      std::make_unique<viz::TestGpuMemoryBufferManager>();
  task_graph_runner_.reset(new TestTaskGraphRunner);

  if (mode == CompositorMode::THREADED)
    settings_.commit_to_active_tree = false;
  // Disable latency recovery to make the scheduler more predictable in its
  // actions and less dependent on timings to make decisions.
  settings_.enable_latency_recovery = false;
  InitializeSettings(&settings_);

  if (use_vulkan()) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    bool use_gpu = command_line->HasSwitch(::switches::kUseGpuInTests);
    command_line->AppendSwitchASCII(
        ::switches::kUseVulkan,
        use_gpu ? ::switches::kVulkanImplementationNameNative
                : ::switches::kVulkanImplementationNameSwiftshader);
    command_line->AppendSwitchASCII(
        ::switches::kGrContextType, ::switches::kGrContextTypeVulkan);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DoBeginTest, base::Unretained(this)));

  base::RunLoop().Run();
  DestroyLayerTreeHost();

  timeout_.Cancel();

  ASSERT_FALSE(layer_tree_host_.get());
  client_ = nullptr;
  if (timed_out_) {
    FAIL() << "Test timed out";
    return;
  }
  AfterTest();
}

void LayerTreeTest::RequestNewLayerTreeFrameSink() {
  scoped_refptr<viz::TestContextProvider> shared_context_provider =
      use_software_renderer() ? nullptr : viz::TestContextProvider::Create();
  scoped_refptr<viz::TestContextProvider> worker_context_provider =
      use_software_renderer() ? nullptr
                              : viz::TestContextProvider::CreateWorker();

  viz::RendererSettings renderer_settings;
  // Spend less time waiting for BeginFrame because the output is
  // mocked out.
  constexpr double refresh_rate = 200.0;
  renderer_settings.use_skia_renderer = use_skia_renderer();
  auto layer_tree_frame_sink = CreateLayerTreeFrameSink(
      renderer_settings, refresh_rate, std::move(shared_context_provider),
      std::move(worker_context_provider));
  layer_tree_frame_sink->SetClient(layer_tree_frame_sink_client_.get());
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

std::unique_ptr<TestLayerTreeFrameSink> LayerTreeTest::CreateLayerTreeFrameSink(
    const viz::RendererSettings& renderer_settings,
    double refresh_rate,
    scoped_refptr<viz::ContextProvider> compositor_context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider) {
  constexpr bool disable_display_vsync = false;
  bool synchronous_composite =
      !HasImplThread() &&
      !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;

  DCHECK(
      !synchronous_composite ||
      !layer_tree_host()->GetSettings().using_synchronous_renderer_compositor);
  if (layer_tree_host()->GetSettings().using_synchronous_renderer_compositor) {
    return std::make_unique<SynchronousLayerTreeFrameSink>(
        compositor_context_provider, std::move(worker_context_provider),
        gpu_memory_buffer_manager(), renderer_settings, impl_task_runner_,
        refresh_rate, begin_frame_source_, use_software_renderer());
  }

  return std::make_unique<TestLayerTreeFrameSink>(
      compositor_context_provider, std::move(worker_context_provider),
      gpu_memory_buffer_manager(), renderer_settings, impl_task_runner_,
      synchronous_composite, disable_display_vsync, refresh_rate,
      begin_frame_source_);
}

std::unique_ptr<viz::SkiaOutputSurface>
LayerTreeTest::CreateDisplaySkiaOutputSurfaceOnThread() {
  return viz::FakeSkiaOutputSurface::Create3d();
}

std::unique_ptr<viz::OutputSurface>
LayerTreeTest::CreateDisplayOutputSurfaceOnThread(
    scoped_refptr<viz::ContextProvider> compositor_context_provider) {
  // By default the Display shares a context with the LayerTreeHostImpl.
  if (use_software_renderer()) {
    return viz::FakeOutputSurface::CreateSoftware(
        std::make_unique<viz::SoftwareOutputDevice>());
  }
  return viz::FakeOutputSurface::Create3d(
      std::move(compositor_context_provider));
}

void LayerTreeTest::DestroyLayerTreeHost() {
  if (layer_tree_host_ && layer_tree_host_->root_layer())
    layer_tree_host_->root_layer()->SetLayerTreeHost(nullptr);
  layer_tree_host_ = nullptr;
}

TaskRunnerProvider* LayerTreeTest::task_runner_provider() const {
  LayerTreeHost* host = layer_tree_host_.get();

  // If this fails, the test has ended and there is no task runners to find
  // anymore.
  DCHECK(host);

  return host->GetTaskRunnerProvider();
}

LayerTreeHost* LayerTreeTest::layer_tree_host() {
  DCHECK(task_runner_provider()->IsMainThread() ||
         task_runner_provider()->IsMainThreadBlocked());
  return layer_tree_host_.get();
}

Proxy* LayerTreeTest::proxy() {
  return layer_tree_host() ? layer_tree_host()->proxy() : nullptr;
}

}  // namespace cc
