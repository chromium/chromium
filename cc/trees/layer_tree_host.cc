// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/layer_tree_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/metrics/ukm_manager.h"
#include "cc/metrics/ukm_smoothness_data.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/tiles/raster_dark_mode_filter.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/commit_state.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mobile_optimized_viewport_util.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/latency/latency_info.h"

namespace {
static base::AtomicSequenceNumber s_layer_tree_host_sequence_number;
static base::AtomicSequenceNumber s_image_decode_sequence_number;
}  // namespace

namespace cc {
namespace {

bool AreEmbedTokensEqual(const viz::LocalSurfaceId& lsi1,
                         const viz::LocalSurfaceId& lsi2) {
  return lsi1.embed_token() == lsi2.embed_token();
}

bool AreParentSequencesEqual(const viz::LocalSurfaceId& lsi1,
                             const viz::LocalSurfaceId& lsi2) {
  return lsi1.parent_sequence_number() == lsi2.parent_sequence_number();
}

}  // namespace

LayerTreeHost::InitParams::InitParams() = default;
LayerTreeHost::InitParams::~InitParams() = default;
LayerTreeHost::InitParams::InitParams(InitParams&&) = default;
LayerTreeHost::InitParams& LayerTreeHost::InitParams::operator=(InitParams&&) =
    default;

LayerTreeHost::ScrollAnimationState::ScrollAnimationState() = default;
LayerTreeHost::ScrollAnimationState::~ScrollAnimationState() = default;

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams params) {
  DCHECK(params.settings);
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      params.main_task_runner;
  DCHECK(main_task_runner);
  DCHECK(impl_task_runner);
  auto layer_tree_host = base::WrapUnique(
      new LayerTreeHost(std::move(params), CompositorMode::THREADED));
  layer_tree_host->InitializeThreaded(std::move(main_task_runner),
                                      std::move(impl_task_runner));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams params) {
  DCHECK(params.settings);
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      params.main_task_runner;
  auto layer_tree_host = base::WrapUnique(
      new LayerTreeHost(std::move(params), CompositorMode::SINGLE_THREADED));
  layer_tree_host->InitializeSingleThreaded(single_thread_client,
                                            std::move(main_task_runner));
  return layer_tree_host;
}

LayerTreeHost::LayerTreeHost(InitParams params, CompositorMode mode)
    : micro_benchmark_controller_(this),
      image_worker_task_runner_(std::move(params.image_worker_task_runner)),
      ukm_recorder_factory_(std::move(params.ukm_recorder_factory)),
      compositor_mode_(mode),
      ui_resource_manager_(std::make_unique<UIResourceManager>()),
      client_(params.client),
      scheduling_client_(params.scheduling_client),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      pending_commit_state_(std::make_unique<CommitState>()),
      thread_unsafe_commit_state_(params.mutator_host, *this),
      settings_(*params.settings),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      task_graph_runner_(params.task_graph_runner),
      mutator_host_(params.mutator_host),
      dark_mode_filter_(params.dark_mode_filter) {
  DCHECK(task_graph_runner_);
  DCHECK(!settings_.enable_checker_imaging || image_worker_task_runner_);

  pending_commit_state_->needs_gpu_rasterization_histogram =
      (mode == CompositorMode::THREADED);
  pending_commit_state_->needs_full_tree_sync = true;
  pending_commit_state_->debug_state = settings_.initial_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      pending_commit_state_->debug_state.RecordRenderingStats());
}

bool LayerTreeHost::IsMobileOptimized() const {
  gfx::SizeF scrollable_viewport_size;
  const auto* inner_node = property_trees()->scroll_tree().Node(
      pending_commit_state()->viewport_property_ids.inner_scroll);
  if (!inner_node)
    scrollable_viewport_size = gfx::SizeF();
  else
    scrollable_viewport_size = gfx::ScaleSize(
        gfx::SizeF(inner_node->container_bounds),
        1.0f / (pending_commit_state()->external_page_scale_factor *
                page_scale_factor()));

  gfx::SizeF scrollable_size;
  const auto* scroll_node = property_trees()->scroll_tree().Node(
      pending_commit_state()->viewport_property_ids.outer_scroll);
  if (!scroll_node) {
    DCHECK(!inner_node);
    scrollable_size = gfx::SizeF();
  } else {
    const auto& scroll_tree = property_trees()->scroll_tree();
    auto size = scroll_tree.scroll_bounds(scroll_node->id);
    size.SetToMax(gfx::SizeF(scroll_tree.container_bounds(scroll_node->id)));
    scrollable_size = size;
  }

  return util::IsMobileOptimized(
      min_page_scale_factor(), max_page_scale_factor(), page_scale_factor(),
      scrollable_viewport_size, scrollable_size,
      pending_commit_state()->is_viewport_mobile_optimized);
}

void LayerTreeHost::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
  std::unique_ptr<ProxyMain> proxy_main =
      std::make_unique<ProxyMain>(this, task_runner_provider_.get());
  InitializeProxy(std::move(proxy_main));
}

void LayerTreeHost::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);
  InitializeProxy(SingleThreadProxy::Create(this, single_thread_client,
                                            task_runner_provider_.get()));
}

void LayerTreeHost::InitializeForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider,
    std::unique_ptr<Proxy> proxy_for_testing) {
  task_runner_provider_ = std::move(task_runner_provider);
  InitializeProxy(std::move(proxy_for_testing));
}

void LayerTreeHost::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  DCHECK(!task_runner_provider_);
  task_runner_provider_ = std::move(task_runner_provider);
  // This is done in InitializeProxy(), but not all tests call it.
  mutator_host_->SetMutatorHostClient(this);
}

void LayerTreeHost::SetUIResourceManagerForTesting(
    std::unique_ptr<UIResourceManager> ui_resource_manager) {
  ui_resource_manager_ = std::move(ui_resource_manager);
}

void LayerTreeHost::InitializeProxy(std::unique_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");
  DCHECK(task_runner_provider_);
  DCHECK(IsMainThread());

  mutator_host_->SetMutatorHostClient(this);

  proxy_ = std::move(proxy);
  proxy_->Start();

  UpdateDeferMainFrameUpdateInternal();
}

LayerTreeHost::~LayerTreeHost() {
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/895883
  CHECK(!inside_main_frame_);
  DCHECK(!in_commit());
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  // Clear any references into the LayerTreeHost.
  mutator_host()->SetMutatorHostClient(nullptr);

  if (root_layer()) {
    root_layer()->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    thread_unsafe_commit_state().root_layer = nullptr;
  }

  // Fail any pending image decodes.
  for (auto& pair : pending_image_decodes_)
    std::move(pair.second).Run(false);

  if (proxy_) {
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }
}

int LayerTreeHost::GetId() const {
  return id_;
}

int LayerTreeHost::SourceFrameNumber() const {
  return pending_commit_state()->source_frame_number;
}

UIResourceManager* LayerTreeHost::GetUIResourceManager() {
  DCHECK(IsMainThread());
  return ui_resource_manager_.get();
}

TaskRunnerProvider* LayerTreeHost::GetTaskRunnerProvider() {
  return task_runner_provider_.get();
}

bool LayerTreeHost::IsMainThread() const {
  return task_runner_provider_ && task_runner_provider_->IsMainThread();
}

bool LayerTreeHost::IsImplThread() const {
  return task_runner_provider_ && task_runner_provider_->IsImplThread();
}

bool LayerTreeHost::IsOwnerThread() const {
  return task_runner_provider_->MainThreadTaskRunner()
      ->RunsTasksInCurrentSequence();
}

bool LayerTreeHost::InProtectedSequence() const {
  return in_commit();
}

SwapPromiseManager* LayerTreeHost::GetSwapPromiseManager() {
  DCHECK(IsMainThread());
  return &swap_promise_manager_;
}

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
LayerTreeHost::GetScopedEventMetricsMonitor(
    EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) {
  DCHECK(IsMainThread());
  return events_metrics_manager_.GetScopedMonitor(std::move(done_callback));
}

void LayerTreeHost::ClearEventsMetrics() {
  // Take events metrics and drop them.
  DCHECK(IsMainThread());
  events_metrics_manager_.TakeSavedEventsMetrics();
}

const LayerTreeSettings& LayerTreeHost::GetSettings() const {
  DCHECK(IsMainThread());
  return settings_;
}

void LayerTreeHost::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(IsMainThread());
  swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
}

void LayerTreeHost::WillBeginMainFrame() {
  DCHECK(IsMainThread());
  inside_main_frame_ = true;
  devtools_instrumentation::WillBeginMainThreadFrame(GetId(),
                                                     SourceFrameNumber());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  DCHECK(IsMainThread());
  inside_main_frame_ = false;
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  DCHECK(IsMainThread());
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  DCHECK(IsMainThread());
  client_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeHost::BeginMainFrame(const viz::BeginFrameArgs& args) {
  DCHECK(IsMainThread());
  client_->BeginMainFrame(args);
}

const LayerTreeDebugState& LayerTreeHost::GetDebugState() const {
  return pending_commit_state()->debug_state;
}

void LayerTreeHost::RequestMainFrameUpdate(bool report_metrics) {
  DCHECK(IsMainThread());
  client_->UpdateLayerTreeHost();
  if (report_metrics)
    pending_commit_state()->begin_main_frame_metrics =
        client_->GetBeginMainFrameMetrics();
  else
    pending_commit_state()->begin_main_frame_metrics.reset();
}

void LayerTreeHost::ImageDecodesFinished(
    const std::vector<std::pair<int, bool>>& results) {
  for (const auto& pair : results) {
    NotifyImageDecodeFinished(pair.first, pair.second);
  }
}

void LayerTreeHost::SetNextCommitWaitsForActivation() {
  pending_commit_state()->commit_waits_for_activation = true;
}

std::unique_ptr<CommitState> LayerTreeHost::WillCommit(
    std::unique_ptr<CompletionEvent> completion,
    bool has_updates) {
  DCHECK(IsMainThread());
  DCHECK(!commit_completion_event_);
  std::unique_ptr<CommitState> result;
  if (has_updates)
    result = ActivateCommitState();
  swap_promise_manager_.WillCommit();
  mutator_host()->RemoveStaleTimelines();
  client_->WillCommit(has_updates ? *result : *pending_commit_state());
  pending_commit_state()->source_frame_number++;
  commit_completion_event_ = std::move(completion);
  return result;
}

std::unique_ptr<CommitState> LayerTreeHost::ActivateCommitState() {
  DCHECK(IsMainThread());
  DCHECK(pending_commit_state());

  // Pull state not stored directly on LayerTreeHost
  pending_commit_state()->event_metrics =
      events_metrics_manager_.TakeSavedEventsMetrics();
  pending_commit_state()->swap_promises =
      GetSwapPromiseManager()->TakeSwapPromises();
  pending_commit_state()->ui_resource_request_queue =
      ui_resource_manager_->TakeUIResourcesRequests();
  pending_commit_state()->ui_resource_sizes =
      ui_resource_manager_->GetUIResourceSizes();
  pending_commit_state()->benchmarks =
      micro_benchmark_controller_.CreateImplBenchmarks();

  // Snapshot PropertyTrees change tracking state prior to resetting it.
  property_trees()->GetChangeState(
      pending_commit_state()->property_trees_change_state);
  property_trees()->ResetAllChangeTracking();

  auto active_commit_state = std::move(pending_commit_state_);
  pending_commit_state_ = std::make_unique<CommitState>(*active_commit_state);
  return active_commit_state;
}

void LayerTreeHost::WaitForProtectedSequenceCompletion() const {
  if (compositor_mode_ == CompositorMode::SINGLE_THREADED)
    return;
  WaitForCommitCompletion(/* for_protected_sequence */ true);
}

void LayerTreeHost::WaitForCommitCompletion(bool for_protected_sequence) const {
  DCHECK(IsMainThread());
  if (commit_completion_event_) {
    TRACE_EVENT0("cc", "LayerTreeHost::WaitForCommitCompletion");
    base::ElapsedTimer timer;
    commit_completion_event_->Wait();
    commit_completion_event_ = nullptr;
    if (for_protected_sequence) {
      waited_for_protected_sequence_ = true;
      auto elapsed = timer.Elapsed();
      base::UmaHistogramMicrosecondsTimes(
          "Compositing.MainThreadBlockedDuringCommitTime", elapsed);
      if (in_apply_compositor_changes_) {
        base::UmaHistogramMicrosecondsTimes(
            "Compositing.MainThreadBlockedDuringCommitTime."
            "ApplyCompositorChanges",
            elapsed);
      }
    }
  }
}

void LayerTreeHost::UpdateDeferMainFrameUpdateInternal() {
  DCHECK(IsMainThread());
  proxy_->SetDeferMainFrameUpdate(MainFrameUpdatesAreDeferred());
}

bool LayerTreeHost::MainFrameUpdatesAreDeferred() const {
  return defer_main_frame_update_count_ > 0 ||
         !pending_commit_state()->local_surface_id_from_parent.is_valid();
}

bool LayerTreeHost::IsUsingLayerLists() const {
  return settings_.use_layer_lists;
}

void LayerTreeHost::CommitComplete(int source_frame_number,
                                   const CommitTimestamps& commit_timestamps) {
  DCHECK(IsMainThread());

  // At this point, commit_completion_event_ could be for the *next* commit, and
  // may not yet have been signaled. If we blocked on a protected sequence
  // during the commit then the completion event for the frame will have been
  // reset, which in turn unblocks starting a commit for the next frame. If we
  // have a commit completion event that has been signaled, it means that we
  // have not been blocked on a protected sequence during the commit. In this
  // case, we still need to call WaitForCommitCompletion, which performs the
  // flag reset; however, the Wait will be non-blocking given that the event was
  // already signaled.
  if (!in_commit()) {
    mutator_host()->RemoveStaleTimelines();
  }
  if (commit_completion_event_ && commit_completion_event_->IsSignaled())
    WaitForCommitCompletion(/* for_protected_sequence */ false);
  client_->DidCommit(source_frame_number, commit_timestamps.start,
                     commit_timestamps.finish);
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation(source_frame_number);
    did_complete_scale_animation_ = false;
  }
  if (compositor_mode_ == CompositorMode::THREADED) {
    UMA_HISTOGRAM_BOOLEAN("Compositing.DidMainThreadBlockDuringCommit",
                          waited_for_protected_sequence_);
  }
  waited_for_protected_sequence_ = false;
}

void LayerTreeHost::NotifyImageDecodeFinished(int request_id,
                                              bool decode_succeeded) {
  DCHECK(IsMainThread());
  auto it = pending_image_decodes_.find(request_id);
  CHECK(it != pending_image_decodes_.end(), base::NotFatalUntil::M130);
  // Issue stored callback and remove them from the pending list.
  std::move(it->second).Run(decode_succeeded);
  pending_image_decodes_.erase(it);
}

void LayerTreeHost::NotifyTransitionRequestsFinished(
    const std::vector<uint32_t>& sequence_ids) {
  DCHECK(IsMainThread());
  // TODO(vmpstr): This might also be a good spot to expire long standing
  // requests if they were not finished.
  for (auto& sequence_id : sequence_ids) {
    auto it = view_transition_callbacks_.find(sequence_id);
    if (it == view_transition_callbacks_.end())
      continue;
    std::move(it->second).Run();
    view_transition_callbacks_.erase(it);
  }
}

void LayerTreeHost::SetLayerTreeFrameSink(
    std::unique_ptr<LayerTreeFrameSink> surface) {
  DCHECK(IsMainThread());
  TRACE_EVENT0("cc", "LayerTreeHost::SetLayerTreeFrameSink");
  DCHECK(surface);

  DCHECK(!new_layer_tree_frame_sink_);
  new_layer_tree_frame_sink_ = std::move(surface);
  proxy_->SetLayerTreeFrameSink(new_layer_tree_frame_sink_.get());
}

std::unique_ptr<LayerTreeFrameSink> LayerTreeHost::ReleaseLayerTreeFrameSink() {
  DCHECK(IsMainThread());
  DCHECK(!visible_);

  DidLoseLayerTreeFrameSink();
  proxy_->ReleaseLayerTreeFrameSink();
  return std::move(current_layer_tree_frame_sink_);
}

void LayerTreeHost::RequestNewLayerTreeFrameSink() {
  DCHECK(IsMainThread());
  client_->RequestNewLayerTreeFrameSink();
  should_warm_up_ = false;
}

void LayerTreeHost::DidInitializeLayerTreeFrameSink() {
  DCHECK(IsMainThread());
  DCHECK(new_layer_tree_frame_sink_);
  current_layer_tree_frame_sink_ = std::move(new_layer_tree_frame_sink_);
  client_->DidInitializeLayerTreeFrameSink();
}

void LayerTreeHost::DidFailToInitializeLayerTreeFrameSink() {
  DCHECK(IsMainThread());
  DCHECK(new_layer_tree_frame_sink_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new LayerTreeFrameSink after failing to initialize the new one.
  current_layer_tree_frame_sink_ = nullptr;
  new_layer_tree_frame_sink_ = nullptr;
  client_->DidFailToInitializeLayerTreeFrameSink();
}

std::unique_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  // This method is special: it should be the only LayerTreeHost method that
  // runs on the impl thread. As such, it cannot use LayerTreeHost getter
  // methods that enforce DCHECK(IsMainThread()). Because it only ever runs when
  // the main thread is blocked, it's safe to access member variables directly.
  DCHECK(IsImplThread());
  DCHECK(task_runner_provider_->IsMainThreadBlocked());
  return CreateLayerTreeHostImplInternal(
      client, thread_unsafe_commit_state_.mutator_host, settings_,
      task_runner_provider_.get(), dark_mode_filter_, id_, task_graph_runner_,
      image_worker_task_runner_, scheduling_client_,
      rendering_stats_instrumentation_.get(), ukm_recorder_factory_,
      compositor_delegate_weak_ptr_);
}

std::unique_ptr<LayerTreeHostImpl>
LayerTreeHost::CreateLayerTreeHostImplInternal(
    LayerTreeHostImplClient* client,
    MutatorHost* mutator_host,
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    raw_ptr<RasterDarkModeFilter>& dark_mode_filter,
    int id,
    raw_ptr<TaskGraphRunner>& task_graph_runner,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    std::unique_ptr<UkmRecorderFactory>& ukm_recorder_factory,
    base::WeakPtr<CompositorDelegateForInput>& compositor_delegate_weak_ptr) {
  std::unique_ptr<MutatorHost> mutator_host_impl =
      mutator_host->CreateImplInstance();

  if (!settings.scroll_animation_duration_for_testing.is_zero()) {
    mutator_host->SetScrollAnimationDurationForTesting(  // IN-TEST
        settings.scroll_animation_duration_for_testing);
  }

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host_impl), dark_mode_filter, id,
      std::move(image_worker_task_runner), scheduling_client);
  if (ukm_recorder_factory) {
    host_impl->InitializeUkm(ukm_recorder_factory->CreateRecorder());
    ukm_recorder_factory.reset();
  }

  task_graph_runner = nullptr;
  dark_mode_filter = nullptr;
  compositor_delegate_weak_ptr = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHost::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseLayerTreeFrameSink");
  DCHECK(task_runner_provider_->IsMainThread());
  SetNeedsCommit();
}

ScopedDeferMainFrameUpdate::ScopedDeferMainFrameUpdate(LayerTreeHost* host)
    : host_(host->weak_ptr_factory_.GetWeakPtr()) {
  host->defer_main_frame_update_count_++;
  host->UpdateDeferMainFrameUpdateInternal();
}

ScopedDeferMainFrameUpdate::~ScopedDeferMainFrameUpdate() {
  LayerTreeHost* host = host_.get();
  if (host) {
    DCHECK_GT(host->defer_main_frame_update_count_, 0u);
    if (--host->defer_main_frame_update_count_ == 0)
      host->UpdateDeferMainFrameUpdateInternal();
  }
}

std::unique_ptr<ScopedDeferMainFrameUpdate>
LayerTreeHost::DeferMainFrameUpdate() {
  DCHECK(IsMainThread());
  return std::make_unique<ScopedDeferMainFrameUpdate>(this);
}

ScopedPauseRendering::ScopedPauseRendering(LayerTreeHost* host)
    : host_(host->weak_ptr_factory_.GetWeakPtr()) {
  host->pause_rendering_count_++;
  host->proxy_->SetPauseRendering(true);
}

ScopedPauseRendering::~ScopedPauseRendering() {
  LayerTreeHost* host = host_.get();
  if (host) {
    DCHECK_GT(host->pause_rendering_count_, 0u);
    if (--host->pause_rendering_count_ == 0) {
      host->SetNeedsCommit();
      host->proxy_->SetPauseRendering(false);
    }
  }
}

std::unique_ptr<ScopedPauseRendering> LayerTreeHost::PauseRendering() {
  DCHECK(IsMainThread());
  return std::make_unique<ScopedPauseRendering>(this);
}

void LayerTreeHost::OnDeferMainFrameUpdatesChanged(bool defer_status) {
  DCHECK(IsMainThread());
  client_->OnDeferMainFrameUpdatesChanged(defer_status);
}

bool LayerTreeHost::StartDeferringCommits(base::TimeDelta timeout,
                                          PaintHoldingReason reason) {
  DCHECK(IsMainThread());
  return proxy_->StartDeferringCommits(timeout, reason);
}

void LayerTreeHost::StopDeferringCommits(PaintHoldingCommitTrigger trigger) {
  DCHECK(IsMainThread());
  proxy_->StopDeferringCommits(trigger);
}

bool LayerTreeHost::IsDeferringCommits() const {
  DCHECK(IsMainThread());
  return proxy_->IsDeferringCommits();
}

bool LayerTreeHost::IsRenderingPaused() const {
  return pause_rendering_count_ > 0;
}

void LayerTreeHost::OnDeferCommitsChanged(
    bool defer_status,
    PaintHoldingReason reason,
    std::optional<PaintHoldingCommitTrigger> trigger) {
  DCHECK(IsMainThread());
  client_->OnDeferCommitsChanged(defer_status, reason, trigger);
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  proxy_->SetNeedsAnimate();
  swap_promise_manager_.NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
}

void LayerTreeHost::SetNeedsAnimateIfNotInsideMainFrame() {
  DCHECK(IsMainThread());
  if (!inside_main_frame_)
    SetNeedsAnimate();
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());
  proxy_->SetNeedsUpdateLayers();
  swap_promise_manager_.NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
}

void LayerTreeHost::SetNeedsCommit() {
  DCHECK(IsMainThread());
  proxy_->SetNeedsCommit();
  swap_promise_manager_.NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
}

void LayerTreeHost::OnCommitRequested() {
  client_->OnCommitRequested();
}

void LayerTreeHost::SetTargetLocalSurfaceId(
    const viz::LocalSurfaceId& target_local_surface_id) {
  DCHECK(IsMainThread());
  proxy_->SetTargetLocalSurfaceId(target_local_surface_id);
}

bool LayerTreeHost::RequestedMainFramePending() const {
  DCHECK(IsMainThread());
  return proxy_->RequestedAnimatePending();
}

void LayerTreeHost::SetNeedsRecalculateRasterScales() {
  pending_commit_state()->next_commit_forces_recalculate_raster_scales = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  DCHECK(IsMainThread());
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHost::CommitRequested() const {
  DCHECK(IsMainThread());
  return proxy_->CommitRequested();
}

void LayerTreeHost::SetNeedsCommitWithForcedRedraw() {
  pending_commit_state()->next_commit_forces_redraw = true;
  // This method is used by tests to ensure a commit before grabbing a screen
  // shot or processing input, so do not defer the commit.
  StopDeferringCommits(PaintHoldingCommitTrigger::kFeatureDisabled);
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetDebugState(const LayerTreeDebugState& new_debug_state) {
  if (pending_commit_state()->debug_state == new_debug_state) {
    return;
  }

  pending_commit_state()->debug_state = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      pending_commit_state()->debug_state.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(float page_scale_delta) {
  DCHECK(IsMainThread());
  DCHECK(syncing_deltas_for_test_ || CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  float page_scale =
      pending_commit_state()->page_scale_factor * page_scale_delta;
  SetPageScaleFromImplSide(page_scale);
}

void LayerTreeHost::SetVisible(bool visible) {
  DCHECK(IsMainThread());
  if (visible_ == visible)
    return;
  visible_ = visible;
  // If the page becomes invisible, corresponding metrics will be discarded
  // because they are invalid. The metrics will be measured when the page
  // becomes visible again.
  if (!visible_) {
    ClearEventsMetrics();
  }
  proxy_->SetVisible(visible);
}

bool LayerTreeHost::IsVisible() const {
  DCHECK(IsMainThread());
  return visible_;
}

void LayerTreeHost::SetShouldWarmUp() {
  DCHECK(IsMainThread());
  CHECK(base::FeatureList::IsEnabled(features::kWarmUpCompositor));
  should_warm_up_ = true;
  proxy_->SetShouldWarmUp();
}

bool LayerTreeHost::ShouldWarmUp() const {
  DCHECK(IsMainThread());
  if (!base::FeatureList::IsEnabled(features::kWarmUpCompositor)) {
    return false;
  }
  return should_warm_up_;
}

void LayerTreeHost::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  RequestMainFrameUpdate(false);
  UpdateLayers();
}

void LayerTreeHost::CompositeForTest(base::TimeTicks frame_begin_time,
                                     bool raster,
                                     base::OnceClosure callback) {
  DCHECK(!settings_.single_thread_proxy_scheduler);
  proxy_->CompositeImmediatelyForTest(frame_begin_time, raster,  // IN-TEST
                                      std::move(callback));
}

bool LayerTreeHost::UpdateLayers() {
  if (!root_layer()) {
    property_trees()->clear();
    pending_commit_state()->viewport_property_ids = ViewportPropertyIds();
    return false;
  }

  DCHECK(!root_layer()->parent());
  base::ElapsedTimer timer;

  client_->WillUpdateLayers();
  bool result = DoUpdateLayers();
  client_->DidUpdateLayers();
  micro_benchmark_controller_.DidUpdateLayers();

  base::TimeDelta elapsed_delta = timer.Elapsed();
  if (pending_commit_state()->begin_main_frame_metrics) {
    pending_commit_state()->begin_main_frame_metrics->update_layers =
        elapsed_delta;
  }
  if (const char* client_name = GetClientNameForMetrics()) {
    auto elapsed = elapsed_delta.InMicroseconds();

    // The histogram name must be a constant for the macro to work.
    // GetClientNameForMetrics() guarantees to always return the same string or
    // null
    UMA_HISTOGRAM_COUNTS_10M(
        base::StringPrintf("Compositing.%s.LayersUpdateTime", client_name),
        elapsed);
  }

  return result;
}

void LayerTreeHost::DidPresentCompositorFrame(
    uint32_t frame_token,
    std::vector<PresentationTimeCallbackBuffer::Callback>
        presentation_callbacks,
    std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
        successful_presentation_callbacks,
    const viz::FrameTimingDetails& frame_timing_details) {
  DCHECK(IsMainThread());
  for (auto& callback : presentation_callbacks)
    std::move(callback).Run(frame_timing_details.presentation_feedback);
  for (auto& callback : successful_presentation_callbacks)
    std::move(callback).Run(frame_timing_details);
  client_->DidPresentCompositorFrame(frame_token, frame_timing_details);
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  DCHECK(IsMainThread());
  did_complete_scale_animation_ = true;
}

std::string LayerTreeHost::LayersAsString() const {
  std::string layers;
  for (const auto* layer : *this)
    layers += layer->ToString() + "\n";
  return layers;
}

bool LayerTreeHost::CaptureContent(std::vector<NodeInfo>* content) const {
  DCHECK(IsMainThread());
  if (visual_device_viewport_intersection_rect_.IsEmpty())
    return false;

  gfx::Rect rect =
      gfx::Rect(visual_device_viewport_intersection_rect_.width(),
                visual_device_viewport_intersection_rect_.height());
  for (const auto* layer : *this) {
    // Normally, the node won't be drawn in multiple layers, even it is, such as
    // text strokes, the visual rect don't have too much different.
    layer->CaptureContent(rect, content);
  }
  return true;
}

void LayerTreeHost::DidObserveFirstScrollDelay(
    int source_frame_number,
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  DCHECK(IsMainThread());
  client_->DidObserveFirstScrollDelay(source_frame_number, first_scroll_delay,
                                      first_scroll_timestamp);
}

void LayerTreeHost::AddViewTransitionRequest(
    std::unique_ptr<ViewTransitionRequest> request) {
  // Store the commit callback on LayerTreeHost, so that we can invoke them
  // when the request is finished.
  DCHECK(!base::Contains(view_transition_callbacks_, request->sequence_id()));
  if (auto callback = request->TakeFinishedCallback()) {
    view_transition_callbacks_[request->sequence_id()] = std::move(callback);
  }
  pending_commit_state()->view_transition_requests.push_back(
      std::move(request));
  SetNeedsCommit();
}

bool LayerTreeHost::DoUpdateLayers() {
  DCHECK(IsMainThread());
  TRACE_EVENT1("cc,benchmark", "LayerTreeHost::DoUpdateLayers",
               "source_frame_number", SourceFrameNumber());

  UpdateHudLayer(pending_commit_state()->debug_state.ShouldCreateHudLayer());

  // In layer lists mode, the cc property trees are built directly and do not
  // need to be built here.
  if (!IsUsingLayerLists()) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::BuildPropertyTrees");
    PropertyTreeBuilder::BuildPropertyTrees(this);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "LayerTreeHost::UpdateLayers_BuiltPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees()->AsTracedValue());
  } else {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "LayerTreeHost::UpdateLayers_ReceivedPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees()->AsTracedValue());
    // The HUD layer is managed outside the layer list sent to LayerTreeHost
    // and needs to have its property tree state set.
    if (hud_layer() && root_layer()) {
      hud_layer()->SetTransformTreeIndex(root_layer()->transform_tree_index());
      hud_layer()->SetEffectTreeIndex(root_layer()->effect_tree_index());
      hud_layer()->SetClipTreeIndex(root_layer()->clip_tree_index());
      hud_layer()->SetScrollTreeIndex(root_layer()->scroll_tree_index());
      hud_layer()->set_property_tree_sequence_number(
          root_layer()->property_tree_sequence_number());
    }
  }

#if DCHECK_IS_ON()
  // Ensure property tree nodes were created for all layers. When using layer
  // lists, this can fail if blink doesn't setup layers or nodes correctly in
  // |PaintArtifactCompositor|. When not using layer lists, this can fail if
  // |PropertyTreeBuilder::BuildPropertyTrees| fails to create property tree
  // nodes.
  for (auto* layer : *this) {
    DCHECK(property_trees()->effect_tree().Node(layer->effect_tree_index()));
    DCHECK(
        property_trees()->transform_tree().Node(layer->transform_tree_index()));
    DCHECK(property_trees()->clip_tree().Node(layer->clip_tree_index()));
    DCHECK(property_trees()->scroll_tree().Node(layer->scroll_tree_index()));
  }
#else
  // This is a quick sanity check for readiness of paint properties.
  // TODO(crbug.com/40605801): This is to help analysis of crashes of the bug.
  // Remove this CHECK when we close the bug.
  CHECK(
      property_trees()->effect_tree().Node(root_layer()->effect_tree_index()));
#endif

  draw_property_utils::UpdatePropertyTrees(this);

  LayerList update_layer_list;
  draw_property_utils::FindLayersThatNeedUpdates(this, &update_layer_list);
  bool did_paint_content = PaintContent(update_layer_list);
  return did_paint_content;
}

void LayerTreeHost::ApplyViewportChanges(
    const CompositorCommitData& commit_data) {
  DCHECK(IsMainThread());
  gfx::Vector2dF inner_viewport_scroll_delta;
  if (commit_data.inner_viewport_scroll.element_id)
    inner_viewport_scroll_delta =
        commit_data.inner_viewport_scroll.scroll_delta;

  // When a new scroll-animation starts, it is necessary to check
  // |commit_data.manipulation_info| to make sure the scroll-animation was
  // started by an input event. If there is already an ongoing scroll-animation,
  // then it is necessary to only look at |commit_data.ongoing_scroll_animation|
  // (since it is possible for the scroll-animation to continue even if no event
  // was handled).
  bool new_ongoing_scroll = scroll_animation_.in_progress
                                ? commit_data.ongoing_scroll_animation
                                : (commit_data.ongoing_scroll_animation &&
                                   commit_data.manipulation_info);
  scroll_animation_.in_progress = new_ongoing_scroll;

  // A pointer-down event on a scrollbar button triggers two scroll animations:
  // one that starts immediately and ends after one scroll increment (typically
  // 40px); and an autoscroll that starts after a 250ms delay and continues
  // until pointer-up or until the end of the scroll node is reached.
  // scroll_animation_.end_notification should wait for the first animation to
  // finish, but it should *not* wait for the autoscroll to finish.
  if (!scroll_animation_.end_notification.is_null() &&
      (!scroll_animation_.in_progress || commit_data.is_auto_scrolling)) {
    std::move(scroll_animation_.end_notification).Run();
  }

  if (inner_viewport_scroll_delta.IsZero() &&
      commit_data.page_scale_delta == 1.f &&
      commit_data.elastic_overscroll_delta.IsZero() &&
      !commit_data.top_controls_delta && !commit_data.bottom_controls_delta &&
      !commit_data.browser_controls_constraint_changed &&
      !commit_data.scroll_end_data.scroll_gesture_did_end &&
      commit_data.is_pinch_gesture_active ==
          is_pinch_gesture_active_from_impl_) {
    return;
  }
  is_pinch_gesture_active_from_impl_ = commit_data.is_pinch_gesture_active;

  // const_cast to ensure the compiler chooses to the const version of
  // property_trees(), to avoid blocking on commit.
  const auto* pt = const_cast<const LayerTreeHost*>(this)->property_trees();
  if (const auto* inner_scroll = pt->scroll_tree().Node(
          pending_commit_state()->viewport_property_ids.inner_scroll)) {
    UpdateScrollOffsetFromImpl(
        inner_scroll->element_id, inner_viewport_scroll_delta,
        commit_data.inner_viewport_scroll.snap_target_element_ids);
  }

  ApplyPageScaleDeltaFromImplSide(commit_data.page_scale_delta);
  SetElasticOverscrollFromImplSide(pending_commit_state()->elastic_overscroll +
                                   commit_data.elastic_overscroll_delta);
  // TODO(ccameron): pass the elastic overscroll here so that input events
  // may be translated appropriately.
  client_->ApplyViewportChanges(
      {inner_viewport_scroll_delta, commit_data.elastic_overscroll_delta,
       commit_data.page_scale_delta, commit_data.is_pinch_gesture_active,
       commit_data.top_controls_delta, commit_data.bottom_controls_delta,
       commit_data.browser_controls_constraint,
       commit_data.scroll_end_data.scroll_gesture_did_end});
  SetNeedsUpdateLayers();
}

void LayerTreeHost::UpdateScrollOffsetFromImpl(
    const ElementId& id,
    const gfx::Vector2dF& delta,
    const std::optional<TargetSnapAreaElementIds>& snap_target_ids) {
  if (IsUsingLayerLists()) {
    auto& scroll_tree = property_trees()->scroll_tree_mutable();
    auto new_offset = scroll_tree.current_scroll_offset(id) + delta;
    TRACE_EVENT_INSTANT2("cc", "NotifyDidScroll", TRACE_EVENT_SCOPE_THREAD,
                         "cur_y", scroll_tree.current_scroll_offset(id).y(),
                         "delta", delta.y());
    if (auto* scroll_node = scroll_tree.FindNodeFromElementId(id)) {
      // This update closely follows
      // blink::PropertyTreeManager::DirectlyUpdateScrollOffsetTransform.

      scroll_tree.SetScrollOffset(id, new_offset);
      // |blink::PropertyTreeManager::DirectlySetScrollOffset| (called from
      // |blink::PropertyTreeManager::DirectlyUpdateScrollOffsetTransform|)
      // marks the layer as needing to push properties in order to clobber
      // animations, but that is not needed for an impl-side scroll.

      // Update the offset in the transform node.
      TransformTree& transform_tree =
          property_trees()->transform_tree_mutable();
      auto* transform_node = transform_tree.Node(scroll_node->transform_id);
      if (transform_node && transform_node->scroll_offset != new_offset) {
        transform_node->scroll_offset = new_offset;
        transform_node->needs_local_transform_update = true;
        transform_node->transform_changed = true;
        transform_tree.set_needs_update(true);

        // If the scroll was realized on the compositor, then its transform node
        // is already updated (see LayerTreeImpl::DidUpdateScrollOffset) and we
        // are now "catching up" to it on main, so we don't need a commit.
        //
        // But if the scroll should be realized on the main thread, we need a
        // commit to push the transform change.
        if (scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node)) {
          SetNeedsCommit();
        }
      }

      // The transform tree has been modified which requires a call to
      // |LayerTreeHost::UpdateLayers| to update the property trees.
      SetNeedsUpdateLayers();
    }

    scroll_tree.NotifyDidCompositorScroll(id, new_offset, snap_target_ids);
  } else if (Layer* layer = LayerByElementId(id)) {
    layer->SetScrollOffsetFromImplSide(layer->scroll_offset() + delta);
    SetNeedsUpdateLayers();
  }
}

void LayerTreeHost::ApplyCompositorChanges(CompositorCommitData* commit_data) {
  DCHECK(IsMainThread());
  DCHECK(commit_data);
  TRACE_EVENT0("cc", "LayerTreeHost::ApplyCompositorChanges");

  DCHECK(!in_apply_compositor_changes_);
  base::AutoReset<bool> in_apply_changes(&in_apply_compositor_changes_, true);

  using perfetto::protos::pbzero::TrackEvent;

  for (auto& swap_promise : commit_data->swap_promises) {
    int64_t trace_id = swap_promise->GetTraceId();
    TRACE_EVENT("input,benchmark", "LatencyInfo.Flow",
                [&](perfetto::EventContext ctx) {
                  ui::LatencyInfo::FillTraceEvent(
                      ctx, trace_id,
                      perfetto::protos::pbzero::ChromeLatencyInfo2::Step::
                          STEP_MAIN_THREAD_SCROLL_UPDATE);
                });
    swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
  }

  if (has_root_layer()) {
    for (auto& scroll : commit_data->scrolls) {
      UpdateScrollOffsetFromImpl(scroll.element_id, scroll.scroll_delta,
                                 scroll.snap_target_element_ids);
    }
    // const_cast to ensure the compiler chooses to the const version of
    // property_trees(), to avoid blocking on commit.
    const auto* pt = const_cast<const LayerTreeHost*>(this)->property_trees();
    for (auto& scrollbar : commit_data->scrollbars) {
      pt->scroll_tree().NotifyDidChangeScrollbarsHidden(scrollbar.element_id,
                                                        scrollbar.hidden);
    }
  }

  client_->UpdateCompositorScrollState(*commit_data);

  // This needs to happen after scroll deltas have been sent to prevent top
  // controls from clamping the layout viewport both on the compositor and
  // on the main thread.
  ApplyViewportChanges(*commit_data);
}

void LayerTreeHost::ApplyMutatorEvents(std::unique_ptr<MutatorEvents> events) {
  DCHECK(IsMainThread());
  if (!events->IsEmpty())
    mutator_host()->SetAnimationEvents(std::move(events));
}

void LayerTreeHost::RecordStartOfFrameMetrics() {
  DCHECK(IsMainThread());
  client_->RecordStartOfFrameMetrics();
}

void LayerTreeHost::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    ActiveFrameSequenceTrackers trackers) {
  DCHECK(IsMainThread());
  client_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

void LayerTreeHost::NotifyThroughputTrackerResults(
    CustomTrackerResults results) {
  DCHECK(IsMainThread());
  client_->NotifyThroughputTrackerResults(std::move(results));
}

const base::WeakPtr<CompositorDelegateForInput>&
LayerTreeHost::GetDelegateForInput() const {
  DCHECK(IsMainThread());
  return compositor_delegate_weak_ptr_;
}

void LayerTreeHost::DetachInputDelegateAndRenderFrameObserver() {
  DCHECK(IsMainThread());
  proxy_->DetachInputDelegateAndRenderFrameObserver();
}

void LayerTreeHost::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info) {
  DCHECK(IsMainThread());
  // Browser controls are only used in threaded mode but Blink layout tests may
  // call into this. The single threaded version is a no-op.
  proxy_->UpdateBrowserControlsState(constraints, current, animate,
                                     offset_tags_info);
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  std::unique_ptr<MutatorEvents> events = mutator_host()->CreateEvents();

  if (mutator_host()->TickAnimations(monotonic_time,
                                     property_trees()->scroll_tree(), true))
    mutator_host()->UpdateAnimationState(true, events.get());

  if (!events->IsEmpty()) {
    // If not using layer lists, animation state changes will require
    // rebuilding property trees to track them.
    if (!IsUsingLayerLists())
      property_trees()->set_needs_rebuild(true);

    // A commit is required to push animation changes to the compositor.
    SetNeedsCommit();
  }
}

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    base::Value::Dict settings,
    MicroBenchmark::DoneCallback callback) {
  DCHECK(IsMainThread());
  return micro_benchmark_controller_.ScheduleRun(
      benchmark_name, std::move(settings), std::move(callback));
}

bool LayerTreeHost::SendMessageToMicroBenchmark(int id,
                                                base::Value::Dict message) {
  DCHECK(IsMainThread());
  return micro_benchmark_controller_.SendMessage(id, std::move(message));
}

void LayerTreeHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  DCHECK(IsMainThread());
  // The animation worklet system assumes that the mutator will never be called
  // from the main thread, which will not be the case if we're running in
  // single-threaded mode.
  if (!task_runner_provider_->HasImplThread()) {
    DLOG(ERROR) << "LayerTreeMutator not supported in single-thread mode";
    return;
  }
  proxy_->SetMutator(std::move(mutator));
}

void LayerTreeHost::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  DCHECK(IsMainThread());
  // The paint worklet system assumes that the painter will never be called from
  // the main thread, which will not be the case if we're running in
  // single-threaded mode.
  DCHECK(task_runner_provider_->HasImplThread())
      << "PaintWorkletLayerPainter not supported in single-thread mode";
  proxy_->SetPaintWorkletLayerPainter(std::move(painter));
}

bool LayerTreeHost::IsSingleThreaded() const {
  DCHECK(IsMainThread());
  DCHECK(compositor_mode_ != CompositorMode::SINGLE_THREADED ||
         !task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::SINGLE_THREADED;
}

bool LayerTreeHost::IsThreaded() const {
  DCHECK(IsMainThread());
  DCHECK(compositor_mode_ != CompositorMode::THREADED ||
         task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::THREADED;
}

void LayerTreeHost::RequestPresentationTimeForNextFrame(
    PresentationTimeCallbackBuffer::Callback callback) {
  pending_commit_state()->pending_presentation_callbacks.push_back(
      std::move(callback));
}

void LayerTreeHost::RequestSuccessfulPresentationTimeForNextFrame(
    PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails callback) {
  pending_commit_state()->pending_successful_presentation_callbacks.push_back(
      std::move(callback));
}

void LayerTreeHost::RequestScrollAnimationEndNotification(
    base::OnceClosure callback) {
  DCHECK(IsMainThread());
  DCHECK(scroll_animation_.end_notification.is_null());
  if (scroll_animation_.in_progress)
    scroll_animation_.end_notification = std::move(callback);
  else
    std::move(callback).Run();
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> new_root_layer) {
  if (root_layer() == new_root_layer.get())
    return;

  if (root_layer()) {
    WaitForProtectedSequenceCompletion();
    root_layer()->SetLayerTreeHost(nullptr);
  }
  thread_unsafe_commit_state().root_layer = new_root_layer;
  if (root_layer()) {
    DCHECK(!root_layer()->parent());
    root_layer()->SetLayerTreeHost(this);
  }

  if (hud_layer()) {
    WaitForProtectedSequenceCompletion();
    hud_layer()->RemoveFromParent();
  }

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  pending_commit_state()->needs_gpu_rasterization_histogram = IsThreaded();

  SetNeedsFullTreeSync();
}

void LayerTreeHost::RegisterViewportPropertyIds(
    const ViewportPropertyIds& ids) {
  DCHECK(IsUsingLayerLists());
  pending_commit_state()->viewport_property_ids = ids;
  // Outer viewport properties exist only if inner viewport property exists.
  DCHECK(ids.inner_scroll != kInvalidPropertyNodeId ||
         (ids.outer_scroll == kInvalidPropertyNodeId &&
          ids.outer_clip == kInvalidPropertyNodeId));
}

Layer* LayerTreeHost::InnerViewportScrollLayerForTesting() {
  auto* scroll_node = property_trees()->scroll_tree_mutable().Node(
      pending_commit_state()->viewport_property_ids.inner_scroll);
  return scroll_node ? LayerByElementId(scroll_node->element_id) : nullptr;
}

Layer* LayerTreeHost::OuterViewportScrollLayerForTesting() {
  return LayerByElementId(OuterViewportScrollElementId());
}

ElementId LayerTreeHost::OuterViewportScrollElementId() const {
  const auto* scroll_node = property_trees()->scroll_tree().Node(
      pending_commit_state()->viewport_property_ids.outer_scroll);
  return scroll_node ? scroll_node->element_id : ElementId();
}

void LayerTreeHost::RegisterSelection(const LayerSelection& selection) {
  if (pending_commit_state()->selection == selection)
    return;

  pending_commit_state()->selection = selection;
  SetNeedsCommit();
}

void LayerTreeHost::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (pending_commit_state()->have_scroll_event_handlers == have_event_handlers)
    return;

  pending_commit_state()->have_scroll_event_handlers = have_event_handlers;
  SetNeedsCommit();
}

void LayerTreeHost::SetEventListenerProperties(
    EventListenerClass event_class,
    EventListenerProperties properties) {
  if (event_listener_properties(event_class) == properties)
    return;
  const size_t index = static_cast<size_t>(event_class);
  pending_commit_state()->event_listener_properties[index] = properties;
  SetNeedsCommit();
}

void LayerTreeHost::SetViewportRectAndScale(
    const gfx::Rect& device_viewport_rect,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id_from_parent) {
  const viz::LocalSurfaceId previous_local_surface_id =
      pending_commit_state()->local_surface_id_from_parent;
  SetLocalSurfaceIdFromParent(local_surface_id_from_parent);

  TRACE_EVENT_NESTABLE_ASYNC_END1("cc", "LayerTreeHostSize",
                                  TRACE_ID_LOCAL(this), "id", id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("cc", "LayerTreeHostSize",
                                    TRACE_ID_LOCAL(this), "size",
                                    device_viewport_rect.ToString(), "lsid",
                                    local_surface_id_from_parent.ToString());

  bool device_viewport_rect_changed = false;
  if (pending_commit_state()->device_viewport_rect != device_viewport_rect) {
    pending_commit_state()->device_viewport_rect = device_viewport_rect;
    device_viewport_rect_changed = true;
  }
  bool painted_device_scale_factor_changed = false;
  bool device_scale_factor_changed = false;
  if (settings_.use_painted_device_scale_factor) {
    DCHECK_EQ(pending_commit_state()->device_scale_factor, 1.f);
    if (pending_commit_state()->painted_device_scale_factor !=
        device_scale_factor) {
      pending_commit_state()->painted_device_scale_factor = device_scale_factor;
      painted_device_scale_factor_changed = true;
    }
  } else {
    DCHECK_EQ(pending_commit_state()->painted_device_scale_factor, 1.f);
    if (pending_commit_state()->device_scale_factor != device_scale_factor) {
      pending_commit_state()->device_scale_factor = device_scale_factor;
      device_scale_factor_changed = true;
    }
  }

  // If a new viz::LocalSurfaceId has been provided, and the viewport has
  // changed, we need not begin new frames until it has activated.
  if (previous_local_surface_id != local_surface_id_from_parent &&
      device_viewport_rect_changed) {
    SetTargetLocalSurfaceId(local_surface_id_from_parent);
  }

  if (device_viewport_rect_changed || painted_device_scale_factor_changed ||
      device_scale_factor_changed) {
    SetPropertyTreesNeedRebuild();
    SetNeedsCommit();
  }
}

void LayerTreeHost::SetVisualDeviceViewportIntersectionRect(
    const gfx::Rect& intersection_rect) {
  DCHECK(IsMainThread());
  if (intersection_rect == visual_device_viewport_intersection_rect_)
    return;

  visual_device_viewport_intersection_rect_ = intersection_rect;
}

void LayerTreeHost::SetVisualDeviceViewportSize(
    const gfx::Size& visual_device_viewport_size) {
  if (visual_device_viewport_size ==
      pending_commit_state()->visual_device_viewport_size)
    return;

  pending_commit_state()->visual_device_viewport_size =
      visual_device_viewport_size;
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsParams(
    const BrowserControlsParams& params) {
  if (pending_commit_state()->browser_controls_params == params)
    return;

  pending_commit_state()->browser_controls_params = params;
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsShownRatio(float top_ratio,
                                                 float bottom_ratio) {
  if (pending_commit_state()->top_controls_shown_ratio == top_ratio &&
      pending_commit_state()->bottom_controls_shown_ratio == bottom_ratio)
    return;

  pending_commit_state()->top_controls_shown_ratio = top_ratio;
  pending_commit_state()->bottom_controls_shown_ratio = bottom_ratio;
  SetNeedsCommit();
}

void LayerTreeHost::SetOverscrollBehavior(const OverscrollBehavior& behavior) {
  if (pending_commit_state()->overscroll_behavior == behavior)
    return;
  pending_commit_state()->overscroll_behavior = behavior;
  SetNeedsCommit();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (pending_commit_state()->page_scale_factor == page_scale_factor &&
      pending_commit_state()->min_page_scale_factor == min_page_scale_factor &&
      pending_commit_state()->max_page_scale_factor == max_page_scale_factor)
    return;
  DCHECK_GE(page_scale_factor, min_page_scale_factor);
  DCHECK_LE(page_scale_factor, max_page_scale_factor);
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this dcheck as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(settings_.is_for_scalable_page ||
         page_scale_factor == pending_commit_state()->page_scale_factor)
      << "Setting PSF in oopif subframe: old psf = "
      << pending_commit_state()->page_scale_factor
      << ", new psf = " << page_scale_factor;

  pending_commit_state()->page_scale_factor = page_scale_factor;
  pending_commit_state()->min_page_scale_factor = min_page_scale_factor;
  pending_commit_state()->max_page_scale_factor = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::StartPageScaleAnimation(const gfx::Point& target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_commit_state()->pending_page_scale_animation =
      std::make_unique<PendingPageScaleAnimation>(target_offset, use_anchor,
                                                  scale, duration);

  SetNeedsCommit();
}

bool LayerTreeHost::HasPendingPageScaleAnimation() const {
  return !!pending_commit_state()->pending_page_scale_animation.get();
}

void LayerTreeHost::SetRecordingScaleFactor(float recording_scale_factor) {
  DCHECK(IsMainThread());
  if (recording_scale_factor_ == recording_scale_factor)
    return;
  recording_scale_factor_ = recording_scale_factor;
}

void LayerTreeHost::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  if (pending_commit_state()->display_color_spaces == display_color_spaces)
    return;
  bool only_hdr_changed = gfx::DisplayColorSpaces::EqualExceptForHdrHeadroom(
      pending_commit_state()->display_color_spaces, display_color_spaces);
  pending_commit_state()->display_color_spaces = display_color_spaces;

  for (auto* layer : *this) {
    if (!only_hdr_changed ||
        layer->RequiresSetNeedsDisplayOnHdrHeadroomChange()) {
      layer->SetNeedsDisplay();
    }
  }
}

void LayerTreeHost::UpdateViewportIsMobileOptimized(
    bool is_viewport_mobile_optimized) {
  if (pending_commit_state()->is_viewport_mobile_optimized ==
      is_viewport_mobile_optimized)
    return;
  pending_commit_state()->is_viewport_mobile_optimized =
      is_viewport_mobile_optimized;
  SetNeedsCommit();
}

void LayerTreeHost::SetPrefersReducedMotion(bool prefers_reduced_motion) {
  if (pending_commit_state()->prefers_reduced_motion == prefers_reduced_motion)
    return;
  pending_commit_state()->prefers_reduced_motion = prefers_reduced_motion;
  SetNeedsCommit();
}

void LayerTreeHost::SetMayThrottleIfUndrawnFrames(
    bool may_throttle_if_undrawn_frames) {
  if (pending_commit_state()->may_throttle_if_undrawn_frames ==
      may_throttle_if_undrawn_frames)
    return;
  pending_commit_state()->may_throttle_if_undrawn_frames =
      may_throttle_if_undrawn_frames;
  SetNeedsCommit();
}

bool LayerTreeHost::GetMayThrottleIfUndrawnFramesForTesting() const {
  return pending_commit_state()->may_throttle_if_undrawn_frames;
}

void LayerTreeHost::SetExternalPageScaleFactor(
    float page_scale_factor,
    bool is_external_pinch_gesture_active) {
  if (pending_commit_state()->external_page_scale_factor == page_scale_factor &&
      pending_commit_state()->is_external_pinch_gesture_active ==
          is_external_pinch_gesture_active) {
    return;
  }

  pending_commit_state()->external_page_scale_factor = page_scale_factor;
  pending_commit_state()->is_external_pinch_gesture_active =
      is_external_pinch_gesture_active;
  SetNeedsCommit();
}

void LayerTreeHost::SetLocalSurfaceIdFromParent(
    const viz::LocalSurfaceId& local_surface_id_from_parent) {
  // If the viz::LocalSurfaceId is invalid we will be hitting the early exit.
  // However creating a hash for tracing of an invalid id throw an error. So
  // exit here. This is either from unit testing where no ids are being setup,
  // or from a disconnect in Renderer and Browser startup, in which Renderers
  // have a partial state before the Browser sends the full visual properties.
  //
  // TODO(jonross): Untangle startup so that we don't have this invalid partial
  // state. (https://crbug.com/1185286) (https://crbug.com/419087)
  if (pending_commit_state()->local_surface_id_from_parent ==
      local_surface_id_from_parent)
    return;
  const viz::LocalSurfaceId current_local_surface_id_from_parent =
      pending_commit_state()->local_surface_id_from_parent;

  // These traces are split into two due to the usage of TRACE_ID_GLOBAL for the
  // incoming flow (it comes from a different process), and TRACE_ID_LOCAL for
  // the outgoing flow. The outgoing flow uses local to ensure that it doesn't
  // flow into the wrong trace in different process.
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(local_surface_id_from_parent.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN, "step", "SetLocalSurfaceIdFromParent",
      "local_surface_id", local_surface_id_from_parent.ToString());
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_LOCAL(local_surface_id_from_parent.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step", "SetLocalSurfaceIdFromParent",
      "local_surface_id", local_surface_id_from_parent.ToString());
  // Always update the cached state of the viz::LocalSurfaceId to reflect the
  // latest value received from our parent.
  pending_commit_state()->local_surface_id_from_parent =
      local_surface_id_from_parent;

  // If the parent sequence number has not advanced, then there is no need to
  // commit anything. This can occur when the child sequence number has
  // advanced. Which means that child has changed visual properites, and the
  // parent agreed upon these without needing to further advance its sequence
  // number. When this occurs the child is already up-to-date and a commit here
  // is simply redundant.
  if (AreEmbedTokensEqual(current_local_surface_id_from_parent,
                          local_surface_id_from_parent) &&
      AreParentSequencesEqual(current_local_surface_id_from_parent,
                              local_surface_id_from_parent)) {
    return;
  }
  UpdateDeferMainFrameUpdateInternal();
  SetNeedsCommit();
}

void LayerTreeHost::RequestViewportScreenshot(
    const base::UnguessableToken& token) {
  CHECK(pending_commit_state()->new_local_surface_id_request)
      << "Must have requested a new LocalSurfaceID before making "
         "this request";
  pending_commit_state()->screenshot_destination_token = token;
  SetNeedsCommit();
}

void LayerTreeHost::SetPrimaryMainFrameItemSequenceNumber(
    int64_t primary_main_frame_item_sequence_number) {
  if (pending_commit_state()->primary_main_frame_item_sequence_number ==
      primary_main_frame_item_sequence_number) {
    return;
  }
  pending_commit_state()->primary_main_frame_item_sequence_number =
      primary_main_frame_item_sequence_number;
  SetNeedsCommit();
}

void LayerTreeHost::RequestNewLocalSurfaceId() {
  // We can still request a new viz::LocalSurfaceId but that request will be
  // deferred until we have a valid viz::LocalSurfaceId from the parent.
  if (pending_commit_state()->new_local_surface_id_request)
    return;
  pending_commit_state()->new_local_surface_id_request = true;
  SetNeedsCommit();
}

void LayerTreeHost::RegisterLayer(Layer* layer) {
  DCHECK(IsMainThread());
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeHost::UnregisterLayer(Layer* layer) {
  DCHECK(IsMainThread());
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  pending_commit_state()->layers_that_should_push_properties.erase(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTreeHost::LayerById(int id) {
  DCHECK(IsMainThread());
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

bool LayerTreeHost::PaintContent(const LayerList& update_layer_list) {
  DCHECK(IsMainThread());
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
  }
  return did_paint_content;
}

void LayerTreeHost::AddSurfaceRange(const viz::SurfaceRange& surface_range) {
  if (++pending_commit_state()->surface_ranges[surface_range] == 1) {
    pending_commit_state()->needs_surface_ranges_sync = true;
    SetNeedsCommit();
  }
}

void LayerTreeHost::RemoveSurfaceRange(const viz::SurfaceRange& surface_range) {
  auto iter = pending_commit_state()->surface_ranges.find(surface_range);
  if (iter == pending_commit_state()->surface_ranges.end())
    return;

  if (--iter->second <= 0) {
    pending_commit_state()->surface_ranges.erase(iter);
    pending_commit_state()->needs_surface_ranges_sync = true;
    SetNeedsCommit();
  }
}

void LayerTreeHost::AddLayerShouldPushProperties(Layer* layer) {
  pending_commit_state()->layers_that_should_push_properties.insert(layer);
}

void LayerTreeHost::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(syncing_deltas_for_test_ || CommitRequested());
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this check as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(settings_.is_for_scalable_page ||
         page_scale == pending_commit_state()->page_scale_factor)
      << "Setting PSF in oopif subframe: old psf = "
      << pending_commit_state()->page_scale_factor
      << ", new psf = " << page_scale;
  bool changed = (page_scale != pending_commit_state()->page_scale_factor);
  pending_commit_state()->page_scale_factor = page_scale;
  if (changed)
    SetPropertyTreesNeedRebuild();
}

void LayerTreeHost::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(syncing_deltas_for_test_ || CommitRequested());
  pending_commit_state()->elastic_overscroll = elastic_overscroll;
}

void LayerTreeHost::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer()) {
      hud_layer_ = HeadsUpDisplayLayer::Create();
      pending_commit_state()->hud_layer_id = hud_layer()->id();
    }
    if (root_layer() && !hud_layer()->parent())
      root_layer()->AddChild(hud_layer());
    hud_layer()->UpdateLocationAndSize(
        pending_commit_state()->device_viewport_rect.size(),
        pending_commit_state()->device_scale_factor);
  } else if (hud_layer()) {
    hud_layer()->RemoveFromParent();
    hud_layer_ = nullptr;
    pending_commit_state()->hud_layer_id = Layer::INVALID_ID;
  }
}

bool LayerTreeHost::is_hud_layer(const Layer* layer) const {
  return hud_layer() == layer;
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  pending_commit_state()->needs_full_tree_sync = true;
  property_trees()->set_needs_rebuild(true);
  SetNeedsCommit();
}

void LayerTreeHost::ResetNeedsFullTreeSyncForTesting() {
  pending_commit_state()->needs_full_tree_sync = false;
}

void LayerTreeHost::SetPropertyTreesNeedRebuild() {
  property_trees()->set_needs_rebuild(true);
  SetNeedsUpdateLayers();
}

Layer* LayerTreeHost::LayerByElementId(ElementId element_id) {
  DCHECK(IsMainThread());
  auto iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

const Layer* LayerTreeHost::LayerByElementId(ElementId element_id) const {
  DCHECK(IsMainThread());
  auto iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTreeHost::RegisterElement(ElementId element_id,
                                    Layer* layer) {
  DCHECK(IsMainThread());
  element_layers_map_[element_id] = layer;
}

void LayerTreeHost::UnregisterElement(ElementId element_id) {
  DCHECK(IsMainThread());
  if (!IsUsingLayerLists()) {
    mutator_host()->RemoveElementId(element_id);
  }
  element_layers_map_.erase(element_id);
}

void LayerTreeHost::SetElementIdsForTesting() {
  for (auto* layer : *this)
    layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeHost::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::BuildPropertyTrees(this);
}

bool LayerTreeHost::IsElementInPropertyTrees(ElementId element_id,
                                             ElementListType list_type) const {
  DCHECK(IsMainThread());
  if (IsUsingLayerLists()) {
    return list_type == ElementListType::ACTIVE &&
           property_trees()->HasElement(element_id);
  }
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTreeHost::SetMutatorsNeedCommit() {
  SetNeedsCommit();
}

void LayerTreeHost::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees()->set_needs_rebuild(true);
}

void LayerTreeHost::SetElementFilterMutated(ElementId element_id,
                                            ElementListType list_type,
                                            const FilterOperations& filters) {
  if (list_type != ElementListType::ACTIVE)
    return;

  if (IsUsingLayerLists()) {
    // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
    // tree nodes and can set the filter directly on the effect node.
    property_trees()->effect_tree_mutable().OnFilterAnimated(element_id,
                                                             filters);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTreeHost::SetElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  if (list_type != ElementListType::ACTIVE)
    return;

  if (IsUsingLayerLists()) {
    // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
    // tree nodes and can set the backdrop_filter directly on the effect node.
    property_trees()->effect_tree_mutable().OnBackdropFilterAnimated(
        element_id, backdrop_filters);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnBackdropFilterAnimated(backdrop_filters);
}

void LayerTreeHost::SetElementOpacityMutated(ElementId element_id,
                                             ElementListType list_type,
                                             float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (list_type != ElementListType::ACTIVE)
    return;

  if (IsUsingLayerLists()) {
    property_trees()->effect_tree_mutable().OnOpacityAnimated(element_id,
                                                              opacity);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);

  if (EffectNode* node = property_trees()->effect_tree_mutable().Node(
          layer->effect_tree_index())) {
    DCHECK_EQ(layer->effect_tree_index(), node->id);
    if (node->opacity == opacity)
      return;

    node->opacity = opacity;
    property_trees()->effect_tree_mutable().set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (list_type != ElementListType::ACTIVE)
    return;

  if (IsUsingLayerLists()) {
    property_trees()->transform_tree_mutable().OnTransformAnimated(element_id,
                                                                   transform);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);

  if (layer->has_transform_node()) {
    TransformNode* node = property_trees()->transform_tree_mutable().Node(
        layer->transform_tree_index());
    if (node->local == transform)
      return;

    node->local = transform;
    node->needs_local_transform_update = true;
    node->has_potential_animation = true;
    property_trees()->transform_tree_mutable().set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::PointF& scroll_offset) {
  DCHECK(IsMainThread());
  // Do nothing. Scroll deltas will be sent from the compositor thread back
  // to the main thread in the same manner as during non-animated
  // compositor-driven scrolling.
}

void LayerTreeHost::ElementIsAnimatingChanged(
    const PropertyToElementIdMap& element_id_map,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  if (list_type == ElementListType::PENDING)
    return;
  property_trees()->ElementIsAnimatingChanged(element_id_map, mask, state,
                                              true);
}

void LayerTreeHost::MaximumScaleChanged(ElementId element_id,
                                        ElementListType list_type,
                                        float maximum_scale) {
  if (list_type == ElementListType::PENDING)
    return;
  property_trees()->MaximumAnimationScaleChanged(element_id, maximum_scale);
}

bool LayerTreeHost::RunsOnCurrentThread() const {
  return !task_runner_provider_ || task_runner_provider_->IsMainThread();
}

void LayerTreeHost::QueueImageDecode(const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("cc", "LayerTreeHost::QueueImageDecode");
  int next_id = s_image_decode_sequence_number.GetNext();
  if (base::FeatureList::IsEnabled(
          features::kSendExplicitDecodeRequestsImmediately)) {
    proxy()->QueueImageDecode(next_id, image);
  } else {
    pending_commit_state()->queued_image_decodes.emplace_back(
        next_id, std::make_unique<PaintImage>(image));
  }
  pending_image_decodes_.emplace(next_id, std::move(callback));
  SetNeedsCommit();
}

LayerListIterator LayerTreeHost::begin() {
  return LayerListIterator(root_layer());
}

LayerListConstIterator LayerTreeHost::begin() const {
  DCHECK(IsMainThread());
  return LayerListConstIterator(root_layer());
}

LayerListIterator LayerTreeHost::end() {
  DCHECK(IsMainThread());
  return LayerListIterator(nullptr);
}

LayerListConstIterator LayerTreeHost::end() const {
  DCHECK(IsMainThread());
  return LayerListConstIterator(nullptr);
}

LayerListReverseIterator LayerTreeHost::rbegin() {
  DCHECK(IsMainThread());
  return LayerListReverseIterator(root_layer());
}

LayerListReverseConstIterator LayerTreeHost::rbegin() const {
  DCHECK(IsMainThread());
  return LayerListReverseConstIterator(root_layer());
}

LayerListReverseIterator LayerTreeHost::rend() {
  return LayerListReverseIterator(nullptr);
}

LayerListReverseConstIterator LayerTreeHost::rend() const {
  return LayerListReverseConstIterator(nullptr);
}

void LayerTreeHost::SetPropertyTreesForTesting(
    const PropertyTrees* property_trees) {
  thread_unsafe_commit_state().property_trees = *property_trees;
}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

void LayerTreeHost::SetHasCopyRequest(bool has_copy_request) {
  DCHECK(IsMainThread());
  has_copy_request_ = has_copy_request;
}

void LayerTreeHost::RequestBeginMainFrameNotExpected(bool new_state) {
  DCHECK(IsMainThread());
  proxy_->RequestBeginMainFrameNotExpected(new_state);
}

void LayerTreeHost::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  DCHECK(IsMainThread());
  // Clears image caches and resets the scheduling history for the content
  // produced by this host so far.
  pending_commit_state()->clear_caches_on_next_commit = true;
  proxy_->SetSourceURL(source_id, url);
  // If this is not used as a common web page, don't show HUD.
  if (!url.SchemeIsHTTPOrHTTPS())
    pending_commit_state()->debug_state.TurnOffHudInfoDisplay();
}

base::ReadOnlySharedMemoryRegion
LayerTreeHost::CreateSharedMemoryForSmoothnessUkm() {
  DCHECK(IsMainThread());
  const auto size = sizeof(UkmSmoothnessDataShared);
  auto ukm_smoothness_mapping = base::ReadOnlySharedMemoryRegion::Create(size);
  if (!ukm_smoothness_mapping.IsValid())
    return {};
  proxy_->SetUkmSmoothnessDestination(
      std::move(ukm_smoothness_mapping.mapping));
  return std::move(ukm_smoothness_mapping.region);
}

void LayerTreeHost::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  DCHECK(IsMainThread());
  proxy_->SetRenderFrameObserver(std::move(observer));
}

void LayerTreeHost::SetDelegatedInkMetadata(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  pending_commit_state()->delegated_ink_metadata = std::move(metadata);
  SetNeedsCommit();
}

std::vector<base::OnceClosure>
LayerTreeHost::TakeViewTransitionCallbacksForTesting() {
  DCHECK(IsMainThread());
  std::vector<base::OnceClosure> result;
  for (auto& item : view_transition_callbacks_)
    result.push_back(std::move(item.second));
  view_transition_callbacks_.clear();
  return result;
}

double LayerTreeHost::GetPercentDroppedFrames() const {
  DCHECK(IsMainThread());
  return proxy_->GetPercentDroppedFrames();
}

void LayerTreeHost::DropActiveScrollDeltaNextCommit(ElementId scroll_element) {
  pending_commit_state()->scrollers_clobbering_active_value.insert(
      scroll_element);
  SetNeedsCommit();
}

}  // namespace cc
