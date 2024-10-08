// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/switches.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/metrics/lcd_text_metrics_reporter.h"
#include "cc/metrics/ukm_smoothness_data.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_worklet_job.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/tiles/eviction_tile_priority_queue.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "cc/tiles/tiles_with_resource_iterator.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/debug_rect_history.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/image_animation_controller.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_context.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mobile_optimized_viewport_util.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/raster_capabilities.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/trace_utils.h"
#include "cc/trees/tree_synchronizer.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/traced_value.h"
#include "components/viz/common/transition_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/skia_span_util.h"

namespace cc {
namespace {

// In BuildHitTestData we iterate all layers to find all layers that overlap
// OOPIFs, but when the number of layers is greater than
// |kAssumeOverlapThreshold|, it can be inefficient to accumulate layer bounds
// for overlap checking. As a result, we are conservative and make OOPIFs
// kHitTestAsk after the threshold is reached.
const size_t kAssumeOverlapThreshold = 100;

// gfx::DisplayColorSpaces stores up to 3 different color spaces. This should be
// updated to match any size changes in DisplayColorSpaces.
constexpr size_t kContainsSrgbCacheSize = 3;
static_assert(kContainsSrgbCacheSize ==
                  gfx::DisplayColorSpaces::kConfigCount / 2,
              "sRGB cache must match the size of DisplayColorSpaces");

bool IsMobileOptimized(LayerTreeImpl* active_tree) {
  return util::IsMobileOptimized(active_tree->min_page_scale_factor(),
                                 active_tree->max_page_scale_factor(),
                                 active_tree->current_page_scale_factor(),
                                 active_tree->ScrollableViewportSize(),
                                 active_tree->ScrollableSize(),
                                 active_tree->viewport_mobile_optimized());
}

void DidVisibilityChange(LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "cc,benchmark", "LayerTreeHostImpl::SetVisible", TRACE_ID_LOCAL(id),
        "LayerTreeHostImpl", static_cast<void*>(id));
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "cc,benchmark", "LayerTreeHostImpl::SetVisible", TRACE_ID_LOCAL(id));
}

void PopulateMetadataContentColorUsage(
    const LayerTreeHostImpl::FrameData* frame,
    viz::CompositorFrameMetadata* metadata) {
  metadata->content_color_usage = gfx::ContentColorUsage::kSRGB;
  for (const LayerImpl* layer : frame->will_draw_layers) {
    metadata->content_color_usage =
        std::max(metadata->content_color_usage, layer->GetContentColorUsage());
  }
}

// Dump verbose log with
// --vmodule=layer_tree_host_impl=3 for renderer only, or
// --vmodule=layer_tree_host_impl=4 for all clients.
bool VerboseLogEnabled() {
  if (!VLOG_IS_ON(3))
    return false;
  if (VLOG_IS_ON(4))
    return true;
  const char* client_name = GetClientNameForMetrics();
  return client_name && strcmp(client_name, "Renderer") == 0;
}

const char* ClientNameForVerboseLog() {
  const char* client_name = GetClientNameForMetrics();
  return client_name ? client_name : "<unknown client>";
}

#define VERBOSE_LOG() \
  VLOG_IF(3, VerboseLogEnabled()) << ClientNameForVerboseLog() << ": "

}  // namespace

// Holds either a created ImageDecodeCache or a ptr to a shared
// GpuImageDecodeCache.
class LayerTreeHostImpl::ImageDecodeCacheHolder {
 public:
  ImageDecodeCacheHolder(bool enable_shared_image_cache_for_gpu,
                         const RasterCapabilities& raster_caps,
                         bool gpu_compositing,
                         scoped_refptr<RasterContextProviderWrapper>
                             worker_context_provider_wrapper,
                         size_t decoded_image_working_set_budget_bytes,
                         RasterDarkModeFilter* dark_mode_filter) {
    if (raster_caps.use_gpu_rasterization) {
      auto color_type = viz::ToClosestSkColorType(
          /*gpu_compositing=*/true, raster_caps.tile_format);
      if (enable_shared_image_cache_for_gpu) {
        image_decode_cache_ptr_ =
            &worker_context_provider_wrapper->GetGpuImageDecodeCache(
                color_type, raster_caps);
      } else {
        image_decode_cache_ = std::make_unique<GpuImageDecodeCache>(
            worker_context_provider_wrapper->GetContext().get(),
            /*use_transfer_cache=*/true, color_type,
            decoded_image_working_set_budget_bytes,
            raster_caps.max_texture_size, dark_mode_filter);
      }
    } else {
      image_decode_cache_ = std::make_unique<SoftwareImageDecodeCache>(
          viz::ToClosestSkColorType(gpu_compositing, raster_caps.tile_format),
          decoded_image_working_set_budget_bytes);
    }

    if (image_decode_cache_) {
      image_decode_cache_ptr_ = image_decode_cache_.get();
    } else {
      DCHECK(image_decode_cache_ptr_);
    }
  }

  void SetShouldAggressivelyFreeResources(bool aggressively_free_resources) {
    // This must only be called if the decode cache is not shared aka is not
    // created via RasterContextProviderWrapper as the cache created via that
    // gets this calls from ContextCacheController, which notifies only after
    // ALL clients are invisible or at least one is visible.
    if (image_decode_cache_) {
      image_decode_cache_->SetShouldAggressivelyFreeResources(
          aggressively_free_resources, /*context_lock_acquired=*/false);
    }
  }

  ImageDecodeCache* image_decode_cache() const {
    DCHECK(image_decode_cache_ptr_);
    return image_decode_cache_ptr_;
  }

  ImageDecodeCacheHolder(const ImageDecodeCacheHolder&) = delete;
  ImageDecodeCacheHolder& operator=(const ImageDecodeCacheHolder&) = delete;

 private:
  std::unique_ptr<ImageDecodeCache> image_decode_cache_;
  // RAW_PTR_EXCLUSION: ImageDecodeCache is marked as not supported by raw_ptr.
  // See raw_ptr.h for more information.
  RAW_PTR_EXCLUSION ImageDecodeCache* image_decode_cache_ptr_ = nullptr;
};

void LayerTreeHostImpl::DidUpdateScrollAnimationCurve() {
  // Because we updated the animation target, notify the
  // `LatencyInfoSwapPromiseMonitor` to tell it that something happened that
  // will cause a swap in the future. This will happen within the scope of the
  // dispatch of a gesture scroll update input event. If we don't notify during
  // the handling of the input event, the `LatencyInfo` associated with the
  // input event will not be added as a swap promise and we won't get any swap
  // results.
  NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
}

void LayerTreeHostImpl::AccumulateScrollDeltaForTracing(
    const gfx::Vector2dF& delta) {
  scroll_accumulated_this_frame_ += delta;
}

void LayerTreeHostImpl::DidStartPinchZoom() {
  client_->RenewTreePriority();
  frame_trackers_.StartSequence(FrameSequenceTrackerType::kPinchZoom);
}

void LayerTreeHostImpl::DidEndPinchZoom() {
  // When a pinch ends, we may be displaying content cached at incorrect scales,
  // so updating draw properties and drawing will ensure we are using the right
  // scales that we want when we're not inside a pinch.
  active_tree_->set_needs_update_draw_properties();
  SetNeedsRedrawOrUpdateDisplayTree();
  frame_trackers_.StopSequence(FrameSequenceTrackerType::kPinchZoom);
}

void LayerTreeHostImpl::DidUpdatePinchZoom() {
  SetNeedsRedrawOrUpdateDisplayTree();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::DidStartScroll() {
  scroll_affects_scroll_handler_ = active_tree()->have_scroll_event_handlers();
  if (!settings().single_thread_proxy_scheduler) {
    client_->SetHasActiveThreadedScroll(true);
  }
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::DidEndScroll() {
  scroll_affects_scroll_handler_ = false;

  if (!settings().single_thread_proxy_scheduler) {
    client_->SetHasActiveThreadedScroll(false);
    client_->SetWaitingForScrollEvent(false);
  }

#if BUILDFLAG(IS_ANDROID)
  if (render_frame_metadata_observer_) {
    render_frame_metadata_observer_->DidEndScroll();
  }
#endif
}

void LayerTreeHostImpl::DidMouseLeave() {
  for (auto& pair : scrollbar_animation_controllers_)
    pair.second->DidMouseLeave();
}

void LayerTreeHostImpl::SetNeedsFullViewportRedraw() {
  // TODO(bokan): Do these really need to be manually called? (Rather than
  // damage/redraw being set from scroll offset changes).
  SetFullViewportDamage();
  SetNeedsRedrawOrUpdateDisplayTree();
}

void LayerTreeHostImpl::SetDeferBeginMainFrame(
    bool defer_begin_main_frame) const {
  client_->SetDeferBeginMainFrameFromImpl(defer_begin_main_frame);
}

void LayerTreeHostImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info) {
  browser_controls_offset_manager_->UpdateBrowserControlsState(
      constraints, current, animate, offset_tags_info);
}

bool LayerTreeHostImpl::HasScrollLinkedAnimation(ElementId for_scroller) const {
  return mutator_host_->HasScrollLinkedAnimation(for_scroller);
}

bool LayerTreeHostImpl::IsInHighLatencyMode() const {
  return impl_thread_phase_ == ImplThreadPhase::IDLE;
}

const LayerTreeSettings& LayerTreeHostImpl::GetSettings() const {
  return settings();
}

LayerTreeHostImpl& LayerTreeHostImpl::GetImplDeprecated() {
  return *this;
}

const LayerTreeHostImpl& LayerTreeHostImpl::GetImplDeprecated() const {
  return *this;
}

LayerTreeHostImpl::FrameData::FrameData() = default;
LayerTreeHostImpl::FrameData::~FrameData() = default;
LayerTreeHostImpl::UIResourceData::UIResourceData() = default;
LayerTreeHostImpl::UIResourceData::~UIResourceData() = default;
LayerTreeHostImpl::UIResourceData::UIResourceData(UIResourceData&&) noexcept =
    default;
LayerTreeHostImpl::UIResourceData& LayerTreeHostImpl::UIResourceData::operator=(
    UIResourceData&&) = default;

std::unique_ptr<LayerTreeHostImpl> LayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    RasterDarkModeFilter* dark_mode_filter,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client) {
  return base::WrapUnique(new LayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), dark_mode_filter, id,
      std::move(image_worker_task_runner), scheduling_client));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    RasterDarkModeFilter* dark_mode_filter,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client)
    : client_(client),
      scheduling_client_(scheduling_client),
      task_runner_provider_(task_runner_provider),
      current_begin_frame_tracker_(FROM_HERE),
      settings_(settings),
      use_layer_context_for_display_(settings_.UseLayerContextForDisplay()),
      is_synchronous_single_threaded_(
          !task_runner_provider->HasImplThread() &&
          !settings_.single_thread_proxy_scheduler &&
          !use_layer_context_for_display_),
      cached_managed_memory_policy_(settings.memory_policy),
      // Must be initialized after is_synchronous_single_threaded_ and
      // task_runner_provider_.
      tile_manager_(this,
                    GetTaskRunner(),
                    std::move(image_worker_task_runner),
                    is_synchronous_single_threaded_
                        ? std::numeric_limits<size_t>::max()
                        : settings.scheduled_raster_task_limit,
                    RunningOnRendererProcess(),
                    settings.ToTileManagerSettings()),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      mutator_host_(std::move(mutator_host)),
      dark_mode_filter_(dark_mode_filter),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      micro_benchmark_controller_(this),
      task_graph_runner_(task_graph_runner),
      id_(id),
      consecutive_frame_with_damage_count_(settings.damaged_frame_limit),
      // It is safe to use base::Unretained here since we will outlive the
      // ImageAnimationController.
      image_animation_controller_(GetTaskRunner(),
                                  this,
                                  settings_.enable_image_animation_resync),
      compositor_frame_reporting_controller_(
          std::make_unique<CompositorFrameReportingController>(
              /*should_report_histograms=*/!settings
                  .single_thread_proxy_scheduler,
              /*should_report_ukm=*/!settings.single_thread_proxy_scheduler,
              id)),
      frame_trackers_(settings.single_thread_proxy_scheduler,
                      compositor_frame_reporting_controller_.get()),
      lcd_text_metrics_reporter_(LCDTextMetricsReporter::CreateIfNeeded(this)),
      frame_rate_estimator_(GetTaskRunner()),
      contains_srgb_cache_(kContainsSrgbCacheSize) {
  resource_provider_ = std::make_unique<viz::ClientResourceProvider>(
      task_runner_provider_->MainThreadTaskRunner(),
      task_runner_provider_->HasImplThread()
          ? task_runner_provider_->ImplThreadTaskRunner()
          : task_runner_provider_->MainThreadTaskRunner(),
      base::BindRepeating(&LayerTreeHostImpl::MaybeFlushPendingWork,
                          weak_factory_.GetWeakPtr()));
  DCHECK(mutator_host_);
  mutator_host_->SetMutatorHostClient(this);
  mutator_events_ = mutator_host_->CreateEvents();

  DCHECK(task_runner_provider_->IsImplThread());
  DidVisibilityChange(this, visible_);

  // LTHI always has an active tree.
  active_tree_ = std::make_unique<LayerTreeImpl>(
      *this, new SyncedScale, new SyncedBrowserControls,
      new SyncedBrowserControls, new SyncedElasticOverscroll);
  active_tree_->property_trees()->set_is_active(true);

  viewport_ = Viewport::Create(this);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                                     "cc::LayerTreeHostImpl", id_);

  browser_controls_offset_manager_ = BrowserControlsOffsetManager::Create(
      this, settings.top_controls_show_threshold,
      settings.top_controls_hide_threshold);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLayerTreeHostMemoryPressure)) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&LayerTreeHostImpl::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  SetDebugState(settings.initial_debug_state);
  compositor_frame_reporting_controller_->SetDroppedFrameCounter(
      &dropped_frame_counter_);
  compositor_frame_reporting_controller_->SetFrameSequenceTrackerCollection(
      &frame_trackers_);

  const bool is_ui = settings.is_layer_tree_for_ui;
  if (is_ui) {
    compositor_frame_reporting_controller_->set_event_latency_tracker(this);

#if BUILDFLAG(IS_CHROMEOS)
    dropped_frame_counter_.EnableReporForUI();
    compositor_frame_reporting_controller_->SetThreadAffectsSmoothness(
        FrameInfo::SmoothEffectDrivingThread::kMain, true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  dropped_frame_counter_.set_total_counter(&total_frame_counter_);
  frame_trackers_.set_custom_tracker_results_added_callback(
      base::BindRepeating(&LayerTreeHostImpl::NotifyThroughputTrackerResults,
                          weak_factory_.GetWeakPtr()));
}

LayerTreeHostImpl::~LayerTreeHostImpl() {
  DCHECK(task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                                     "cc::LayerTreeHostImpl", id_);

  // The frame sink is released before shutdown, which takes down
  // all the resource and raster structures.
  DCHECK(!layer_tree_frame_sink_);
  DCHECK(!resource_pool_);
  DCHECK(!image_decode_cache_holder_);
  DCHECK(!single_thread_synchronous_task_graph_runner_);

  DetachInputDelegateAndRenderFrameObserver();

  // The layer trees must be destroyed before the LayerTreeHost. Also, if they
  // are holding onto any resources, destroying them will release them, before
  // we mark any leftover resources as lost.
  if (recycle_tree_)
    recycle_tree_->Shutdown();
  if (pending_tree_)
    pending_tree_->Shutdown();
  active_tree_->Shutdown();
  recycle_tree_ = nullptr;
  pending_tree_ = nullptr;
  active_tree_ = nullptr;

  // All resources should already be removed, so lose anything still exported.
  resource_provider_->ShutdownAndReleaseAllResources();

  mutator_host_->ClearMutators();
  mutator_host_->SetMutatorHostClient(nullptr);

  // `frame_trackers_` holds a pointer to
  // `compositor_frame_reporting_controller_`. Setting
  // `compositor_frame_reporting_controller_` to nullptr here leads to
  // `frame_trackers_` holding a dangling ptr. Don't set to null here and let
  // members be destroyed in reverse order of declaration.
  // Since `frame_trackers_` is destroyed first, we need to clear the ptr that
  // `compositor_frame_reporting_controller_` holds.
  compositor_frame_reporting_controller_->SetFrameSequenceTrackerCollection(
      nullptr);
  // Similar to the logic above. The `compositor_frame_reporting_controller_`
  // was given a `this` pointer for the event_latency_tracker and thus needs
  // to be nulled to prevent it dangling.
  compositor_frame_reporting_controller_->set_event_latency_tracker(nullptr);
}

InputHandler& LayerTreeHostImpl::GetInputHandler() {
  DCHECK(input_delegate_) << "Requested InputHandler when one wasn't bound. "
                             "Call BindToInputHandler to bind to one";
  return static_cast<InputHandler&>(*input_delegate_.get());
}

const InputHandler& LayerTreeHostImpl::GetInputHandler() const {
  DCHECK(input_delegate_) << "Requested InputHandler when one wasn't bound. "
                             "Call BindToInputHandler to bind to one";
  return static_cast<const InputHandler&>(*input_delegate_.get());
}

void LayerTreeHostImpl::BeginMainFrameAborted(
    CommitEarlyOutReason reason,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises,
    const viz::BeginFrameArgs& args,
    bool next_bmf,
    bool scroll_and_viewport_changes_synced) {
  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  bool main_frame_applied_deltas = MainFrameAppliedDeltas(reason);
  active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit(
      next_bmf, main_frame_applied_deltas);
  if (main_frame_applied_deltas) {
    if (pending_tree_) {
      pending_tree_->AppendSwapPromises(std::move(swap_promises));
    } else {
      base::TimeTicks timestamp = base::TimeTicks::Now();
      for (const auto& swap_promise : swap_promises) {
        SwapPromise::DidNotSwapAction action =
            swap_promise->DidNotSwap(SwapPromise::COMMIT_NO_UPDATE, timestamp);
        DCHECK_EQ(action, SwapPromise::DidNotSwapAction::BREAK_PROMISE);
      }
    }
  }

  // Notify the browser controls manager that we have processed any
  // controls constraint update.
  if (scroll_and_viewport_changes_synced && browser_controls_manager()) {
    browser_controls_manager()->NotifyConstraintSyncedToMainThread();
  }
}

void LayerTreeHostImpl::ReadyToCommit(
    const viz::BeginFrameArgs& commit_args,
    bool scroll_and_viewport_changes_synced,
    const BeginMainFrameMetrics* begin_main_frame_metrics,
    bool commit_timeout) {
  if (!is_measuring_smoothness_ &&
      ((begin_main_frame_metrics &&
        begin_main_frame_metrics->should_measure_smoothness) ||
       commit_timeout)) {
    is_measuring_smoothness_ = true;
    total_frame_counter_.Reset();
    dropped_frame_counter_.OnFcpReceived();
  }

  // Notify the browser controls manager that we have processed any
  // controls constraint update.
  if (scroll_and_viewport_changes_synced && browser_controls_manager()) {
    browser_controls_manager()->NotifyConstraintSyncedToMainThread();
  }

  // If the scoll offsets were not synchronized, undo the sending of offsets
  // similar to what's done when the commit is aborted.
  if (!scroll_and_viewport_changes_synced) {
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit(
        /*next_bmf=*/false, /*main_frame_applied_deltas=*/false);
  }
}

void LayerTreeHostImpl::BeginCommit(int source_frame_number,
                                    BeginMainFrameTraceId trace_id) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  if (!CommitsToActiveTree()) {
    CreatePendingTree();
  }
  sync_tree()->set_source_frame_number(source_frame_number);
  sync_tree()->set_trace_id(trace_id);
}

// This function commits the LayerTreeHost, as represented by CommitState, to an
// impl tree.  When modifying this function -- and all code that it calls into
// -- care must be taken to avoid using LayerTreeHost directly (e.g., via
// state.root_layer->layer_tree_host()) as that will likely introduce thread
// safety violations.  Any information that is needed from LayerTreeHost should
// instead be plumbed through CommitState (see
// LayerTreeHost::ActivateCommitState() for reference).
void LayerTreeHostImpl::FinishCommit(
    CommitState& state,
    const ThreadUnsafeCommitState& unsafe_state) {
  TRACE_EVENT0("cc,benchmark", "LayerTreeHostImpl::FinishCommit");
  LayerTreeImpl* tree = sync_tree();
  {
    // Instead of individual `Layer::PushPropertiesTo` triggering separate
    // thread hops to the main-thread, to complete releasing resources. Batch
    // all of them together for after `PullPropertiesFrom` completes.
    viz::ClientResourceProvider::ScopedBatchResourcesRelease
        scoped_resource_release =
            resource_provider_->CreateScopedBatchResourcesRelease();
    tree->PullPropertiesFrom(state, unsafe_state);
  }

  // Check whether the impl scroll animating nodes were removed by the commit.
  mutator_host()->HandleRemovedScrollAnimatingElements(CommitsToActiveTree());

  PullLayerTreeHostPropertiesFrom(state);

  // Transfer image decode requests to the impl thread.
  for (auto& entry : state.queued_image_decodes)
    QueueImageDecode(entry.first, *entry.second);

  for (auto& benchmark : state.benchmarks)
    ScheduleMicroBenchmark(std::move(benchmark));

  // Dump property trees and layers if VerboseLogEnabled().
  VERBOSE_LOG() << "After finishing commit on impl, the sync tree:"
                << "\nproperty_trees:\n"
                << tree->property_trees()->ToString() << "\n"
                << "cc::LayerImpls:\n"
                << tree->LayerListAsJson();
}

void LayerTreeHostImpl::PullLayerTreeHostPropertiesFrom(
    const CommitState& commit_state) {
  // TODO(bokan): The |external_pinch_gesture_active| should not be going
  // through the LayerTreeHost but directly from InputHandler to InputHandler.
  SetExternalPinchGestureActive(commit_state.is_external_pinch_gesture_active);
  if (commit_state.needs_gpu_rasterization_histogram)
    RecordGpuRasterizationHistogram();
  SetDebugState(commit_state.debug_state);
  SetVisualDeviceViewportSize(commit_state.visual_device_viewport_size);
  set_viewport_mobile_optimized(commit_state.is_viewport_mobile_optimized);
  SetPrefersReducedMotion(commit_state.prefers_reduced_motion);
  SetMayThrottleIfUndrawnFrames(commit_state.may_throttle_if_undrawn_frames);
}

void LayerTreeHostImpl::RecordGpuRasterizationHistogram() {
  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu allowlist/denylist into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        raster_caps().use_gpu_rasterization);
}

void LayerTreeHostImpl::CommitComplete() {
  DCHECK(!settings_.is_display_tree);

  TRACE_EVENT(
      "cc,benchmark", "LayerTreeHostImpl::CommitComplete",
      [&](perfetto::EventContext ctx) {
        EmitMainFramePipelineStep(
            ctx, sync_tree()->trace_id(),
            perfetto::protos::pbzero::MainFramePipeline::Step::COMMIT_COMPLETE);
      });

  if (input_delegate_)
    input_delegate_->DidCommit();

  if (CommitsToActiveTree()) {
    active_tree_->HandleScrollbarShowRequests();

    // We have to activate animations here or "IsActive()" is true on the layers
    // but the animations aren't activated yet so they get ignored by
    // UpdateDrawProperties.
    ActivateAnimations();
  }

  // We clear the entries that were never mutated by CC animations from the last
  // commit until now. Moreover, we reset the values of input properties and
  // relies on the fact that CC animation will mutate those values when pending
  // tree is animated below.
  // With that, when CC finishes animating an input property, the value of that
  // property stays at finish state until a commit kicks in, which is consistent
  // with current composited animations.
  paint_worklet_tracker_.ClearUnusedInputProperties();

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results. When doing commit to the active tree, this must happen
  // after ActivateAnimations() in order for this ticking to be propagated
  // to layers on the active tree.
  if (CommitsToActiveTree()) {
    Animate();
  } else {
    AnimatePendingTreeAfterCommit();
  }

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
  micro_benchmark_controller_.DidCompleteCommit();

  if (mutator_host_->CurrentFrameHadRAF())
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kRAF);
  if (mutator_host_->HasCanvasInvalidation())
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kCanvasAnimation);
  if (mutator_host_->CurrentFrameHadRAF() || mutator_host_->HasJSAnimation())
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kJSAnimation);

  if (mutator_host_->MainThreadAnimationsCount() > 0 ||
      mutator_host_->HasSmilAnimation()) {
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kMainThreadAnimation);
    if (mutator_host_->HasViewTransition()) {
      frame_trackers_.StartSequence(
          FrameSequenceTrackerType::kSETMainThreadAnimation);
    }
  }

  for (const auto& info : mutator_host_->TakePendingThroughputTrackerInfos()) {
    const MutatorHost::TrackedAnimationSequenceId sequence_id = info.id;
    const bool start = info.start;
    if (start)
      frame_trackers_.StartCustomSequence(sequence_id);
    else
      frame_trackers_.StopCustomSequence(sequence_id);
  }
}

void LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation() {
  DCHECK(!settings_.is_display_tree);

  sync_tree()->set_needs_update_draw_properties();

  // We need an update immediately post-commit to have the opportunity to create
  // tilings.
  // We can avoid updating the ImageAnimationController during this
  // DrawProperties update since it will be done when we animate the controller
  // below.
  bool update_tiles = true;
  bool update_image_animation_controller = false;
  sync_tree()->UpdateDrawProperties(update_tiles,
                                    update_image_animation_controller);

  // Defer invalidating images until UpdateDrawProperties is performed since
  // that updates whether an image should be animated based on its visibility
  // and the updated data for the image from the main frame.
  PaintImageIdFlatSet images_to_invalidate =
      tile_manager_.TakeImagesToInvalidateOnSyncTree();

  const auto& animated_images =
      image_animation_controller_.AnimateForSyncTree(CurrentBeginFrameArgs());
  images_to_invalidate.insert(animated_images.begin(), animated_images.end());

  // Invalidate cached PaintRecords for worklets whose input properties were
  // mutated since the last pending tree. We keep requesting invalidations until
  // the animation is ticking on impl thread. Note that this works since the
  // animation starts ticking on the pending tree
  // (AnimatePendingTreeAfterCommit) which committed this animation timeline.
  // After this the animation may only tick on the active tree for impl-side
  // invalidations (since AnimatePendingTreeAfterCommit is not done for pending
  // trees created by impl-side invalidations). But we ensure here that we
  // request another invalidation if an input property was mutated on the active
  // tree.
  if (paint_worklet_tracker_.InvalidatePaintWorkletsOnPendingTree()) {
    client_->SetNeedsImplSideInvalidation(
        true /* needs_first_draw_on_activation */);
  }
  PaintImageIdFlatSet dirty_paint_worklet_ids;
  PaintWorkletJobMap dirty_paint_worklets =
      GatherDirtyPaintWorklets(&dirty_paint_worklet_ids);
  images_to_invalidate.insert(dirty_paint_worklet_ids.begin(),
                              dirty_paint_worklet_ids.end());

  sync_tree()->InvalidateRegionForImages(images_to_invalidate);

  if (!pending_invalidation_raster_inducing_scrolls_.empty()) {
    base::flat_set<ElementId> scrolls_to_invalidate;
    std::swap(scrolls_to_invalidate,
              pending_invalidation_raster_inducing_scrolls_);
    sync_tree()->InvalidateRasterInducingScrolls(scrolls_to_invalidate);
  }

  // Note that it is important to push the state for checkerboarded and animated
  // images prior to PrepareTiles here when committing to the active tree. This
  // is because new tiles on the active tree depend on tree specific state
  // cached in these components, which must be pushed to active before preparing
  // tiles for the updated active tree.
  if (CommitsToActiveTree()) {
    ActivateStateForImages();
  }

  if (!paint_worklet_painter_) {
    // Blink should not send us any PaintWorklet inputs until we have a painter
    // registered.
    DCHECK(sync_tree()->picture_layers_with_paint_worklets().empty());
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  if (!dirty_paint_worklets.size()) {
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  client_->NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState::PROCESSING);
  auto done_callback = base::BindOnce(
      &LayerTreeHostImpl::OnPaintWorkletResultsReady, base::Unretained(this));
  paint_worklet_painter_->DispatchWorklets(std::move(dirty_paint_worklets),
                                           std::move(done_callback));
}

PaintWorkletJobMap LayerTreeHostImpl::GatherDirtyPaintWorklets(
    PaintImageIdFlatSet* dirty_paint_worklet_ids) const {
  PaintWorkletJobMap dirty_paint_worklets;
  for (PictureLayerImpl* layer :
       sync_tree()->picture_layers_with_paint_worklets()) {
    for (const auto& entry : layer->GetPaintWorkletRecordMap()) {
      const scoped_refptr<const PaintWorkletInput>& input = entry.first;
      const PaintImage::Id& paint_image_id = entry.second.first;
      const std::optional<PaintRecord>& record = entry.second.second;
      // If we already have a record we can reuse it and so the
      // PaintWorkletInput isn't dirty.
      if (record)
        continue;

      // Mark this PaintWorklet as needing invalidation.
      dirty_paint_worklet_ids->insert(paint_image_id);

      // Create an entry in the appropriate PaintWorkletJobVector for this dirty
      // PaintWorklet.
      int worklet_id = input->WorkletId();
      auto& job_vector = dirty_paint_worklets[worklet_id];
      if (!job_vector)
        job_vector = base::MakeRefCounted<PaintWorkletJobVector>();

      PaintWorkletJob::AnimatedPropertyValues animated_property_values;
      for (const auto& element : input->GetPropertyKeys()) {
        DCHECK(!animated_property_values.contains(element));
        const PaintWorkletInput::PropertyValue& animated_property_value =
            paint_worklet_tracker_.GetPropertyAnimationValue(element);
        // No value indicates that the input property was not mutated by CC
        // animation.
        if (animated_property_value.has_value())
          animated_property_values.emplace(element, animated_property_value);
      }

      job_vector->data.emplace_back(layer->id(), input,
                                    std::move(animated_property_values));
    }
  }
  return dirty_paint_worklets;
}

void LayerTreeHostImpl::OnPaintWorkletResultsReady(PaintWorkletJobMap results) {
#if DCHECK_IS_ON()
  // Nothing else should have painted the PaintWorklets while we were waiting,
  // and the results should have painted every PaintWorklet, so these should be
  // the same.
  PaintImageIdFlatSet dirty_paint_worklet_ids;
  DCHECK_EQ(results.size(),
            GatherDirtyPaintWorklets(&dirty_paint_worklet_ids).size());
#endif

  for (const auto& entry : results) {
    for (const PaintWorkletJob& job : entry.second->data) {
      LayerImpl* layer_impl =
          pending_tree_->FindPendingTreeLayerById(job.layer_id());
      // Painting the pending tree occurs asynchronously but stalls the pending
      // tree pipeline, so nothing should have changed while we were doing that.
      DCHECK(layer_impl);
      static_cast<PictureLayerImpl*>(layer_impl)
          ->SetPaintWorkletRecord(job.input(), job.output());
    }
  }

  // While the pending tree is being painted by PaintWorklets, we restrict the
  // tiles the TileManager is able to see. This may cause the TileManager to
  // believe that it has finished rastering all the necessary tiles. When we
  // finish painting the tree and release all the tiles, we need to mark the
  // tile priorities as dirty so that the TileManager logic properly re-runs.
  tile_priorities_dirty_ = true;

  // Set the painted state before calling the scheduler, to ensure any callback
  // running as a result sees the correct painted state.
  pending_tree_fully_painted_ = true;
  client_->NotifyPaintWorkletStateChange(Scheduler::PaintWorkletState::IDLE);

  // The pending tree may have been force activated from the signal to the
  // scheduler above, in which case there is no longer a tree to paint.
  if (pending_tree_)
    NotifyPendingTreeFullyPainted();
}

void LayerTreeHostImpl::NotifyPendingTreeFullyPainted() {
  // The pending tree must be fully painted at this point.
  DCHECK(pending_tree_fully_painted_ && !settings_.is_display_tree);

  // Nobody should claim the pending tree is fully painted if there is an
  // ongoing dispatch.
  DCHECK(!paint_worklet_painter_ ||
         !paint_worklet_painter_->HasOngoingDispatch());

  // Start working on newly created tiles immediately if needed.
  // TODO(vmpstr): Investigate always having PrepareTiles issue
  // NotifyReadyToActivate, instead of handling it here.
  bool did_prepare_tiles = PrepareTiles();
  if (!did_prepare_tiles) {
    NotifyReadyToActivate();

    // Ensure we get ReadyToDraw signal even when PrepareTiles not run. This
    // is important for SingleThreadProxy and impl-side painting case. For
    // STP, we commit to active tree and RequiresHighResToDraw, and set
    // Scheduler to wait for ReadyToDraw signal to avoid Checkerboard.
    if (CommitsToActiveTree() ||
        settings_.wait_for_all_pipeline_stages_before_draw) {
      NotifyReadyToDraw();
    }
  }
}

bool LayerTreeHostImpl::CanDraw() const {
  // Note: If you are changing this function or any other function that might
  // affect the result of CanDraw, make sure to call
  // client_->OnCanDrawStateChanged in the proper places and update the
  // NotifyIfCanDrawChanged test.

  if (!layer_tree_frame_sink_) {
    TRACE_EVENT_INSTANT0("cc",
                         "LayerTreeHostImpl::CanDraw no LayerTreeFrameSink",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // TODO(boliu): Make draws without layers work and move this below
  // |resourceless_software_draw_| check. Tracked in crbug.com/264967.
  if (active_tree_->LayerListIsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no root layer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (resourceless_software_draw_)
    return true;

  // Do not draw while evicted. Await the activation of a tree containing a
  // newer viz::Surface
  if (base::FeatureList::IsEnabled(features::kEvictionThrottlesDraw) &&
      evicted_local_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostImpl::CanDraw viz::Surface evicted and not recreated",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (active_tree_->GetDeviceViewport().IsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw empty viewport",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (EvictedUIResourcesExist()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw UI resources evicted not recreated",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

void LayerTreeHostImpl::AnimatePendingTreeAfterCommit() {
  // Animate the pending tree layer animations to put them at initial positions
  // and starting state. There is no need to run other animations on pending
  // tree because they depend on user inputs so the state is identical to what
  // the active tree has.
  AnimateLayers(CurrentBeginFrameArgs().frame_time, /* is_active_tree */ false);
}

void LayerTreeHostImpl::Animate() {
  AnimateInternal();
}

void LayerTreeHostImpl::AnimateInternal() {
  DCHECK(task_runner_provider_->IsImplThread());
  base::TimeTicks monotonic_time = CurrentBeginFrameArgs().frame_time;

  // mithro(TODO): Enable these checks.
  // DCHECK(!current_begin_frame_tracker_.HasFinished());
  // DCHECK(monotonic_time == current_begin_frame_tracker_.Current().frame_time)
  //  << "Called animate with unknown frame time!?";

  bool did_animate = false;

  // TODO(bokan): This should return did_animate, see TODO in
  // ElasticOverscrollController::Animate. crbug.com/551138.
  if (input_delegate_)
    input_delegate_->TickAnimations(monotonic_time);

  did_animate |= AnimatePageScale(monotonic_time);
  did_animate |= AnimateLayers(monotonic_time, /* is_active_tree */ true);
  did_animate |= AnimateScrollbars(monotonic_time);
  did_animate |= AnimateBrowserControls(monotonic_time);

  if (did_animate) {
    // Animating stuff can change the root scroll offset, so inform the
    // synchronous input handler.
    if (input_delegate_)
      input_delegate_->RootLayerStateMayHaveChanged();

    // If the tree changed, then we want to draw at the end of the current
    // frame.
    SetNeedsRedraw();
  }
}

bool LayerTreeHostImpl::PrepareTiles() {
  DCHECK(!settings_.is_display_tree);

  tile_priorities_dirty_ |= active_tree() && active_tree()->UpdateTiles();
  tile_priorities_dirty_ |= pending_tree() && pending_tree()->UpdateTiles();

  if (!tile_priorities_dirty_)
    return false;

  bool did_prepare_tiles = tile_manager_.PrepareTiles(global_tile_state_);
  if (did_prepare_tiles)
    tile_priorities_dirty_ = false;
  client_->DidPrepareTiles();
  return did_prepare_tiles;
}

void LayerTreeHostImpl::StartPageScaleAnimation(const gfx::Point& target_offset,
                                                bool anchor_point,
                                                float page_scale,
                                                base::TimeDelta duration) {
  if (!InnerViewportScrollNode())
    return;

  gfx::PointF scroll_total = active_tree_->TotalScrollOffset();
  gfx::SizeF scrollable_size = active_tree_->ScrollableSize();
  gfx::SizeF viewport_size(
      active_tree_->InnerViewportScrollNode()->container_bounds);

  if (viewport_size.IsEmpty()) {
    // Avoid divide by zero. Besides nothing should see the animation anyway.
    return;
  }

  // TODO(miletus) : Pass in ScrollOffset.
  page_scale_animation_ = PageScaleAnimation::Create(
      scroll_total, active_tree_->current_page_scale_factor(), viewport_size,
      scrollable_size);

  if (anchor_point) {
    gfx::PointF anchor(target_offset);
    page_scale_animation_->ZoomWithAnchor(anchor, page_scale,
                                          duration.InSecondsF());
  } else {
    gfx::PointF scaled_target_offset(target_offset);
    page_scale_animation_->ZoomTo(scaled_target_offset, page_scale,
                                  duration.InSecondsF());
  }

  SetNeedsOneBeginImplFrame();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetNeedsAnimateInput() {
  SetNeedsOneBeginImplFrame();
}

std::unique_ptr<LatencyInfoSwapPromiseMonitor>
LayerTreeHostImpl::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return std::make_unique<LatencyInfoSwapPromiseMonitor>(latency, this);
}

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
LayerTreeHostImpl::GetScopedEventMetricsMonitor(
    EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) {
  return events_metrics_manager_.GetScopedMonitor(std::move(done_callback));
}

void LayerTreeHostImpl::NotifyInputEvent() {
  frame_rate_estimator_.NotifyInputEvent();
}

void LayerTreeHostImpl::QueueSwapPromiseForMainThreadScrollUpdate(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promises_for_main_thread_scroll_update_.push_back(
      std::move(swap_promise));
}

void LayerTreeHostImpl::FrameData::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_no_damage", has_no_damage);

  // Quad data can be quite large, so only dump render passes if we are
  // logging verbosely or viz.quads tracing category is enabled.
  bool quads_enabled = VerboseLogEnabled();
  if (!quads_enabled) {
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                       &quads_enabled);
  }
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (const auto& render_pass : render_passes) {
      value->BeginDictionary();
      render_pass->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

std::string LayerTreeHostImpl::FrameData::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

DrawMode LayerTreeHostImpl::GetDrawMode() const {
  if (resourceless_software_draw_) {
    return DRAW_MODE_RESOURCELESS_SOFTWARE;
  } else if (layer_tree_frame_sink_->context_provider() ||
             settings_.is_display_tree) {
    return DRAW_MODE_HARDWARE;
  } else {
    return DRAW_MODE_SOFTWARE;
  }
}

static void AppendQuadsToFillScreen(
    viz::CompositorRenderPass* target_render_pass,
    const RenderSurfaceImpl* root_render_surface,
    SkColor4f screen_background_color,
    const Region& fill_region) {
  if (!root_render_surface || !screen_background_color.fA)
    return;
  if (fill_region.IsEmpty())
    return;

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  const gfx::Rect root_target_rect = root_render_surface->content_rect();
  viz::SharedQuadState* shared_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      gfx::Transform(), root_target_rect, root_target_rect,
      gfx::MaskFilterInfo(), std::nullopt, screen_background_color.isOpaque(),
      /*opacity_f=*/1.f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  for (gfx::Rect screen_space_rect : fill_region) {
    gfx::Rect visible_screen_space_rect = screen_space_rect;
    // Skip the quad culler and just append the quads directly to avoid
    // occlusion checks.
    auto* quad =
        target_render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, screen_space_rect,
                 visible_screen_space_rect, screen_background_color, false);
  }
}

static viz::CompositorRenderPass* FindRenderPassById(
    const viz::CompositorRenderPassList& list,
    viz::CompositorRenderPassId id) {
  auto it = base::ranges::find(list, id, &viz::CompositorRenderPass::id);
  return it == list.end() ? nullptr : it->get();
}

bool LayerTreeHostImpl::HasDamage() const {
  DCHECK(!active_tree()->needs_update_draw_properties());
  DCHECK(CanDraw());

  // When touch handle visibility changes there is no visible damage
  // because touch handles are composited in the browser. However we
  // still want the browser to be notified that the handles changed
  // through the |ViewHostMsg_SwapCompositorFrame| IPC so we keep
  // track of handle visibility changes here.
  if (active_tree()->HandleVisibilityChanged())
    return true;

  if (!viewport_damage_rect_.IsEmpty())
    return true;

  // If the set of referenced surfaces has changed then we must submit a new
  // CompositorFrame to update surface references.
  if (last_draw_referenced_surfaces_ != active_tree()->SurfaceRanges())
    return true;

  // If we have a new LocalSurfaceId, we must always submit a CompositorFrame
  // because the parent is blocking on us.
  if (last_draw_local_surface_id_ !=
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId()) {
    return true;
  }

  const LayerTreeImpl* active_tree = active_tree_.get();
  // Make sure we propagate the primary main item sequence number. If there is
  // no stored sequence number, we don't need to damage: either damage will
  // happen anyway, or we're not generating metadata entries.
  if (last_draw_render_frame_metadata_ &&
      last_draw_render_frame_metadata_
              ->primary_main_frame_item_sequence_number !=
          active_tree->primary_main_frame_item_sequence_number()) {
    return true;
  }

  // If the root render surface has no visible damage, then don't generate a
  // frame at all.
  const RenderSurfaceImpl* root_surface = active_tree->RootRenderSurface();
  bool root_surface_has_visible_damage =
      root_surface->GetDamageRect().Intersects(root_surface->content_rect());
  bool hud_wants_to_draw_ = active_tree->hud_layer() &&
                            active_tree->hud_layer()->IsAnimatingHUDContents();

  return root_surface_has_visible_damage ||
         active_tree_->property_trees()->effect_tree().HasCopyRequests() ||
         hud_wants_to_draw_ || active_tree_->HasViewTransitionRequests();
}

DrawResult LayerTreeHostImpl::CalculateRenderPasses(FrameData* frame) {
  DCHECK(frame->render_passes.empty());
  DCHECK(CanDraw());
  DCHECK(!active_tree_->LayerListIsEmpty());

  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.
  DamageTracker::UpdateDamageTracking(active_tree_.get());
  frame->damage_reasons =
      active_tree_->RootRenderSurface()->damage_tracker()->GetDamageReasons();

  if (HasDamage()) {
    consecutive_frame_with_damage_count_++;
  } else {
    TRACE_EVENT0("cc",
                 "LayerTreeHostImpl::CalculateRenderPasses::EmptyDamageRect");
    frame->has_no_damage = true;
    DCHECK(!resourceless_software_draw_);
    consecutive_frame_with_damage_count_ = 0;
    return DrawResult::kSuccess;
  }

  TRACE_EVENT_BEGIN2("cc,benchmark", "LayerTreeHostImpl::CalculateRenderPasses",
                     "render_surface_list.size()",
                     static_cast<uint64_t>(frame->render_surface_list->size()),
                     "RequiresHighResToDraw", RequiresHighResToDraw());

  // HandleVisibilityChanged contributed to the above damage check, so reset it
  // now that we're going to draw.
  // TODO(jamwalla): only call this if we are sure the frame draws. Tracked in
  // crbug.com/805673.
  active_tree_->ResetHandleVisibilityChanged();

  base::flat_set<viz::ViewTransitionElementResourceId> known_resource_ids;
  // Create the render passes in dependency order.
  size_t render_surface_list_size = frame->render_surface_list->size();
  for (size_t i = 0; i < render_surface_list_size; ++i) {
    const size_t surface_index = render_surface_list_size - 1 - i;
    RenderSurfaceImpl* render_surface =
        (*frame->render_surface_list)[surface_index];

    const bool is_root_surface =
        render_surface->EffectTreeIndex() == kContentsRootPropertyNodeId;
    const bool should_draw_into_render_pass =
        is_root_surface || render_surface->contributes_to_drawn_surface() ||
        render_surface->CopyOfOutputRequired();
    if (should_draw_into_render_pass)
      frame->render_passes.push_back(render_surface->CreateRenderPass());
    if (render_surface->OwningEffectNode()
            ->view_transition_element_resource_id.IsValid()) {
      known_resource_ids.insert(render_surface->OwningEffectNode()
                                    ->view_transition_element_resource_id);
    }
  }

  frame->has_view_transition_save_directive =
      active_tree_->HasViewTransitionSaveRequest();

  // When we are displaying the HUD, change the root damage rect to cover the
  // entire root surface. This will disable partial-swap/scissor optimizations
  // that would prevent the HUD from updating, since the HUD does not cause
  // damage itself, to prevent it from messing with damage visualizations. Since
  // damage visualizations are done off the LayerImpls and RenderSurfaceImpls,
  // changing the RenderPass does not affect them.
  if (active_tree_->hud_layer()) {
    viz::CompositorRenderPass* root_pass = frame->render_passes.back().get();
    root_pass->damage_rect = root_pass->output_rect;
  }

  // Grab this region here before iterating layers. Taking copy requests from
  // the layers while constructing the render passes will dirty the render
  // surface layer list and this unoccluded region, flipping the dirty bit to
  // true, and making us able to query for it without doing
  // UpdateDrawProperties again. The value inside the Region is not actually
  // changed until UpdateDrawProperties happens, so a reference to it is safe.
  const Region& unoccluded_screen_space_region =
      active_tree_->UnoccludedScreenSpaceRegion();

  // Typically when we are missing a texture and use a checkerboard quad, we
  // still draw the frame. However when the layer being checkerboarded is moving
  // due to an impl-animation, we drop the frame to avoid flashing due to the
  // texture suddenly appearing in the future.
  DrawResult draw_result = DrawResult::kSuccess;

  const DrawMode draw_mode = GetDrawMode();

  int num_missing_tiles = 0;
  bool has_incompletely_rastered_tiles = false;
  bool has_incompletely_recorded_tiles = false;

  bool have_copy_request =
      active_tree()->property_trees()->effect_tree().HasCopyRequests();
  bool have_missing_animated_tiles = false;
  int num_of_layers_with_videos = 0;

  const bool compute_video_layer_preferred_interval =
      !features::UseSurfaceLayerForVideo() &&
      features::IsUsingFrameIntervalDecider();

  if (settings_.enable_compositing_based_throttling)
    throttle_decider_.Prepare();
  for (EffectTreeLayerListIterator it(active_tree());
       it.state() != EffectTreeLayerListIterator::State::kEnd; ++it) {
    auto target_render_pass_id = it.target_render_surface()->render_pass_id();
    viz::CompositorRenderPass* target_render_pass =
        FindRenderPassById(frame->render_passes, target_render_pass_id);

    AppendQuadsData append_quads_data;

    if (it.state() == EffectTreeLayerListIterator::State::kTargetSurface) {
      RenderSurfaceImpl* render_surface = it.target_render_surface();
      if (render_surface->HasCopyRequest()) {
        active_tree()
            ->property_trees()
            ->effect_tree_mutable()
            .TakeCopyRequestsAndTransformToSurface(
                render_surface->EffectTreeIndex(),
                &target_render_pass->copy_requests);
      }
      if (settings_.enable_compositing_based_throttling && target_render_pass)
        throttle_decider_.ProcessRenderPass(*target_render_pass);
    } else if (it.state() ==
               EffectTreeLayerListIterator::State::kContributingSurface) {
      RenderSurfaceImpl* render_surface = it.current_render_surface();
      if (render_surface->contributes_to_drawn_surface()) {
        render_surface->AppendQuads(draw_mode, target_render_pass,
                                    &append_quads_data);
      }
    } else if (it.state() == EffectTreeLayerListIterator::State::kLayer) {
      LayerImpl* layer = it.current_layer();
      if (layer->WillDraw(draw_mode, resource_provider_.get())) {
        DCHECK_EQ(active_tree_.get(), layer->layer_tree_impl());

        frame->will_draw_layers.push_back(layer);
        if (layer->may_contain_video()) {
          num_of_layers_with_videos++;
          frame->may_contain_video = true;
        }
        if (compute_video_layer_preferred_interval &&
            layer->GetLayerType() == mojom::LayerType::kVideo) {
          VideoLayerImpl* video_layer = static_cast<VideoLayerImpl*>(layer);
          std::optional<base::TimeDelta> video_preferred_interval =
              video_layer->GetPreferredRenderInterval();
          if (video_preferred_interval) {
            frame->video_layer_preferred_intervals[video_preferred_interval
                                                       .value()]++;
          }
        }
        layer->NotifyKnownResourceIdsBeforeAppendQuads(known_resource_ids);
        layer->AppendQuads(target_render_pass, &append_quads_data);
      } else {
        if (settings_.enable_compositing_based_throttling)
          throttle_decider_.ProcessLayerNotToDraw(layer);
      }

      rendering_stats_instrumentation_->AddVisibleContentArea(
          append_quads_data.visible_layer_area);
      rendering_stats_instrumentation_->AddApproximatedVisibleContentArea(
          append_quads_data.approximated_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedVisibleContentArea(
          append_quads_data.checkerboarded_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNeedsRecordContentArea(
          append_quads_data.checkerboarded_needs_record_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNeedsRasterContentArea(
          append_quads_data.checkerboarded_needs_raster_content_area);

      num_missing_tiles += append_quads_data.num_missing_tiles;
      has_incompletely_rastered_tiles |=
          append_quads_data.num_incompletely_rastered_tiles > 0;
      has_incompletely_recorded_tiles |=
          append_quads_data.num_incompletely_recorded_tiles > 0;

      if (append_quads_data.num_missing_tiles > 0) {
        have_missing_animated_tiles |=
            layer->screen_space_transform_is_animating();
      }
    }
    frame->activation_dependencies.insert(
        frame->activation_dependencies.end(),
        append_quads_data.activation_dependencies.begin(),
        append_quads_data.activation_dependencies.end());
    if (append_quads_data.deadline_in_frames) {
      if (!frame->deadline_in_frames) {
        frame->deadline_in_frames = append_quads_data.deadline_in_frames;
      } else {
        frame->deadline_in_frames = std::max(
            *frame->deadline_in_frames, *append_quads_data.deadline_in_frames);
      }
    }
    frame->use_default_lower_bound_deadline |=
        append_quads_data.use_default_lower_bound_deadline;
    frame->has_shared_element_resources |=
        append_quads_data.has_shared_element_resources;
  }

  // If CommitsToActiveTree() is true, then we wait to draw until
  // NotifyReadyToDraw. That means we're in as good shape as is possible now,
  // so there's no reason to stop the draw now (and this is not supported by
  // SingleThreadProxy).
  if (have_missing_animated_tiles && !CommitsToActiveTree()) {
    draw_result = DrawResult::kAbortedCheckerboardAnimations;
  }

  // When we require high res to draw, abort the draw (almost) always. This does
  // not cause the scheduler to do a main frame, instead it will continue to try
  // drawing until we finally complete, so the copy request will not be lost.
  // TODO(weiliangc): Remove RequiresHighResToDraw. crbug.com/469175
  if (has_incompletely_rastered_tiles || num_missing_tiles) {
    if (RequiresHighResToDraw())
      draw_result = DrawResult::kAbortedMissingHighResContent;
  }

  // Only enable frame rate estimation if it would help lower the composition
  // rate for videos.
  const bool assumes_video_conference_mode = num_of_layers_with_videos > 1;
  frame_rate_estimator_.SetVideoConferenceMode(assumes_video_conference_mode);

  // When doing a resourceless software draw, we don't have control over the
  // surface the compositor draws to, so even though the frame may not be
  // complete, the previous frame has already been potentially lost, so an
  // incomplete frame is better than nothing, so this takes highest precidence.
  if (resourceless_software_draw_)
    draw_result = DrawResult::kSuccess;

#if DCHECK_IS_ON()
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list)
      DCHECK(quad->shared_quad_state);
  }
  DCHECK_EQ(frame->render_passes.back()->output_rect.origin(),
            active_tree_->GetDeviceViewport().origin());
#endif
  bool has_transparent_background =
      !active_tree_->background_color().isOpaque();
  auto* root_render_surface = active_tree_->RootRenderSurface();
  if (root_render_surface && !has_transparent_background) {
    frame->render_passes.back()->has_transparent_background = false;

    // If any tiles are missing, then fill behind the entire root render
    // surface.  This is a workaround for this edge case, instead of tracking
    // individual tiles that are missing.
    Region fill_region = unoccluded_screen_space_region;
    if (num_missing_tiles > 0)
      fill_region = root_render_surface->content_rect();

    AppendQuadsToFillScreen(frame->render_passes.back().get(),
                            root_render_surface,
                            active_tree_->background_color(), fill_region);
  }

  RemoveRenderPasses(frame);
  // If we're making a frame to draw, it better have at least one render pass.
  DCHECK(!frame->render_passes.empty());

  if (have_copy_request) {
    // Any copy requests left in the tree are not going to get serviced, and
    // should be aborted.
    active_tree()->property_trees()->effect_tree_mutable().ClearCopyRequests();

    // Draw properties depend on copy requests.
    active_tree()->set_needs_update_draw_properties();
  }

  frame->checkerboarded_needs_raster =
      num_missing_tiles > 0 || has_incompletely_rastered_tiles;
  frame->checkerboarded_needs_record = has_incompletely_recorded_tiles;

  TRACE_EVENT_END2("cc,benchmark", "LayerTreeHostImpl::CalculateRenderPasses",
                   "draw_result", draw_result, "missing tiles",
                   num_missing_tiles);

  // Draw has to be successful to not drop the copy request layer.
  // When we have a copy request for a layer, we need to draw even if there
  // would be animating checkerboards, because failing under those conditions
  // triggers a new main frame, which may cause the copy request layer to be
  // destroyed.
  // TODO(weiliangc): Test copy request w/ LayerTreeFrameSink recreation. Would
  // trigger this DCHECK.
  DCHECK(!have_copy_request || draw_result == DrawResult::kSuccess);

  // TODO(crbug.com/40447355): This workaround to prevent creating unnecessarily
  // persistent render passes. When a copy request is made, it may force a
  // separate render pass for the layer, which will persist until a new commit
  // removes it. Force a commit after copy requests, to remove extra render
  // passes.
  if (have_copy_request)
    client_->SetNeedsCommitOnImplThread();

  return draw_result;
}

void LayerTreeHostImpl::DidAnimateScrollOffset() {
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetViewportDamage(const gfx::Rect& damage_rect) {
  viewport_damage_rect_.Union(damage_rect);
}

void LayerTreeHostImpl::InvalidateContentOnImplSide() {
  DCHECK(!pending_tree_ && !settings_.is_display_tree);
  // Invalidation should never be ran outside the impl frame for non
  // synchronous compositor mode. For devices that use synchronous compositor,
  // e.g. Android Webview, the assertion is not guaranteed because it may ask
  // for a frame at any time.
  DCHECK(impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME ||
         settings_.using_synchronous_renderer_compositor);

  if (!CommitsToActiveTree()) {
    CreatePendingTree();
    AnimatePendingTreeAfterCommit();
  }

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
}

void LayerTreeHostImpl::InvalidateLayerTreeFrameSink(bool needs_redraw) {
  DCHECK(layer_tree_frame_sink());

  layer_tree_frame_sink()->Invalidate(needs_redraw);
}

DrawResult LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  DCHECK(!use_layer_context_for_display_);

  TRACE_EVENT1("cc", "LayerTreeHostImpl::PrepareToDraw", "SourceFrameNumber",
               active_tree_->source_frame_number());
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(CurrentBeginFrameArgs().trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_GENERATE_RENDER_PASS);
        data->set_display_trace_id(CurrentBeginFrameArgs().trace_id);
      });
  if (input_delegate_)
    input_delegate_->WillDraw();

  // No need to record metrics each time we draw, 1% is enough.
  constexpr double kSamplingFrequency = .01;
  if (!downsample_metrics_ ||
      metrics_subsampler_.ShouldSample(kSamplingFrequency)) {
    // These metrics are only for the renderer process.
    if (RunningOnRendererProcess()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Compositing.Renderer.NumActiveLayers",
          base::saturated_cast<int>(active_tree_->NumLayers()), 1, 1000, 20);

      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Compositing.Renderer.NumActivePictureLayers",
          base::saturated_cast<int>(active_tree_->picture_layers().size()), 1,
          1000, 20);
    }
  }

  // Tick worklet animations here, just before draw, to give animation worklets
  // as much time as possible to produce their output for this frame. Note that
  // an animation worklet is asked to produce its output at the beginning of the
  // frame along side other animations but its output arrives asynchronously so
  // we tick worklet animations and apply that output here instead.
  mutator_host_->TickWorkletAnimations();

  bool ok = active_tree_->UpdateDrawProperties(
      /*update_tiles=*/true, /*update_image_animation_controller=*/true);
  DCHECK(ok) << "UpdateDrawProperties failed during draw";

  if (!settings_.is_display_tree) {
    // This will cause NotifyTileStateChanged() to be called for any tiles that
    // completed, which will add damage for visible tiles to the frame for them
    // so they appear as part of the current frame being drawn.
    tile_manager_.PrepareToDraw();
  }

  frame->render_surface_list = &active_tree_->GetRenderSurfaceList();
  frame->render_passes.clear();
  frame->will_draw_layers.clear();
  frame->has_no_damage = false;
  frame->may_contain_video = false;

  if (active_tree_->RootRenderSurface()) {
    active_tree_->RootRenderSurface()->damage_tracker()->AddDamageNextUpdate(
        viewport_damage_rect_);
    viewport_damage_rect_ = gfx::Rect();
  }

  DrawResult draw_result = CalculateRenderPasses(frame);

  // Dump render passes and draw quads if VerboseLogEnabled().
  VERBOSE_LOG() << "Prepare to draw\n" << frame->ToString();

  if (draw_result != DrawResult::kSuccess) {
    DCHECK(!resourceless_software_draw_);
    return draw_result;
  }

  // If we return DrawResult::kSuccess, then we expect DrawLayers() to be called
  // before this function is called again.
  return DrawResult::kSuccess;
}

void LayerTreeHostImpl::RemoveRenderPasses(FrameData* frame) {
  // There is always at least a root RenderPass.
  DCHECK_GE(frame->render_passes.size(), 1u);

  // A set of RenderPasses that we have seen.
  base::flat_set<viz::CompositorRenderPassId> pass_exists;
  // A set of viz::RenderPassDrawQuads that we have seen (stored by the
  // RenderPasses they refer to).
  base::flat_map<viz::CompositorRenderPassId, int> pass_references;
  // A set of viz::SharedElementDrawQuad references that we have seen.
  base::flat_set<viz::ViewTransitionElementResourceId>
      view_transition_quad_references;

  // Iterate RenderPasses in draw order, removing empty render passes (except
  // the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size(); ++i) {
    viz::CompositorRenderPass* pass = frame->render_passes[i].get();

    // Remove orphan viz::RenderPassDrawQuads.
    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end();) {
      if (it->material == viz::DrawQuad::Material::kSharedElement) {
        view_transition_quad_references.insert(
            viz::SharedElementDrawQuad::MaterialCast(*it)->resource_id);
        ++it;
        continue;
      }

      if (it->material != viz::DrawQuad::Material::kCompositorRenderPass) {
        ++it;
        continue;
      }
      const viz::CompositorRenderPassDrawQuad* quad =
          viz::CompositorRenderPassDrawQuad::MaterialCast(*it);
      // If the RenderPass doesn't exist, we can remove the quad.
      if (pass_exists.count(quad->render_pass_id)) {
        // Otherwise, save a reference to the RenderPass so we know there's a
        // quad using it.
        pass_references[quad->render_pass_id]++;
        ++it;
      } else {
        it = pass->quad_list.EraseAndInvalidateAllPointers(it);
      }
    }

    if (i == frame->render_passes.size() - 1) {
      // Don't remove the root RenderPass.
      break;
    }

    if (pass->quad_list.empty() && pass->copy_requests.empty() &&
        !pass->subtree_capture_id.is_valid() && pass->filters.IsEmpty() &&
        pass->backdrop_filters.IsEmpty() &&
        // TODO(khushalsagar) : Send information about no-op passes to viz to
        // retain this optimization for shared elements. See crbug.com/1265178.
        !pass->view_transition_element_resource_id.IsValid()) {
      // Remove the pass and decrement |i| to counter the for loop's increment,
      // so we don't skip the next pass in the loop.
      frame->render_passes.erase(frame->render_passes.begin() + i);
      --i;
      continue;
    }

    pass_exists.insert(pass->id);
  }

  // Remove RenderPasses that are not referenced by any draw quads or copy
  // requests (except the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size() - 1; ++i) {
    // Iterating from the back of the list to the front, skipping over the
    // back-most (root) pass, in order to remove each qualified RenderPass, and
    // drop references to earlier RenderPasses allowing them to be removed to.
    viz::CompositorRenderPass* pass =
        frame->render_passes[frame->render_passes.size() - 2 - i].get();

    if (!pass->copy_requests.empty()) {
      continue;
    }

    if (pass_references[pass->id])
      continue;

    // Retain render passes generating ViewTransition snapshots if they are
    // referenced by a quad or this frame will process a save directive. We need
    // to render and screenshot offscreen content as well, which won't be
    // referenced by a quad.
    if (pass->view_transition_element_resource_id.IsValid() &&
        (frame->has_view_transition_save_directive ||
         view_transition_quad_references.contains(
             pass->view_transition_element_resource_id))) {
      continue;
    }

    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end(); ++it) {
      if (const viz::CompositorRenderPassDrawQuad* quad =
              it->DynamicCast<viz::CompositorRenderPassDrawQuad>()) {
        pass_references[quad->render_pass_id]--;
      }
    }

    frame->render_passes.erase(frame->render_passes.end() - 2 - i);
    --i;
  }
}

void LayerTreeHostImpl::EvictTexturesForTesting() {
  UpdateTileManagerMemoryPolicy(ManagedMemoryPolicy(0));
}

void LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting(
    bool block,
    bool notify_if_blocked) {
  NOTREACHED();
}

void LayerTreeHostImpl::BlockImplSideInvalidationRequestsForTesting(
    bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::ResetTreesForTesting() {
  if (active_tree_)
    active_tree_->DetachLayers();
  active_tree_ = std::make_unique<LayerTreeImpl>(
      *this, active_tree()->page_scale_factor(),
      active_tree()->top_controls_shown_ratio(),
      active_tree()->bottom_controls_shown_ratio(),
      active_tree()->elastic_overscroll());
  active_tree_->property_trees()->set_is_active(true);
  active_tree_->property_trees()->clear();
  if (pending_tree_)
    pending_tree_->DetachLayers();
  pending_tree_ = nullptr;
  if (recycle_tree_)
    recycle_tree_->DetachLayers();
  recycle_tree_ = nullptr;
}

size_t LayerTreeHostImpl::SourceAnimationFrameNumberForTesting() const {
  return *next_frame_token_;
}

void LayerTreeHostImpl::UpdateTileManagerMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (!resource_pool_)
    return;

  global_tile_state_.hard_memory_limit_in_bytes = 0;
  global_tile_state_.soft_memory_limit_in_bytes = 0;
  if (visible_ && policy.bytes_limit_when_visible > 0) {
    global_tile_state_.hard_memory_limit_in_bytes =
        policy.bytes_limit_when_visible;
    global_tile_state_.soft_memory_limit_in_bytes =
        (static_cast<int64_t>(global_tile_state_.hard_memory_limit_in_bytes) *
         settings_.max_memory_for_prepaint_percentage) /
        100;
  }
  global_tile_state_.memory_limit_policy =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          visible_ ? policy.priority_cutoff_when_visible
                   : gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  global_tile_state_.num_resources_limit = policy.num_resources_limit;

  if (global_tile_state_.hard_memory_limit_in_bytes > 0) {
    // If |global_tile_state_.hard_memory_limit_in_bytes| is greater than 0, we
    // consider our contexts visible. Notify the contexts here. We handle
    // becoming invisible in NotifyAllTileTasksComplete to avoid interrupting
    // running work.
    SetContextVisibility(true);

    // If |global_tile_state_.hard_memory_limit_in_bytes| is greater than 0, we
    // allow the image decode controller to retain resources. We handle the
    // equal to 0 case in NotifyAllTileTasksComplete to avoid interrupting
    // running work.
    if (image_decode_cache_holder_)
      image_decode_cache_holder_->SetShouldAggressivelyFreeResources(false);
  } else {
    // When the memory policy is set to zero, its important to release any
    // decoded images cached by the tracker. But we can not re-checker any
    // images that have been displayed since the resources, if held by the
    // browser, may be re-used. Which is why its important to maintain the
    // decode policy tracking.
    bool can_clear_decode_policy_tracking = false;
    tile_manager_.ClearCheckerImageTracking(can_clear_decode_policy_tracking);
  }

  DCHECK(resource_pool_);
  // Soft limit is used for resource pool such that memory returns to soft
  // limit after going over.
  resource_pool_->SetResourceUsageLimits(
      global_tile_state_.soft_memory_limit_in_bytes,
      global_tile_state_.num_resources_limit);

  DidModifyTilePriorities(/*pending_update_tiles=*/false);
}

void LayerTreeHostImpl::DidModifyTilePriorities(bool pending_update_tiles) {
  if (settings_.is_display_tree) {
    return;
  }

  // Mark priorities as (maybe) dirty and schedule a PrepareTiles().
  if (!pending_update_tiles) {
    tile_priorities_dirty_ = true;
    tile_manager_.DidModifyTilePriorities();
  }

  client_->SetNeedsPrepareTilesOnImplThread();
}

void LayerTreeHostImpl::SetTargetLocalSurfaceId(
    const viz::LocalSurfaceId& target_local_surface_id) {
  target_local_surface_id_ = target_local_surface_id;
}

std::unique_ptr<RasterTilePriorityQueue> LayerTreeHostImpl::BuildRasterQueue(
    TreePriority tree_priority,
    RasterTilePriorityQueue::Type type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildRasterQueue");

  return RasterTilePriorityQueue::Create(
      active_tree_->picture_layers(),
      pending_tree_ && pending_tree_fully_painted_
          ? pending_tree_->picture_layers()
          : std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>(),
      tree_priority, type);
}

std::unique_ptr<EvictionTilePriorityQueue>
LayerTreeHostImpl::BuildEvictionQueue(TreePriority tree_priority) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildEvictionQueue");

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      new EvictionTilePriorityQueue);
  queue->Build(
      active_tree_->picture_layers(),
      pending_tree_
          ? pending_tree_->picture_layers()
          : std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>(),
      tree_priority);
  return queue;
}

std::unique_ptr<TilesWithResourceIterator>
LayerTreeHostImpl::CreateTilesWithResourceIterator() {
  return std::make_unique<TilesWithResourceIterator>(
      &active_tree_->picture_layers(),
      pending_tree_ ? &pending_tree_->picture_layers() : nullptr);
}

gfx::DisplayColorSpaces LayerTreeHostImpl::GetDisplayColorSpaces() const {
  // The pending tree will has the most recently updated color space, so use it.
  if (pending_tree_) {
    return pending_tree_->display_color_spaces();
  }

  if (active_tree_) {
    return active_tree_->display_color_spaces();
  }
  return gfx::DisplayColorSpaces();
}

void LayerTreeHostImpl::SetIsLikelyToRequireADraw(
    bool is_likely_to_require_a_draw) {
  // Proactively tell the scheduler that we expect to draw within each vsync
  // until we get all the tiles ready to draw. If we happen to miss a required
  // for draw tile here, then we will miss telling the scheduler each frame that
  // we intend to draw so it may make worse scheduling decisions.
  is_likely_to_require_a_draw_ = is_likely_to_require_a_draw;
}

TargetColorParams LayerTreeHostImpl::GetTargetColorParams(
    gfx::ContentColorUsage content_color_usage) const {
  TargetColorParams params;

  // If we are likely to software composite the resource, we use sRGB because
  // software compositing is unable to perform color conversion.
  if (!layer_tree_frame_sink_ || !layer_tree_frame_sink_->context_provider())
    return params;

  gfx::DisplayColorSpaces display_cs = GetDisplayColorSpaces();
  params.sdr_max_luminance_nits = display_cs.GetSDRMaxLuminanceNits();

  if (settings_.prefer_raster_in_srgb &&
      content_color_usage == gfx::ContentColorUsage::kSRGB) {
    return params;
  }

  auto hdr_color_space =
      display_cs.GetOutputColorSpace(gfx::ContentColorUsage::kHDR,
                                     /*needs_alpha=*/false);

  // Always specify a color space if color correct rasterization is requested
  // (not specifying a color space indicates that no color conversion is
  // required).
  if (!hdr_color_space.IsValid())
    return params;

  if (hdr_color_space.IsHDR()) {
    if (content_color_usage == gfx::ContentColorUsage::kHDR) {
      // Rasterization of HDR content is always done in extended-sRGB space.
      params.color_space = gfx::ColorSpace::CreateExtendedSRGB();

      // Only report the HDR capabilities if they are requested.
      params.hdr_max_luminance_relative =
          display_cs.GetHDRMaxLuminanceRelative();
    } else {
      // If the content is not HDR, then use Display P3 as the rasterization
      // color space.
      params.color_space = gfx::ColorSpace::CreateDisplayP3D65();
    }
    return params;
  }

  params.color_space = hdr_color_space;
  return params;
}

bool LayerTreeHostImpl::CheckColorSpaceContainsSrgb(
    const gfx::ColorSpace& color_space) const {
  constexpr gfx::ColorSpace srgb = gfx::ColorSpace::CreateSRGB();

  // Color spaces without a custom primary matrix are cheap to compute, so the
  // cache can be bypassed.
  if (color_space.GetPrimaryID() != gfx::ColorSpace::PrimaryID::CUSTOM)
    return color_space.Contains(srgb);

  auto it = contains_srgb_cache_.Get(color_space);
  if (it != contains_srgb_cache_.end())
    return it->second;

  bool result = color_space.Contains(srgb);
  contains_srgb_cache_.Put(color_space, result);
  return result;
}

void LayerTreeHostImpl::RequestImplSideInvalidationForCheckerImagedTiles() {
  // When using impl-side invalidation for checker-imaging, a pending tree does
  // not need to be flushed as an independent update through the pipeline.
  bool needs_first_draw_on_activation = false;
  client_->SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
}

size_t LayerTreeHostImpl::GetFrameIndexForImage(const PaintImage& paint_image,
                                                WhichTree tree) const {
  if (!paint_image.ShouldAnimate())
    return PaintImage::kDefaultFrameIndex;

  return image_animation_controller_.GetFrameIndexForImage(
      paint_image.stable_id(), tree);
}

int LayerTreeHostImpl::GetMSAASampleCountForRaster(
    const DisplayItemList& display_list) const {
  if (display_list.num_slow_paths_up_to_min_for_MSAA() <
      kMinNumberOfSlowPathsForMSAA) {
    return 0;
  }
  if (!raster_caps().can_use_msaa) {
    return 0;
  }

  if (display_list.has_non_aa_paint()) {
    return 0;
  }

  return RequestedMSAASampleCount();
}

bool LayerTreeHostImpl::HasPendingTree() {
  return pending_tree_ != nullptr;
}

void LayerTreeHostImpl::NotifyReadyToActivate() {
  // The TileManager may call this method while the pending tree is still being
  // painted, as it isn't aware of the ongoing paint. We shouldn't tell the
  // scheduler we are ready to activate in that case, as if we do it will
  // immediately activate once we call NotifyPaintWorkletStateChange, rather
  // than wait for the TileManager to actually raster the content!
  if (!pending_tree_fully_painted_)
    return;
  client_->NotifyReadyToActivate();
}

void LayerTreeHostImpl::NotifyReadyToDraw() {
  // Tiles that are ready will cause NotifyTileStateChanged() to be called so we
  // don't need to schedule a draw here. Just stop WillBeginImplFrame() from
  // causing optimistic requests to draw a frame.
  is_likely_to_require_a_draw_ = false;

  client_->NotifyReadyToDraw();
}

void LayerTreeHostImpl::NotifyAllTileTasksCompleted() {
  DCHECK(!settings_.is_display_tree);

  // The tile tasks started by the most recent call to PrepareTiles have
  // completed. Now is a good time to free resources if necessary.
  if (global_tile_state_.hard_memory_limit_in_bytes == 0) {
    // Free image decode controller resources before notifying the
    // contexts of visibility change. This ensures that the imaged decode
    // controller has released all Skia refs at the time Skia's cleanup
    // executes (within worker context's cleanup).
    if (image_decode_cache_holder_)
      image_decode_cache_holder_->SetShouldAggressivelyFreeResources(true);
    SetContextVisibility(false);
  }
}

void LayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  DCHECK(!settings_.is_display_tree);

  TRACE_EVENT0("cc", "LayerTreeHostImpl::NotifyTileStateChanged");

  LayerImpl* layer_impl = nullptr;

  // We must have a pending or active tree layer here, since the layer is
  // guaranteed to outlive its tiles.
  const bool is_pending_tree =
      tile->tiling()->tree() == WhichTree::PENDING_TREE;
  if (is_pending_tree) {
    layer_impl = pending_tree_->FindPendingTreeLayerById(tile->layer_id());
  } else {
    layer_impl = active_tree_->FindActiveTreeLayerById(tile->layer_id());
  }

  layer_impl->NotifyTileStateChanged(tile);

  if (settings_.UseLayerContextForDisplay() && !is_pending_tree) {
    // Pending tree tile updates are pushed to the display tree after
    // activation. For active tree tile updates we push immediately.
    layer_context_->UpdateDisplayTile(
        static_cast<PictureLayerImpl&>(*layer_impl), *tile,
        *resource_provider(), *layer_tree_frame_sink_->context_provider());
  }

  if (!client_->IsInsideDraw() && tile->required_for_draw()) {
    // The LayerImpl::NotifyTileStateChanged() should damage the layer, so this
    // redraw will make those tiles be displayed.
    SetNeedsRedrawOrUpdateDisplayTree();
  }
}

void LayerTreeHostImpl::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  DCHECK(task_runner_provider_->IsImplThread());

  SetMemoryPolicyImpl(policy);

  // This is short term solution to synchronously drop tile resources when
  // using synchronous compositing to avoid memory usage regression.
  // TODO(boliu): crbug.com/499004 to track removing this.
  if (!policy.bytes_limit_when_visible && resource_pool_ &&
      settings_.using_synchronous_renderer_compositor) {
    ReleaseTileResources();
    CleanUpTileManagerResources();

    // Force a call to NotifyAllTileTasks completed - otherwise this logic may
    // be skipped if no work was enqueued at the time the tile manager was
    // destroyed.
    NotifyAllTileTasksCompleted();

    CreateTileManagerResources();
    RecreateTileResources();
  }
}

void LayerTreeHostImpl::SetTreeActivationCallback(
    base::RepeatingClosure callback) {
  DCHECK(task_runner_provider_->IsImplThread());
  tree_activation_callback_ = std::move(callback);
}

void LayerTreeHostImpl::SetMemoryPolicyImpl(const ManagedMemoryPolicy& policy) {
  if (cached_managed_memory_policy_ == policy)
    return;

  ManagedMemoryPolicy old_policy = ActualManagedMemoryPolicy();
  cached_managed_memory_policy_ = policy;
  ManagedMemoryPolicy actual_policy = ActualManagedMemoryPolicy();

  if (old_policy == actual_policy)
    return;

  UpdateTileManagerMemoryPolicy(actual_policy);

  // If there is already enough memory to draw everything imaginable and the
  // new memory limit does not change this, then do not re-commit. Don't bother
  // skipping commits if this is not visible (commits don't happen when not
  // visible, there will almost always be a commit when this becomes visible).
  bool needs_commit = true;
  if (visible() &&
      actual_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      old_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      actual_policy.priority_cutoff_when_visible ==
          old_policy.priority_cutoff_when_visible) {
    needs_commit = false;
  }

  if (needs_commit)
    client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {
  const bool tile_priority_params_changed =
      viewport_rect_for_tile_priority_ != viewport_rect;
  viewport_rect_for_tile_priority_ = viewport_rect;

  if (tile_priority_params_changed) {
    active_tree_->set_needs_update_draw_properties();
    if (pending_tree_)
      pending_tree_->set_needs_update_draw_properties();

    // Compositor, not LayerTreeFrameSink, is responsible for setting damage
    // and triggering redraw for constraint changes.
    SetFullViewportDamage();
    SetNeedsRedrawOrUpdateDisplayTree();
  }
}

void LayerTreeHostImpl::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAckOnImplThread();
}

void LayerTreeHostImpl::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  PresentationTimeCallbackBuffer::PendingCallbacks activated_callbacks =
      presentation_time_callbacks_.PopPendingCallbacks(
          frame_token, details.presentation_feedback.failed());

  // Send all tasks to the client so that it can decide which tasks
  // should run on which thread.
  client_->DidPresentCompositorFrameOnImplThread(
      frame_token, std::move(activated_callbacks), details);

  // Send all pending lag events waiting on the frame pointed by |frame_token|.
  // It is posted as a task because LayerTreeHostImpl::DidPresentCompositorFrame
  // is in the rendering critical path (it is called by AsyncLayerTreeFrameSink
  // ::OnBeginFrame).
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LayerTreeHostImpl::LogAverageLagEvents,
                     weak_factory_.GetWeakPtr(), frame_token, details));
}

void LayerTreeHostImpl::LogAverageLagEvents(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  lag_tracking_manager_.DidPresentCompositorFrame(frame_token, details);
}

void LayerTreeHostImpl::NotifyThroughputTrackerResults(
    const CustomTrackerResults& results) {
  client_->NotifyThroughputTrackerResults(results);
}

void LayerTreeHostImpl::DidNotNeedBeginFrame() {
  frame_trackers_.NotifyPauseFrameProduction();
  if (lcd_text_metrics_reporter_)
    lcd_text_metrics_reporter_->NotifyPauseFrameProduction();
}

void LayerTreeHostImpl::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  resource_provider_->ReceiveReturnsFromParent(std::move(resources));

  // In OOM, we now might be able to release more resources that were held
  // because they were exported.
  if (resource_pool_) {
    resource_pool_->ReduceResourceUsage();
  }

  // If we're not visible, we likely released resources, so we want to
  // aggressively flush here to make sure those DeleteSharedImage() calls make
  // it to the GPU process to free up the memory.
  MaybeFlushPendingWork();

  if (base::FeatureList::IsEnabled(
          features::kReclaimResourcesDelayedFlushInBackground)) {
    // There are cases where the release callbacks executed from the call above
    // don't actually free the GPU resource from this thread. For instance, for
    // TextureLayer,
    // TextureLayer::TransferableResourceHolder::~TransferableResourceHolder()
    // posts a task to the main thread, and so flushing here is not sufficient.
    //
    // Ideally, we would not rely on a time-based delay, but given layering,
    // threading and possibly unknown cases where the release can jump from
    // thread to thread, this is likely a more practical solution. See
    // crbug.com/1449271 for an example.
    GetTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeHostImpl::MaybeFlushPendingWork,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(1));
  }
}

void LayerTreeHostImpl::MaybeFlushPendingWork() {
  // If we're not in background, delayed work will be flushed "at some point",
  // and we also may have something better to do.
  if (visible_ || !has_valid_layer_tree_frame_sink_ ||
      !base::FeatureList::IsEnabled(
          features::kReclaimResourcesFlushInBackground)) {
    return;
  }

  auto* compositor_context = layer_tree_frame_sink_->context_provider();
  if (!compositor_context || !compositor_context->ContextSupport()) {
    return;
  }
  compositor_context->ContextSupport()->FlushPendingWork();
}

void LayerTreeHostImpl::OnDraw(const gfx::Transform& transform,
                               const gfx::Rect& viewport,
                               bool resourceless_software_draw,
                               bool skip_draw) {
  DCHECK(!resourceless_software_draw_);
  // This function is only ever called by Android WebView, in which case we
  // expect the device viewport to be at the origin. We never expect an
  // external viewport to be set otherwise.
  DCHECK(active_tree_->internal_device_viewport().origin().IsOrigin());

#if DCHECK_IS_ON()
  base::AutoReset<bool> reset_sync_draw(&doing_sync_draw_, true);
#endif

  if (skip_draw) {
    client_->OnDrawForLayerTreeFrameSink(resourceless_software_draw_, true);
    return;
  }

  const bool transform_changed = external_transform_ != transform;
  const bool viewport_changed = external_viewport_ != viewport;

  external_transform_ = transform;
  external_viewport_ = viewport;

  {
    base::AutoReset<bool> resourceless_software_draw_reset(
        &resourceless_software_draw_, resourceless_software_draw);

    // For resourceless software draw, always set full damage to ensure they
    // always swap. Otherwise, need to set redraw for any changes to draw
    // parameters.
    if (transform_changed || viewport_changed || resourceless_software_draw_) {
      SetFullViewportDamage();
      SetNeedsRedraw();
      active_tree_->set_needs_update_draw_properties();
    }

    if (resourceless_software_draw)
      client_->OnCanDrawStateChanged(CanDraw());

    client_->OnDrawForLayerTreeFrameSink(resourceless_software_draw_,
                                         skip_draw);
  }

  if (resourceless_software_draw) {
    active_tree_->set_needs_update_draw_properties();
    client_->OnCanDrawStateChanged(CanDraw());
    // This draw may have reset all damage, which would lead to subsequent
    // incorrect hardware draw, so explicitly set damage for next hardware
    // draw as well.
    SetFullViewportDamage();
  }
}

void LayerTreeHostImpl::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t sequence_id) {
  client_->NotifyTransitionRequestFinished(sequence_id);
}

void LayerTreeHostImpl::OnSurfaceEvicted(
    const viz::LocalSurfaceId& local_surface_id) {
  // Don't evict if the host has given us a newer viz::SurfaceId. Instead handle
  // resource returns as normal, and begin producing from the new tree.
  if (target_local_surface_id_.IsNewerThanOrEmbeddingChanged(
          local_surface_id)) {
    return;
  }
  evicted_local_surface_id_ = local_surface_id;
  resource_provider_->SetEvicted(true);
  client_->OnCanDrawStateChanged(CanDraw());
}

void LayerTreeHostImpl::ReportEventLatency(
    std::vector<EventLatencyTracker::LatencyData> latencies) {
  if (auto* recorder = CustomMetricRecorder::Get())
    recorder->ReportEventLatency(std::move(latencies));
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

viz::RegionCaptureBounds LayerTreeHostImpl::CollectRegionCaptureBounds() {
  viz::RegionCaptureBounds bounds;
  for (const auto* layer : base::Reversed(*active_tree())) {
    if (!layer->capture_bounds())
      continue;

    for (const auto& bounds_pair : layer->capture_bounds()->bounds()) {
      // Perform transformation from the coordinate system of this |layer|
      // to that of the root render surface.
      gfx::Rect bounds_in_screen_space = MathUtil::ProjectEnclosingClippedRect(
          layer->ScreenSpaceTransform(), bounds_pair.second);

      const RenderSurfaceImpl* root_surface =
          active_tree()->RootRenderSurface();
      const gfx::Rect content_rect_in_screen_space = gfx::ToEnclosedRect(
          MathUtil::MapClippedRect(root_surface->screen_space_transform(),
                                   root_surface->DrawableContentRect()));

      // The transformed bounds may be partially or entirely offscreen.
      bounds_in_screen_space.Intersect(content_rect_in_screen_space);
      bounds.Set(bounds_pair.first, bounds_in_screen_space);
    }
  }
  return bounds;
}

viz::CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() {
  viz::CompositorFrameMetadata metadata;
  metadata.frame_token = ++next_frame_token_;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();

  metadata.root_background_color = active_tree_->background_color();
  metadata.may_throttle_if_undrawn_frames = may_throttle_if_undrawn_frames_;

  presentation_time_callbacks_.RegisterMainThreadCallbacks(
      metadata.frame_token, active_tree_->TakePresentationCallbacks());
  presentation_time_callbacks_.RegisterMainThreadSuccessfulCallbacks(
      metadata.frame_token,
      active_tree_->TakeSuccessfulPresentationCallbacks());

  if (input_delegate_) {
    metadata.is_handling_interaction =
        GetActivelyScrollingType() != ActivelyScrollingType::kNone ||
        input_delegate_->IsHandlingTouchSequence();
  }

  const base::flat_set<viz::SurfaceRange>& referenced_surfaces =
      active_tree_->SurfaceRanges();
  for (auto& surface_range : referenced_surfaces)
    metadata.referenced_surfaces.push_back(surface_range);

  if (last_draw_referenced_surfaces_ != referenced_surfaces)
    last_draw_referenced_surfaces_ = referenced_surfaces;

  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();

  if (browser_controls_offset_manager_->TopControlsHeight() > 0) {
    float visible_height =
        browser_controls_offset_manager_->TopControlsHeight() *
        browser_controls_offset_manager_->TopControlsShownRatio();
    metadata.top_controls_visible_height.emplace(visible_height);

#if BUILDFLAG(IS_ANDROID)
    if (features::IsBrowserControlsInVizEnabled()) {
      const viz::OffsetTag& tag =
          browser_controls_offset_manager_->TopControlsOffsetTag();
      if (tag) {
        float offset = browser_controls_offset_manager_->TopControlsHeight() -
                       visible_height;
        // ViewAndroid::OnTopControlsChanged() also rounds the offset before
        // handing it off to Android.
        gfx::Vector2dF offset2d(0.0f, -std::round(offset));
        metadata.offset_tag_values.emplace_back(tag, offset2d);
      }
    }
#endif
  }

  if (InnerViewportScrollNode()) {
    // TODO(miletus) : Change the metadata to hold ScrollOffset.
    metadata.root_scroll_offset = active_tree_->TotalScrollOffset();
  }

  metadata.display_transform_hint = active_tree_->display_transform_hint();

  if (const gfx::DelegatedInkMetadata* delegated_ink_metadata_ptr =
          active_tree_->delegated_ink_metadata()) {
    std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata =
        std::make_unique<gfx::DelegatedInkMetadata>(
            *delegated_ink_metadata_ptr);
    delegated_ink_metadata->set_frame_time(CurrentBeginFrameArgs().frame_time);
    TRACE_EVENT_WITH_FLOW1(
        "delegated_ink_trails",
        "Delegated Ink Metadata set on compositor frame metadata",
        TRACE_ID_GLOBAL(delegated_ink_metadata->trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "metadata",
        delegated_ink_metadata->ToString());
    metadata.delegated_ink_metadata = std::move(delegated_ink_metadata);
  }

  metadata.capture_bounds = CollectRegionCaptureBounds();

  if (!screenshot_destination_.is_empty()) {
    metadata.screenshot_destination =
        blink::SameDocNavigationScreenshotDestinationToken(
            screenshot_destination_);
    screenshot_destination_ = base::UnguessableToken();
  }

  metadata.is_software = !layer_tree_frame_sink_->context_provider();

  return metadata;
}

RenderFrameMetadata LayerTreeHostImpl::MakeRenderFrameMetadata(
    FrameData* frame) {
  RenderFrameMetadata metadata;
  metadata.root_scroll_offset = active_tree_->TotalScrollOffset();

  metadata.root_background_color = active_tree_->background_color();
  metadata.is_scroll_offset_at_top = active_tree_->TotalScrollOffset().y() == 0;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();
  active_tree_->GetViewportSelection(&metadata.selection);
  metadata.is_mobile_optimized = IsMobileOptimized(active_tree_.get());
  metadata.viewport_size_in_pixels = active_tree_->GetDeviceViewport().size();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.external_page_scale_factor =
      active_tree_->external_page_scale_factor();

  metadata.top_controls_height =
      browser_controls_offset_manager_->TopControlsHeight();
  metadata.top_controls_shown_ratio =
      browser_controls_offset_manager_->TopControlsShownRatio();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  metadata.bottom_controls_height =
      browser_controls_offset_manager_->BottomControlsHeight();
  metadata.bottom_controls_shown_ratio =
      browser_controls_offset_manager_->BottomControlsShownRatio();
  metadata.top_controls_min_height_offset =
      browser_controls_offset_manager_->TopControlsMinHeightOffset();
  metadata.bottom_controls_min_height_offset =
      browser_controls_offset_manager_->BottomControlsMinHeightOffset();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  if (InnerViewportScrollNode()) {
    DCHECK(OuterViewportScrollNode());
    metadata.root_overflow_y_hidden =
        !OuterViewportScrollNode()->user_scrollable_vertical ||
        !InnerViewportScrollNode()->user_scrollable_vertical;
  }
  metadata.has_transparent_background =
      frame->render_passes.back()->has_transparent_background;
#endif

  bool allocate_new_local_surface_id = false;

  if (last_draw_render_frame_metadata_) {
    const float last_root_scroll_offset_y =
        last_draw_render_frame_metadata_->root_scroll_offset
            .value_or(gfx::PointF())
            .y();

    const float new_root_scroll_offset_y =
        metadata.root_scroll_offset.value().y();

    if (!MathUtil::IsWithinEpsilon(last_root_scroll_offset_y,
                                   new_root_scroll_offset_y)) {
      viz::VerticalScrollDirection new_vertical_scroll_direction =
          (last_root_scroll_offset_y < new_root_scroll_offset_y)
              ? viz::VerticalScrollDirection::kDown
              : viz::VerticalScrollDirection::kUp;

      // Changes in vertical scroll direction happen instantaneously. This being
      // the case, a new vertical scroll direction should only be present in the
      // singular metadata for the render frame in which the direction change
      // occurred. If the vertical scroll direction detected here matches that
      // which we've previously cached, then this frame is not the instant in
      // which the direction change occurred and is therefore not propagated.
      if (last_vertical_scroll_direction_ != new_vertical_scroll_direction)
        metadata.new_vertical_scroll_direction = new_vertical_scroll_direction;
    }

    allocate_new_local_surface_id =
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
        last_draw_render_frame_metadata_->top_controls_height !=
            metadata.top_controls_height ||
        last_draw_render_frame_metadata_->top_controls_shown_ratio !=
            metadata.top_controls_shown_ratio;
#elif BUILDFLAG(IS_ANDROID)
        last_draw_render_frame_metadata_->top_controls_height !=
            metadata.top_controls_height ||
        last_draw_render_frame_metadata_->bottom_controls_height !=
            metadata.bottom_controls_height ||
        last_draw_render_frame_metadata_->selection != metadata.selection ||
        last_draw_render_frame_metadata_->has_transparent_background !=
            metadata.has_transparent_background;

    if (!features::IsBrowserControlsInVizEnabled()) {
      allocate_new_local_surface_id |=
          last_draw_render_frame_metadata_->top_controls_shown_ratio !=
              metadata.top_controls_shown_ratio ||
          last_draw_render_frame_metadata_->bottom_controls_shown_ratio !=
              metadata.bottom_controls_shown_ratio;
    } else {
      // When AndroidBrowserControlsInViz is enabled, don't always use
      // bottom_controls_shown_ratio to determine if surface sync is needed,
      // because it changes even when there are no bottom controls.
      bool bottom_controls_exist =
          metadata.bottom_controls_height != 0 ||
          last_draw_render_frame_metadata_->bottom_controls_height != 0;
      allocate_new_local_surface_id |=
          bottom_controls_exist &&
          last_draw_render_frame_metadata_->bottom_controls_shown_ratio !=
              metadata.bottom_controls_shown_ratio;
    }
#else
        last_draw_render_frame_metadata_->top_controls_height !=
            metadata.top_controls_height ||
        last_draw_render_frame_metadata_->top_controls_shown_ratio !=
            metadata.top_controls_shown_ratio ||
        last_draw_render_frame_metadata_->bottom_controls_height !=
            metadata.bottom_controls_height ||
        last_draw_render_frame_metadata_->bottom_controls_shown_ratio !=
            metadata.bottom_controls_shown_ratio ||
        last_draw_render_frame_metadata_->selection != metadata.selection ||
        last_draw_render_frame_metadata_->has_transparent_background !=
            metadata.has_transparent_background;
#endif
  }

  if (child_local_surface_id_allocator_.GetCurrentLocalSurfaceId().is_valid()) {
    if (allocate_new_local_surface_id)
      AllocateLocalSurfaceId();
    metadata.local_surface_id =
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  }

  metadata.primary_main_frame_item_sequence_number =
      active_tree()->primary_main_frame_item_sequence_number();

  return metadata;
}

std::optional<SubmitInfo> LayerTreeHostImpl::DrawLayers(FrameData* frame) {
  DCHECK(!use_layer_context_for_display_);
  DCHECK(CanDraw());
  DCHECK_EQ(frame->has_no_damage, frame->render_passes.empty());
  ResetRequiresHighResToDraw();

  if (frame->has_no_damage) {
    DCHECK(!resourceless_software_draw_);
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoDamage", TRACE_EVENT_SCOPE_THREAD);
    active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);
    active_tree()->ResetAllChangeTracking();

    // Drop pending event metrics for UI when the frame has no damage because
    // it could leave the event metrics pending indefinitely and also breaks the
    // association between input events and screen updates.
    // See b/297940877.
    if (settings_.is_layer_tree_for_ui) {
      std::ignore = active_tree()->TakeEventsMetrics();
      std::ignore = events_metrics_manager_.TakeSavedEventsMetrics();
    }

    return std::nullopt;
  }

  layer_tree_frame_sink_->set_source_frame_number(
      active_tree_->source_frame_number());

  auto compositor_frame = GenerateCompositorFrame(frame);
  const auto frame_token = compositor_frame.metadata.frame_token;
  frame->frame_token = frame_token;
  bool top_controls_moved = false;
  float current_top_controls =
      compositor_frame.metadata.top_controls_visible_height.value_or(0.f);
  if (current_top_controls != top_controls_visible_height_) {
    top_controls_moved = true;
    top_controls_visible_height_ = current_top_controls;
  }
#if DCHECK_IS_ON()
  const viz::BeginFrameId begin_frame_ack_frame_id =
      compositor_frame.metadata.begin_frame_ack.frame_id;
#endif

  EventMetricsSet events_metrics(
      active_tree()->TakeEventsMetrics(),
      events_metrics_manager_.TakeSavedEventsMetrics());
  lag_tracking_manager_.CollectScrollEventsFromFrame(frame_token,
                                                     events_metrics);

  // Dump property trees and layers if VerboseLogEnabled().
  VERBOSE_LOG() << "Submitting a frame:\n"
                << viz::TransitionUtils::RenderPassListToString(
                       compositor_frame.render_pass_list);

  base::TimeTicks submit_time = base::TimeTicks::Now();
  layer_tree_frame_sink_->SubmitCompositorFrame(
      std::move(compositor_frame),
      /*hit_test_data_changed=*/false);

#if DCHECK_IS_ON()
  if (!doing_sync_draw_) {
    // The throughput computation (in |FrameSequenceTracker|) depends on the
    // compositor-frame submission to happen while a BeginFrameArgs is 'active'
    // (i.e. between calls to WillBeginImplFrame() and DidFinishImplFrame()).
    // Verify that this is the case.
    // No begin-frame is available when doing sync draws, so avoid doing this
    // check in that case.
    const auto& bfargs = current_begin_frame_tracker_.Current();
    DCHECK_EQ(bfargs.frame_id, begin_frame_ack_frame_id);
  }
#endif

  if (!mutator_host_->NextFrameHasPendingRAF())
    frame_trackers_.StopSequence(FrameSequenceTrackerType::kRAF);
  if (!mutator_host_->HasCanvasInvalidation())
    frame_trackers_.StopSequence(FrameSequenceTrackerType::kCanvasAnimation);
  if (!mutator_host_->NextFrameHasPendingRAF() &&
      !mutator_host_->HasJSAnimation())
    frame_trackers_.StopSequence(FrameSequenceTrackerType::kJSAnimation);

  if (mutator_host_->MainThreadAnimationsCount() == 0 &&
      !mutator_host_->HasSmilAnimation()) {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kMainThreadAnimation);
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kSETMainThreadAnimation);
  } else if (!mutator_host_->HasViewTransition()) {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kSETMainThreadAnimation);
  }

  if (lcd_text_metrics_reporter_) {
    lcd_text_metrics_reporter_->NotifySubmitFrame(
        frame->origin_begin_main_frame_args);
  }

  // Clears the list of swap promises after calling DidSwap on each of them to
  // signal that the swap is over.
  active_tree()->ClearSwapPromises();

  // The next frame should start by assuming nothing has changed, and changes
  // are noted as they occur.
  // TODO(boliu): If we did a temporary software renderer frame, propogate the
  // damage forward to the next frame.
  for (size_t i = 0; i < frame->render_surface_list->size(); i++) {
    auto* surface = (*frame->render_surface_list)[i];
    surface->damage_tracker()->DidDrawDamagedArea();
  }
  active_tree_->ResetAllChangeTracking();

  devtools_instrumentation::DidDrawFrame(
      id_, frame->begin_frame_ack.frame_id.sequence_number);
  benchmark_instrumentation::IssueImplThreadRenderingStatsEvent(
      rendering_stats_instrumentation_->TakeImplThreadRenderingStats());

  if (settings_.enable_compositing_based_throttling &&
      throttle_decider_.HasThrottlingChanged()) {
    client_->FrameSinksToThrottleUpdated(throttle_decider_.ids());
  }

  return SubmitInfo{frame_token,
                    submit_time,
                    frame->checkerboarded_needs_raster,
                    frame->checkerboarded_needs_record,
                    top_controls_moved,
                    std::move(events_metrics)};
}

viz::CompositorFrame LayerTreeHostImpl::GenerateCompositorFrame(
    FrameData* frame) {
  TRACE_EVENT_BEGIN(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(CurrentBeginFrameArgs().trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_GENERATE_COMPOSITOR_FRAME);
        data->set_display_trace_id(CurrentBeginFrameArgs().trace_id);
      });

  rendering_stats_instrumentation_->IncrementFrameCount(1);

  if (!settings_.is_display_tree) {
    memory_history_->SaveEntry(tile_manager_.memory_stats_from_last_assign());
  }

  if (debug_state_.ShowDebugRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree(), active_tree_->hud_layer(), *frame->render_surface_list,
        debug_state_);
  }

  TRACE_EVENT_INSTANT2("cc", "Scroll Delta This Frame",
                       TRACE_EVENT_SCOPE_THREAD, "x",
                       scroll_accumulated_this_frame_.x(), "y",
                       scroll_accumulated_this_frame_.y());
  scroll_accumulated_this_frame_ = gfx::Vector2dF();

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace) {
    if (pending_tree_) {
      for (auto* layer : *pending_tree_)
        layer->DidBeginTracing();
    }
    for (auto* layer : *active_tree_)
      layer->DidBeginTracing();
  }

  {
    TRACE_EVENT0("cc", "DrawLayers.FrameViewerTracing");
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        frame_viewer_instrumentation::CategoryLayerTree(),
        "cc::LayerTreeHostImpl", id_, AsValueWithFrame(frame));
  }

  const DrawMode draw_mode = GetDrawMode();

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer()) {
    TRACE_EVENT0("cc", "DrawLayers.UpdateHudTexture");
    active_tree_->hud_layer()->UpdateHudTexture(
        draw_mode, layer_tree_frame_sink_, resource_provider_.get(),
        raster_caps(), frame->render_passes);
  }

  viz::CompositorFrameMetadata metadata = MakeCompositorFrameMetadata();

  ViewTransitionRequest::ViewTransitionElementMap view_transition_element_map;
  for (RenderSurfaceImpl* render_surface : *frame->render_surface_list) {
    const auto& view_transition_element_resource_id =
        render_surface->OwningEffectNode()->view_transition_element_resource_id;
    if (!view_transition_element_resource_id.IsValid()) {
      continue;
    }

    DCHECK(!base::Contains(view_transition_element_map,
                           view_transition_element_resource_id))
        << "Cannot map " << view_transition_element_resource_id.ToString()
        << " to render pass "
        << render_surface->render_pass_id().GetUnsafeValue()
        << "; It already maps to render pass "
        << view_transition_element_map[view_transition_element_resource_id]
               .GetUnsafeValue();

    view_transition_element_map[view_transition_element_resource_id] =
        render_surface->render_pass_id();
  }

  auto display_color_spaces = GetDisplayColorSpaces();
  for (auto& request : active_tree_->TakeViewTransitionRequests()) {
    metadata.transition_directives.push_back(request->ConstructDirective(
        view_transition_element_map, display_color_spaces));
  }

  PopulateMetadataContentColorUsage(frame, &metadata);
  metadata.has_shared_element_resources = frame->has_shared_element_resources;
  metadata.may_contain_video = frame->may_contain_video;
  metadata.deadline = viz::FrameDeadline(
      CurrentBeginFrameArgs().frame_time,
      frame->deadline_in_frames.value_or(0u), CurrentBeginFrameArgs().interval,
      frame->use_default_lower_bound_deadline);
  metadata.frame_interval_inputs.frame_time =
      CurrentBeginFrameArgs().frame_time;
  metadata.frame_interval_inputs.has_input =
      frame_rate_estimator_.input_priority_mode();

  if (!frame->video_layer_preferred_intervals.empty() &&
      frame->damage_reasons.Has(DamageReason::kVideoLayer)) {
    for (auto& [video_interval, count] :
         frame->video_layer_preferred_intervals) {
      metadata.frame_interval_inputs.content_interval_info.push_back(
          {viz::ContentFrameIntervalType::kVideo, video_interval, count - 1u});
    }
    frame->damage_reasons.Remove(DamageReason::kVideoLayer);
  }

  if (frame->damage_reasons.Has(DamageReason::kAnimatedImage)) {
    std::optional<ImageAnimationController::ConsistentFrameDuration>
        animating_image_duration =
            image_animation_controller_.GetConsistentContentFrameDuration();
    if (animating_image_duration) {
      metadata.frame_interval_inputs.content_interval_info.push_back(
          {viz::ContentFrameIntervalType::kAnimatingImage,
           animating_image_duration->frame_duration,
           animating_image_duration->num_images - 1u});
      frame->damage_reasons.Remove(DamageReason::kAnimatedImage);
    }
  }

  if (frame->damage_reasons.Has(DamageReason::kScrollbarFadeOutAnimation)) {
    // Lower fade out animation to 20hz somewhat arbitrarily since it's small
    // and hard to notice a low frame rate.
    metadata.frame_interval_inputs.content_interval_info.push_back(
        {viz::ContentFrameIntervalType::kScrollBarFadeOutAnimation,
         base::Hertz(20)});
    frame->damage_reasons.Remove(DamageReason::kScrollbarFadeOutAnimation);
  }

  // If all RedrawReasons have been recorded in `content_interval_info` and
  // removed, then can set `has_only_content_frame_interval_updates`.
  metadata.frame_interval_inputs.has_only_content_frame_interval_updates =
      frame->damage_reasons.empty();

  base::TimeDelta preferred_frame_interval;
  constexpr auto kFudgeDelta = base::Milliseconds(1);
  constexpr auto kTwiceOfDefaultInterval =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  constexpr auto kMinDelta = kTwiceOfDefaultInterval - kFudgeDelta;
  if (mutator_host_->MainThreadAnimationsCount() == 0 &&
      !mutator_host_->HasSmilAnimation() &&
      mutator_host_->NeedsTickAnimations() &&
      !frame_rate_estimator_.input_priority_mode() &&
      mutator_host_->MinimumTickInterval() > kMinDelta) {
    // All animations are impl-thread animations that tick at no more than
    // half the default display compositing fps.
    // Here and below with FrameRateEstimator::GetPreferredInterval(), the
    // meta data's preferred_frame_interval is constrainted to either 0 or
    // twice the default interval. The reason is because GPU process side
    // viz::FrameRateDecider is optimized for when all the preferred frame
    // rates are similar.
    // In general it may cause an animation to be less smooth if its fps is
    // less than 30 fps and it updates at 30 fps. However, the frame rate
    // reduction optimization is only applied when a webpage has two or more
    // videos, i.e., very likely a video conferencing scene. It doesn't apply
    // to general webpages.
    preferred_frame_interval = kTwiceOfDefaultInterval;
  } else {
    // There are main-thread, high frequency impl-thread animations, or input
    // events.
    frame_rate_estimator_.WillDraw(CurrentBeginFrameArgs().frame_time);
    preferred_frame_interval = frame_rate_estimator_.GetPreferredInterval();
  }

  metadata.activation_dependencies = std::move(frame->activation_dependencies);
  active_tree()->FinishSwapPromises(&metadata);
  // The swap-promises should not change the frame-token.
  DCHECK_EQ(metadata.frame_token, *next_frame_token_);

  if (render_frame_metadata_observer_) {
    last_draw_render_frame_metadata_ = MakeRenderFrameMetadata(frame);
    if (gfx::DelegatedInkMetadata* ink_metadata =
            metadata.delegated_ink_metadata.get()) {
      last_draw_render_frame_metadata_->delegated_ink_metadata =
          DelegatedInkBrowserMetadata(ink_metadata->is_hovering());
    }

    // We cache the value of any new vertical scroll direction so that we can
    // accurately determine when the next change in vertical scroll direction
    // occurs. Note that |kNull| is only used to indicate the absence of a
    // vertical scroll direction and should therefore be ignored.
    if (last_draw_render_frame_metadata_->new_vertical_scroll_direction !=
        viz::VerticalScrollDirection::kNull) {
      last_vertical_scroll_direction_ =
          last_draw_render_frame_metadata_->new_vertical_scroll_direction;
    }

    render_frame_metadata_observer_->OnRenderFrameSubmission(
        *last_draw_render_frame_metadata_, &metadata,
        active_tree()->TakeForceSendMetadataRequest());
  }

  if (!CommitsToActiveTree() && !metadata.latency_info.empty()) {
    base::TimeTicks draw_time = base::TimeTicks::Now();

    ApplyFirstScrollTracking(metadata.latency_info.front(),
                             metadata.frame_token);
    for (auto& latency : metadata.latency_info) {
      latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, draw_time);
    }
  }

  // Collect all resource ids in the render passes into a single array.
  std::vector<viz::ResourceId> resources;
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list) {
      for (viz::ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }

  DCHECK(frame->begin_frame_ack.frame_id.IsSequenceValid());
  metadata.begin_frame_ack = frame->begin_frame_ack;
  metadata.begin_frame_ack.preferred_frame_interval = preferred_frame_interval;

  viz::CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  if (!settings_.is_display_tree) {
    resource_provider_->PrepareSendToParent(
        resources, &compositor_frame.resource_list,
        layer_tree_frame_sink_->context_provider());
  }
  compositor_frame.render_pass_list = std::move(frame->render_passes);

  // We should always have a valid LocalSurfaceId in LayerTreeImpl unless we
  // don't have a scheduler because without a scheduler commits are not deferred
  // and LayerTrees without valid LocalSurfaceId might slip through, but
  // single-thread-without-scheduler mode is only used in tests so it doesn't
  // matter.
  CHECK(!settings_.single_thread_proxy_scheduler ||
        active_tree()->local_surface_id_from_parent().is_valid());

  if (settings_.is_display_tree) {
    UpdateChildLocalSurfaceId();
  }
  layer_tree_frame_sink_->SetLocalSurfaceId(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId());

  last_draw_local_surface_id_ =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();

  if (const char* client_name = GetClientNameForMetrics()) {
    size_t total_quad_count = 0;
    for (const auto& pass : compositor_frame.render_pass_list) {
      total_quad_count += pass->quad_list.size();
    }
    UMA_HISTOGRAM_COUNTS_1000(
        base::StringPrintf("Compositing.%s.CompositorFrame.Quads", client_name),
        total_quad_count);
  }

  // TODO(b/368050735): future-proof this event against early returns.
  TRACE_EVENT_END("viz,benchmark,graphics.pipeline",
                  perfetto::Flow::Global(CurrentBeginFrameArgs().trace_id),
                  [&](perfetto::EventContext ctx) {
                    auto* event =
                        ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                    auto* data = event->set_chrome_graphics_pipeline();

                    for (const ui::LatencyInfo& latency :
                         compositor_frame.metadata.latency_info) {
                      data->add_latency_ids(latency.trace_id());
                    }
                  });

  return compositor_frame;
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  // TODO(lethalantidote): LayerImpl::DidDraw can be removed when
  // VideoLayerImpl is removed.
  for (LayerImpl* layer : frame.will_draw_layers) {
    layer->DidDraw(resource_provider_.get());
  }

  for (VideoFrameController* it : video_frame_controllers_) {
    it->DidDrawFrame();
  }
}

void LayerTreeHostImpl::UpdateDisplayTree(FrameData& frame) {
  DCHECK(use_layer_context_for_display_);
  DCHECK(layer_context_);

  if (!active_tree()->LayerListIsEmpty()) {
    bool ok = active_tree()->UpdateDrawProperties(
        /*update_tiles=*/true, /*update_image_animation_controller=*/true);
    DCHECK(ok) << "UpdateDrawProperties failed during display tree update";
  }

  tile_manager_.PrepareToDraw();
  layer_context_->UpdateDisplayTreeFrom(
      *active_tree(), *resource_provider(),
      *layer_tree_frame_sink_->context_provider());
  UpdateAnimationState(true);
  active_tree()->ResetAllChangeTracking();
}

int LayerTreeHostImpl::RequestedMSAASampleCount() const {
  if (settings_.gpu_rasterization_msaa_sample_count == -1) {
    // On "low-end" devices use 4 samples per pixel to save memory.
    if (base::SysInfo::IsLowEndDevice())
      return 4;

    // Use the most up-to-date version of device_scale_factor that we have.
    float device_scale_factor = pending_tree_
                                    ? pending_tree_->device_scale_factor()
                                    : active_tree_->device_scale_factor();

    // Note: this feature ensures that we correctly report the device scale
    // factor. As of June 2023, without this feature, the vast majority (or
    // possibly all?) high-dpi screens are incorrectly considered normal DPI
    // ones here. See the UMA histogram
    // "Gpu.Rasterization.Raster.MSAASampleCountLog2", which almost always
    // report "3", i.e. 8xMSAA on macOS, where High-DPI screens are prevalent.
    if (base::FeatureList::IsEnabled(features::kDetectHiDpiForMsaa)) {
      float painted_device_scale_factor =
          pending_tree_ ? pending_tree_->painted_device_scale_factor()
                        : active_tree_->painted_device_scale_factor();
      DCHECK(painted_device_scale_factor == 1 || device_scale_factor == 1);

      device_scale_factor *= painted_device_scale_factor;
    }

    return device_scale_factor >= 2.0f ? 4 : 8;
  }

  return settings_.gpu_rasterization_msaa_sample_count;
}

void LayerTreeHostImpl::UpdateRasterCapabilities() {
  CHECK(layer_tree_frame_sink_);

  raster_caps_ = RasterCapabilities();

  auto* context_provider = layer_tree_frame_sink_->context_provider();
  auto* worker_context_provider =
      layer_tree_frame_sink_->worker_context_provider();
  CHECK_EQ(!!worker_context_provider, !!context_provider);

  if (!worker_context_provider) {
    // No context provider means software raster + compositing.
    raster_caps_.max_texture_size = settings_.max_render_buffer_bounds_for_sw;

    // Software compositing always uses the native skia RGBA N32 format, but we
    // just call it RGBA_8888 everywhere even though it can be BGRA ordering,
    // because we don't need to communicate the actual ordering as the code all
    // assumes the native skia format.
    raster_caps_.tile_format = viz::SinglePlaneFormat::kRGBA_8888;
    raster_caps_.ui_rgba_format =
        layer_tree_frame_sink_->shared_image_interface()
            ? viz::SinglePlaneFormat::kBGRA_8888
            : viz::SinglePlaneFormat::kRGBA_8888;
    return;
  }

  viz::RasterContextProvider::ScopedRasterContextLock scoped_lock(
      worker_context_provider);
  const auto& context_caps = worker_context_provider->ContextCapabilities();
  const auto& shared_image_caps =
      worker_context_provider->SharedImageInterface()->GetCapabilities();

  raster_caps_.max_texture_size = context_caps.max_texture_size;
  raster_caps_.ui_rgba_format =
      viz::PlatformColor::BestSupportedTextureFormat(context_caps);

  raster_caps_.tile_overlay_candidate =
      settings_.use_gpu_memory_buffer_resources &&
      shared_image_caps.supports_scanout_shared_images;

  if (settings_.gpu_rasterization_disabled || !context_caps.gpu_rasterization) {
    // This is the GPU compositing but software rasterization path. Pick the
    // best format for GPU textures to be uploaded to.
    raster_caps_.tile_format =
        settings_.use_rgba_4444
            ? viz::SinglePlaneFormat::kRGBA_4444
            : viz::PlatformColor::BestSupportedTextureFormat(context_caps);
    return;
  }

  // GPU compositing + rasterization is enabled if we get this far.
  raster_caps_.use_gpu_rasterization = true;

  raster_caps_.can_use_msaa =
      !context_caps.msaa_is_slow && !context_caps.avoid_stencil_buffers;

  raster_caps_.tile_format =
      settings_.use_rgba_4444
          ? viz::SinglePlaneFormat::kRGBA_4444
          : viz::PlatformColor::BestSupportedRenderBufferFormat(context_caps);
}

ImageDecodeCache* LayerTreeHostImpl::GetImageDecodeCache() const {
  return image_decode_cache_holder_
             ? image_decode_cache_holder_->image_decode_cache()
             : nullptr;
}

void LayerTreeHostImpl::RegisterMainThreadPresentationTimeCallbackForTesting(
    uint32_t frame_token,
    PresentationTimeCallbackBuffer::Callback callback) {
  std::vector<PresentationTimeCallbackBuffer::Callback> as_vector;
  as_vector.push_back(std::move(callback));
  presentation_time_callbacks_.RegisterMainThreadCallbacks(
      frame_token, std::move(as_vector));
}

void LayerTreeHostImpl::
    RegisterMainThreadSuccessfulPresentationTimeCallbackForTesting(
        uint32_t frame_token,
        PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails
            callback) {
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
      as_vector;
  as_vector.push_back(std::move(callback));
  presentation_time_callbacks_.RegisterMainThreadSuccessfulCallbacks(
      frame_token, std::move(as_vector));
}

void LayerTreeHostImpl::
    RegisterCompositorThreadSuccessfulPresentationTimeCallbackForTesting(
        uint32_t frame_token,
        PresentationTimeCallbackBuffer::SuccessfulCallback callback) {
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallback> as_vector;
  as_vector.push_back(std::move(callback));
  presentation_time_callbacks_.RegisterCompositorThreadSuccessfulCallbacks(
      frame_token, std::move(as_vector));
}

bool LayerTreeHostImpl::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  if (!settings().single_thread_proxy_scheduler) {
    client_->SetWaitingForScrollEvent(input_delegate_ &&
                                      input_delegate_->IsCurrentlyScrolling() &&
                                      !input_delegate_->HasQueuedInput());
  }
  impl_thread_phase_ = ImplThreadPhase::INSIDE_IMPL_FRAME;
  current_begin_frame_tracker_.Start(args);
  frame_trackers_.NotifyBeginImplFrame(args);
  total_frame_counter_.OnBeginFrame(args);
  devtools_instrumentation::DidBeginFrame(id_, args.frame_time,
                                          args.frame_id.sequence_number);

  // When there is a |target_local_surface_id_|, we do not wish to begin
  // producing Impl Frames for an older viz::LocalSurfaceId, as it will never
  // be displayed.
  //
  // Once the Main thread has finished adjusting to the new visual properties,
  // it will push the updated viz::LocalSurfaceId. Begin Impl Frame production
  // if it has already become activated, or is on the |pending_tree| to be
  // activated during this frame's production.
  //
  // However when using a synchronous compositor we skip this throttling
  // completely.
  if (!settings_.using_synchronous_renderer_compositor) {
    const viz::LocalSurfaceId& upcoming_lsid =
        pending_tree() ? pending_tree()->local_surface_id_from_parent()
                       : active_tree()->local_surface_id_from_parent();
    if (target_local_surface_id_.IsNewerThan(upcoming_lsid)) {
      return false;
    }
  }

  if (is_likely_to_require_a_draw_) {
    // Optimistically schedule a draw. This will let us expect the tile manager
    // to complete its work so that we can draw new tiles within the impl frame
    // we are beginning now.
    SetNeedsRedraw();
  }

  if (input_delegate_)
    input_delegate_->WillBeginImplFrame(args);

  Animate();

  image_animation_controller_.WillBeginImplFrame(args);

  for (VideoFrameController* it : video_frame_controllers_) {
    it->OnBeginFrame(args);
  }

  bool recent_frame_had_no_damage =
      consecutive_frame_with_damage_count_ < settings_.damaged_frame_limit;
  // Check damage early if the setting is enabled and a recent frame had no
  // damage. HasDamage() expects CanDraw to be true. If we can't check damage,
  // return true to indicate that there might be damage in this frame.
  if (settings_.enable_early_damage_check && recent_frame_had_no_damage &&
      CanDraw()) {
    bool ok = active_tree()->UpdateDrawProperties(
        /*update_tiles=*/true, /*update_image_animation_controller=*/true);
    DCHECK(ok);
    DamageTracker::UpdateDamageTracking(active_tree_.get());
    bool has_damage = HasDamage();
    // Animations are updated after we attempt to draw. If the frame is aborted,
    // update animations now.
    if (!has_damage)
      UpdateAnimationState(true);
    return has_damage;
  }
  // Assume there is damage if we cannot check for damage.
  return true;
}

void LayerTreeHostImpl::DidFinishImplFrame(const viz::BeginFrameArgs& args) {
  frame_trackers_.NotifyFrameEnd(current_begin_frame_tracker_.Current(), args);
  impl_thread_phase_ = ImplThreadPhase::IDLE;
  current_begin_frame_tracker_.Finish();
  if (input_delegate_) {
    input_delegate_->DidFinishImplFrame();
  }
}

void LayerTreeHostImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                           FrameSkippedReason reason) {
  frame_rate_estimator_.DidNotProduceFrame();
  if (layer_tree_frame_sink_) {
    static const bool feature_allowed = base::FeatureList::IsEnabled(
        features::kThrottleFrameRateOnManyDidNotProduceFrame);
    if (feature_allowed) {
      viz::BeginFrameAck adjust_ack = ack;
      adjust_ack.preferred_frame_interval =
          frame_rate_estimator_.GetPreferredInterval();
      layer_tree_frame_sink_->DidNotProduceFrame(adjust_ack, reason);
    } else {
      layer_tree_frame_sink_->DidNotProduceFrame(ack, reason);
    }
  }
}

void LayerTreeHostImpl::OnBeginImplFrameDeadline() {
  if (!input_delegate_) {
    return;
  }
  input_delegate_->OnBeginImplFrameDeadline();
}

void LayerTreeHostImpl::SynchronouslyInitializeAllTiles() {
  // Only valid for the single-threaded non-scheduled/synchronous case
  // using the zero copy raster worker pool.
  single_thread_synchronous_task_graph_runner_->RunUntilIdle();
}

static uint32_t GetFlagsForSurfaceLayer(const SurfaceLayerImpl* layer) {
  uint32_t flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch;
  if (layer->range().IsValid()) {
    flags |= viz::HitTestRegionFlags::kHitTestChildSurface;
  } else {
    flags |= viz::HitTestRegionFlags::kHitTestMine;
  }
  return flags;
}

static void PopulateHitTestRegion(viz::HitTestRegion* hit_test_region,
                                  const LayerImpl* layer,
                                  uint32_t flags,
                                  uint32_t async_hit_test_reasons,
                                  const gfx::Rect& rect,
                                  const viz::SurfaceId& surface_id,
                                  float device_scale_factor) {
  hit_test_region->frame_sink_id = surface_id.frame_sink_id();
  hit_test_region->flags = flags;
  hit_test_region->async_hit_test_reasons = async_hit_test_reasons;
  DCHECK_EQ(!!async_hit_test_reasons,
            !!(flags & viz::HitTestRegionFlags::kHitTestAsk));

  hit_test_region->rect = rect;
  // The transform of hit test region maps a point from parent hit test region
  // to the local space. This is the inverse of screen space transform. Because
  // hit test query wants the point in target to be in Pixel space, we
  // counterscale the transform here. Note that the rect is scaled by dsf, so
  // the point and the rect are still in the same space.
  gfx::Transform surface_to_root_transform = layer->ScreenSpaceTransform();
  surface_to_root_transform.Scale(SK_Scalar1 / device_scale_factor,
                                  SK_Scalar1 / device_scale_factor);
  surface_to_root_transform.Flatten();
  // TODO(sunxd): Avoid losing precision by not using inverse if possible.
  // Note: |transform| is set to the identity if |surface_to_root_transform| is
  // not invertible, which is what we want.
  hit_test_region->transform = surface_to_root_transform.InverseOrIdentity();
}

std::optional<viz::HitTestRegionList> LayerTreeHostImpl::BuildHitTestData() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BuildHitTestData");

  std::optional<viz::HitTestRegionList> hit_test_region_list(std::in_place);
  hit_test_region_list->flags = viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds = active_tree_->GetDeviceViewport();
  hit_test_region_list->transform = DrawTransform();

  float device_scale_factor = active_tree()->device_scale_factor();

  Region overlapping_region;
  size_t num_iterated_layers = 0;
  // If the layer tree contains more than 100 layers, we stop accumulating
  // layers in |overlapping_region| to save compositor frame submitting time, as
  // a result we do async hit test on any surface layers that
  bool assume_overlap = false;
  for (const auto* layer : base::Reversed(*active_tree())) {
    if (layer->is_surface_layer()) {
      const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
      // We should not skip a non-hit-testable surface layer if
      // - it has pointer-events: none because viz hit test needs to know the
      //   information to ensure all descendant OOPIFs to ignore hit tests; or
      // - it draws content to track overlaps.
      if (!layer->HitTestable() && !layer->draws_content() &&
          !surface_layer->has_pointer_events_none()) {
        continue;
      }
      // If a surface layer is created not by child frame compositor or the
      // frame owner has pointer-events: none property, the surface layer
      // becomes not hit testable. We should not generate data for it.
      if (!surface_layer->surface_hit_testable() ||
          !surface_layer->range().IsValid()) {
        // We collect any overlapped regions that does not have pointer-events:
        // none.
        if (!surface_layer->has_pointer_events_none() && !assume_overlap) {
          overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
              layer->ScreenSpaceTransform(),
              gfx::Rect(surface_layer->bounds())));
        }
        continue;
      }

      gfx::Rect content_rect(gfx::ScaleToEnclosingRect(
          gfx::Rect(surface_layer->bounds()), device_scale_factor));

      gfx::Rect layer_screen_space_rect = MathUtil::MapEnclosingClippedRect(
          surface_layer->ScreenSpaceTransform(),
          gfx::Rect(surface_layer->bounds()));
      auto flag = GetFlagsForSurfaceLayer(surface_layer);
      uint32_t async_hit_test_reasons =
          viz::AsyncHitTestReasons::kNotAsyncHitTest;
      if (surface_layer->has_pointer_events_none())
        flag |= viz::HitTestRegionFlags::kHitTestIgnore;
      if (assume_overlap ||
          overlapping_region.Intersects(layer_screen_space_rect)) {
        flag |= viz::HitTestRegionFlags::kHitTestAsk;
        async_hit_test_reasons |= viz::AsyncHitTestReasons::kOverlappedRegion;
      }
      bool layer_hit_test_region_is_masked =
          active_tree()
              ->property_trees()
              ->effect_tree()
              .HitTestMayBeAffectedByMask(surface_layer->effect_tree_index());
      if (surface_layer->is_clipped() || layer_hit_test_region_is_masked) {
        bool layer_hit_test_region_is_rectangle =
            !layer_hit_test_region_is_masked &&
            surface_layer->ScreenSpaceTransform().Preserves2dAxisAlignment() &&
            active_tree()
                ->property_trees()
                ->effect_tree()
                .ClippedHitTestRegionIsRectangle(
                    surface_layer->effect_tree_index());
        content_rect =
            gfx::ScaleToEnclosingRect(surface_layer->visible_layer_rect(),
                                      device_scale_factor, device_scale_factor);
        if (!layer_hit_test_region_is_rectangle) {
          flag |= viz::HitTestRegionFlags::kHitTestAsk;
          async_hit_test_reasons |= viz::AsyncHitTestReasons::kIrregularClip;
        }
      }
      const auto& surface_id = surface_layer->range().end();
      hit_test_region_list->regions.emplace_back();
      PopulateHitTestRegion(&hit_test_region_list->regions.back(), layer, flag,
                            async_hit_test_reasons, content_rect, surface_id,
                            device_scale_factor);
      continue;
    }

    if (!layer->HitTestable()) {
      continue;
    }

    // TODO(sunxd): Submit all overlapping layer bounds as hit test regions.
    // Also investigate if we can use visible layer rect as overlapping regions.
    num_iterated_layers++;
    if (num_iterated_layers > kAssumeOverlapThreshold)
      assume_overlap = true;
    if (!assume_overlap) {
      overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
          layer->ScreenSpaceTransform(), gfx::Rect(layer->bounds())));
    }
  }

  return hit_test_region_list;
}

void LayerTreeHostImpl::DidLoseLayerTreeFrameSink() {
  // Check that we haven't already detected context loss because we get it via
  // two paths: compositor context loss on the compositor thread and worker
  // context loss posted from main thread to compositor thread. We do not want
  // to reset the context recovery state in the scheduler.
  if (!has_valid_layer_tree_frame_sink_)
    return;
  has_valid_layer_tree_frame_sink_ = false;
  client_->DidLoseLayerTreeFrameSinkOnImplThread();
  lag_tracking_manager_.Clear();

  dropped_frame_counter_.ResetPendingFrames(base::TimeTicks::Now());
}

bool LayerTreeHostImpl::OnlyExpandTopControlsAtPageTop() const {
  return active_tree_->only_expand_top_controls_at_page_top();
}

bool LayerTreeHostImpl::HaveRootScrollNode() const {
  return InnerViewportScrollNode();
}

void LayerTreeHostImpl::SetNeedsCommit() {
  client_->SetNeedsCommitOnImplThread();
}

ScrollNode* LayerTreeHostImpl::InnerViewportScrollNode() const {
  return active_tree_->InnerViewportScrollNode();
}

ScrollNode* LayerTreeHostImpl::OuterViewportScrollNode() const {
  return active_tree_->OuterViewportScrollNode();
}

ScrollNode* LayerTreeHostImpl::CurrentlyScrollingNode() {
  return active_tree()->CurrentlyScrollingNode();
}

const ScrollNode* LayerTreeHostImpl::CurrentlyScrollingNode() const {
  return active_tree()->CurrentlyScrollingNode();
}

bool LayerTreeHostImpl::IsPinchGestureActive() const {
  if (!input_delegate_)
    return false;
  return GetInputHandler().pinch_gesture_active();
}

ActivelyScrollingType LayerTreeHostImpl::GetActivelyScrollingType() const {
  if (!input_delegate_)
    return ActivelyScrollingType::kNone;
  return input_delegate_->GetActivelyScrollingType();
}

bool LayerTreeHostImpl::IsCurrentScrollMainRepainted() const {
  return input_delegate_ && input_delegate_->IsCurrentScrollMainRepainted();
}

bool LayerTreeHostImpl::ScrollAffectsScrollHandler() const {
  if (!input_delegate_)
    return false;
  return settings_.enable_synchronized_scrolling &&
         scroll_affects_scroll_handler_;
}

void LayerTreeHostImpl::SetExternalPinchGestureActive(bool active) {
  DCHECK(input_delegate_ || !active);
  if (input_delegate_)
    GetInputHandler().set_external_pinch_gesture_active(active);
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!CommitsToActiveTree());
  CHECK(!pending_tree_);
  if (recycle_tree_) {
    recycle_tree_.swap(pending_tree_);
  } else {
    pending_tree_ = std::make_unique<LayerTreeImpl>(
        *this, active_tree()->page_scale_factor(),
        active_tree()->top_controls_shown_ratio(),
        active_tree()->bottom_controls_shown_ratio(),
        active_tree()->elastic_overscroll());
  }
  pending_tree_fully_painted_ = false;

  client_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "cc", "PendingTree:waiting", TRACE_ID_LOCAL(pending_tree_.get()),
      "active_lsid", active_tree()->local_surface_id_from_parent().ToString());
}

void LayerTreeHostImpl::PushScrollbarOpacitiesFromActiveToPending() {
  if (!active_tree())
    return;
  for (auto& pair : scrollbar_animation_controllers_) {
    for (auto* scrollbar : pair.second->Scrollbars()) {
      if (const EffectNode* source_effect_node =
              active_tree()
                  ->property_trees()
                  ->effect_tree()
                  .FindNodeFromElementId(scrollbar->element_id())) {
        if (EffectNode* target_effect_node =
                pending_tree()
                    ->property_trees()
                    ->effect_tree_mutable()
                    .FindNodeFromElementId(scrollbar->element_id())) {
          DCHECK(target_effect_node);
          float source_opacity = source_effect_node->opacity;
          float target_opacity = target_effect_node->opacity;
          if (source_opacity == target_opacity)
            continue;
          target_effect_node->opacity = source_opacity;
          pending_tree()
              ->property_trees()
              ->effect_tree_mutable()
              .set_needs_update(true);
        }
      }
    }
  }
}

void LayerTreeHostImpl::ActivateSyncTree() {
  TRACE_EVENT(
      "cc,benchmark", "LayerTreeHostImpl::ActivateSyncTree",
      [&](perfetto::EventContext ctx) {
        EmitMainFramePipelineStep(
            ctx, sync_tree()->trace_id(),
            perfetto::protos::pbzero::MainFramePipeline::Step::ACTIVATE);
      });
  if (pending_tree_) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "cc", "PendingTree:waiting", TRACE_ID_LOCAL(pending_tree_.get()),
        "pending_lsid",
        pending_tree_->local_surface_id_from_parent().ToString());
    active_tree_->lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

    // Process any requests in the UI resource queue.  The request queue is
    // given in LayerTreeHost::FinishCommit.  This must take place before the
    // swap.
    pending_tree_->ProcessUIResourceRequestQueue();

    if (pending_tree_->needs_full_tree_sync()) {
      TreeSynchronizer::SynchronizeTrees(pending_tree_.get(),
                                         active_tree_.get());

      // If this tree uses a LayerContext for display, ensure the new layer list
      // is pushed to Viz during the next update.
      active_tree_->set_needs_full_tree_sync(true);
    }

    PushScrollbarOpacitiesFromActiveToPending();
    pending_tree_->PushPropertyTreesTo(active_tree_.get());
    active_tree_->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedPropertyTrees);

    TreeSynchronizer::PushLayerProperties(pending_tree(), active_tree());

    active_tree_->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedLayerProperties);

    pending_tree_->PushPropertiesTo(active_tree_.get());
    if (!pending_tree_->LayerListIsEmpty())
      pending_tree_->property_trees()->ResetAllChangeTracking();

    active_tree_->lifecycle().AdvanceTo(LayerTreeLifecycle::kNotSyncing);

    // Now that we've synced everything from the pending tree to the active
    // tree, rename the pending tree the recycle tree so we can reuse it on the
    // next sync.
    DCHECK(!recycle_tree_);
    pending_tree_.swap(recycle_tree_);

    // ScrollTimelines track a scroll source (i.e. a scroll node in the scroll
    // tree), whose ElementId may change between the active and pending trees.
    // Therefore we must inform all ScrollTimelines when the pending tree is
    // promoted to active.
    mutator_host_->PromoteScrollTimelinesPendingToActive();

    // If we commit to the active tree directly, this is already done during
    // commit.
    ActivateAnimations();

    // Update the state for images in ImageAnimationController and TileManager
    // before dirtying tile priorities. Since these components cache tree
    // specific state, these should be updated before DidModifyTilePriorities
    // which can synchronously issue a PrepareTiles. Note that if we commit to
    // the active tree directly, this is already done during commit.
    ActivateStateForImages();
  } else {
    active_tree_->ProcessUIResourceRequestQueue();
  }

  active_tree_->UpdateViewportContainerSizes();

  if (InnerViewportScrollNode()) {
    active_tree_->property_trees()
        ->scroll_tree_mutable()
        .ClampScrollToMaxScrollOffset(*InnerViewportScrollNode(),
                                      active_tree_.get());

    DCHECK(OuterViewportScrollNode());
    active_tree_->property_trees()
        ->scroll_tree_mutable()
        .ClampScrollToMaxScrollOffset(*OuterViewportScrollNode(),
                                      active_tree_.get());
  }

  active_tree_->DidBecomeActive();
  client_->RenewTreePriority();

  // If we have any picture layers, then by activating we also modified tile
  // priorities.
  if (!active_tree_->picture_layers().empty())
    DidModifyTilePriorities(/*pending_update_tiles=*/false);

  auto screenshot_token = active_tree()->TakeScreenshotDestinationToken();
  if (child_local_surface_id_allocator_.GetCurrentLocalSurfaceId().is_valid()) {
    // Since the screenshot will be issued against the previous `viz::Surface`
    // we need to make sure the renderer has at least embedded a valid surface
    // previously.
    screenshot_destination_ = std::move(screenshot_token);
  } else if (!screenshot_token.is_empty()) {
    LOG(ERROR)
        << "Cannot issue a copy because the previous surface is invalid.";
  }

  UpdateChildLocalSurfaceId();
  client_->OnCanDrawStateChanged(CanDraw());
  client_->DidActivateSyncTree();
  if (!tree_activation_callback_.is_null())
    tree_activation_callback_.Run();

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation =
      active_tree_->TakePendingPageScaleAnimation();
  if (pending_page_scale_animation) {
    StartPageScaleAnimation(pending_page_scale_animation->target_offset,
                            pending_page_scale_animation->use_anchor,
                            pending_page_scale_animation->scale,
                            pending_page_scale_animation->duration);
  }

  if (input_delegate_)
    input_delegate_->DidActivatePendingTree();

  // Dump property trees and layers if VerboseLogEnabled().
  VERBOSE_LOG() << "After activating sync tree, the active tree:"
                << "\nproperty_trees:\n"
                << active_tree_->property_trees()->ToString() << "\n"
                << "cc::LayerImpls:\n"
                << active_tree_->LayerListAsJson();
}

void LayerTreeHostImpl::ActivateStateForImages() {
  if (settings_.is_display_tree) {
    return;
  }

  image_animation_controller_.DidActivate();
  tile_manager_.DidActivateSyncTree();
}

void LayerTreeHostImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (settings_.is_display_tree) {
    return;
  }

  // Only work for low-end devices for now.
  if (!base::SysInfo::IsLowEndDevice())
    return;

  if (!ImageDecodeCacheUtils::ShouldEvictCaches(level))
    return;

    // TODO(crbug.com/42050253): Unlocking decoded-image-tracker images causes
    // flickering in visible trees if Out-Of-Process rasterization is enabled.
#if BUILDFLAG(IS_FUCHSIA)
  if (use_gpu_rasterization() && visible())
    return;
#endif  // BUILDFLAG(IS_FUCHSIA)

  ReleaseTileResources();
  active_tree_->OnPurgeMemory();
  if (pending_tree_)
    pending_tree_->OnPurgeMemory();
  if (recycle_tree_)
    recycle_tree_->OnPurgeMemory();

  EvictAllUIResources();
  if (resource_pool_)
    resource_pool_->OnMemoryPressure(level);

  tile_manager_.decoded_image_tracker().UnlockAllImages();

  // There is no need to notify the |image_decode_cache| about the memory
  // pressure as it (the gpu one as the software one doesn't keep outstanding
  // images pinned) listens to memory pressure events and purges memory base on
  // the ImageDecodeCacheUtils::ShouldEvictCaches' return value.
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;

  if (layer_context_) {
    layer_context_->SetVisible(visible);
  }

  if (visible_) {
    auto now = base::TimeTicks::Now();
    total_frame_counter_.OnShow(now);
  } else {
    auto now = base::TimeTicks::Now();
    total_frame_counter_.OnHide(now);
    dropped_frame_counter_.ResetPendingFrames(now);

    // When page is invisible, throw away corresponding EventsMetrics since
    // these metrics will be incorrect due to duration of page being invisible.
    active_tree()->TakeEventsMetrics();
    events_metrics_manager_.TakeSavedEventsMetrics();
    if (pending_tree()) {
      pending_tree()->TakeEventsMetrics();
    }
  }
  // Notify reporting controller of transition between visible and invisible
  compositor_frame_reporting_controller_->SetVisible(visible_);
  DidVisibilityChange(this, visible_);

  if (!settings_.is_display_tree) {
    UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  }

  // If we just became visible, we have to ensure that we draw high res tiles,
  // to prevent checkerboard/low res flashes.
  if (visible_) {
    // TODO(crbug.com/40410467): Replace with RequiresHighResToDraw.
    SetRequiresHighResToDraw();
    // Prior CompositorFrame may have been discarded and thus we need to ensure
    // that we submit a new one, even if there are no tiles. Therefore, force a
    // full viewport redraw. However, this is unnecessary when we become visible
    // for the first time (before the first commit) as there is no prior
    // CompositorFrame to replace. We can safely use |!active_tree_->
    // LayerListIsEmpty()| as a proxy for this, because we wouldn't be able to
    // draw anything even if this is not the first time we become visible.
    if (!active_tree_->LayerListIsEmpty()) {
      SetFullViewportDamage();
      SetNeedsRedrawOrUpdateDisplayTree();
    }
  } else if (!settings_.is_display_tree) {
    EvictAllUIResources();
    // Call PrepareTiles to evict tiles when we become invisible.
    PrepareTiles();
    tile_manager_.decoded_image_tracker().UnlockAllImages();
  }

  active_tree_->SetVisible(visible);
  resource_provider_->SetVisible(visible);
}

void LayerTreeHostImpl::SetNeedsOneBeginImplFrame() {
  NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
  client_->SetNeedsOneBeginImplFrameOnImplThread();
}

void LayerTreeHostImpl::SetNeedsRedraw() {
  NotifyLatencyInfoSwapPromiseMonitors();
  events_metrics_manager_.SaveActiveEventMetrics();
  client_->SetNeedsRedrawOnImplThread();
}

void LayerTreeHostImpl::SetNeedsUpdateDisplayTree() {
  client_->SetNeedsUpdateDisplayTreeOnImplThread();
}

ManagedMemoryPolicy LayerTreeHostImpl::ActualManagedMemoryPolicy() const {
  ManagedMemoryPolicy actual = cached_managed_memory_policy_;
  // The following may lower the cutoff, but should never raise it.
  if (debug_state_.rasterize_only_visible_content) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;
  } else if (use_gpu_rasterization() &&
             actual.priority_cutoff_when_visible ==
                 gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
  }
  return actual;
}

void LayerTreeHostImpl::ReleaseTreeResources() {
  active_tree_->ReleaseResources();
  if (pending_tree_)
    pending_tree_->ReleaseResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseResources();

  EvictAllUIResources();
}

void LayerTreeHostImpl::ReleaseTileResources() {
  if (settings_.is_display_tree) {
    return;
  }

  active_tree_->ReleaseTileResources();
  if (pending_tree_)
    pending_tree_->ReleaseTileResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseTileResources();

  // Need to update tiles again in order to kick of raster work for all the
  // tiles that are dropped here.
  active_tree_->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::RecreateTileResources() {
  if (settings_.is_display_tree) {
    return;
  }

  active_tree_->RecreateTileResources();
  if (pending_tree_) {
    pending_tree_->RecreateTileResources();
  }
  if (recycle_tree_) {
    recycle_tree_->RecreateTileResources();
  }
}

void LayerTreeHostImpl::CreateTileManagerResources() {
  DCHECK(!settings_.is_display_tree);
  const bool gpu_compositing = !!layer_tree_frame_sink_->context_provider();
  image_decode_cache_holder_ = std::make_unique<ImageDecodeCacheHolder>(
      settings_.enable_shared_image_cache_for_gpu, raster_caps(),
      gpu_compositing,
      layer_tree_frame_sink_->worker_context_provider_wrapper(),
      settings_.decoded_image_working_set_budget_bytes, dark_mode_filter_);

  if (raster_caps().use_gpu_rasterization) {
    pending_raster_queries_ = std::make_unique<RasterQueryQueue>(
        layer_tree_frame_sink_->worker_context_provider());
  }

  raster_buffer_provider_ = CreateRasterBufferProvider();

  // Pass the single-threaded synchronous task graph runner to the worker pool
  // if we're in synchronous single-threaded mode.
  TaskGraphRunner* task_graph_runner = task_graph_runner_;
  if (is_synchronous_single_threaded_) {
    DCHECK(!single_thread_synchronous_task_graph_runner_);
    single_thread_synchronous_task_graph_runner_ =
        std::make_unique<SynchronousTaskGraphRunner>();
    task_graph_runner = single_thread_synchronous_task_graph_runner_.get();
  }

  tile_manager_.SetResources(resource_pool_.get(), GetImageDecodeCache(),
                             task_graph_runner, raster_buffer_provider_.get(),
                             raster_caps().use_gpu_rasterization,
                             pending_raster_queries_.get());
  tile_manager_.SetCheckerImagingForceDisabled(
      settings_.only_checker_images_with_gpu_raster &&
      !raster_caps().use_gpu_rasterization);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
}

std::unique_ptr<RasterBufferProvider>
LayerTreeHostImpl::CreateRasterBufferProvider() {
  DCHECK(!settings_.is_display_tree);
  DCHECK(GetTaskRunner());
  viz::RasterContextProvider* compositor_context_provider =
      layer_tree_frame_sink_->context_provider();

  if (!compositor_context_provider) {
    return std::make_unique<BitmapRasterBufferProvider>(layer_tree_frame_sink_);
  }

  const gpu::Capabilities& caps =
      compositor_context_provider->ContextCapabilities();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink_->worker_context_provider();

  if (raster_caps().use_gpu_rasterization) {
    DCHECK(worker_context_provider);

    return std::make_unique<GpuRasterBufferProvider>(
        compositor_context_provider, worker_context_provider, raster_caps_,
        settings_.max_gpu_raster_tile_size,
        settings_.unpremultiply_and_dither_low_bit_depth_tiles,
        pending_raster_queries_.get());
  }

  bool use_zero_copy = settings_.use_zero_copy;
  // TODO(reveman): Remove this when mojo supports worker contexts.
  // crbug.com/522440
  if (!use_zero_copy && !worker_context_provider) {
    LOG(ERROR)
        << "Forcing zero-copy tile initialization as worker context is missing";
    use_zero_copy = true;
  }

  if (use_zero_copy) {
    return std::make_unique<ZeroCopyRasterBufferProvider>(
        compositor_context_provider, raster_caps_);
  }

  const int max_copy_texture_chromium_size =
      caps.max_copy_texture_chromium_size;
  return std::make_unique<OneCopyRasterBufferProvider>(
      GetTaskRunner(), compositor_context_provider, worker_context_provider,
      max_copy_texture_chromium_size, settings_.use_partial_raster,
      settings_.max_staging_buffer_usage_in_bytes, raster_caps_);
}

void LayerTreeHostImpl::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  mutator_host_->SetLayerTreeMutator(std::move(mutator));
}

void LayerTreeHostImpl::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  paint_worklet_painter_ = std::move(painter);
}

void LayerTreeHostImpl::QueueImageDecode(int request_id,
                                         const PaintImage& image) {
  DCHECK(!settings_.is_display_tree);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::QueueImageDecode", "frame_key",
               image.GetKeyForFrame(PaintImage::kDefaultFrameIndex).ToString());
  // Optimistically specify the current raster color space, since we assume that
  // it won't change.
  auto content_color_usage = image.GetContentColorUsage();
  tile_manager_.decoded_image_tracker().QueueImageDecode(
      image, GetTargetColorParams(content_color_usage),
      base::BindOnce(&LayerTreeHostImpl::ImageDecodeFinished,
                     weak_factory_.GetWeakPtr(), request_id));
  tile_manager_.checker_image_tracker().DisallowCheckeringForImage(image);
}

void LayerTreeHostImpl::ImageDecodeFinished(int request_id,
                                            bool decode_succeeded) {
  DCHECK(!settings_.is_display_tree);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::ImageDecodeFinished");
  if (!base::FeatureList::IsEnabled(
          features::kSendExplicitDecodeRequestsImmediately)) {
    completed_image_decode_requests_.emplace_back(request_id, decode_succeeded);
  }
  client_->NotifyImageDecodeRequestFinished(request_id, decode_succeeded);
}

std::vector<std::pair<int, bool>>
LayerTreeHostImpl::TakeCompletedImageDecodeRequests() {
  auto result = std::move(completed_image_decode_requests_);
  completed_image_decode_requests_.clear();
  return result;
}

std::unique_ptr<MutatorEvents> LayerTreeHostImpl::TakeMutatorEvents() {
  std::unique_ptr<MutatorEvents> events = mutator_host_->CreateEvents();
  std::swap(events, mutator_events_);
  mutator_host_->TakeTimeUpdatedEvents(events.get());
  return events;
}

void LayerTreeHostImpl::ClearHistory() {
  client_->ClearHistory();
}

size_t LayerTreeHostImpl::CommitDurationSampleCountForTesting() const {
  return client_->CommitDurationSampleCountForTesting();  // IN-TEST
}

void LayerTreeHostImpl::ClearCaches() {
  // It is safe to clear the decode policy tracking on navigations since it
  // comes with an invalidation and the image ids are never re-used.
  bool can_clear_decode_policy_tracking = true;
  tile_manager_.ClearCheckerImageTracking(can_clear_decode_policy_tracking);
  // TODO(crbug.com/40243840): add tracking for which clients have used an image
  // and remove entries used by only one client when the URL on that client
  // changes. This should be fixed to correctly clear caches for web contents.
  // This is only a problem when
  // LayerTreeSettings::enable_shared_image_cache_for_gpu is true.
  if (GetImageDecodeCache())
    GetImageDecodeCache()->ClearCache();
  image_animation_controller_.set_did_navigate();
}

void LayerTreeHostImpl::DidChangeScrollbarVisibility() {
  // Need a commit since input handling for scrollbars is handled in Blink so
  // we need to communicate to Blink when the compositor shows/hides the
  // scrollbars.
  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::CleanUpTileManagerResources() {
  DCHECK(!settings_.is_display_tree);
  tile_manager_.FinishTasksAndCleanUp();
  single_thread_synchronous_task_graph_runner_ = nullptr;
  image_decode_cache_holder_ = nullptr;
  raster_buffer_provider_ = nullptr;
  pending_raster_queries_ = nullptr;
  // Any resources that were allocated previously should be considered not good
  // for reuse, as the RasterBufferProvider will be replaced and it may choose
  // to allocate future resources differently.
  resource_pool_->InvalidateResources();

  // We've potentially just freed a large number of resources on our various
  // contexts. Flushing now helps ensure these are cleaned up quickly
  // preventing driver cache growth. See crbug.com/643251
  if (layer_tree_frame_sink_) {
    if (auto* worker_context =
            layer_tree_frame_sink_->worker_context_provider()) {
      viz::RasterContextProvider::ScopedRasterContextLock hold(worker_context);
      hold.RasterInterface()->OrderingBarrierCHROMIUM();
    }
    // There can sometimes be a compositor context with no worker context so use
    // it to flush. cc doesn't use the compositor context to issue GPU work so
    // it doesn't require an ordering barrier.
    if (auto* compositor_context = layer_tree_frame_sink_->context_provider()) {
      compositor_context->ContextSupport()->FlushPendingWork();
    }
  }
}

void LayerTreeHostImpl::ReleaseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ReleaseLayerTreeFrameSink");

  if (!layer_tree_frame_sink_) {
    DCHECK(!has_valid_layer_tree_frame_sink_);
    return;
  }

  has_valid_layer_tree_frame_sink_ = false;

  ReleaseTreeResources();
  if (!settings_.is_display_tree) {
    CleanUpTileManagerResources();
    resource_pool_ = nullptr;
    ClearUIResources();
  }

  bool should_finish = true;
#if BUILDFLAG(IS_WIN)
  // Windows does not have stability issues that require calling Finish.
  // To minimize risk, only avoid waiting for the UI layer tree.
  should_finish = !settings_.is_layer_tree_for_ui;
#endif

  if (should_finish && layer_tree_frame_sink_->context_provider()) {
    // TODO(kylechar): Exactly where this finish call is still required is not
    // obvious. Attempts have been made to remove it which caused problems, eg.
    // https://crbug.com/846709. We should test removing it via finch to find
    // out if this is still needed on any platforms.
    layer_tree_frame_sink_->context_provider()->RasterInterface()->Finish();
  }

  // Release any context visibility before we destroy the LayerTreeFrameSink.
  SetContextVisibility(false);

  // Destroy the submit-frame trackers before destroying the frame sink.
  frame_trackers_.ClearAll();

  // Detach from the old LayerTreeFrameSink and reset |layer_tree_frame_sink_|
  // pointer as this surface is going to be destroyed independent of if binding
  // the new LayerTreeFrameSink succeeds or not.
  layer_tree_frame_sink_->DetachFromClient();
  layer_tree_frame_sink_ = nullptr;
  layer_context_.reset();

  // If gpu compositing, then any resources created with the gpu context in the
  // LayerTreeFrameSink were exported to the display compositor may be modified
  // by it, and thus we would be unable to determine what state they are in, in
  // order to reuse them, so they must be lost. Note that this includes
  // resources created using the gpu context associated with
  // |layer_tree_frame_sink_| internally by the compositor and any resources
  // received from an external source (for instance, TextureLayers). This is
  // because the API contract for releasing these external resources requires
  // that the compositor return them with a valid sync token and no
  // modifications to their GL state. Since that can not be guaranteed, these
  // must also be marked lost.
  //
  // In software compositing, the resources are not modified by the display
  // compositor (there is no stateful metadata for shared memory), so we do not
  // need to consider them lost.
  //
  // In both cases, the resources that are exported to the display compositor
  // will have no means of being returned to this client without the
  // LayerTreeFrameSink, so they should no longer be considered as exported. Do
  // this *after* any interactions with the |layer_tree_frame_sink_| in case it
  // tries to return resources during destruction.
  //
  // The assumption being made here is that the display compositor WILL NOT use
  // any resources previously exported when the CompositorFrameSink is closed.
  // This should be true as the connection is closed when the display compositor
  // shuts down/crashes, or when it believes we are a malicious client in which
  // case it will not display content from the previous CompositorFrameSink. If
  // this assumption is violated, we may modify resources no longer considered
  // as exported while the display compositor is still making use of them,
  // leading to visual mistakes.
  resource_provider_->ReleaseAllExportedResources(/*lose=*/true);

  // We don't know if the next LayerTreeFrameSink will support GPU
  // rasterization. Make sure to clear the flag so that we force a
  // re-computation.
  raster_caps_.use_gpu_rasterization = false;
}

bool LayerTreeHostImpl::InitializeFrameSink(
    LayerTreeFrameSink* layer_tree_frame_sink) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::InitializeFrameSink");

  ReleaseLayerTreeFrameSink();
  if (!layer_tree_frame_sink->BindToClient(this)) {
    // Avoid recreating tree resources because we might not have enough
    // information to do this yet (eg. we don't have a TileManager at this
    // point).
    return false;
  }

  layer_tree_frame_sink_ = layer_tree_frame_sink;
  has_valid_layer_tree_frame_sink_ = true;
  if (use_layer_context_for_display_) {
    layer_context_ = layer_tree_frame_sink_->CreateLayerContext(*this);
  }

  UpdateRasterCapabilities();

  // See note in LayerTreeImpl::UpdateDrawProperties, new LayerTreeFrameSink
  // means a new max texture size which affects draw properties. Also, if the
  // draw properties were up to date, layers still lost resources and we need to
  // UpdateDrawProperties() after calling RecreateTreeResources().
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();

  if (!settings_.is_display_tree) {
    resource_pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), layer_tree_frame_sink_->context_provider(),
        GetTaskRunner(), ResourcePool::kDefaultExpirationDelay,
        settings_.disallow_non_exact_resource_reuse);

    CreateTileManagerResources();
    RecreateTileResources();
  }

  client_->OnCanDrawStateChanged(CanDraw());
  SetFullViewportDamage();
  // There will not be anything to draw here, so set high res
  // to avoid checkerboards, typically when we are recovering
  // from lost context.
  // TODO(crbug.com/40410467): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();

  // Always allocate a new viz::LocalSurfaceId when we get a new
  // LayerTreeFrameSink to ensure that we do not reuse the same surface after
  // it might have been garbage collected.
  const viz::LocalSurfaceId& local_surface_id =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  if (local_surface_id.is_valid())
    AllocateLocalSurfaceId();

  return true;
}

void LayerTreeHostImpl::SetBeginFrameSource(viz::BeginFrameSource* source) {
  client_->SetBeginFrameSource(source);
}

const gfx::Transform& LayerTreeHostImpl::DrawTransform() const {
  return external_transform_;
}

void LayerTreeHostImpl::DidChangeBrowserControlsPosition() {
  active_tree_->UpdateViewportContainerSizes();
  if (pending_tree_)
    pending_tree_->UpdateViewportContainerSizes();
  SetNeedsRedrawOrUpdateDisplayTree();
  SetNeedsOneBeginImplFrame();
  SetFullViewportDamage();
}

void LayerTreeHostImpl::DidObserveScrollDelay(
    int source_frame_number,
    base::TimeDelta scroll_delay,
    base::TimeTicks scroll_timestamp) {
  // Record First Scroll Delay.
  if (!has_observed_first_scroll_delay_) {
    client_->DidObserveFirstScrollDelay(source_frame_number, scroll_delay,
                                        scroll_timestamp);
    has_observed_first_scroll_delay_ = true;
  }
}

float LayerTreeHostImpl::TopControlsHeight() const {
  return active_tree_->top_controls_height();
}

float LayerTreeHostImpl::TopControlsMinHeight() const {
  return active_tree_->top_controls_min_height();
}

float LayerTreeHostImpl::BottomControlsHeight() const {
  return active_tree_->bottom_controls_height();
}

float LayerTreeHostImpl::BottomControlsMinHeight() const {
  return active_tree_->bottom_controls_min_height();
}

void LayerTreeHostImpl::SetCurrentBrowserControlsShownRatio(
    float top_ratio,
    float bottom_ratio) {
  if (active_tree_->SetCurrentBrowserControlsShownRatio(top_ratio,
                                                        bottom_ratio))
    DidChangeBrowserControlsPosition();
}

float LayerTreeHostImpl::CurrentTopControlsShownRatio() const {
  return active_tree_->CurrentTopControlsShownRatio();
}

float LayerTreeHostImpl::CurrentBottomControlsShownRatio() const {
  return active_tree_->CurrentBottomControlsShownRatio();
}

gfx::PointF LayerTreeHostImpl::ViewportScrollOffset() const {
  return viewport_->TotalScrollOffset();
}

void LayerTreeHostImpl::AutoScrollAnimationCreate(
    const ScrollNode& scroll_node,
    const gfx::PointF& target_offset,
    float autoscroll_velocity) {
  // Start the animation one full frame in. Without any offset, the animation
  // doesn't start until next frame, increasing latency, and preventing our
  // input latency tracking architecture from working.
  base::TimeDelta animation_start_offset = CurrentBeginFrameArgs().interval;

  const ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree();
  gfx::PointF current_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);

  mutator_host_->ImplOnlyAutoScrollAnimationCreate(
      scroll_node.element_id, target_offset, current_offset,
      autoscroll_velocity, animation_start_offset);

  SetNeedsOneBeginImplFrame();
}

bool LayerTreeHostImpl::ScrollAnimationCreate(const ScrollNode& scroll_node,
                                              const gfx::Vector2dF& delta,
                                              base::TimeDelta delayed_by) {
  ScrollTree& scroll_tree =
      active_tree_->property_trees()->scroll_tree_mutable();

  const float kEpsilon = 0.1f;
  bool scroll_animated =
      std::abs(delta.x()) > kEpsilon || std::abs(delta.y()) > kEpsilon;
  if (!scroll_animated) {
    scroll_tree.ScrollBy(scroll_node, delta, active_tree());
    TRACE_EVENT_INSTANT0("cc", "no scroll animation due to small delta",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  gfx::PointF current_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::PointF target_offset = scroll_tree.ClampScrollOffsetToLimits(
      current_offset + delta, scroll_node);

  // Start the animation one full frame in. Without any offset, the animation
  // doesn't start until next frame, increasing latency, and preventing our
  // input latency tracking architecture from working.
  base::TimeDelta animation_start_offset = CurrentBeginFrameArgs().interval;

  mutator_host_->ImplOnlyScrollAnimationCreate(
      scroll_node.element_id, target_offset, current_offset, delayed_by,
      animation_start_offset);

  SetNeedsOneBeginImplFrame();

  return true;
}

void LayerTreeHostImpl::UpdateImageDecodingHints(
    base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
        decoding_mode_map) {
  tile_manager_.checker_image_tracker().UpdateImageDecodingHints(
      std::move(decoding_mode_map));
}

void LayerTreeHostImpl::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  render_frame_metadata_observer_ = std::move(observer);
  if (render_frame_metadata_observer_) {
    render_frame_metadata_observer_->BindToCurrentSequence();
  }
}

void LayerTreeHostImpl::WillScrollContent(ElementId element_id) {
  // Flash the overlay scrollbar even if the scroll delta is 0.
  if (settings().scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(false);
  } else {
    if (ScrollbarAnimationController* animation_controller =
            ScrollbarAnimationControllerForElementId(element_id))
      animation_controller->WillUpdateScroll();
  }
}

void LayerTreeHostImpl::DidScrollContent(ElementId element_id, bool animated) {
  if (settings().scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(true);
  } else {
    if (ScrollbarAnimationController* animation_controller =
            ScrollbarAnimationControllerForElementId(element_id))
      animation_controller->DidScrollUpdate();
  }

  // We may wish to prioritize smoothness over raster when the user is
  // interacting with content, but this needs to be evaluated only for direct
  // user scrolls, not for programmatic scrolls.
  if (input_delegate_->IsCurrentlyScrolling()) {
    if (!settings().single_thread_proxy_scheduler) {
      client_->SetWaitingForScrollEvent(false);
    }
    client_->RenewTreePriority();
  }

  if (!animated) {
    // SetNeedsRedraw is only called in non-animated cases since an animation
    // won't actually update any scroll offsets until a frame produces a
    // tick. Scheduling a redraw here before ticking means the draw gets
    // aborted due to no damage and the swap promises broken so a LatencyInfo
    // won't be recorded.
    SetNeedsRedraw();
  }
}

float LayerTreeHostImpl::DeviceScaleFactor() const {
  return active_tree_->device_scale_factor();
}

float LayerTreeHostImpl::PageScaleFactor() const {
  return active_tree_->page_scale_factor_for_scroll();
}

void LayerTreeHostImpl::BindToInputHandler(
    std::unique_ptr<InputDelegateForCompositor> delegate) {
  input_delegate_ = std::move(delegate);
  input_delegate_->SetPrefersReducedMotion(prefers_reduced_motion_);
}

void LayerTreeHostImpl::DetachInputDelegateAndRenderFrameObserver() {
  if (input_delegate_) {
    input_delegate_->WillShutdown();
  }
  input_delegate_ = nullptr;
  SetRenderFrameObserver(nullptr);
}

void LayerTreeHostImpl::SetVisualDeviceViewportSize(
    const gfx::Size& visual_device_viewport_size) {
  visual_device_viewport_size_ = visual_device_viewport_size;
}

gfx::Size LayerTreeHostImpl::VisualDeviceViewportSize() const {
  return visual_device_viewport_size_;
}

void LayerTreeHostImpl::SetPrefersReducedMotion(bool prefers_reduced_motion) {
  if (prefers_reduced_motion_ == prefers_reduced_motion)
    return;

  prefers_reduced_motion_ = prefers_reduced_motion;
  if (input_delegate_)
    input_delegate_->SetPrefersReducedMotion(prefers_reduced_motion_);
}

void LayerTreeHostImpl::SetMayThrottleIfUndrawnFrames(
    bool may_throttle_if_undrawn_frames) {
  may_throttle_if_undrawn_frames_ = may_throttle_if_undrawn_frames;
}

ScrollTree& LayerTreeHostImpl::GetScrollTree() const {
  return active_tree_->property_trees()->scroll_tree_mutable();
}

bool LayerTreeHostImpl::HasAnimatedScrollbars() const {
  return !scrollbar_animation_controllers_.empty();
}

void LayerTreeHostImpl::UpdateChildLocalSurfaceId() {
  if (!active_tree()->local_surface_id_from_parent().is_valid()) {
    return;
  }

  child_local_surface_id_allocator_.UpdateFromParent(
      active_tree()->local_surface_id_from_parent());
  if (active_tree()->TakeNewLocalSurfaceIdRequest()) {
    AllocateLocalSurfaceId();
  }

  // We have a newer surface than the evicted one, or the embedding has
  // changed, clear eviction state resume drawing.
  if (evicted_local_surface_id_.is_valid() &&
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId()
          .IsNewerThanOrEmbeddingChanged(evicted_local_surface_id_)) {
    evicted_local_surface_id_ = viz::LocalSurfaceId();
    if (resource_provider_) {
      resource_provider_->SetEvicted(false);
    }
  }
}

void LayerTreeHostImpl::CollectScrollbarUpdatesForCommit(
    CompositorCommitData* commit_data) const {
  commit_data->scrollbars.reserve(scrollbar_animation_controllers_.size());
  for (auto& pair : scrollbar_animation_controllers_) {
    if (pair.second->visibility_changed()) {
      commit_data->scrollbars.push_back(
          {pair.first, pair.second->ScrollbarsHidden()});
      pair.second->ClearVisibilityChanged();
    }
  }
}

std::unique_ptr<CompositorCommitData>
LayerTreeHostImpl::ProcessCompositorDeltas(
    const MutatorHost* main_thread_mutator_host) {
  auto commit_data = std::make_unique<CompositorCommitData>();

  if (input_delegate_) {
    input_delegate_->ProcessCommitDeltas(commit_data.get(),
                                         main_thread_mutator_host);
  }
  CollectScrollbarUpdatesForCommit(commit_data.get());

  commit_data->page_scale_delta =
      active_tree_->page_scale_factor()->PullDeltaForMainThread(
          main_thread_mutator_host);
  commit_data->is_pinch_gesture_active = active_tree_->PinchGestureActive();
  commit_data->is_scroll_active =
      input_delegate_ && GetInputHandler().IsCurrentlyScrolling();
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this DCHECK as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(settings().is_for_scalable_page ||
         commit_data->page_scale_delta == 1.f);
  commit_data->top_controls_delta =
      active_tree()->top_controls_shown_ratio()->PullDeltaForMainThread(
          main_thread_mutator_host);
  commit_data->bottom_controls_delta =
      active_tree()->bottom_controls_shown_ratio()->PullDeltaForMainThread(
          main_thread_mutator_host);
  commit_data->elastic_overscroll_delta =
      active_tree_->elastic_overscroll()->PullDeltaForMainThread(
          main_thread_mutator_host);
  commit_data->swap_promises.swap(swap_promises_for_main_thread_scroll_update_);

  commit_data->ongoing_scroll_animation =
      mutator_host_->HasImplOnlyScrollAnimatingElement();
  commit_data->is_auto_scrolling =
      mutator_host_->HasImplOnlyAutoScrollAnimatingElement();

  if (browser_controls_manager()) {
    commit_data->browser_controls_constraint =
        browser_controls_manager()->PullConstraintForMainThread(
            &commit_data->browser_controls_constraint_changed);
  }

  return commit_data;
}

void LayerTreeHostImpl::SetFullViewportDamage() {
  // In non-Android-WebView cases, we expect GetDeviceViewport() to be the same
  // as internal_device_viewport(), so the full-viewport damage rect is just
  // the internal viewport rect. In the case of Android WebView,
  // GetDeviceViewport returns the external viewport, but we still want to use
  // the internal viewport's origin for setting the damage.
  // See https://chromium-review.googlesource.com/c/chromium/src/+/1257555.
  SetViewportDamage(gfx::Rect(active_tree_->internal_device_viewport().origin(),
                              active_tree_->GetDeviceViewport().size()));
}

bool LayerTreeHostImpl::AnimatePageScale(base::TimeTicks monotonic_time) {
  if (!page_scale_animation_)
    return false;

  gfx::PointF scroll_total = active_tree_->TotalScrollOffset();

  if (!page_scale_animation_->IsAnimationStarted())
    page_scale_animation_->StartAnimation(monotonic_time);

  active_tree_->SetPageScaleOnActiveTree(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time));
  gfx::PointF next_scroll =
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time);

  viewport().ScrollByInnerFirst(next_scroll - scroll_total);

  if (page_scale_animation_->IsAnimationCompleteAtTime(monotonic_time)) {
    page_scale_animation_ = nullptr;
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
    client_->DidCompletePageScaleAnimationOnImplThread();
  } else {
    SetNeedsOneBeginImplFrame();
  }
  return true;
}

bool LayerTreeHostImpl::AnimateBrowserControls(base::TimeTicks time) {
  if (!browser_controls_offset_manager_->HasAnimation())
    return false;

  gfx::Vector2dF scroll_delta = browser_controls_offset_manager_->Animate(time);

  if (browser_controls_offset_manager_->HasAnimation())
    SetNeedsOneBeginImplFrame();

  if (active_tree_->TotalScrollOffset().y() == 0.f ||
      OnlyExpandTopControlsAtPageTop()) {
    return false;
  }

  if (scroll_delta.IsZero())
    return false;

  // This counter-scrolls the page to keep the appearance of the page content
  // being fixed while the browser controls animate.
  viewport().ScrollBy(scroll_delta,
                      /*viewport_point=*/gfx::Point(),
                      /*is_wheel_scroll=*/false,
                      /*affect_browser_controls=*/false,
                      /*scroll_outer_viewport=*/true);

  // If the viewport has scroll snap styling, we may need to snap after
  // scrolling it. Browser controls animations may happen after scrollend, so
  // it is too late for InputHandler to do the snapping.
  viewport().SnapIfNeeded();

  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
  return true;
}

bool LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks monotonic_time) {
  bool animated = false;
  for (auto& pair : scrollbar_animation_controllers_) {
    animated |= pair.second->Animate(monotonic_time);
  }
  return animated;
}

bool LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time,
                                      bool is_active_tree) {
  const ScrollTree& scroll_tree =
      is_active_tree ? active_tree_->property_trees()->scroll_tree()
                     : pending_tree_->property_trees()->scroll_tree();
  const bool animated = mutator_host_->TickAnimations(
      monotonic_time, scroll_tree, is_active_tree);

  // TODO(crbug.com/40443202): Only do this if the animations are on the active
  // tree, or if they are on the pending tree waiting for some future time to
  // start.
  // TODO(crbug.com/40443205): We currently have a single signal from the
  // animation_host, so on the last frame of an animation we will
  // still request an extra SetNeedsAnimate here.
  if (animated) {
    // TODO(crbug.com/40667010): If only scroll animations present, schedule a
    // frame only if scroll changes.
    SetNeedsOneBeginImplFrame();
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kCompositorAnimation);
  } else {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kCompositorAnimation);
  }

  if (animated && mutator_host_->HasViewTransition()) {
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kSETCompositorAnimation);
  } else {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kSETCompositorAnimation);
  }

  // TODO(crbug.com/40443205): We could return true only if the animations are
  // on the active tree. There's no need to cause a draw to take place from
  // animations starting/ticking on the pending tree.
  return animated;
}

void LayerTreeHostImpl::UpdateAnimationState(bool start_ready_animations) {
  const bool has_active_animations = mutator_host_->UpdateAnimationState(
      start_ready_animations, mutator_events_.get());

  if (has_active_animations) {
    SetNeedsOneBeginImplFrame();
    if (!mutator_events_->IsEmpty())
      SetNeedsCommit();
  }
}

void LayerTreeHostImpl::ActivateAnimations() {
  const bool activated =
      mutator_host_->ActivateAnimations(mutator_events_.get());
  if (activated) {
    // Activating an animation changes layer draw properties, such as
    // screen_space_transform_is_animating. So when we see a new animation get
    // activated, we need to update the draw properties on the active tree.
    active_tree()->set_needs_update_draw_properties();
    // Request another frame to run the next tick of the animation.
    SetNeedsOneBeginImplFrame();
    if (!mutator_events_->IsEmpty())
      SetNeedsCommit();
  }
}

void LayerTreeHostImpl::RegisterScrollbarAnimationController(
    ElementId scroll_element_id,
    float scrollbar_opacity) {
  if (ScrollbarAnimationControllerForElementId(scroll_element_id))
    return;

  scrollbar_animation_controllers_[scroll_element_id] =
      active_tree_->CreateScrollbarAnimationController(scroll_element_id,
                                                       scrollbar_opacity);
}

void LayerTreeHostImpl::DidRegisterScrollbarLayer(
    ElementId scroll_element_id,
    ScrollbarOrientation orientation) {
  if (input_delegate_)
    input_delegate_->DidRegisterScrollbar(scroll_element_id, orientation);
}

void LayerTreeHostImpl::DidUnregisterScrollbarLayer(
    ElementId scroll_element_id,
    ScrollbarOrientation orientation) {
  if (ScrollbarsFor(scroll_element_id).empty())
    scrollbar_animation_controllers_.erase(scroll_element_id);
  if (input_delegate_)
    input_delegate_->DidUnregisterScrollbar(scroll_element_id, orientation);
}

ScrollbarAnimationController*
LayerTreeHostImpl::ScrollbarAnimationControllerForElementId(
    ElementId scroll_element_id) const {
  // The viewport layers have only one set of scrollbars. On Android, these are
  // registered with the inner viewport, otherwise they're registered with the
  // outer viewport. If a controller for one exists, the other shouldn't.
  if (InnerViewportScrollNode()) {
    DCHECK(OuterViewportScrollNode());
    if (scroll_element_id == InnerViewportScrollNode()->element_id ||
        scroll_element_id == OuterViewportScrollNode()->element_id) {
      auto itr = scrollbar_animation_controllers_.find(
          InnerViewportScrollNode()->element_id);
      if (itr != scrollbar_animation_controllers_.end())
        return itr->second.get();

      itr = scrollbar_animation_controllers_.find(
          OuterViewportScrollNode()->element_id);
      if (itr != scrollbar_animation_controllers_.end())
        return itr->second.get();

      return nullptr;
    }
  }

  auto i = scrollbar_animation_controllers_.find(scroll_element_id);
  if (i == scrollbar_animation_controllers_.end())
    return nullptr;
  return i->second.get();
}

void LayerTreeHostImpl::FlashAllScrollbars(bool did_scroll) {
  for (auto& pair : scrollbar_animation_controllers_) {
    if (did_scroll)
      pair.second->DidScrollUpdate();
    else
      pair.second->WillUpdateScroll();
  }
}

void LayerTreeHostImpl::PostDelayedScrollbarAnimationTask(
    base::OnceClosure task,
    base::TimeDelta delay) {
  client_->PostDelayedAnimationTaskOnImplThread(std::move(task), delay);
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation");
  SetNeedsOneBeginImplFrame();
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsRedrawForScrollbarAnimation() {
  SetNeedsRedraw();
}

ScrollbarSet LayerTreeHostImpl::ScrollbarsFor(ElementId id) const {
  return active_tree_->ScrollbarsFor(id);
}

bool LayerTreeHostImpl::IsFluentOverlayScrollbar() const {
  return settings().enable_fluent_overlay_scrollbar;
}

void LayerTreeHostImpl::AddVideoFrameController(
    VideoFrameController* controller) {
  bool was_empty = video_frame_controllers_.empty();
  video_frame_controllers_.insert(controller);
  if (current_begin_frame_tracker_.DangerousMethodHasStarted() &&
      !current_begin_frame_tracker_.DangerousMethodHasFinished())
    controller->OnBeginFrame(current_begin_frame_tracker_.Current());
  if (was_empty)
    client_->SetVideoNeedsBeginFrames(true);
}

void LayerTreeHostImpl::RemoveVideoFrameController(
    VideoFrameController* controller) {
  video_frame_controllers_.erase(controller);
  if (video_frame_controllers_.empty())
    client_->SetVideoNeedsBeginFrames(false);
}

void LayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  if (global_tile_state_.tree_priority == priority)
    return;
  global_tile_state_.tree_priority = priority;
  DidModifyTilePriorities(/*pending_update_tiles=*/false);
}

TreePriority LayerTreeHostImpl::GetTreePriority() const {
  return global_tile_state_.tree_priority;
}

const viz::BeginFrameArgs& LayerTreeHostImpl::CurrentBeginFrameArgs() const {
  // TODO(mithro): Replace call with current_begin_frame_tracker_.Current()
  // once all calls which happens outside impl frames are fixed.
  return current_begin_frame_tracker_.DangerousMethodCurrentOrLast();
}

base::TimeDelta LayerTreeHostImpl::CurrentBeginFrameInterval() const {
  return current_begin_frame_tracker_.Interval();
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
LayerTreeHostImpl::AsValueWithFrame(FrameData* frame) const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueWithFrameInto(frame, state.get());
  return std::move(state);
}

void LayerTreeHostImpl::AsValueWithFrameInto(
    FrameData* frame,
    base::trace_event::TracedValue* state) const {
  if (this->pending_tree_) {
    state->BeginDictionary("activation_state");
    ActivationStateAsValueInto(state);
    state->EndDictionary();
  }
  MathUtil::AddToTracedValue("device_viewport_size",
                             active_tree_->GetDeviceViewport().size(), state);

  std::vector<PrioritizedTile> prioritized_tiles;
  active_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);
  if (pending_tree_)
    pending_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);

  state->BeginArray("active_tiles");
  for (const auto& prioritized_tile : prioritized_tiles) {
    state->BeginDictionary();
    prioritized_tile.AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();

  state->BeginDictionary("tile_manager_basic_state");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("active_tree");
  active_tree_->AsValueInto(state);
  state->EndDictionary();
  if (pending_tree_) {
    state->BeginDictionary("pending_tree");
    pending_tree_->AsValueInto(state);
    state->EndDictionary();
  }
  if (frame) {
    state->BeginDictionary("frame");
    frame->AsValueInto(state);
    state->EndDictionary();
  }
}

void LayerTreeHostImpl::ActivationStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  viz::TracedValue::SetIDRef(this, state, "lthi");
  state->BeginDictionary("tile_manager");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();
}

void LayerTreeHostImpl::SetDebugState(
    const LayerTreeDebugState& new_debug_state) {
  if (debug_state_ == new_debug_state) {
    return;
  }

  debug_state_ = new_debug_state;
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  SetFullViewportDamage();
}

// TODO(https://crbug.com/365813260): Remove once the bug is analyzed and
// solved.
void LayerTreeHostImpl::CrashWhenMaxTextureSizeIsUninitialized() const {
  CHECK_GT(raster_caps().max_texture_size, 0);
}

void LayerTreeHostImpl::CreateUIResource(UIResourceId uid,
                                         const UIResourceBitmap& bitmap) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::CreateUIResource");
  DCHECK_GT(uid, 0);

  // Allow for multiple creation requests with the same UIResourceId.  The
  // previous resource is simply deleted.
  viz::ResourceId id = ResourceIdForUIResource(uid);
  if (id)
    DeleteUIResource(uid);

  if (!has_valid_layer_tree_frame_sink_) {
    evicted_ui_resources_.insert(uid);
    return;
  }

  viz::SharedImageFormat format;
  switch (bitmap.GetFormat()) {
    case UIResourceBitmap::RGBA8:
      format = raster_caps_.ui_rgba_format;
      break;
    case UIResourceBitmap::ALPHA_8:
      format = viz::SinglePlaneFormat::kALPHA_8;
      break;
    case UIResourceBitmap::ETC1:
      format = viz::SinglePlaneFormat::kETC1;
      break;
  }

  const gfx::Size source_size = bitmap.GetSize();
  gfx::Size upload_size = bitmap.GetSize();
  bool scaled = false;
  // UIResources are assumed to be rastered in SRGB.
  const gfx::ColorSpace& color_space = gfx::ColorSpace::CreateSRGB();

  // TODO(https://crbug.com/365813260): Remove once the bug is analyzed and
  // solved.
  CrashWhenMaxTextureSizeIsUninitialized();

  if (source_size.width() > raster_caps().max_texture_size ||
      source_size.height() > raster_caps().max_texture_size) {
    // Must resize the bitmap to fit within the max texture size.
    scaled = true;
    int edge = std::max(source_size.width(), source_size.height());
    float scale = static_cast<float>(raster_caps().max_texture_size - 1) / edge;
    DCHECK_LT(scale, 1.f);
    upload_size = gfx::ScaleToCeiledSize(source_size, scale, scale);
  }

  // For gpu compositing, a SharedImage mailbox will be allocated and the
  // UIResource will be uploaded into it.
  scoped_refptr<gpu::ClientSharedImage> client_shared_image;
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  // For software compositing, shared memory will be allocated and the
  // UIResource will be copied into it.
  base::MappedReadOnlyRegion shm;
  base::WritableSharedMemoryMapping shared_mapping;
  viz::SharedBitmapId shared_bitmap_id;
  bool overlay_candidate = false;
  // Use sharedImage for software composition;
  bool use_shared_image_software =
      !!layer_tree_frame_sink_->shared_image_interface();

  if (layer_tree_frame_sink_->context_provider()) {
    viz::RasterContextProvider* context_provider =
        layer_tree_frame_sink_->context_provider();
    const auto& shared_image_caps =
        context_provider->SharedImageInterface()->GetCapabilities();
    overlay_candidate =
        settings_.use_gpu_memory_buffer_resources &&
        shared_image_caps.supports_scanout_shared_images &&
        viz::CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(format);
    if (overlay_candidate) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
  } else if (use_shared_image_software) {
    DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);
    // Must not include gpu::SHARED_IMAGE_USAGE_DISPLAY_READ here because
    // DISPLAY_READ means gpu composition.
    shared_image_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE;
    auto sii = layer_tree_frame_sink_->shared_image_interface();
    CHECK(sii);

    auto shared_image_mapping =
        sii->CreateSharedImage({format, upload_size, color_space,
                                shared_image_usage, "LayerTreeHostUIResource"});
    client_shared_image = std::move(shared_image_mapping.shared_image);
    shared_mapping = std::move(shared_image_mapping.mapping);
    CHECK(client_shared_image);
  } else {
    shm = viz::bitmap_allocation::AllocateSharedBitmap(upload_size, format);
    shared_mapping = std::move(shm.mapping);
    shared_bitmap_id = viz::SharedBitmap::GenerateId();
  }

  if (!scaled) {
    // If not scaled, we can copy the pixels 1:1 from the source bitmap to our
    // destination backing of a texture or shared bitmap.
    if (layer_tree_frame_sink_->context_provider()) {
      viz::RasterContextProvider* context_provider =
          layer_tree_frame_sink_->context_provider();
      auto* sii = context_provider->SharedImageInterface();
      client_shared_image = sii->CreateSharedImage(
          {format, upload_size, color_space, shared_image_usage,
           "LayerTreeHostUIResource"},
          bitmap.GetPixels());
      CHECK(client_shared_image);
    } else {
      DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);
      SkImageInfo src_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(source_size));
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));
      sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
          dst_info, shared_mapping.memory(), dst_info.minRowBytes());
      surface->getCanvas()->writePixels(src_info, bitmap.GetPixels().data(),
                                        bitmap.row_bytes(), 0, 0);
    }
  } else {
    // Only support auto-resizing for N32 textures (since this is primarily for
    // scrollbars). Users of other types need to ensure they are not too big.
    DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);

    float canvas_scale_x =
        upload_size.width() / static_cast<float>(source_size.width());
    float canvas_scale_y =
        upload_size.height() / static_cast<float>(source_size.height());

    // Uses N32Premul since that is what SkBitmap's allocN32Pixels makes, and we
    // only support the RGBA8 format here.
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(source_size));

    SkBitmap source_bitmap;
    source_bitmap.setInfo(info, bitmap.row_bytes());
    source_bitmap.setPixels(const_cast<uint8_t*>(bitmap.GetPixels().data()));

    // This applies the scale to draw the |bitmap| into |scaled_surface|. For
    // gpu compositing, we scale into a software bitmap-backed SkSurface here,
    // then upload from there into a texture. For software compositing, we scale
    // directly into the shared memory backing.
    sk_sp<SkSurface> scaled_surface;
    if (layer_tree_frame_sink_->context_provider()) {
      scaled_surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
          upload_size.width(), upload_size.height()));
      CHECK(scaled_surface);  // This would fail in OOM situations.
    } else {
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));
      scaled_surface = SkSurfaces::WrapPixels(dst_info, shared_mapping.memory(),
                                              dst_info.minRowBytes());
      CHECK(scaled_surface);  // This could fail on invalid parameters.
    }
    SkCanvas* scaled_canvas = scaled_surface->getCanvas();
    scaled_canvas->scale(canvas_scale_x, canvas_scale_y);
    // The |canvas_scale_x| and |canvas_scale_y| may have some floating point
    // error for large enough values, causing pixels on the edge to be not
    // fully filled by drawBitmap(), so we ensure they start empty. (See
    // crbug.com/642011 for an example.)
    scaled_canvas->clear(SK_ColorTRANSPARENT);
    scaled_canvas->drawImage(source_bitmap.asImage(), 0, 0);

    if (layer_tree_frame_sink_->context_provider()) {
      SkPixmap pixmap;
      scaled_surface->peekPixels(&pixmap);
      viz::RasterContextProvider* context_provider =
          layer_tree_frame_sink_->context_provider();
      auto* sii = context_provider->SharedImageInterface();
      client_shared_image = sii->CreateSharedImage(
          {format, upload_size, color_space, shared_image_usage,
           "LayerTreeHostUIResource"},
          gfx::SkPixmapToSpan(pixmap));
      CHECK(client_shared_image);
    }
  }

  // Once the backing has the UIResource inside it, we have to prepare it for
  // export to the display compositor via ImportResource(). For gpu compositing,
  // this requires a Mailbox+SyncToken as well. For software compositing, the
  // SharedBitmapId must be notified to the LayerTreeFrameSink. The
  // OnUIResourceReleased() method will be called once the resource is deleted
  // and the display compositor is no longer using it, to free the memory
  // allocated in this method above.
  viz::TransferableResource transferable;
  if (layer_tree_frame_sink_->context_provider()) {
    gpu::SyncToken sync_token = layer_tree_frame_sink_->context_provider()
                                    ->SharedImageInterface()
                                    ->GenUnverifiedSyncToken();

    GLenum texture_target = client_shared_image->GetTextureTarget();
    transferable = viz::TransferableResource::MakeGpu(
        client_shared_image, texture_target, sync_token, upload_size, format,
        overlay_candidate, viz::TransferableResource::ResourceSource::kUI);
  } else if (use_shared_image_software) {
    auto sii = layer_tree_frame_sink_->shared_image_interface();
    gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();
    transferable = viz::TransferableResource::MakeSoftwareSharedImage(
        client_shared_image, sync_token, upload_size, format,
        viz::TransferableResource::ResourceSource::kUI);
  } else {
    layer_tree_frame_sink_->DidAllocateSharedBitmap(std::move(shm.region),
                                                    shared_bitmap_id);
    transferable = viz::TransferableResource::MakeSoftwareSharedBitmap(
        shared_bitmap_id, gpu::SyncToken(), upload_size, format,
        viz::TransferableResource::ResourceSource::kUI);
  }
  transferable.color_space = color_space;
  id = resource_provider_->ImportResource(
      transferable,
      // The OnUIResourceReleased method is bound with a WeakPtr, but the
      // resource backing will be deleted when the LayerTreeFrameSink is
      // removed before shutdown, so nothing leaks if the WeakPtr is
      // invalidated.
      base::BindOnce(&LayerTreeHostImpl::OnUIResourceReleased, AsWeakPtr(),
                     uid));

  UIResourceData data;
  data.opaque = bitmap.GetOpaque();
  if (!use_shared_image_software) {
    data.shared_bitmap_id = shared_bitmap_id;
    data.shared_mapping = std::move(shared_mapping);
  }
  data.shared_image = std::move(client_shared_image);
  data.resource_id_for_export = id;
  ui_resource_map_[uid] = std::move(data);

  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::DeleteUIResource");
  auto it = ui_resource_map_.find(uid);
  if (it != ui_resource_map_.end()) {
    UIResourceData& data = it->second;
    viz::ResourceId id = data.resource_id_for_export;
    // Move the |data| to |deleted_ui_resources_| before removing it from the
    // viz::ClientResourceProvider, so that the ReleaseCallback can see it
    // there.
    deleted_ui_resources_[uid] = std::move(data);
    ui_resource_map_.erase(it);

    resource_provider_->RemoveImportedResource(id);
  }
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResourceBacking(
    UIResourceData data,
    const gpu::SyncToken& sync_token) {
  // Resources are either software or gpu backed, not both.
  DCHECK(!(data.shared_mapping.IsValid() && data.shared_image));

  if (data.shared_mapping.IsValid()) {
    layer_tree_frame_sink_->DidDeleteSharedBitmap(data.shared_bitmap_id);
  }

  if (data.shared_image) {
    if (layer_tree_frame_sink_->context_provider()) {
      auto* sii =
          layer_tree_frame_sink_->context_provider()->SharedImageInterface();
      if (sii) {
        sii->DestroySharedImage(sync_token, std::move(data.shared_image));
      }
    } else {
      auto sii = layer_tree_frame_sink_->shared_image_interface();
      if (sii) {
        sii->DestroySharedImage(sync_token, std::move(data.shared_image));
      }
    }
  }
  // |data| goes out of scope and deletes anything it owned.
}

void LayerTreeHostImpl::OnUIResourceReleased(UIResourceId uid,
                                             const gpu::SyncToken& sync_token,
                                             bool lost) {
  auto it = deleted_ui_resources_.find(uid);
  if (it == deleted_ui_resources_.end()) {
    // Backing was already deleted, eg if the context was lost.
    return;
  }
  UIResourceData& data = it->second;
  // We don't recycle backings here, so |lost| is not relevant, we always delete
  // them.
  DeleteUIResourceBacking(std::move(data), sync_token);
  deleted_ui_resources_.erase(it);
}

void LayerTreeHostImpl::ClearUIResources() {
  for (auto& pair : ui_resource_map_) {
    UIResourceId uid = pair.first;
    UIResourceData& data = pair.second;
    resource_provider_->RemoveImportedResource(data.resource_id_for_export);
    // Immediately drop the backing instead of waiting for the resource to be
    // returned from the ResourceProvider, as this is called in cases where the
    // ability to clean up the backings will go away (context loss, shutdown).
    DeleteUIResourceBacking(std::move(data), gpu::SyncToken());
    // This resource is not deleted, and its |uid| is still valid, so it moves
    // to the evicted list, not the |deleted_ui_resources_| set. Also, its
    // backing is gone, so it would not belong in |deleted_ui_resources_|.
    evicted_ui_resources_.insert(uid);
  }
  ui_resource_map_.clear();
  for (auto& pair : deleted_ui_resources_) {
    UIResourceData& data = pair.second;
    // Immediately drop the backing instead of waiting for the resource to be
    // returned from the ResourceProvider, as this is called in cases where the
    // ability to clean up the backings will go away (context loss, shutdown).
    DeleteUIResourceBacking(std::move(data), gpu::SyncToken());
  }
  deleted_ui_resources_.clear();
}

void LayerTreeHostImpl::EvictAllUIResources() {
  if (ui_resource_map_.empty())
    return;
  while (!ui_resource_map_.empty()) {
    UIResourceId uid = ui_resource_map_.begin()->first;
    DeleteUIResource(uid);
    evicted_ui_resources_.insert(uid);
  }
  client_->SetNeedsCommitOnImplThread();
  client_->OnCanDrawStateChanged(CanDraw());
  client_->RenewTreePriority();
}

viz::ResourceId LayerTreeHostImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  auto iter = ui_resource_map_.find(uid);
  if (iter != ui_resource_map_.end())
    return iter->second.resource_id_for_export;
  return viz::kInvalidResourceId;
}

bool LayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid) const {
  auto iter = ui_resource_map_.find(uid);
  CHECK(iter != ui_resource_map_.end(), base::NotFatalUntil::M130);
  return iter->second.opaque;
}

bool LayerTreeHostImpl::EvictedUIResourcesExist() const {
  return !evicted_ui_resources_.empty();
}

void LayerTreeHostImpl::MarkUIResourceNotEvicted(UIResourceId uid) {
  auto found_in_evicted = evicted_ui_resources_.find(uid);
  if (found_in_evicted == evicted_ui_resources_.end())
    return;
  evicted_ui_resources_.erase(found_in_evicted);
  if (evicted_ui_resources_.empty())
    client_->OnCanDrawStateChanged(CanDraw());
}

void LayerTreeHostImpl::ScheduleMicroBenchmark(
    std::unique_ptr<MicroBenchmarkImpl> benchmark) {
  micro_benchmark_controller_.ScheduleRun(std::move(benchmark));
}

void LayerTreeHostImpl::InsertLatencyInfoSwapPromiseMonitor(
    LatencyInfoSwapPromiseMonitor* monitor) {
  latency_info_swap_promise_monitor_.insert(monitor);
}

void LayerTreeHostImpl::RemoveLatencyInfoSwapPromiseMonitor(
    LatencyInfoSwapPromiseMonitor* monitor) {
  latency_info_swap_promise_monitor_.erase(monitor);
}

void LayerTreeHostImpl::NotifyLatencyInfoSwapPromiseMonitors() {
  for (LatencyInfoSwapPromiseMonitor* monitor :
       latency_info_swap_promise_monitor_) {
    monitor->OnSetNeedsRedrawOnImpl();
  }
}

bool LayerTreeHostImpl::IsOwnerThread() const {
  bool result;
  if (task_runner_provider_->ImplThreadTaskRunner()) {
    result = task_runner_provider_->ImplThreadTaskRunner()
                 ->RunsTasksInCurrentSequence();
  } else {
    result = task_runner_provider_->MainThreadTaskRunner()
                 ->RunsTasksInCurrentSequence();
    // There's no impl thread, and we're not on the main thread; where are we?
    DCHECK(result);
  }
  return result;
}

// LayerTreeHostImpl has no "protected sequence" (yet).
bool LayerTreeHostImpl::InProtectedSequence() const {
  return false;
}

void LayerTreeHostImpl::WaitForProtectedSequenceCompletion() const {}

bool LayerTreeHostImpl::IsElementInPropertyTrees(
    ElementId element_id,
    ElementListType list_type) const {
  if (list_type == ElementListType::ACTIVE)
    return active_tree() && active_tree()->IsElementInPropertyTree(element_id);

  return (pending_tree() &&
          pending_tree()->IsElementInPropertyTree(element_id)) ||
         (recycle_tree() &&
          recycle_tree()->IsElementInPropertyTree(element_id));
}

void LayerTreeHostImpl::SetMutatorsNeedCommit() {}

void LayerTreeHostImpl::SetMutatorsNeedRebuildPropertyTrees() {}

void LayerTreeHostImpl::SetTreeLayerScrollOffsetMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const gfx::PointF& scroll_offset) {
  if (!tree)
    return;

  PropertyTrees* property_trees = tree->property_trees();
  DCHECK_EQ(1u, property_trees->scroll_tree().element_id_to_node_index().count(
                    element_id));
  const ScrollNode* scroll_node =
      property_trees->scroll_tree().FindNodeFromElementId(element_id);
  // TODO(crbug.com/40828469): We should aim to prevent this condition from
  // happening and either remove this check or make it fatal.
  DCHECK(scroll_node);
  if (!scroll_node)
    return;
  property_trees->scroll_tree_mutable().OnScrollOffsetAnimated(
      element_id, scroll_node->id, scroll_offset, tree);
}

void LayerTreeHostImpl::SetElementFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& filters) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetFilterMutated(element_id, filters);
  } else {
    if (pending_tree())
      pending_tree()->SetFilterMutated(element_id, filters);
    if (recycle_tree())
      recycle_tree()->SetFilterMutated(element_id, filters);
  }
}

void LayerTreeHostImpl::OnCustomPropertyMutated(
    PaintWorkletInput::PropertyKey property_key,
    PaintWorkletInput::PropertyValue property_value) {
  paint_worklet_tracker_.OnCustomPropertyMutated(std::move(property_key),
                                                 std::move(property_value));
}

bool LayerTreeHostImpl::RunsOnCurrentThread() const {
  // If there is no impl thread, then we assume the current thread is ok.
  return !task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread();
}

void LayerTreeHostImpl::SetElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
  } else {
    if (pending_tree())
      pending_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
    if (recycle_tree())
      recycle_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
  }
}

void LayerTreeHostImpl::SetElementOpacityMutated(ElementId element_id,
                                                 ElementListType list_type,
                                                 float opacity) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetOpacityMutated(element_id, opacity);
  } else {
    if (pending_tree())
      pending_tree()->SetOpacityMutated(element_id, opacity);
    if (recycle_tree())
      recycle_tree()->SetOpacityMutated(element_id, opacity);
  }
}

void LayerTreeHostImpl::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetTransformMutated(element_id, transform);
  } else {
    if (pending_tree())
      pending_tree()->SetTransformMutated(element_id, transform);
    if (recycle_tree())
      recycle_tree()->SetTransformMutated(element_id, transform);
  }
}

void LayerTreeHostImpl::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::PointF& scroll_offset) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerScrollOffsetMutated(element_id, active_tree(), scroll_offset);
    ShowScrollbarsForImplScroll(element_id);
  } else {
    SetTreeLayerScrollOffsetMutated(element_id, pending_tree(), scroll_offset);
    SetTreeLayerScrollOffsetMutated(element_id, recycle_tree(), scroll_offset);
  }
}

void LayerTreeHostImpl::ElementIsAnimatingChanged(
    const PropertyToElementIdMap& element_id_map,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  LayerTreeImpl* tree =
      list_type == ElementListType::ACTIVE ? active_tree() : pending_tree();
  // TODO(wkorman): Explore enabling DCHECK in ElementIsAnimatingChanged()
  // below. Currently enabling causes batch of unit test failures.
  if (tree && tree->property_trees()->ElementIsAnimatingChanged(
                  element_id_map, mask, state, false))
    tree->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::MaximumScaleChanged(ElementId element_id,
                                            ElementListType list_type,
                                            float maximum_scale) {
  if (LayerTreeImpl* tree = list_type == ElementListType::ACTIVE
                                ? active_tree()
                                : pending_tree()) {
    tree->property_trees()->MaximumAnimationScaleChanged(element_id,
                                                         maximum_scale);
  }
}

void LayerTreeHostImpl::ScrollOffsetAnimationFinished() {
  if (input_delegate_)
    input_delegate_->ScrollOffsetAnimationFinished();
}

void LayerTreeHostImpl::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType tree_type) {
  client_->NotifyAnimationWorkletStateChange(state, tree_type);
  if (state != AnimationWorkletMutationState::CANCELED) {
    // We have at least one active worklet animation. We need to request a new
    // frame to keep the animation ticking.
    SetNeedsOneBeginImplFrame();
    if (state == AnimationWorkletMutationState::COMPLETED_WITH_UPDATE &&
        tree_type == ElementListType::ACTIVE) {
      SetNeedsRedraw();
    }
  }
}

bool LayerTreeHostImpl::CommitsToActiveTree() const {
  return settings_.commit_to_active_tree;
}

void LayerTreeHostImpl::SetContextVisibility(bool is_visible) {
  if (!layer_tree_frame_sink_)
    return;

  // Update the compositor context. If we are already in the correct visibility
  // state, skip. This can happen if we transition invisible/visible rapidly,
  // before we get a chance to go invisible in NotifyAllTileTasksComplete.
  auto* compositor_context = layer_tree_frame_sink_->context_provider();
  if (compositor_context && is_visible != !!compositor_context_visibility_) {
    if (is_visible) {
      compositor_context_visibility_ =
          compositor_context->CacheController()->ClientBecameVisible();
    } else {
      compositor_context->CacheController()->ClientBecameNotVisible(
          std::move(compositor_context_visibility_));
    }
  }

  // Update the worker context. If we are already in the correct visibility
  // state, skip. This can happen if we transition invisible/visible rapidly,
  // before we get a chance to go invisible in NotifyAllTileTasksComplete.
  auto* worker_context = layer_tree_frame_sink_->worker_context_provider();
  if (worker_context && is_visible != !!worker_context_visibility_) {
    viz::RasterContextProvider::ScopedRasterContextLock hold(worker_context);
    if (is_visible) {
      worker_context_visibility_ =
          worker_context->CacheController()->ClientBecameVisible();
    } else {
      worker_context->CacheController()->ClientBecameNotVisible(
          std::move(worker_context_visibility_));
    }
  }
}

void LayerTreeHostImpl::ShowScrollbarsForImplScroll(ElementId element_id) {
  if (settings_.scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(true);
    return;
  }
  if (!element_id)
    return;
  if (ScrollbarAnimationController* animation_controller =
          ScrollbarAnimationControllerForElementId(element_id))
    animation_controller->DidScrollUpdate();
}

void LayerTreeHostImpl::InitializeUkm(
    std::unique_ptr<ukm::UkmRecorder> recorder) {
  compositor_frame_reporting_controller_->InitializeUkmManager(
      std::move(recorder));
}

void LayerTreeHostImpl::SetActiveURL(const GURL& url, ukm::SourceId source_id) {
  tile_manager_.set_active_url(url);
  has_observed_first_scroll_delay_ = false;
  // The active tree might still be from content for the previous page when the
  // recorder is updated here, since new content will be pushed with the next
  // main frame. But we should only get a few impl frames wrong here in that
  // case. Also, since checkerboard stats are only recorded with user
  // interaction, it must be in progress when the navigation commits for this
  // case to occur.
  // The source id has already been associated to the URL.
  compositor_frame_reporting_controller_->SetSourceId(source_id);
  total_frame_counter_.Reset();
  dropped_frame_counter_.Reset();
  is_measuring_smoothness_ = false;
}

void LayerTreeHostImpl::SetUkmSmoothnessDestination(
    base::WritableSharedMemoryMapping ukm_smoothness_data) {
  dropped_frame_counter_.SetUkmSmoothnessDestination(
      ukm_smoothness_data.GetMemoryAs<UkmSmoothnessDataShared>());
  ukm_smoothness_mapping_ = std::move(ukm_smoothness_data);
}

void LayerTreeHostImpl::NotifyDidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    std::vector<PresentationTimeCallbackBuffer::SuccessfulCallback> callbacks,
    const viz::FrameTimingDetails& details) {
  for (auto& callback : callbacks)
    std::move(callback).Run(details.presentation_feedback.timestamp);
}

void LayerTreeHostImpl::AllocateLocalSurfaceId() {
  child_local_surface_id_allocator_.GenerateId();
}

void LayerTreeHostImpl::RequestBeginFrameForAnimatedImages() {
  SetNeedsOneBeginImplFrame();
}

void LayerTreeHostImpl::RequestInvalidationForAnimatedImages() {
  DCHECK_EQ(impl_thread_phase_, ImplThreadPhase::INSIDE_IMPL_FRAME);

  // If we are animating an image, we want at least one draw of the active tree
  // before a new tree is activated.
  bool needs_first_draw_on_activation = true;
  client_->SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
}

bool LayerTreeHostImpl::IsReadyToActivate() const {
  return client_->IsReadyToActivate();
}

void LayerTreeHostImpl::RequestImplSideInvalidationForRerasterTiling() {
  bool needs_first_draw_on_activation = true;
  client_->SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
}

void LayerTreeHostImpl::RequestImplSideInvalidationForRasterInducingScroll(
    ElementId scroll_element_id) {
  client_->SetNeedsImplSideInvalidation(
      /*needs_first_draw_on_activation=*/true);
  pending_invalidation_raster_inducing_scrolls_.insert(scroll_element_id);
}

base::WeakPtr<LayerTreeHostImpl> LayerTreeHostImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void LayerTreeHostImpl::ApplyFirstScrollTracking(const ui::LatencyInfo& latency,
                                                 uint32_t frame_token) {
  base::TimeTicks creation_timestamp;

  // If `latency` isn't tracking a scroll, we don't need to do extra
  // first-scroll tracking.
  if (!latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          &creation_timestamp) &&
      !latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          &creation_timestamp)) {
    return;
  }

  // Construct a callback that, given a successful presentation timestamp, will
  // report the time span between the scroll input-event creation and the
  // presentation timestamp.
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallback> callbacks;
  callbacks.push_back(base::BindOnce(
      [](base::TimeTicks event_creation, int source_frame_number,
         LayerTreeHostImpl* layer_tree_host_impl,
         base::TimeTicks presentation_timestamp) {
        layer_tree_host_impl->DidObserveScrollDelay(
            source_frame_number, presentation_timestamp - event_creation,
            event_creation);
      },
      creation_timestamp, active_tree_->source_frame_number(), this));

  // Register the callback to run with the presentation timestamp corresponding
  // to the given `frame_token`.
  presentation_time_callbacks_.RegisterCompositorThreadSuccessfulCallbacks(
      frame_token, std::move(callbacks));
}

bool LayerTreeHostImpl::RunningOnRendererProcess() const {
  // The browser process uses |SingleThreadProxy| whereas the renderers use
  // |ProxyMain|. This is more of an implementation detail, but we can use
  // that here to determine the process type.
  return !settings().single_thread_proxy_scheduler;
}

void LayerTreeHostImpl::SetNeedsRedrawOrUpdateDisplayTree() {
  if (use_layer_context_for_display_) {
    SetNeedsUpdateDisplayTree();
  } else {
    SetNeedsRedraw();
  }
}

}  // namespace cc
