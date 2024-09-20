// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

#include <memory>
#include <string>

#include "base/cfi_buildflags.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
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
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

namespace cc {
namespace {

class SynchronousLayerTreeFrameSink : public TestLayerTreeFrameSink {
 public:
  SynchronousLayerTreeFrameSink(
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const viz::RendererSettings& renderer_settings,
      const viz::DebugRendererSettings* const debug_settings,
      TaskRunnerProvider* task_runner_provider,
      double refresh_rate,
      viz::BeginFrameSource* begin_frame_source,
      bool use_software_renderer)
      : TestLayerTreeFrameSink(std::move(compositor_context_provider),
                               std::move(worker_context_provider),
                               gpu_memory_buffer_manager,
                               renderer_settings,
                               debug_settings,
                               task_runner_provider,
                               false,
                               false,
                               refresh_rate,
                               begin_frame_source),
        use_software_renderer_(use_software_renderer) {}
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
                             bool hit_test_data_changed) override {
    frame_ack_pending_ = true;
    TestLayerTreeFrameSink::SubmitCompositorFrame(std::move(frame),
                                                  hit_test_data_changed);
  }
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override {
    if (!frame_ack_pending_) {
      DCHECK(resources.empty());
      return;
    }
    DCHECK(frame_ack_pending_);
    frame_ack_pending_ = false;
    TestLayerTreeFrameSink::DidReceiveCompositorFrameAck(std::move(resources));
    InvalidateIfPossible();
  }

 private:
  void InvalidateIfPossible() {
    if (!frame_request_pending_ || frame_ack_pending_)
      return;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SynchronousLayerTreeFrameSink::DispatchInvalidation,
                       weak_factory_.GetWeakPtr()));
  }
  void DispatchInvalidation() {
    frame_request_pending_ = false;
    client_->OnDraw(gfx::Transform(), viewport_, use_software_renderer_, false);
  }

  bool frame_request_pending_ = false;
  bool frame_ack_pending_ = false;
  raw_ptr<LayerTreeFrameSinkClient> client_ = nullptr;
  gfx::Rect viewport_;
  const bool use_software_renderer_;
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
      : LayerTreeHostImpl(
            settings,
            host_impl_client,
            task_runner_provider,
            stats_instrumentation,
            task_graph_runner,
            AnimationHost::CreateForTesting(ThreadInstance::kImpl),
            nullptr,
            0,
            std::move(image_worker_task_runner),
            scheduling_client),
        test_hooks_(test_hooks) {}

  std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider() override {
    return test_hooks_->CreateRasterBufferProvider(this);
  }

  bool WillBeginImplFrame(const viz::BeginFrameArgs& args) override {
    bool has_damage = LayerTreeHostImpl::WillBeginImplFrame(args);
    test_hooks_->WillBeginImplFrameOnThread(this, args, has_damage);
    return has_damage;
  }

  void DidFinishImplFrame(const viz::BeginFrameArgs& main_args) override {
    LayerTreeHostImpl::DidFinishImplFrame(main_args);
    test_hooks_->DidFinishImplFrameOnThread(this);
  }

  void WillSendBeginMainFrame() override {
    LayerTreeHostImpl::WillSendBeginMainFrame();
    test_hooks_->WillSendBeginMainFrameOnThread(this);
  }

  void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      const viz::BeginFrameArgs& args,
      bool next_bmf,
      bool scroll_and_viewport_changes_synced) override {
    LayerTreeHostImpl::BeginMainFrameAborted(
        reason, std::move(swap_promises), args, next_bmf,
        scroll_and_viewport_changes_synced);
    test_hooks_->BeginMainFrameAbortedOnThread(
        this, reason, scroll_and_viewport_changes_synced);
  }

  void ReadyToCommit(const viz::BeginFrameArgs& commit_args,
                     bool scroll_and_viewport_changes_synced,
                     const BeginMainFrameMetrics* begin_main_frame_metrics,
                     bool commit_timeout) override {
    LayerTreeHostImpl::ReadyToCommit(commit_args,
                                     scroll_and_viewport_changes_synced,
                                     begin_main_frame_metrics, commit_timeout);
    test_hooks_->ReadyToCommitOnThread(this);
  }

  void BeginCommit(int source_frame_number,
                   BeginMainFrameTraceId trace_id) override {
    LayerTreeHostImpl::BeginCommit(source_frame_number, trace_id);
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

  std::optional<SubmitInfo> DrawLayers(FrameData* frame) override {
    auto r = LayerTreeHostImpl::DrawLayers(frame);
    test_hooks_->DrawLayersOnThread(this);
    return r;
  }

  viz::CompositorFrame GenerateCompositorFrame(FrameData* frame) override {
    auto f = LayerTreeHostImpl::GenerateCompositorFrame(frame);
    test_hooks_->WillSubmitCompositorFrame(this, f);
    return f;
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

  void BlockNotifyReadyToActivateForTesting(bool block,
                                            bool notify_if_blocked) override {
    CHECK(task_runner_provider()->ImplThreadTaskRunner())
        << "Not supported for single-threaded mode.";
    block_notify_ready_to_activate_for_testing_ = block;
    if (!block && notify_ready_to_activate_was_blocked_) {
      if (notify_if_blocked) {
        task_runner_provider_->ImplThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostImplForTesting::NotifyReadyToActivate,
                           base::Unretained(this)));
      }
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
      if (it->keyframe_effect()->HasTickingKeyframeModel()) {
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
  raw_ptr<TestHooks> test_hooks_;
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
  void OnDeferCommitsChanged(
      bool,
      PaintHoldingReason,
      std::optional<PaintHoldingCommitTrigger>) override {}
  void OnCommitRequested() override {}

  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(base::TimeTicks,
                               ActiveFrameSequenceTrackers) override {}
  std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() override {
    return test_hooks_->GetBeginMainFrameMetrics();
  }
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override {
    test_hooks_->NotifyThroughputTrackerResults(std::move(results));
  }

  void UpdateLayerTreeHost() override { test_hooks_->UpdateLayerTreeHost(); }

  void ApplyViewportChanges(const ApplyViewportChangesArgs& args) override {
    test_hooks_->ApplyViewportChanges(args);
  }

  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override {}

  void UpdateCompositorScrollState(
      const CompositorCommitData& commit_data) override {}

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

  void WillCommit(const CommitState& commit_state) override {
    test_hooks_->WillCommit(commit_state);
  }

  void DidCommit(int source_frame_number,
                 const base::TimeTicks,
                 const base::TimeTicks) override {
    test_hooks_->DidCommit();
  }

  void DidCommitAndDrawFrame(int source_frame_number) override {
    test_hooks_->DidCommitAndDrawFrame();
  }

  void DidRunBeginMainFrame() override { test_hooks_->DidRunBeginMainFrame(); }

  void DidSubmitCompositorFrame() override {}
  void DidLoseLayerTreeFrameSink() override {}
  void DidCompletePageScaleAnimation(int source_frame_number) override {}
  void BeginMainFrameNotExpectedSoon() override {
    test_hooks_->BeginMainFrameNotExpectedSoon();
  }
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& frame_timing_details) override {
    test_hooks_->DidPresentCompositorFrame(frame_token, frame_timing_details);
  }

 private:
  explicit LayerTreeHostClientForTesting(TestHooks* test_hooks)
      : test_hooks_(test_hooks) {}

  raw_ptr<TestHooks> test_hooks_;
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

  std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImplInternal(
      LayerTreeHostImplClient* host_impl_client,
      MutatorHost*,
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      raw_ptr<RasterDarkModeFilter>&,
      int,
      raw_ptr<TaskGraphRunner>& task_graph_runner,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      std::unique_ptr<UkmRecorderFactory>& ukm_recorder_factory,
      base::WeakPtr<CompositorDelegateForInput>& compositor_delegate_weak_ptr)
      override {
    std::unique_ptr<LayerTreeHostImpl> host_impl =
        LayerTreeHostImplForTesting::Create(
            test_hooks_, settings, host_impl_client, scheduling_client,
            task_runner_provider, task_graph_runner,
            rendering_stats_instrumentation, image_worker_task_runner);

    host_impl->InitializeUkm(ukm_recorder_factory->CreateRecorder());
    compositor_delegate_weak_ptr = host_impl->AsWeakPtr();

    // Many tests using this class are specifically meant as input tests so
    // we'll need an input handler. Ideally these would be split out into a
    // separate test harness.
    InputHandler::Create(*compositor_delegate_weak_ptr);

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

  void ApplyCompositorChanges(CompositorCommitData* commit_data) override {
    test_hooks_->WillApplyCompositorChanges();
    LayerTreeHost::ApplyCompositorChanges(commit_data);
  }

  void WaitForProtectedSequenceCompletion() const override {
    wait_count_++;
    LayerTreeHost::WaitForProtectedSequenceCompletion();
  }

  size_t NumCallsToWaitForProtectedSequenceCompletion() const {
    return wait_count_;
  }

  void set_test_started(bool started) { test_started_ = started; }

 private:
  LayerTreeHostForTesting(TestHooks* test_hooks,
                          LayerTreeHost::InitParams params,
                          CompositorMode mode)
      : LayerTreeHost(std::move(params), mode), test_hooks_(test_hooks) {}

  raw_ptr<TestHooks> test_hooks_;
  bool test_started_ = false;
  mutable size_t wait_count_ = 0u;
};

class LayerTreeTestLayerTreeFrameSinkClient
    : public TestLayerTreeFrameSinkClient {
 public:
  explicit LayerTreeTestLayerTreeFrameSinkClient(
      TestHooks* hooks,
      TaskRunnerProvider* task_runner_provider)
      : hooks_(hooks), task_runner_provider_(task_runner_provider) {}

  // TestLayerTreeFrameSinkClient implementation.
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayController() override {
    DCHECK(task_runner_provider_->IsImplThread());
    return hooks_->CreateDisplayControllerOnThread();
  }
  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurface(
      viz::DisplayCompositorMemoryAndTaskController* display_controller)
      override {
    DCHECK(task_runner_provider_->IsImplThread());
    return hooks_->CreateSkiaOutputSurfaceOnThread(display_controller);
  }

  std::unique_ptr<viz::OutputSurface> CreateSoftwareOutputSurface() override {
    DCHECK(task_runner_provider_->IsImplThread());
    return hooks_->CreateSoftwareOutputSurfaceOnThread();
  }
  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {
    DCHECK(task_runner_provider_->IsImplThread());
    hooks_->DisplayReceivedLocalSurfaceIdOnThread(local_surface_id);
  }
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {
    DCHECK(task_runner_provider_->IsImplThread());
    hooks_->DisplayReceivedCompositorFrameOnThread(frame);
  }
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      viz::AggregatedRenderPassList* render_passes) override {
    DCHECK(task_runner_provider_->IsImplThread());
    hooks_->DisplayWillDrawAndSwapOnThread(will_draw_and_swap, *render_passes);
  }
  void DisplayDidDrawAndSwap() override {
    DCHECK(task_runner_provider_->IsImplThread());
    hooks_->DisplayDidDrawAndSwapOnThread();
  }

 private:
  raw_ptr<TestHooks> hooks_;
  raw_ptr<TaskRunnerProvider, AcrossTasksDanglingUntriaged>
      task_runner_provider_;
};

LayerTreeTest::LayerTreeTest(viz::RendererType renderer_type)
    : renderer_type_(renderer_type), initial_root_bounds_(1, 1) {
  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();

  // Tests should timeout quickly unless --cc-layer-tree-test-no-timeout was
  // specified (for running in a debugger).
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kCCLayerTreeTestNoTimeout)) {
    timeout_seconds_ = 10;
#if defined(THREAD_SANITIZER)
    // SwiftShader is a multi-threaded renderer and TSAN takes a lot longer to
    // run tests when using SwiftShader
    timeout_seconds_ = 35;
#elif BUILDFLAG(IS_WIN) && defined(_DEBUG)
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
#elif defined(ADDRESS_SANITIZER) || defined(_DEBUG)
    // ASAN and Debug builds are slower than release builds, as expected.
    timeout_seconds_ = 30;
#elif BUILDFLAG(IS_OZONE)
    // Ozone builds go through a slower path than regular Linux builds.
    timeout_seconds_ = 30;
#elif BUILDFLAG(IS_MAC) && BUILDFLAG(USE_CLANG_COVERAGE)
    // TODO(crbug.com/337055578) SkiaGraphiteDawn renderer is at least 20x
    // slower than the other renderers with clang coverage. Investigate why.
    if (renderer_type_ == viz::RendererType::kSkiaGraphiteDawn) {
      timeout_seconds_ = 25;
    }
#endif
  }

  if (command_line->HasSwitch(switches::kCCLayerTreeTestLongTimeout))
    timeout_seconds_ = 5 * 60;

  // Check if the graphics backend needs to initialize Vulkan.
  bool init_vulkan = false;
  bool init_dawn = false;
  if (renderer_type_ == viz::RendererType::kSkiaVk) {
    scoped_feature_list_.InitAndEnableFeature(features::kVulkan);
    init_vulkan = true;
  } else if (renderer_type_ == viz::RendererType::kSkiaGraphiteDawn) {
    scoped_feature_list_.InitAndEnableFeature(features::kSkiaGraphite);
    bool use_gpu = command_line->HasSwitch(::switches::kUseGpuInTests);
    // Force the use of Graphite even if disallowed for other reasons e.g.
    // ANGLE Metal is not enabled on Mac. Use dawn-swiftshader backend if
    // kUseGpuInTests is not set.
    command_line->AppendSwitch(::switches::kEnableSkiaGraphite);
    command_line->AppendSwitchASCII(
        ::switches::kSkiaGraphiteBackend,
        use_gpu ? ::switches::kSkiaGraphiteBackendDawn
                : ::switches::kSkiaGraphiteBackendDawnSwiftshader);
    init_dawn = true;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    init_vulkan = true;
#endif
  } else if (renderer_type_ == viz::RendererType::kSkiaGraphiteMetal) {
    scoped_feature_list_.InitAndEnableFeature(features::kSkiaGraphite);
    // Force the use of Graphite even if disallowed for other reasons.
    command_line->AppendSwitch(::switches::kEnableSkiaGraphite);
    command_line->AppendSwitchASCII(::switches::kSkiaGraphiteBackend,
                                    ::switches::kSkiaGraphiteBackendMetal);
  } else {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kVulkan, features::kSkiaGraphite});
  }

  if (init_vulkan) {
    bool use_gpu = command_line->HasSwitch(::switches::kUseGpuInTests);
    command_line->AppendSwitchASCII(
        ::switches::kUseVulkan,
        use_gpu ? ::switches::kVulkanImplementationNameNative
                : ::switches::kVulkanImplementationNameSwiftshader);
  }

  if (init_dawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    dawnProcSetProcs(&dawn::native::GetProcs());
#endif
  }
}

LayerTreeTest::~LayerTreeTest() {
  if (animation_host_)
    animation_host_->SetMutatorHostClient(nullptr);
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
      base::Milliseconds(delay_milliseconds));
}

void LayerTreeTest::PostAddNoDamageAnimationToMainThread(
    Animation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddNoDamageAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 1.0));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThread(
    Animation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LayerTreeTest::DispatchAddOpacityAnimation, main_thread_weak_ptr_,
          base::Unretained(animation_to_receive_animation), 0.000004));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThreadInstantly(
    Animation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddOpacityAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 0.0));
}

void LayerTreeTest::PostAddOpacityAnimationToMainThreadDelayed(
    Animation* animation_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DispatchAddOpacityAnimation,
                     main_thread_weak_ptr_,
                     base::Unretained(animation_to_receive_animation), 1.0));
}

void LayerTreeTest::PostSetLocalSurfaceIdToMainThread(
    const viz::LocalSurfaceId& local_surface_id) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchSetLocalSurfaceId,
                                main_thread_weak_ptr_, local_surface_id));
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

void LayerTreeTest::PostDeferringCommitsStatusToMainThread(
    bool is_deferring_commits) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeTest::DispatchDeferringCommitsStatus,
                                main_thread_weak_ptr_, is_deferring_commits));
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
      base::SingleThreadTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner =
      impl_thread_ ? impl_thread_->task_runner() : nullptr;
  LayerTreeHostSchedulingClient* scheduling_client =
      impl_thread_ ? client_.get() : nullptr;

  animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::kMain);

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
    main_task_runner_->PostDelayedTask(FROM_HERE, timeout_.callback(),
                                       base::Seconds(timeout_seconds_));
  }

  started_ = true;
  beginning_ = true;
  SetupTree();
  WillBeginTest();
  if (!skip_allocate_initial_local_surface_id_)
    GenerateNewLocalSurfaceId();
  BeginTest();
  if (!skip_allocate_initial_local_surface_id_) {
    PostSetLocalSurfaceIdToMainThread(GetCurrentLocalSurfaceId());
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

const viz::LocalSurfaceId& LayerTreeTest::GetCurrentLocalSurfaceId() const {
  return allocator_.GetCurrentLocalSurfaceId();
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
                                             viz::LocalSurfaceId());
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

  std::move(quit_closure_).Run();
}

void LayerTreeTest::DispatchAddNoDamageAnimation(
    Animation* animation_to_receive_animation,
    double animation_duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (animation_to_receive_animation) {
    AddOpacityTransitionToAnimation(animation_to_receive_animation,
                                    animation_duration, 0, 0, true);
  }
}

void LayerTreeTest::DispatchAddOpacityAnimation(
    Animation* animation_to_receive_animation,
    double animation_duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (animation_to_receive_animation) {
    AddOpacityTransitionToAnimation(animation_to_receive_animation,
                                    animation_duration, 0, 0.5, true);
  }
}

void LayerTreeTest::DispatchSetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_) {
    layer_tree_host_->SetLocalSurfaceIdFromParent(local_surface_id);
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

void LayerTreeTest::DispatchDeferringCommitsStatus(bool is_deferring_commits) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (is_deferring_commits) {
    layer_tree_host_->StartDeferringCommits(
        base::Milliseconds(1000), PaintHoldingReason::kFirstContentfulPaint);
  } else {
    layer_tree_host_->StopDeferringCommits(
        PaintHoldingCommitTrigger::kFirstContentfulPaint);
  }
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
  if (layer_tree_host_) {
    layer_tree_host_->CompositeForTest(base::TimeTicks::Now(), true,
                                       base::OnceClosure());
  }
}

void LayerTreeTest::DispatchNextCommitWaitsForActivation() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitWaitsForActivation();
}

void LayerTreeTest::RunTest(CompositorMode mode) {
  mode_ = mode;
  if (mode_ == CompositorMode::THREADED) {
    impl_thread_ = std::make_unique<base::Thread>("Compositor");
    ASSERT_TRUE(impl_thread_->Start());
  }

  image_worker_ = std::make_unique<base::Thread>("ImageWorker");
  ASSERT_TRUE(image_worker_->Start());

  gpu_memory_buffer_manager_ =
      std::make_unique<gpu::TestGpuMemoryBufferManager>();
  task_graph_runner_ = std::make_unique<TestTaskGraphRunner>();

  if (mode == CompositorMode::THREADED) {
    settings_.commit_to_active_tree = false;
    settings_.single_thread_proxy_scheduler = false;
  }
  InitializeSettings(&settings_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeTest::DoBeginTest, base::Unretained(this)));

  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
  CleanupBeforeDestroy();
  DestroyLayerTreeHost();

  timeout_.Cancel();

  ASSERT_FALSE(layer_tree_host_.get());
  client_ = nullptr;
  if (timed_out_) {
    FAIL() << "Test timed out";
  }
  AfterTest();
}

void LayerTreeTest::RequestNewLayerTreeFrameSink() {
  scoped_refptr<viz::TestContextProvider> shared_context_provider =
      use_software_renderer() ? nullptr
                              : viz::TestContextProvider::CreateRaster();
  scoped_refptr<viz::TestContextProvider> worker_context_provider =
      use_software_renderer() ? nullptr
                              : viz::TestContextProvider::CreateWorker();

  if (!use_software_renderer()) {
    SetUpUnboundContextProviders(shared_context_provider.get(),
                                 worker_context_provider.get());
  }

  viz::RendererSettings renderer_settings;
  // Spend less time waiting for BeginFrame because the output is
  // mocked out.
  constexpr double refresh_rate = 200.0;
  auto layer_tree_frame_sink = CreateLayerTreeFrameSink(
      renderer_settings, refresh_rate, std::move(shared_context_provider),
      std::move(worker_context_provider));
  if (!layer_tree_frame_sink_client_) {
    layer_tree_frame_sink_client_ =
        std::make_unique<LayerTreeTestLayerTreeFrameSinkClient>(
            this, task_runner_provider());
  }
  layer_tree_frame_sink->SetClient(layer_tree_frame_sink_client_.get());
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void LayerTreeTest::SetUpUnboundContextProviders(
    viz::TestContextProvider* context_provider,
    viz::TestContextProvider* worker_context_provider) {}

std::unique_ptr<TestLayerTreeFrameSink> LayerTreeTest::CreateLayerTreeFrameSink(
    const viz::RendererSettings& renderer_settings,
    double refresh_rate,
    scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
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
        gpu_memory_buffer_manager(), renderer_settings, &debug_settings_,
        task_runner_provider(), refresh_rate, begin_frame_source_,
        use_software_renderer());
  }

  return std::make_unique<TestLayerTreeFrameSink>(
      compositor_context_provider, std::move(worker_context_provider),
      gpu_memory_buffer_manager(), renderer_settings, &debug_settings_,
      task_runner_provider(), synchronous_composite, disable_display_vsync,
      refresh_rate, begin_frame_source_);
}

std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
LayerTreeTest::CreateDisplayControllerOnThread() {
  // In this implementation, none of the output surface has a real gpu thread,
  // and there is no overlay support.
  return nullptr;
}

std::unique_ptr<viz::SkiaOutputSurface>
LayerTreeTest::CreateSkiaOutputSurfaceOnThread(
    viz::DisplayCompositorMemoryAndTaskController*) {
  return viz::FakeSkiaOutputSurface::Create3d();
}

std::unique_ptr<viz::OutputSurface>
LayerTreeTest::CreateSoftwareOutputSurfaceOnThread() {
  return std::make_unique<viz::FakeSoftwareOutputSurface>(
      std::make_unique<viz::SoftwareOutputDevice>());
}

size_t LayerTreeTest::NumCallsToWaitForProtectedSequenceCompletion() const {
  return static_cast<LayerTreeHostForTesting*>(layer_tree_host_.get())
      ->NumCallsToWaitForProtectedSequenceCompletion();
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

LayerTreeHost* LayerTreeTest::layer_tree_host() const {
  return layer_tree_host_.get();
}

Proxy* LayerTreeTest::proxy() {
  return layer_tree_host() ? layer_tree_host()->proxy_.get() : nullptr;
}

}  // namespace cc
