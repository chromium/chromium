// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "cc/trees/ukm_manager.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/presentation_feedback.h"

namespace {
static base::AtomicSequenceNumber s_layer_tree_host_sequence_number;
static base::AtomicSequenceNumber s_image_decode_sequence_number;
}

namespace cc {

LayerTreeHost::InitParams::InitParams() = default;
LayerTreeHost::InitParams::~InitParams() = default;
LayerTreeHost::InitParams::InitParams(InitParams&&) = default;
LayerTreeHost::InitParams& LayerTreeHost::InitParams::operator=(InitParams&&) =
    default;

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
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      settings_(*params.settings),
      debug_state_(settings_.initial_debug_state),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      task_graph_runner_(params.task_graph_runner),
      content_source_id_(0),
      event_listener_properties_(),
      mutator_host_(params.mutator_host),
      defer_commits_weak_ptr_factory_(this) {
  DCHECK(task_graph_runner_);
  DCHECK(!settings_.enable_checker_imaging || image_worker_task_runner_);

  mutator_host_->SetMutatorHostClient(this);

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
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
}

void LayerTreeHost::SetUIResourceManagerForTesting(
    std::unique_ptr<UIResourceManager> ui_resource_manager) {
  ui_resource_manager_ = std::move(ui_resource_manager);
}

void LayerTreeHost::InitializeProxy(std::unique_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");
  DCHECK(task_runner_provider_);

  proxy_ = std::move(proxy);
  proxy_->Start();

  UpdateDeferCommitsInternal();

  mutator_host_->SetSupportsScrollAnimations(proxy_->SupportsImplScrolling());
}

LayerTreeHost::~LayerTreeHost() {
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/654672
  DCHECK(!inside_main_frame_);
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  // Clear any references into the LayerTreeHost.
  mutator_host_->SetMutatorHostClient(nullptr);

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(ViewportLayers());

  if (root_layer_) {
    root_layer_->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    root_layer_ = nullptr;
  }

  // Fail any pending image decodes.
  for (auto& pair : pending_image_decodes_)
    std::move(pair.second).Run(false);

  if (proxy_) {
    DCHECK(task_runner_provider_->IsMainThread());
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }
}

int LayerTreeHost::GetId() const {
  return id_;
}

int LayerTreeHost::SourceFrameNumber() const {
  return source_frame_number_;
}

UIResourceManager* LayerTreeHost::GetUIResourceManager() const {
  return ui_resource_manager_.get();
}

TaskRunnerProvider* LayerTreeHost::GetTaskRunnerProvider() const {
  return task_runner_provider_.get();
}

SwapPromiseManager* LayerTreeHost::GetSwapPromiseManager() {
  return &swap_promise_manager_;
}

const LayerTreeSettings& LayerTreeHost::GetSettings() const {
  return settings_;
}

void LayerTreeHost::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));

  // Request a main frame if one is not already in progress. This might either
  // A) request a commit ahead of time or B) request a commit which is not
  // needed because there are not pending updates. If B) then the frame will
  // be aborted early and the swap promises will be broken (see
  // EarlyOut_NoUpdates).
  if (!inside_main_frame_)
    SetNeedsAnimate();
}

void LayerTreeHost::WillBeginMainFrame() {
  inside_main_frame_ = true;
  devtools_instrumentation::WillBeginMainThreadFrame(GetId(),
                                                     SourceFrameNumber());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  inside_main_frame_ = false;
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  client_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeHost::BeginMainFrame(const viz::BeginFrameArgs& args) {
  client_->BeginMainFrame(args);
}

const LayerTreeDebugState& LayerTreeHost::GetDebugState() const {
  return debug_state_;
}

void LayerTreeHost::RequestMainFrameUpdate() {
  client_->UpdateLayerTreeHost();
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(
    LayerTreeHostImpl* host_impl) {
  DCHECK(task_runner_provider_->IsImplThread());

  TRACE_EVENT0("cc,benchmark", "LayerTreeHost::FinishCommitOnImplThread");

  LayerTreeImpl* sync_tree = host_impl->sync_tree();
  sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }
  if (next_commit_forces_recalculate_raster_scales_) {
    sync_tree->ForceRecalculateRasterScales();
    next_commit_forces_recalculate_raster_scales_ = false;
  }

  sync_tree->set_source_frame_number(SourceFrameNumber());
  bool request_presentation_time =
      settings_.always_request_presentation_time ||
      !pending_presentation_time_callbacks_.empty();
  if (request_presentation_time && pending_presentation_time_callbacks_.empty())
    pending_presentation_time_callbacks_.push_back(base::DoNothing());
  sync_tree->AddPresentationCallbacks(
      std::move(pending_presentation_time_callbacks_));

  if (needs_full_tree_sync_)
    TreeSynchronizer::SynchronizeTrees(root_layer(), sync_tree);

  if (clear_caches_on_next_commit_) {
    clear_caches_on_next_commit_ = false;
    proxy_->ClearHistory();
    host_impl->ClearCaches();
  }

  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");

    PushPropertyTreesTo(sync_tree);
    sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kSyncedPropertyTrees);

    PushSurfaceRangesTo(sync_tree);
    TreeSynchronizer::PushLayerProperties(this, sync_tree);
    sync_tree->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedLayerProperties);

    PushLayerTreePropertiesTo(sync_tree);
    PushLayerTreeHostPropertiesTo(host_impl);

    sync_tree->PassSwapPromises(swap_promise_manager_.TakeSwapPromises());

    sync_tree->set_ui_resource_request_queue(
        ui_resource_manager_->TakeUIResourcesRequests());

    // This must happen after synchronizing property trees and after pushing
    // properties, which updates the clobber_active_value flag.
    // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
    sync_tree->property_trees()->scroll_tree.PushScrollUpdatesFromMainThread(
        property_trees(), sync_tree);

    // This must happen after synchronizing property trees and after push
    // properties, which updates property tree indices, but before animation
    // host pushes properties as animation host push properties can change
    // KeyframeModel::InEffect and we want the old InEffect value for updating
    // property tree scrolling and animation.
    // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
    sync_tree->UpdatePropertyTreeAnimationFromMainThread();

    TRACE_EVENT0("cc", "LayerTreeHost::AnimationHost::PushProperties");
    DCHECK(host_impl->mutator_host());
    mutator_host_->PushPropertiesTo(host_impl->mutator_host());

    sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kNotSyncing);
  }

  // Transfer image decode requests to the impl thread.
  for (auto& request : queued_image_decodes_) {
    int next_id = s_image_decode_sequence_number.GetNext();
    pending_image_decodes_[next_id] = std::move(request.second);
    host_impl->QueueImageDecode(next_id, std::move(request.first));
  }
  queued_image_decodes_.clear();

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
  property_trees_.ResetAllChangeTracking();

  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_host=3
  if (VLOG_IS_ON(3)) {
    std::string property_trees;
    base::JSONWriter::WriteWithOptions(
        *sync_tree->property_trees()->AsTracedValue()->ToBaseValue(),
        base::JSONWriter::OPTIONS_PRETTY_PRINT, &property_trees);
    VLOG(3) << "After finishing commit on impl, the sync tree:"
            << "\nproperty_trees:\n"
            << property_trees << "\nlayers:\n"
            << host_impl->LayerListAsJson();
  }
}

void LayerTreeHost::ImageDecodesFinished(
    const std::vector<std::pair<int, bool>>& results) {
  // Issue stored callbacks and remove them from the pending list.
  for (const auto& pair : results) {
    auto it = pending_image_decodes_.find(pair.first);
    DCHECK(it != pending_image_decodes_.end());
    std::move(it->second).Run(pair.second);
    pending_image_decodes_.erase(it);
  }
}

void LayerTreeHost::PushPropertyTreesTo(LayerTreeImpl* tree_impl) {
  bool property_trees_changed_on_active_tree =
      tree_impl->IsActiveTree() && tree_impl->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (root_layer_ && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        tree_impl->property_trees()->sequence_number)
      tree_impl->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      tree_impl->MoveChangeTrackingToLayers();
  }

  tree_impl->SetPropertyTrees(&property_trees_);
}

void LayerTreeHost::WillCommit() {
  swap_promise_manager_.WillCommit();
  client_->WillCommit();

  if (frame_viewer_instrumentation::IsTracingLayerTreeSnapshots()) {
    bool is_new_trace;
    TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
    if (is_new_trace) {
      // We'll be dumping layer trees as part of trace, so make sure
      // PushPropertiesTo() propagates layer debug info to the impl side --
      // otherwise this won't happen for the layers that remain unchanged since
      // tracing started.
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          this, [](Layer* layer) { layer->SetNeedsPushProperties(); });
    }

    for (Layer* layer : LayersThatShouldPushProperties())
      layer->UpdateDebugInfo();
  }
}


void LayerTreeHost::UpdateDeferCommitsInternal() {
  proxy_->SetDeferCommits(defer_commits_count_ > 0 ||
                          (settings_.enable_surface_synchronization &&
                           !local_surface_id_from_parent_.is_valid()));
}

bool LayerTreeHost::IsUsingLayerLists() const {
  return settings_.use_layer_lists;
}

void LayerTreeHost::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHost::SetLayerTreeFrameSink(
    std::unique_ptr<LayerTreeFrameSink> surface) {
  TRACE_EVENT0("cc", "LayerTreeHost::SetLayerTreeFrameSink");
  DCHECK(surface);

  DCHECK(!new_layer_tree_frame_sink_);
  new_layer_tree_frame_sink_ = std::move(surface);
  proxy_->SetLayerTreeFrameSink(new_layer_tree_frame_sink_.get());
}

std::unique_ptr<LayerTreeFrameSink> LayerTreeHost::ReleaseLayerTreeFrameSink() {
  DCHECK(!visible_);

  DidLoseLayerTreeFrameSink();
  proxy_->ReleaseLayerTreeFrameSink();
  return std::move(current_layer_tree_frame_sink_);
}

void LayerTreeHost::RequestNewLayerTreeFrameSink() {
  client_->RequestNewLayerTreeFrameSink();
}

void LayerTreeHost::DidInitializeLayerTreeFrameSink() {
  DCHECK(new_layer_tree_frame_sink_);
  current_layer_tree_frame_sink_ = std::move(new_layer_tree_frame_sink_);
  client_->DidInitializeLayerTreeFrameSink();
}

void LayerTreeHost::DidFailToInitializeLayerTreeFrameSink() {
  DCHECK(new_layer_tree_frame_sink_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new LayerTreeFrameSink after failing to initialize the new one.
  current_layer_tree_frame_sink_ = nullptr;
  new_layer_tree_frame_sink_ = nullptr;
  client_->DidFailToInitializeLayerTreeFrameSink();
}

std::unique_ptr<LayerTreeHostImpl>
LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(task_runner_provider_->IsImplThread());

  const bool supports_impl_scrolling = task_runner_provider_->HasImplThread();
  std::unique_ptr<MutatorHost> mutator_host_impl =
      mutator_host_->CreateImplInstance(supports_impl_scrolling);

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, task_runner_provider_.get(),
      rendering_stats_instrumentation_.get(), task_graph_runner_,
      std::move(mutator_host_impl), id_, std::move(image_worker_task_runner_));
  if (ukm_recorder_factory_) {
    host_impl->InitializeUkm(ukm_recorder_factory_->CreateRecorder());
    ukm_recorder_factory_.reset();
  }

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentHasSlowPaths(content_has_slow_paths_);
  host_impl->SetContentHasNonAAPaint(content_has_non_aa_paint_);
  task_graph_runner_ = nullptr;
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHost::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseLayerTreeFrameSink");
  DCHECK(task_runner_provider_->IsMainThread());
  SetNeedsCommit();
}

ScopedDeferCommits::ScopedDeferCommits(LayerTreeHost* host)
    : host_(host->defer_commits_weak_ptr_factory_.GetWeakPtr()) {
  host->defer_commits_count_++;
  host->UpdateDeferCommitsInternal();
}

ScopedDeferCommits::~ScopedDeferCommits() {
  LayerTreeHost* host = host_.get();
  if (host) {
    DCHECK_GT(host->defer_commits_count_, 0u);
    if (--host->defer_commits_count_ == 0)
      host->UpdateDeferCommitsInternal();
  }
}

std::unique_ptr<ScopedDeferCommits> LayerTreeHost::DeferCommits() {
  return std::make_unique<ScopedDeferCommits>(this);
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsCommit() {
  proxy_->SetNeedsCommit();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

bool LayerTreeHost::RequestedMainFramePending() {
  return proxy_->RequestedAnimatePending();
}

void LayerTreeHost::SetNeedsRecalculateRasterScales() {
  next_commit_forces_recalculate_raster_scales_ = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

void LayerTreeHost::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHost::SetNeedsCommitWithForcedRedraw() {
  next_commit_forces_redraw_ = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> events) {
  DCHECK(task_runner_provider_->IsMainThread());
  mutator_host_->SetAnimationEvents(std::move(events));
}

void LayerTreeHost::SetDebugState(
    const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1("cc", "LayerTreeHost::SetHasGpuRasterizationTrigger",
                       TRACE_EVENT_SCOPE_THREAD, "has_trigger",
                       has_gpu_rasterization_trigger_);
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(
    float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  float page_scale = page_scale_factor_ * page_scale_delta;
  SetPageScaleFromImplSide(page_scale);
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

bool LayerTreeHost::IsVisible() const {
  return visible_;
}

void LayerTreeHost::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
}

void LayerTreeHost::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  RequestMainFrameUpdate();
  UpdateLayers();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time, bool raster) {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  proxy->CompositeImmediately(frame_begin_time, raster);
}

bool LayerTreeHost::UpdateLayers() {
  if (!root_layer()) {
    property_trees_.clear();
    return false;
  }
  DCHECK(!root_layer()->parent());
  base::ElapsedTimer timer;

  bool result = DoUpdateLayers(root_layer());
  micro_benchmark_controller_.DidUpdateLayers();

  if (const char* client_name = GetClientNameForMetrics()) {
    auto elapsed = timer.Elapsed().InMicroseconds();

    std::string histogram_name =
        base::StringPrintf("Compositing.%s.LayersUpdateTime", client_name);
    base::UmaHistogramCounts10M(histogram_name, elapsed);
  }

  return result;
}

void LayerTreeHost::DidPresentCompositorFrame(
    uint32_t frame_token,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
    const gfx::PresentationFeedback& feedback) {
  for (auto& callback : callbacks)
    std::move(callback).Run(feedback);
  client_->DidPresentCompositorFrame(frame_token, feedback);
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

void LayerTreeHost::RecordGpuRasterizationHistogram(
    const LayerTreeHostImpl* host_impl) {
  // Gpu rasterization is only supported for Renderer compositors.
  // Checking for IsSingleThreaded() to exclude Browser compositors.
  if (gpu_rasterization_histogram_recorded_ || IsSingleThreaded())
    return;

  bool gpu_rasterization_enabled = false;
  if (host_impl->layer_tree_frame_sink()) {
    viz::ContextProvider* compositor_context_provider =
        host_impl->layer_tree_frame_sink()->context_provider();
    if (compositor_context_provider) {
      gpu_rasterization_enabled =
          compositor_context_provider->ContextCapabilities().gpu_rasterization;
    }
  }

  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu whitelisting/backlisting into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        gpu_rasterization_enabled);
  if (gpu_rasterization_enabled) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationTriggered",
                          has_gpu_rasterization_trigger_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSuitableContent",
                          !content_has_slow_paths_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSlowPathsWithNonAAPaint",
                          content_has_slow_paths_ && content_has_non_aa_paint_);
    // Record how many pages actually get gpu rasterization when enabled.
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.GpuRasterizationUsed",
        (has_gpu_rasterization_trigger_ && !content_has_slow_paths_));
  }

  gpu_rasterization_histogram_recorded_ = true;
}

bool LayerTreeHost::DoUpdateLayers(Layer* root_layer) {
  TRACE_EVENT1("cc,benchmark", "LayerTreeHost::DoUpdateLayers",
               "source_frame_number", SourceFrameNumber());

  UpdateHudLayer(debug_state_.ShowHudInfo());

  // The non-layer-list mode is used when blink provides cc with a layer tree
  // and cc needs to compute property trees from that.
  // In layer lists mode, blink sends cc property trees directly so they do not
  // need to be built here. Layer lists mode is used by BlinkGenPropertyTrees
  // and SlimmingPaintV2.
  if (!IsUsingLayerLists()) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::BuildPropertyTrees");
    Layer* root_scroll =
        PropertyTreeBuilder::FindFirstScrollableLayer(root_layer);
    Layer* page_scale_layer = viewport_layers_.page_scale.get();
    if (!page_scale_layer && root_scroll)
      page_scale_layer = root_scroll->parent();
    gfx::Transform identity_transform;
    PropertyTreeBuilder::BuildPropertyTrees(
        root_layer, page_scale_layer, inner_viewport_scroll_layer(),
        outer_viewport_scroll_layer(), overscroll_elasticity_element_id(),
        elastic_overscroll_, page_scale_factor_, device_scale_factor_,
        gfx::Rect(device_viewport_size_), identity_transform, &property_trees_);
    TRACE_EVENT_INSTANT1("cc", "LayerTreeHost::UpdateLayers_BuiltPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees_.AsTracedValue());
  } else {
    TRACE_EVENT_INSTANT1("cc",
                         "LayerTreeHost::UpdateLayers_ReceivedPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees_.AsTracedValue());
    // The HUD layer is managed outside the layer list sent to LayerTreeHost
    // and needs to have its property tree state set.
    if (hud_layer_ && root_layer_.get()) {
      hud_layer_->SetTransformTreeIndex(root_layer_->transform_tree_index());
      hud_layer_->SetEffectTreeIndex(root_layer_->effect_tree_index());
      hud_layer_->SetClipTreeIndex(root_layer_->clip_tree_index());
      hud_layer_->SetScrollTreeIndex(root_layer_->scroll_tree_index());
      hud_layer_->set_property_tree_sequence_number(
          root_layer_->property_tree_sequence_number());
    }
  }

#if DCHECK_IS_ON()
  // Ensure property tree nodes were created for all layers. When using layer
  // lists, this can fail if blink doesn't setup layers or nodes correctly in
  // |PaintArtifactCompositor|. When not using layer lists, this can fail if
  // |PropertyTreeBuilder::BuildPropertyTrees| fails to create property tree
  // nodes.
  for (auto* layer : *this) {
    DCHECK(property_trees_.effect_tree.Node(layer->effect_tree_index()));
    DCHECK(property_trees_.transform_tree.Node(layer->transform_tree_index()));
    DCHECK(property_trees_.clip_tree.Node(layer->clip_tree_index()));
    DCHECK(property_trees_.scroll_tree.Node(layer->scroll_tree_index()));
  }
#endif

  draw_property_utils::UpdatePropertyTrees(this, &property_trees_);

  LayerList update_layer_list;
  draw_property_utils::FindLayersThatNeedUpdates(this, &property_trees_,
                                                 &update_layer_list);

  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_host=3
  // This only prints output for the renderer.
  if (VLOG_IS_ON(3) && GetClientNameForMetrics() == std::string("Renderer")) {
    std::string property_trees;
    base::JSONWriter::WriteWithOptions(
        *property_trees_.AsTracedValue()->ToBaseValue(),
        base::JSONWriter::OPTIONS_PRETTY_PRINT, &property_trees);
    std::ostringstream layers;
    for (auto* layer : *this) {
      layers << "\n  layer id " << layer->id();
      layers << "\n    element_id: " << layer->element_id();
      layers << "\n    bounds: " << layer->bounds().ToString();
      layers << "\n    opacity: " << layer->opacity();
      layers << "\n    position: " << layer->position().ToString();
      layers << "\n    draws_content: " << layer->DrawsContent();
      layers << "\n    scrollable: " << layer->scrollable();
      layers << "\n    contents_opaque: " << layer->contents_opaque();
      layers << "\n    transform_tree_index: " << layer->transform_tree_index();
      layers << "\n    clip_tree_index: " << layer->clip_tree_index();
      layers << "\n    effect_tree_index: " << layer->effect_tree_index();
      layers << "\n    scroll_tree_index: " << layer->scroll_tree_index();
    }
    VLOG(3) << "After updating layers on the main thread:\nproperty trees:\n"
            << property_trees << "\nlayers:" << layers.str();
  }

  bool painted_content_has_slow_paths = false;
  bool painted_content_has_non_aa_paint = false;
  bool did_paint_content =
      PaintContent(update_layer_list, &painted_content_has_slow_paths,
                   &painted_content_has_non_aa_paint);

  // |painted_content_has_non_aa_paint| is a correctness (not performance)
  // modifier, if it changes we immediately update. To prevent churn, this flag
  // is sticky.
  content_has_non_aa_paint_ |= painted_content_has_non_aa_paint;

  // If no slow-path content has appeared for a required number of frames,
  // update the flag.
  if (!painted_content_has_slow_paths) {
    ++num_consecutive_frames_without_slow_paths_;
    if (num_consecutive_frames_without_slow_paths_ >=
        kNumFramesToConsiderBeforeRemovingSlowPathFlag) {
      content_has_slow_paths_ = false;
    }
  } else {
    num_consecutive_frames_without_slow_paths_ = 0;
    content_has_slow_paths_ = true;
  }

  return did_paint_content;
}

void LayerTreeHost::ApplyViewportChanges(const ScrollAndScaleSet& info) {
  gfx::ScrollOffset inner_viewport_scroll_delta;
  if (info.inner_viewport_scroll.element_id)
    inner_viewport_scroll_delta = info.inner_viewport_scroll.scroll_delta;

  if (inner_viewport_scroll_delta.IsZero() && info.page_scale_delta == 1.f &&
      info.elastic_overscroll_delta.IsZero() && !info.top_controls_delta &&
      !info.browser_controls_constraint_changed) {
    return;
  }

  // Preemptively apply the scroll offset and scale delta here before sending
  // it to the client.  If the client comes back and sets it to the same
  // value, then the layer can early out without needing a full commit.
  if (viewport_layers_.inner_viewport_scroll) {
    viewport_layers_.inner_viewport_scroll->SetScrollOffsetFromImplSide(
        viewport_layers_.inner_viewport_scroll->CurrentScrollOffset() +
        inner_viewport_scroll_delta);
  }

  ApplyPageScaleDeltaFromImplSide(info.page_scale_delta);
  SetElasticOverscrollFromImplSide(elastic_overscroll_ +
                                   info.elastic_overscroll_delta);
  // TODO(ccameron): pass the elastic overscroll here so that input events
  // may be translated appropriately.
  client_->ApplyViewportChanges({inner_viewport_scroll_delta,
                                 info.elastic_overscroll_delta,
                                 info.page_scale_delta, info.top_controls_delta,
                                 info.browser_controls_constraint});
  SetNeedsUpdateLayers();
}

void LayerTreeHost::RecordWheelAndTouchScrollingCount(
    const ScrollAndScaleSet& info) {
  bool has_scrolled_by_wheel = info.has_scrolled_by_wheel;
  bool has_scrolled_by_touch = info.has_scrolled_by_touch;

  if (has_scrolled_by_wheel || has_scrolled_by_touch) {
    client_->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                               has_scrolled_by_touch);
  }
}

void LayerTreeHost::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  DCHECK(info);
  for (auto& swap_promise : info->swap_promises) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Main thread scroll update");
    swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
  }

  if (root_layer_) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = LayerByElementId(info->scrolls[i].element_id);
      if (!layer)
        continue;
      layer->SetScrollOffsetFromImplSide(layer->CurrentScrollOffset() +
                                         info->scrolls[i].scroll_delta);
      SetNeedsUpdateLayers();
    }
    for (size_t i = 0; i < info->scrollbars.size(); ++i) {
      Layer* layer = LayerByElementId(info->scrollbars[i].element_id);
      if (!layer)
        continue;
      layer->SetScrollbarsHiddenFromImplSide(info->scrollbars[i].hidden);
    }
  }

  // This needs to happen after scroll deltas have been sent to prevent top
  // controls from clamping the layout viewport both on the compositor and
  // on the main thread.
  ApplyViewportChanges(*info);

  RecordWheelAndTouchScrollingCount(*info);
}

void LayerTreeHost::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  client_->RecordEndOfFrameMetrics(frame_begin_time);
}

const base::WeakPtr<InputHandler>& LayerTreeHost::GetInputHandler()
    const {
  return input_handler_weak_ptr_;
}

void LayerTreeHost::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  // Browser controls are only used in threaded mode but Blink layout tests may
  // call into this. The single threaded version is a no-op.
  proxy_->UpdateBrowserControlsState(constraints, current, animate);
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  // We do not need to animate on the main thread in this case because all
  // relevant changes will be processed on the compositor thread, or proxy,
  // and propagated back to the correct trees.
  // TODO(majidvp): We should be able to eliminate this in the non-
  // slimming path and will do so in a follow up. (762717)
  if (IsUsingLayerLists())
    return;

  std::unique_ptr<MutatorEvents> events = mutator_host_->CreateEvents();

  if (mutator_host_->TickAnimations(monotonic_time,
                                    property_trees()->scroll_tree, true))
    mutator_host_->UpdateAnimationState(true, events.get());

  if (!events->IsEmpty())
    property_trees_.needs_rebuild = true;
}

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    MicroBenchmark::DoneCallback callback) {
  return micro_benchmark_controller_.ScheduleRun(
      benchmark_name, std::move(value), std::move(callback));
}

bool LayerTreeHost::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, std::move(value));
}

void LayerTreeHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  // The animation worklet system assumes that the mutator will never be called
  // from the main thread, which will not be the case if we're running in
  // single-threaded mode.
  if (!task_runner_provider_->HasImplThread()) {
    DLOG(ERROR) << "LayerTreeMutator not supported in single-thread mode";
    return;
  }
  proxy_->SetMutator(std::move(mutator));
}

bool LayerTreeHost::IsSingleThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::SINGLE_THREADED ||
         !task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::SINGLE_THREADED;
}

bool LayerTreeHost::IsThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::THREADED ||
         task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::THREADED;
}

void LayerTreeHost::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  pending_presentation_time_callbacks_.push_back(std::move(callback));
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(nullptr);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(this);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  content_has_slow_paths_ = false;
  content_has_non_aa_paint_ = false;
  gpu_rasterization_histogram_recorded_ = false;

  SetNeedsFullTreeSync();
}

LayerTreeHost::ViewportLayers::ViewportLayers() = default;

LayerTreeHost::ViewportLayers::~ViewportLayers() = default;

void LayerTreeHost::RegisterViewportLayers(const ViewportLayers& layers) {
  DCHECK(!layers.inner_viewport_scroll ||
         layers.inner_viewport_scroll != layers.outer_viewport_scroll);
  // The page scale layer only affects the innter viewport scroll layer. The
  // page scale layer should be sandwiched between the inner viewport scroll and
  // inner viewport container layers:
  //   inner viewport container
  //     overscroll elasticity (optional)
  //       page scale
  //         inner viewport scroll
  DCHECK(IsUsingLayerLists() || !layers.page_scale ||
         layers.inner_viewport_scroll->parent() == layers.page_scale);
  DCHECK(IsUsingLayerLists() || !layers.page_scale ||
         layers.page_scale->parent()->element_id() ==
             layers.overscroll_elasticity_element_id ||
         layers.page_scale->parent() == layers.inner_viewport_container);
  viewport_layers_.overscroll_elasticity_element_id =
      layers.overscroll_elasticity_element_id;
  viewport_layers_.page_scale = layers.page_scale;
  viewport_layers_.inner_viewport_container = layers.inner_viewport_container;
  viewport_layers_.outer_viewport_container = layers.outer_viewport_container;
  viewport_layers_.inner_viewport_scroll = layers.inner_viewport_scroll;
  viewport_layers_.outer_viewport_scroll = layers.outer_viewport_scroll;
}

void LayerTreeHost::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  selection_ = selection;
  SetNeedsCommit();
}

void LayerTreeHost::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (have_scroll_event_handlers_ == have_event_handlers)
    return;

  have_scroll_event_handlers_ = have_event_handlers;
  SetNeedsCommit();
}

void LayerTreeHost::SetEventListenerProperties(
    EventListenerClass event_class,
    EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (event_listener_properties_[index] == properties)
    return;

  // If the mouse wheel event listener is blocking, then every layer in the
  // layer tree sets a wheel event handler region to be its entire bounds,
  // otherwise it sets it to empty.
  //
  // Thus when it changes, we might want to request every layer to push
  // properties and recompute its wheel event handler region, since the
  // computation is done in PushPropertiesTo. However neither
  // SetSubtreePropertyChanged() nor SetNeedsFullTreeSync() do this, so
  // it is unclear why we call them.
  // Also why we don't want to recompute the wheel event handler region for all
  // layers when the blocking state goes away is unclear. Also why we mark all
  // layers below the root layer as damaged is unclear.
  // TODO(bokan): Sort out what should be set and why. https://crbug.com/881011
  //
  // TODO(sunxd): Remove NeedsFullTreeSync when computing mouse wheel event
  // handler region is done.
  if (event_class == EventListenerClass::kMouseWheel) {
    bool new_property_is_blocking =
        properties == EventListenerProperties::kBlocking ||
        properties == EventListenerProperties::kBlockingAndPassive;
    EventListenerProperties old_properties = event_listener_properties_[index];
    bool old_property_is_blocking =
        old_properties == EventListenerProperties::kBlocking ||
        old_properties == EventListenerProperties::kBlockingAndPassive;

    if (!old_property_is_blocking && new_property_is_blocking) {
      if (root_layer())
        root_layer()->SetSubtreePropertyChanged();
      SetNeedsFullTreeSync();
    }
  }

  event_listener_properties_[index] = properties;
  SetNeedsCommit();
}

void LayerTreeHost::SetViewportSizeAndScale(
    const gfx::Size& device_viewport_size,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id_from_parent,
    base::TimeTicks local_surface_id_allocation_time_from_parent) {
  SetLocalSurfaceIdFromParent(local_surface_id_from_parent,
                              local_surface_id_allocation_time_from_parent);

  bool changed = false;
  if (device_viewport_size_ != device_viewport_size) {
    device_viewport_size_ = device_viewport_size;
    changed = true;
  }
  if (settings_.use_painted_device_scale_factor) {
    DCHECK_EQ(device_scale_factor_, 1.f);
    if (painted_device_scale_factor_ != device_scale_factor) {
      painted_device_scale_factor_ = device_scale_factor;
      changed = true;
    }
  } else {
    DCHECK_EQ(painted_device_scale_factor_, 1.f);
    if (device_scale_factor_ != device_scale_factor) {
      device_scale_factor_ = device_scale_factor;
      changed = true;
    }
  }

  if (changed) {
    SetPropertyTreesNeedRebuild();
    SetNeedsCommit();
#if defined(OS_MACOSX)
    // TODO(ccameron): This check is not valid on Aura or Mus yet, but should
    // be.
    CHECK(!has_pushed_local_surface_id_from_parent_ ||
          new_local_surface_id_request_ ||
          !local_surface_id_from_parent_.is_valid());
#endif
  }
}

void LayerTreeHost::SetViewportVisibleRect(const gfx::Rect& visible_rect) {
  if (visible_rect == viewport_visible_rect_)
    return;

  viewport_visible_rect_ = visible_rect;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsHeight(float top_height,
                                             float bottom_height,
                                             bool shrink) {
  if (top_controls_height_ == top_height &&
      bottom_controls_height_ == bottom_height &&
      browser_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_height_ = top_height;
  bottom_controls_height_ = bottom_height;
  browser_controls_shrink_blink_size_ = shrink;
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsShownRatio(float ratio) {
  if (top_controls_shown_ratio_ == ratio)
    return;

  top_controls_shown_ratio_ = ratio;
  SetNeedsCommit();
}

void LayerTreeHost::SetOverscrollBehavior(const OverscrollBehavior& behavior) {
  if (overscroll_behavior_ == behavior)
    return;
  overscroll_behavior_ = behavior;
  SetNeedsCommit();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor_ == page_scale_factor &&
      min_page_scale_factor_ == min_page_scale_factor &&
      max_page_scale_factor_ == max_page_scale_factor)
    return;
  DCHECK_GE(page_scale_factor, min_page_scale_factor);
  DCHECK_LE(page_scale_factor, max_page_scale_factor);
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this check as a pre-condition to closing the bug.
  // https://crbug.com/845097
  CHECK(!settings_.is_layer_tree_for_subframe ||
        page_scale_factor == page_scale_factor_)
      << "Setting PSF in oopif subframe: old psf = " << page_scale_factor_
      << ", new psf = " << page_scale_factor;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(new PendingPageScaleAnimation(
      target_offset, use_anchor, scale, duration));

  SetNeedsCommit();
}

bool LayerTreeHost::HasPendingPageScaleAnimation() const {
  return !!pending_page_scale_animation_.get();
}

void LayerTreeHost::SetRecordingScaleFactor(float recording_scale_factor) {
  if (recording_scale_factor_ == recording_scale_factor)
    return;
  recording_scale_factor_ = recording_scale_factor;
}

void LayerTreeHost::SetRasterColorSpace(
    const gfx::ColorSpace& raster_color_space) {
  if (raster_color_space_ == raster_color_space)
    return;
  raster_color_space_id_ = gfx::ColorSpace::GetNextId();
  raster_color_space_ = raster_color_space;
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      this, [](Layer* layer) { layer->SetNeedsDisplay(); });
}

void LayerTreeHost::SetContentSourceId(uint32_t id) {
  content_source_id_ = id;
}

void LayerTreeHost::SetLocalSurfaceIdFromParent(
    const viz::LocalSurfaceId& local_surface_id_from_parent,
    base::TimeTicks local_surface_id_allocation_time_from_parent) {
  if (local_surface_id_from_parent_.parent_sequence_number() ==
          local_surface_id_from_parent.parent_sequence_number() &&
      local_surface_id_from_parent_.embed_token() ==
          local_surface_id_from_parent.embed_token()) {
    return;
  }

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(local_surface_id_from_parent.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SetLocalSurfaceIdFromParent", "local_surface_id",
      local_surface_id_from_parent.ToString());
  local_surface_id_from_parent_ = local_surface_id_from_parent;
  local_surface_id_allocation_time_from_parent_ =
      local_surface_id_allocation_time_from_parent;
  has_pushed_local_surface_id_from_parent_ = false;
  UpdateDeferCommitsInternal();
  SetNeedsCommit();
}

void LayerTreeHost::ClearCachesOnNextCommit() {
  clear_caches_on_next_commit_ = true;
}

void LayerTreeHost::RequestNewLocalSurfaceId() {
  // If surface synchronization is enabled, then we can still request a new
  // viz::LocalSurfaceId but that request will be deferred until we have a valid
  // viz::LocalSurfaceId from the parent.
  DCHECK(settings_.enable_surface_synchronization ||
         local_surface_id_from_parent_.is_valid());
  if (new_local_surface_id_request_)
    return;
  new_local_surface_id_request_ = true;
  SetNeedsCommit();
}

bool LayerTreeHost::TakeNewLocalSurfaceIdRequest() {
  bool new_local_surface_id_request = new_local_surface_id_request_;
  new_local_surface_id_request_ = false;
  return new_local_surface_id_request;
}

void LayerTreeHost::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (!IsUsingLayerLists() && layer->element_id()) {
    mutator_host_->RegisterElement(layer->element_id(),
                                   ElementListType::ACTIVE);
  }
}

void LayerTreeHost::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (!IsUsingLayerLists() && layer->element_id()) {
    mutator_host_->UnregisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
  layers_that_should_push_properties_.erase(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTreeHost::LayerById(int id) const {
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

bool LayerTreeHost::PaintContent(const LayerList& update_layer_list,
                                 bool* content_has_slow_paths,
                                 bool* content_has_non_aa_paint) {
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    *content_has_slow_paths |= layer->HasSlowPaths();
    *content_has_non_aa_paint |= layer->HasNonAAPaint();
  }
  return did_paint_content;
}

void LayerTreeHost::AddSurfaceRange(const viz::SurfaceRange& surface_range) {
  if (++surface_ranges_[surface_range] == 1)
    needs_surface_ranges_sync_ = true;
}

void LayerTreeHost::RemoveSurfaceRange(const viz::SurfaceRange& surface_range) {
  auto iter = surface_ranges_.find(surface_range);
  if (iter == surface_ranges_.end())
    return;

  if (--iter->second <= 0) {
    surface_ranges_.erase(iter);
    needs_surface_ranges_sync_ = true;
  }
}

base::flat_set<viz::SurfaceRange> LayerTreeHost::SurfaceRanges() const {
  base::flat_set<viz::SurfaceRange> ranges;
  for (auto& map_entry : surface_ranges_)
    ranges.insert(map_entry.first);
  return ranges;
}

void LayerTreeHost::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeHost::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

void LayerTreeHost::ClearLayersThatShouldPushProperties() {
  layers_that_should_push_properties_.clear();
}

void LayerTreeHost::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(CommitRequested());
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this check as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(!settings_.is_layer_tree_for_subframe ||
         page_scale == page_scale_factor_)
      << "Setting PSF in oopif subframe: old psf = " << page_scale_factor_
      << ", new psf = " << page_scale;
  page_scale_factor_ = page_scale;
  SetPropertyTreesNeedRebuild();
}

void LayerTreeHost::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(CommitRequested());
  elastic_overscroll_ = elastic_overscroll;
}

void LayerTreeHost::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer_.get())
      hud_layer_ = HeadsUpDisplayLayer::Create();
    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
    hud_layer_->UpdateLocationAndSize(device_viewport_size_,
                                      device_scale_factor_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = nullptr;
  }
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  SetNeedsUpdateLayers();
}

void LayerTreeHost::PushLayerTreePropertiesTo(LayerTreeImpl* tree_impl) {
  tree_impl->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = tree_impl->LayerById(hud_layer_->id());
    tree_impl->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    tree_impl->set_hud_layer(nullptr);
  }

  tree_impl->set_background_color(background_color_);
  tree_impl->set_have_scroll_event_handlers(have_scroll_event_handlers_);
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (viewport_layers_.inner_viewport_scroll) {
    LayerTreeImpl::ViewportLayerIds ids;
    if (viewport_layers_.overscroll_elasticity_element_id) {
      ids.overscroll_elasticity_element_id =
          viewport_layers_.overscroll_elasticity_element_id;
    }
    ids.page_scale = viewport_layers_.page_scale->id();
    if (viewport_layers_.inner_viewport_container)
      ids.inner_viewport_container =
          viewport_layers_.inner_viewport_container->id();
    if (viewport_layers_.outer_viewport_container)
      ids.outer_viewport_container =
          viewport_layers_.outer_viewport_container->id();
    ids.inner_viewport_scroll = viewport_layers_.inner_viewport_scroll->id();
    if (viewport_layers_.outer_viewport_scroll)
      ids.outer_viewport_scroll = viewport_layers_.outer_viewport_scroll->id();
    tree_impl->SetViewportLayersFromIds(ids);
    DCHECK(viewport_layers_.inner_viewport_scroll
               ->IsContainerForFixedPositionLayers());
  } else {
    tree_impl->ClearViewportLayers();
  }

  tree_impl->RegisterSelection(selection_);

  tree_impl->PushPageScaleFromMainThread(
      page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);

  tree_impl->set_browser_controls_shrink_blink_size(
      browser_controls_shrink_blink_size_);
  tree_impl->SetTopControlsHeight(top_controls_height_);
  tree_impl->SetBottomControlsHeight(bottom_controls_height_);
  tree_impl->set_overscroll_behavior(overscroll_behavior_);
  tree_impl->PushBrowserControlsFromMainThread(top_controls_shown_ratio_);
  tree_impl->elastic_overscroll()->PushMainToPending(elastic_overscroll_);
  if (tree_impl->IsActiveTree())
    tree_impl->elastic_overscroll()->PushPendingToActive();

  tree_impl->SetRasterColorSpace(raster_color_space_id_, raster_color_space_);

  tree_impl->set_content_source_id(content_source_id_);

  tree_impl->set_painted_device_scale_factor(painted_device_scale_factor_);
  tree_impl->SetDeviceScaleFactor(device_scale_factor_);
  tree_impl->SetDeviceViewportSize(device_viewport_size_);
  tree_impl->SetViewportVisibleRect(viewport_visible_rect_);

  if (TakeNewLocalSurfaceIdRequest())
    tree_impl->RequestNewLocalSurfaceId();

  tree_impl->SetLocalSurfaceIdFromParent(
      local_surface_id_from_parent_,
      local_surface_id_allocation_time_from_parent_);
  has_pushed_local_surface_id_from_parent_ = true;

  if (pending_page_scale_animation_) {
    tree_impl->SetPendingPageScaleAnimation(
        std::move(pending_page_scale_animation_));
  }

  tree_impl->set_has_ever_been_drawn(false);
}

void LayerTreeHost::PushSurfaceRangesTo(LayerTreeImpl* tree_impl) {
  if (needs_surface_ranges_sync()) {
    tree_impl->ClearSurfaceRanges();
    tree_impl->SetSurfaceRanges(SurfaceRanges());
    // Reset for next update
    set_needs_surface_ranges_sync(false);
  }
}

void LayerTreeHost::PushLayerTreeHostPropertiesTo(
    LayerTreeHostImpl* host_impl) {
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentHasSlowPaths(content_has_slow_paths_);
  host_impl->SetContentHasNonAAPaint(content_has_non_aa_paint_);
  RecordGpuRasterizationHistogram(host_impl);

  host_impl->SetDebugState(debug_state_);
}

Layer* LayerTreeHost::LayerByElementId(ElementId element_id) const {
  auto iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTreeHost::RegisterElement(ElementId element_id,
                                    ElementListType list_type,
                                    Layer* layer) {
  element_layers_map_[element_id] = layer;
  mutator_host_->RegisterElement(element_id, list_type);
}

void LayerTreeHost::UnregisterElement(ElementId element_id,
                                      ElementListType list_type) {
  mutator_host_->UnregisterElement(element_id, list_type);
  element_layers_map_.erase(element_id);
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeHost::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

void LayerTreeHost::BuildPropertyTreesForTesting() {
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer(), page_scale_layer(), inner_viewport_scroll_layer(),
      outer_viewport_scroll_layer(), overscroll_elasticity_element_id(),
      elastic_overscroll(), page_scale_factor(), device_scale_factor(),
      gfx::Rect(device_viewport_size()), identity_transform, property_trees());
}

bool LayerTreeHost::IsElementInList(ElementId element_id,
                                    ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTreeHost::SetMutatorsNeedCommit() {
  SetNeedsCommit();
}

void LayerTreeHost::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTreeHost::SetElementFilterMutated(ElementId element_id,
                                            ElementListType list_type,
                                            const FilterOperations& filters) {
  if (IsUsingLayerLists()) {
    // In SPv2 we always have property trees and can set the filter
    // directly on the effect node.
    property_trees_.effect_tree.OnFilterAnimated(element_id, filters);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTreeHost::SetElementOpacityMutated(ElementId element_id,
                                             ElementListType list_type,
                                             float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (IsUsingLayerLists()) {
    property_trees_.effect_tree.OnOpacityAnimated(element_id, opacity);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);

  if (EffectNode* node =
          property_trees_.effect_tree.Node(layer->effect_tree_index())) {
    DCHECK_EQ(layer->effect_tree_index(), node->id);
    if (node->opacity == opacity)
      return;

    node->opacity = opacity;
    property_trees_.effect_tree.set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (IsUsingLayerLists()) {
    property_trees_.transform_tree.OnTransformAnimated(element_id, transform);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);

  if (layer->has_transform_node()) {
    TransformNode* node =
        property_trees_.transform_tree.Node(layer->transform_tree_index());
    if (node->local == transform)
      return;

    node->local = transform;
    node->needs_local_transform_update = true;
    node->has_potential_animation = true;
    property_trees_.transform_tree.set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  // Do nothing. Scroll deltas will be sent from the compositor thread back
  // to the main thread in the same manner as during non-animated
  // compositor-driven scrolling.
}

void LayerTreeHost::ElementIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  DCHECK_EQ(ElementListType::ACTIVE, list_type);
  property_trees()->ElementIsAnimatingChanged(mutator_host(), element_id,
                                              list_type, mask, state, true);
}

gfx::ScrollOffset LayerTreeHost::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  return property_trees()->scroll_tree.current_scroll_offset(element_id);
}

void LayerTreeHost::QueueImageDecode(const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("cc", "LayerTreeHost::QueueImageDecode");
  queued_image_decodes_.emplace_back(image, std::move(callback));
  SetNeedsCommit();
}

LayerListIterator<Layer> LayerTreeHost::begin() const {
  return LayerListIterator<Layer>(root_layer_.get());
}

LayerListIterator<Layer> LayerTreeHost::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTreeHost::rbegin() {
  return LayerListReverseIterator<Layer>(root_layer_.get());
}

LayerListReverseIterator<Layer> LayerTreeHost::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

void LayerTreeHost::SetHasCopyRequest(bool has_copy_request) {
  has_copy_request_ = has_copy_request;
}

void LayerTreeHost::RequestBeginMainFrameNotExpected(bool new_state) {
  proxy_->RequestBeginMainFrameNotExpected(new_state);
}

void LayerTreeHost::SetURLForUkm(const GURL& url) {
  proxy_->SetURLForUkm(url);
}

void LayerTreeHost::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  proxy_->SetRenderFrameObserver(std::move(observer));
}

}  // namespace cc
