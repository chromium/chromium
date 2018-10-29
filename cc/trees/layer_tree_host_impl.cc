// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_state.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/scroller_size_metrics.h"
#include "cc/input/snap_selection_strategy.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/viewport.h"
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
#include "cc/trees/clip_node.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/debug_rect_history.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/frame_rate_counter.h"
#include "cc/trees/image_animation_controller.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/gpu/texture_allocation.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/traced_value.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

// Used to accommodate finite precision when comparing scaled viewport and
// content widths. While this value may seem large, width=device-width on an N7
// V1 saw errors of ~0.065 between computed window and content widths.
const float kMobileViewportWidthEpsilon = 0.15f;

bool HasFixedPageScale(LayerTreeImpl* active_tree) {
  return active_tree->min_page_scale_factor() ==
         active_tree->max_page_scale_factor();
}

bool HasMobileViewport(LayerTreeImpl* active_tree) {
  float window_width_dip = active_tree->current_page_scale_factor() *
                           active_tree->ScrollableViewportSize().width();
  float content_width_css = active_tree->ScrollableSize().width();
  return content_width_css <= window_width_dip + kMobileViewportWidthEpsilon;
}

bool IsMobileOptimized(LayerTreeImpl* active_tree) {
  bool has_mobile_viewport = HasMobileViewport(active_tree);
  bool has_fixed_page_scale = HasFixedPageScale(active_tree);
  return has_fixed_page_scale || has_mobile_viewport;
}

viz::ResourceFormat TileRasterBufferFormat(
    const LayerTreeSettings& settings,
    viz::ContextProvider* context_provider,
    bool use_gpu_rasterization) {
  // Software compositing always uses the native skia RGBA N32 format, but we
  // just call it RGBA_8888 everywhere even though it can be BGRA ordering,
  // because we don't need to communicate the actual ordering as the code all
  // assumes the native skia format.
  if (!context_provider)
    return viz::RGBA_8888;

  // RGBA4444 overrides the defaults if specified, but only for gpu compositing.
  // It is always supported on platforms where it is specified.
  if (settings.use_rgba_4444)
    return viz::RGBA_4444;
  // Otherwise we use BGRA textures if we can but it depends on the context
  // capabilities, and we have different preferences when rastering to textures
  // vs uploading textures.
  const gpu::Capabilities& caps = context_provider->ContextCapabilities();
  if (use_gpu_rasterization)
    return viz::PlatformColor::BestSupportedRenderBufferFormat(caps);
  return viz::PlatformColor::BestSupportedTextureFormat(caps);
}

// Small helper class that saves the current viewport location as the user sees
// it and resets to the same location.
class ViewportAnchor {
 public:
  ViewportAnchor(ScrollNode* inner_scroll,
                 LayerImpl* outer_scroll,
                 LayerTreeImpl* tree_impl)
      : inner_(inner_scroll), outer_(outer_scroll), tree_impl_(tree_impl) {
    viewport_in_content_coordinates_ =
        scroll_tree().current_scroll_offset(inner_->element_id);

    if (outer_)
      viewport_in_content_coordinates_ += outer_->CurrentScrollOffset();
  }

  void ResetViewportToAnchoredPosition() {
    DCHECK(outer_);

    scroll_tree().ClampScrollToMaxScrollOffset(inner_, tree_impl_);
    outer_->ClampScrollToMaxScrollOffset();

    gfx::ScrollOffset viewport_location =
        scroll_tree().current_scroll_offset(inner_->element_id) +
        outer_->CurrentScrollOffset();

    gfx::Vector2dF delta =
        viewport_in_content_coordinates_.DeltaFrom(viewport_location);

    delta = scroll_tree().ScrollBy(inner_, delta, tree_impl_);
    outer_->ScrollBy(delta);
  }

 private:
  ScrollTree& scroll_tree() {
    return tree_impl_->property_trees()->scroll_tree;
  }

  ScrollNode* inner_;
  LayerImpl* outer_;
  LayerTreeImpl* tree_impl_;
  gfx::ScrollOffset viewport_in_content_coordinates_;
};

void DidVisibilityChange(LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_ASYNC_BEGIN1("cc", "LayerTreeHostImpl::SetVisible", id,
                             "LayerTreeHostImpl", id);
    return;
  }

  TRACE_EVENT_ASYNC_END0("cc", "LayerTreeHostImpl::SetVisible", id);
}

bool IsWheelBasedScroll(InputHandler::ScrollInputType type) {
  return type == InputHandler::WHEEL;
}

enum ScrollThread { MAIN_THREAD, CC_THREAD };

void RecordCompositorSlowScrollMetric(InputHandler::ScrollInputType type,
                                      ScrollThread scroll_thread) {
  bool scroll_on_main_thread = (scroll_thread == MAIN_THREAD);
  if (IsWheelBasedScroll(type)) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorWheelScrollUpdateThread",
                          scroll_on_main_thread);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorTouchScrollUpdateThread",
                          scroll_on_main_thread);
  }
}

ui::FrameMetricsSettings LTHI_FrameMetricsSettings(
    const LayerTreeSettings& settings) {
  ui::FrameMetricsSource source =
      settings.commit_to_active_tree
          ? ui::FrameMetricsSource::UiCompositor
          : ui::FrameMetricsSource::RendererCompositor;
  ui::FrameMetricsSourceThread source_thread =
      settings.commit_to_active_tree
          ? ui::FrameMetricsSourceThread::UiCompositor
          : ui::FrameMetricsSourceThread::RendererCompositor;
  ui::FrameMetricsCompileTarget compile_target =
      settings.using_synchronous_renderer_compositor
          ? ui::FrameMetricsCompileTarget::SynchronousCompositor
          : settings.wait_for_all_pipeline_stages_before_draw
                ? ui::FrameMetricsCompileTarget::Headless
                : ui::FrameMetricsCompileTarget::Chromium;
  return ui::FrameMetricsSettings(source, source_thread, compile_target);
}

}  // namespace

DEFINE_SCOPED_UMA_HISTOGRAM_TIMER(PendingTreeDurationHistogramTimer,
                                  "Scheduling.%s.PendingTreeDuration");
DEFINE_SCOPED_UMA_HISTOGRAM_TIMER(PendingTreeRasterDurationHistogramTimer,
                                  "Scheduling.%s.PendingTreeRasterDuration");

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
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner) {
  return base::WrapUnique(new LayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), id,
      std::move(image_worker_task_runner)));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner)
    : client_(client),
      task_runner_provider_(task_runner_provider),
      current_begin_frame_tracker_(BEGINFRAMETRACKER_FROM_HERE),
      settings_(settings),
      is_synchronous_single_threaded_(!task_runner_provider->HasImplThread() &&
                                      !settings_.single_thread_proxy_scheduler),
      resource_provider_(settings_.delegated_sync_points_required),
      cached_managed_memory_policy_(settings.memory_policy),
      // Must be initialized after is_synchronous_single_threaded_ and
      // task_runner_provider_.
      tile_manager_(this,
                    GetTaskRunner(),
                    std::move(image_worker_task_runner),
                    is_synchronous_single_threaded_
                        ? std::numeric_limits<size_t>::max()
                        : settings.scheduled_raster_task_limit,
                    settings.ToTileManagerSettings()),
      fps_counter_(
          FrameRateCounter::Create(task_runner_provider_->HasImplThread())),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      mutator_host_(std::move(mutator_host)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      micro_benchmark_controller_(this),
      task_graph_runner_(task_graph_runner),
      id_(id),
      consecutive_frame_with_damage_count_(settings.damaged_frame_limit),
      scroll_animating_latched_element_id_(kInvalidElementId),
      // It is safe to use base::Unretained here since we will outlive the
      // ImageAnimationController.
      image_animation_controller_(
          GetTaskRunner(),
          base::BindRepeating(
              &LayerTreeHostImpl::RequestInvalidationForAnimatedImages,
              base::Unretained(this)),
          settings_.enable_image_animation_resync),
      frame_metrics_(LTHI_FrameMetricsSettings(settings_)),
      skipped_frame_tracker_(&frame_metrics_),
      is_animating_for_snap_(false),
      paint_image_generator_client_id_(PaintImage::GetNextGeneratorClientId()) {
  DCHECK(mutator_host_);
  mutator_host_->SetMutatorHostClient(this);

  DCHECK(task_runner_provider_->IsImplThread());
  DidVisibilityChange(this, visible_);

  // LTHI always has an active tree.
  active_tree_ = std::make_unique<LayerTreeImpl>(
      this, new SyncedProperty<ScaleGroup>, new SyncedBrowserControls,
      new SyncedElasticOverscroll);
  active_tree_->property_trees()->is_active = true;

  viewport_ = Viewport::Create(this);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                                     "cc::LayerTreeHostImpl", id_);

  browser_controls_offset_manager_ = BrowserControlsOffsetManager::Create(
      this, settings.top_controls_show_threshold,
      settings.top_controls_hide_threshold);

  memory_pressure_listener_.reset(
      new base::MemoryPressureListener(base::BindRepeating(
          &LayerTreeHostImpl::OnMemoryPressure, base::Unretained(this))));

  SetDebugState(settings.initial_debug_state);
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
  DCHECK(!image_decode_cache_);
  DCHECK(!single_thread_synchronous_task_graph_runner_);

  if (input_handler_client_) {
    input_handler_client_->WillShutdown();
    input_handler_client_ = nullptr;
  }
  if (scroll_elasticity_helper_)
    scroll_elasticity_helper_.reset();

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
  resource_provider_.ShutdownAndReleaseAllResources();

  mutator_host_->ClearMutators();
  mutator_host_->SetMutatorHostClient(nullptr);
}

void LayerTreeHostImpl::BeginMainFrameAborted(
    CommitEarlyOutReason reason,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises) {
  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  if (CommitEarlyOutHandledCommit(reason)) {
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit();
    if (pending_tree_) {
      pending_tree_->AppendSwapPromises(std::move(swap_promises));
    } else {
      for (const auto& swap_promise : swap_promises)
        swap_promise->DidNotSwap(SwapPromise::COMMIT_NO_UPDATE);
    }
  }
}

void LayerTreeHostImpl::BeginCommit() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  if (!CommitToActiveTree())
    CreatePendingTree();
}

void LayerTreeHostImpl::CommitComplete() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::CommitComplete");

  // In high latency mode commit cannot finish within the same frame. We need to
  // flush input here to make sure they got picked up by |PrepareTiles()|.
  if (input_handler_client_ && impl_thread_phase_ == ImplThreadPhase::IDLE)
    input_handler_client_->DeliverInputForBeginFrame();

  if (CommitToActiveTree()) {
    active_tree_->HandleScrollbarShowRequestsFromMain();

    // We have to activate animations here or "IsActive()" is true on the layers
    // but the animations aren't activated yet so they get ignored by
    // UpdateDrawProperties.
    ActivateAnimations();
  }

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results. When doing commit to the active tree, this must happen
  // after ActivateAnimations() in order for this ticking to be propogated
  // to layers on the active tree.
  if (CommitToActiveTree())
    Animate();
  else
    AnimatePendingTreeAfterCommit();

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
  micro_benchmark_controller_.DidCompleteCommit();
}

void LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation() {
  // LayerTreeHost may have changed the GPU rasterization flags state, which
  // may require an update of the tree resources.
  UpdateTreeResourcesForGpuRasterizationIfNeeded();
  sync_tree()->set_needs_update_draw_properties();

  // We need an update immediately post-commit to have the opportunity to create
  // tilings.
  // We can avoid updating the ImageAnimationController during this
  // DrawProperties update since it will be done when we animate the controller
  // below.
  bool update_image_animation_controller = false;
  sync_tree()->UpdateDrawProperties(update_image_animation_controller);
  // Because invalidations may be coming from the main thread, it's
  // safe to do an update for lcd text at this point and see if lcd text needs
  // to be disabled on any layers.
  // It'd be ideal if this could be done earlier, but when the raster source
  // is updated from the main thread during push properties, update draw
  // properties has not occurred yet and so it's not clear whether or not the
  // layer can or cannot use lcd text.  So, this is the cleanup pass to
  // determine if lcd state needs to switch due to draw properties.
  sync_tree()->UpdateCanUseLCDText();

  // Defer invalidating images until UpdateDrawProperties is performed since
  // that updates whether an image should be animated based on its visibility
  // and the updated data for the image from the main frame.
  PaintImageIdFlatSet images_to_invalidate =
      tile_manager_.TakeImagesToInvalidateOnSyncTree();
  if (ukm_manager_)
    ukm_manager_->AddCheckerboardedImages(images_to_invalidate.size());

  const auto& animated_images = image_animation_controller_.AnimateForSyncTree(
      CurrentBeginFrameArgs().frame_time);
  images_to_invalidate.insert(animated_images.begin(), animated_images.end());
  sync_tree()->InvalidateRegionForImages(images_to_invalidate);

  // Note that it is important to push the state for checkerboarded and animated
  // images prior to PrepareTiles here when committing to the active tree. This
  // is because new tiles on the active tree depend on tree specific state
  // cached in these components, which must be pushed to active before preparing
  // tiles for the updated active tree.
  if (CommitToActiveTree())
    ActivateStateForImages();

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
    if (CommitToActiveTree())
      NotifyReadyToDraw();
  } else if (!CommitToActiveTree()) {
    DCHECK(!pending_tree_raster_duration_timer_);
    pending_tree_raster_duration_timer_ =
        std::make_unique<PendingTreeRasterDurationHistogramTimer>();
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

  if (input_handler_client_) {
    // This animates fling scrolls. But on Android WebView root flings are
    // controlled by the application, so the compositor does not animate them.
    bool ignore_fling =
        settings_.ignore_root_layer_flings && IsCurrentlyScrollingViewport();
    if (!ignore_fling) {
      // This does not set did_animate, because if the InputHandlerClient
      // changes anything it will be through the InputHandler interface which
      // does SetNeedsRedraw.
      input_handler_client_->Animate(monotonic_time);
    }
  }

  did_animate |= AnimatePageScale(monotonic_time);
  did_animate |= AnimateLayers(monotonic_time, /* is_active_tree */ true);
  did_animate |= AnimateScrollbars(monotonic_time);
  did_animate |= AnimateBrowserControls(monotonic_time);

    // Animating stuff can change the root scroll offset, so inform the
    // synchronous input handler.
  UpdateRootLayerStateForSynchronousInputHandler();
  if (did_animate) {
    // If the tree changed, then we want to draw at the end of the current
    // frame.
    SetNeedsRedraw();
  }
}


bool LayerTreeHostImpl::PrepareTiles() {
  if (!tile_priorities_dirty_)
    return false;

  client_->WillPrepareTiles();
  bool did_prepare_tiles = tile_manager_.PrepareTiles(global_tile_state_);
  if (did_prepare_tiles)
    tile_priorities_dirty_ = false;
  client_->DidPrepareTiles();
  return did_prepare_tiles;
}

void LayerTreeHostImpl::StartPageScaleAnimation(
    const gfx::Vector2d& target_offset,
    bool anchor_point,
    float page_scale,
    base::TimeDelta duration) {
  if (!InnerViewportScrollNode())
    return;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();
  gfx::SizeF scrollable_size = active_tree_->ScrollableSize();
  gfx::SizeF viewport_size =
      gfx::SizeF(active_tree_->InnerViewportContainerLayer()->bounds());

  // TODO(miletus) : Pass in ScrollOffset.
  page_scale_animation_ =
      PageScaleAnimation::Create(ScrollOffsetToVector2dF(scroll_total),
                                 active_tree_->current_page_scale_factor(),
                                 viewport_size, scrollable_size);

  if (anchor_point) {
    gfx::Vector2dF anchor(target_offset);
    page_scale_animation_->ZoomWithAnchor(anchor, page_scale,
                                          duration.InSecondsF());
  } else {
    gfx::Vector2dF scaled_target_offset = target_offset;
    page_scale_animation_->ZoomTo(scaled_target_offset, page_scale,
                                  duration.InSecondsF());
  }

  SetNeedsOneBeginImplFrame();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetNeedsAnimateInput() {
  DCHECK(!IsCurrentlyScrollingViewport() ||
         !settings_.ignore_root_layer_flings);
  SetNeedsOneBeginImplFrame();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingViewport() const {
  auto* node = CurrentlyScrollingNode();
  if (!node)
    return false;
  if (!viewport()->MainScrollLayer())
    return false;
  return node->id == viewport()->MainScrollLayer()->scroll_tree_index();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingLayerAt(
    const gfx::Point& viewport_point,
    InputHandler::ScrollInputType type) const {
  auto* scrolling_node = CurrentlyScrollingNode();
  if (!scrolling_node)
    return false;

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  bool scroll_on_main_thread = false;
  uint32_t main_thread_scrolling_reasons;
  auto* test_scroll_node = FindScrollNodeForDeviceViewportPoint(
      device_viewport_point, type, layer_impl, &scroll_on_main_thread,
      &main_thread_scrolling_reasons);

  if (scroll_on_main_thread)
    return false;

  if (scrolling_node == test_scroll_node)
    return true;

  // For active scrolling state treat the inner/outer viewports interchangeably.
  if (scrolling_node->scrolls_inner_viewport ||
      scrolling_node->scrolls_outer_viewport) {
    return test_scroll_node == OuterViewportScrollNode();
  }

  return false;
}

EventListenerProperties LayerTreeHostImpl::GetEventListenerProperties(
    EventListenerClass event_class) const {
  return active_tree_->event_listener_properties(event_class);
}

// Return true if scrollable node for 'ancestor' is the same as 'child' or an
// ancestor along the scroll tree.
bool LayerTreeHostImpl::IsScrolledBy(LayerImpl* child, ScrollNode* ancestor) {
  DCHECK(ancestor && ancestor->scrollable);
  if (!child)
    return false;
  DCHECK_EQ(child->layer_tree_impl(), active_tree_.get());
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  for (ScrollNode* scroll_node = scroll_tree.Node(child->scroll_tree_index());
       scroll_node; scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->id == ancestor->id)
      return true;
  }
  return false;
}

InputHandler::TouchStartOrMoveEventListenerType
LayerTreeHostImpl::EventListenerTypeForTouchStartOrMoveAt(
    const gfx::Point& viewport_point,
    TouchAction* out_touch_action) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl_with_touch_handler =
      active_tree_->FindLayerThatIsHitByPointInTouchHandlerRegion(
          device_viewport_point);

  if (layer_impl_with_touch_handler == nullptr) {
    if (out_touch_action)
      *out_touch_action = kTouchActionAuto;
    return InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
  }

  if (out_touch_action) {
    gfx::Transform layer_screen_space_transform =
        layer_impl_with_touch_handler->ScreenSpaceTransform();
    gfx::Transform inverse_layer_screen_space(
        gfx::Transform::kSkipInitialization);
    bool can_be_inversed =
        layer_screen_space_transform.GetInverse(&inverse_layer_screen_space);
    // Getting here indicates that |layer_impl_with_touch_handler| is non-null,
    // which means that the |hit| in FindClosestMatchingLayer() is true, which
    // indicates that the inverse is available.
    DCHECK(can_be_inversed);
    bool clipped = false;
    gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
        inverse_layer_screen_space, device_viewport_point, &clipped);
    gfx::PointF hit_test_point_in_layer_space =
        gfx::PointF(planar_point.x(), planar_point.y());
    const auto& region = layer_impl_with_touch_handler->touch_action_region();
    gfx::Point point = gfx::ToRoundedPoint(hit_test_point_in_layer_space);
    *out_touch_action = region.GetWhiteListedTouchAction(point);
  }

  if (!CurrentlyScrollingNode())
    return InputHandler::TouchStartOrMoveEventListenerType::HANDLER;

  // Check if the touch start (or move) hits on the current scrolling layer or
  // its descendant. layer_impl_with_touch_handler is the layer hit by the
  // pointer and has an event handler, otherwise it is null. We want to compare
  // the most inner layer we are hitting on which may not have an event listener
  // with the actual scrolling layer.
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);
  bool is_ancestor = IsScrolledBy(layer_impl, CurrentlyScrollingNode());
  return is_ancestor ? InputHandler::TouchStartOrMoveEventListenerType::
                           HANDLER_ON_SCROLLING_LAYER
                     : InputHandler::TouchStartOrMoveEventListenerType::HANDLER;
}

bool LayerTreeHostImpl::HasBlockingWheelEventHandlerAt(
    const gfx::Point& viewport_point) const {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl_with_wheel_event_handler =
      active_tree_->FindLayerThatIsHitByPointInWheelEventHandlerRegion(
          device_viewport_point);

  return layer_impl_with_wheel_event_handler;
}

std::unique_ptr<SwapPromiseMonitor>
LayerTreeHostImpl::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return base::WrapUnique(
      new LatencyInfoSwapPromiseMonitor(latency, nullptr, this));
}

ScrollElasticityHelper* LayerTreeHostImpl::CreateScrollElasticityHelper() {
  DCHECK(!scroll_elasticity_helper_);
  if (settings_.enable_elastic_overscroll) {
    scroll_elasticity_helper_.reset(
        ScrollElasticityHelper::CreateForLayerTreeHostImpl(this));
  }
  return scroll_elasticity_helper_.get();
}

bool LayerTreeHostImpl::GetScrollOffsetForLayer(ElementId element_id,
                                                gfx::ScrollOffset* offset) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;
  *offset = scroll_tree.current_scroll_offset(element_id);
  return true;
}

bool LayerTreeHostImpl::ScrollLayerTo(ElementId element_id,
                                      const gfx::ScrollOffset& offset) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;

  scroll_tree.ScrollBy(
      scroll_node,
      ScrollOffsetToVector2dF(offset -
                              scroll_tree.current_scroll_offset(element_id)),
      active_tree());
  return true;
}

bool LayerTreeHostImpl::ScrollingShouldSwitchtoMainThread() {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();

  if (!scroll_node)
    return true;

  for (; scroll_tree.parent(scroll_node);
       scroll_node = scroll_tree.parent(scroll_node)) {
    if (!!scroll_node->main_thread_scrolling_reasons)
      return true;
  }

  return false;
}

void LayerTreeHostImpl::QueueSwapPromiseForMainThreadScrollUpdate(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promises_for_main_thread_scroll_update_.push_back(
      std::move(swap_promise));
}

void LayerTreeHostImpl::FrameData::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_no_damage", has_no_damage);

  // Quad data can be quite large, so only dump render passes if we select
  // viz.quads.
  bool quads_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     &quads_enabled);
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (size_t i = 0; i < render_passes.size(); ++i) {
      value->BeginDictionary();
      render_passes[i]->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

DrawMode LayerTreeHostImpl::GetDrawMode() const {
  if (resourceless_software_draw_) {
    return DRAW_MODE_RESOURCELESS_SOFTWARE;
  } else if (layer_tree_frame_sink_->context_provider()) {
    return DRAW_MODE_HARDWARE;
  } else {
    return DRAW_MODE_SOFTWARE;
  }
}

static void AppendQuadsToFillScreen(
    const gfx::Rect& root_scroll_layer_rect,
    viz::RenderPass* target_render_pass,
    const RenderSurfaceImpl* root_render_surface,
    SkColor screen_background_color,
    const Region& fill_region) {
  if (!root_render_surface || !SkColorGetA(screen_background_color))
    return;
  if (fill_region.IsEmpty())
    return;

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  gfx::Rect root_target_rect = root_render_surface->content_rect();
  float opacity = 1.f;
  int sorting_context_id = 0;
  bool are_contents_opaque = SkColorGetA(screen_background_color) == 0xFF;
  viz::SharedQuadState* shared_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), root_target_rect,
                            root_target_rect, root_target_rect, false,
                            are_contents_opaque, opacity, SkBlendMode::kSrcOver,
                            sorting_context_id);

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

static viz::RenderPass* FindRenderPassById(const viz::RenderPassList& list,
                                           viz::RenderPassId id) {
  auto it = std::find_if(
      list.begin(), list.end(),
      [id](const std::unique_ptr<viz::RenderPass>& p) { return p->id == id; });
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

  // If the root render surface has no visible damage, then don't generate a
  // frame at all.
  const RenderSurfaceImpl* root_surface = active_tree->RootRenderSurface();
  bool root_surface_has_visible_damage =
      root_surface->GetDamageRect().Intersects(root_surface->content_rect());
  bool hud_wants_to_draw_ = active_tree->hud_layer() &&
                            active_tree->hud_layer()->IsAnimatingHUDContents();

  return root_surface_has_visible_damage ||
         active_tree_->property_trees()->effect_tree.HasCopyRequests() ||
         hud_wants_to_draw_;
}

DrawResult LayerTreeHostImpl::CalculateRenderPasses(FrameData* frame) {
  DCHECK(frame->render_passes.empty());
  DCHECK(CanDraw());
  DCHECK(!active_tree_->LayerListIsEmpty());

  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.
  DamageTracker::UpdateDamageTracking(active_tree_.get(),
                                      active_tree_->GetRenderSurfaceList());

  if (HasDamage()) {
    consecutive_frame_with_damage_count_++;
  } else {
    TRACE_EVENT0("cc",
                 "LayerTreeHostImpl::CalculateRenderPasses::EmptyDamageRect");
    frame->has_no_damage = true;
    DCHECK(!resourceless_software_draw_);
    consecutive_frame_with_damage_count_ = 0;
    return DRAW_SUCCESS;
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

  // Create the render passes in dependency order.
  size_t render_surface_list_size = frame->render_surface_list->size();
  for (size_t i = 0; i < render_surface_list_size; ++i) {
    size_t surface_index = render_surface_list_size - 1 - i;
    RenderSurfaceImpl* render_surface =
        (*frame->render_surface_list)[surface_index];

    bool is_root_surface =
        render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
    bool should_draw_into_render_pass =
        is_root_surface || render_surface->contributes_to_drawn_surface() ||
        render_surface->HasCopyRequest() ||
        render_surface->ShouldCacheRenderSurface();
    if (should_draw_into_render_pass)
      frame->render_passes.push_back(render_surface->CreateRenderPass());
  }

  // Damage rects for non-root passes aren't meaningful, so set them to be
  // equal to the output rect.
  for (size_t i = 0; i + 1 < frame->render_passes.size(); ++i) {
    viz::RenderPass* pass = frame->render_passes[i].get();
    pass->damage_rect = pass->output_rect;
  }

  // When we are displaying the HUD, change the root damage rect to cover the
  // entire root surface. This will disable partial-swap/scissor optimizations
  // that would prevent the HUD from updating, since the HUD does not cause
  // damage itself, to prevent it from messing with damage visualizations. Since
  // damage visualizations are done off the LayerImpls and RenderSurfaceImpls,
  // changing the RenderPass does not affect them.
  if (active_tree_->hud_layer()) {
    viz::RenderPass* root_pass = frame->render_passes.back().get();
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
  DrawResult draw_result = DRAW_SUCCESS;

  const DrawMode draw_mode = GetDrawMode();

  int num_missing_tiles = 0;
  int num_incomplete_tiles = 0;
  int64_t checkerboarded_no_recording_content_area = 0;
  int64_t checkerboarded_needs_raster_content_area = 0;
  int64_t total_visible_area = 0;
  bool have_copy_request =
      active_tree()->property_trees()->effect_tree.HasCopyRequests();
  bool have_missing_animated_tiles = false;

  int num_layers = 0;
  int num_mask_layers = 0;
  int num_rounded_corner_mask_layers = 0;
  int64_t visible_mask_layer_area = 0;
  int64_t visible_rounded_corner_mask_layer_area = 0;

  for (EffectTreeLayerListIterator it(active_tree());
       it.state() != EffectTreeLayerListIterator::State::END; ++it) {
    auto target_render_pass_id = it.target_render_surface()->id();
    viz::RenderPass* target_render_pass =
        FindRenderPassById(frame->render_passes, target_render_pass_id);

    AppendQuadsData append_quads_data;

    if (it.state() == EffectTreeLayerListIterator::State::TARGET_SURFACE) {
      RenderSurfaceImpl* render_surface = it.target_render_surface();
      if (render_surface->HasCopyRequest()) {
        active_tree()
            ->property_trees()
            ->effect_tree.TakeCopyRequestsAndTransformToSurface(
                render_surface->EffectTreeIndex(),
                &target_render_pass->copy_requests);
      }
    } else if (it.state() ==
               EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE) {
      RenderSurfaceImpl* render_surface = it.current_render_surface();
      if (render_surface->contributes_to_drawn_surface()) {
        render_surface->AppendQuads(draw_mode, target_render_pass,
                                    &append_quads_data);
      }
    } else if (it.state() == EffectTreeLayerListIterator::State::LAYER) {
      LayerImpl* layer = it.current_layer();
      if (layer->WillDraw(draw_mode, &resource_provider_)) {
        DCHECK_EQ(active_tree_.get(), layer->layer_tree_impl());

        frame->will_draw_layers.push_back(layer);
        if (layer->may_contain_video())
          frame->may_contain_video = true;

        layer->AppendQuads(target_render_pass, &append_quads_data);
        ++num_layers;
      }

      rendering_stats_instrumentation_->AddVisibleContentArea(
          append_quads_data.visible_layer_area);
      rendering_stats_instrumentation_->AddApproximatedVisibleContentArea(
          append_quads_data.approximated_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedVisibleContentArea(
          append_quads_data.checkerboarded_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNoRecordingContentArea(
          append_quads_data.checkerboarded_no_recording_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNeedsRasterContentArea(
          append_quads_data.checkerboarded_needs_raster_content_area);

      num_missing_tiles += append_quads_data.num_missing_tiles;
      num_incomplete_tiles += append_quads_data.num_incomplete_tiles;
      checkerboarded_no_recording_content_area +=
          append_quads_data.checkerboarded_no_recording_content_area;
      checkerboarded_needs_raster_content_area +=
          append_quads_data.checkerboarded_needs_raster_content_area;
      total_visible_area += append_quads_data.visible_layer_area;
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
    num_mask_layers += append_quads_data.num_mask_layers;
    num_rounded_corner_mask_layers +=
        append_quads_data.num_rounded_corner_mask_layers;
    visible_mask_layer_area += append_quads_data.visible_mask_layer_area;
    visible_rounded_corner_mask_layer_area +=
        append_quads_data.visible_rounded_corner_mask_layer_area;
  }

  // If CommitToActiveTree() is true, then we wait to draw until
  // NotifyReadyToDraw. That means we're in as good shape as is possible now,
  // so there's no reason to stop the draw now (and this is not supported by
  // SingleThreadProxy).
  if (have_missing_animated_tiles && !CommitToActiveTree())
    draw_result = DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;

  // When we require high res to draw, abort the draw (almost) always. This does
  // not cause the scheduler to do a main frame, instead it will continue to try
  // drawing until we finally complete, so the copy request will not be lost.
  // TODO(weiliangc): Remove RequiresHighResToDraw. crbug.com/469175
  if (num_incomplete_tiles || num_missing_tiles) {
    if (RequiresHighResToDraw())
      draw_result = DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
  }

  // When doing a resourceless software draw, we don't have control over the
  // surface the compositor draws to, so even though the frame may not be
  // complete, the previous frame has already been potentially lost, so an
  // incomplete frame is better than nothing, so this takes highest precidence.
  if (resourceless_software_draw_)
    draw_result = DRAW_SUCCESS;

#if DCHECK_IS_ON()
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list)
      DCHECK(quad->shared_quad_state);
  }
  DCHECK(frame->render_passes.back()->output_rect.origin().IsOrigin());
#endif
  bool has_transparent_background =
      SkColorGetA(active_tree_->background_color()) != SK_AlphaOPAQUE;
  if (!has_transparent_background) {
    frame->render_passes.back()->has_transparent_background = false;
    AppendQuadsToFillScreen(
        active_tree_->RootScrollLayerDeviceViewportBounds(),
        frame->render_passes.back().get(), active_tree_->RootRenderSurface(),
        active_tree_->background_color(), unoccluded_screen_space_region);
  }

  RemoveRenderPasses(frame);
  // If we're making a frame to draw, it better have at least one render pass.
  DCHECK(!frame->render_passes.empty());

  if (have_copy_request) {
    // Any copy requests left in the tree are not going to get serviced, and
    // should be aborted.
    active_tree()->property_trees()->effect_tree.ClearCopyRequests();

    // Draw properties depend on copy requests.
    active_tree()->set_needs_update_draw_properties();
  }

  if (ukm_manager_) {
    ukm_manager_->AddCheckerboardStatsForFrame(
        checkerboarded_no_recording_content_area +
            checkerboarded_needs_raster_content_area,
        num_missing_tiles, total_visible_area);
  }

  if (active_tree_->has_ever_been_drawn()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumMissingTiles",
        num_missing_tiles);
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumIncompleteTiles",
        num_incomplete_tiles);
    UMA_HISTOGRAM_COUNTS_1M(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNoRecordingContentArea",
        checkerboarded_no_recording_content_area);
    UMA_HISTOGRAM_COUNTS_1M(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNeedRasterContentArea",
        checkerboarded_needs_raster_content_area);
  }

  // Only record these umas on the first frame, and only in the renderer,
  // for which we use having a compositor thread as a proxy.
  if (!active_tree_->has_ever_been_drawn() && SupportsImplScrolling()) {
    int mask_layer_percent = static_cast<int>(
        num_mask_layers / static_cast<float>(num_layers) * 100);
    int rc_mask_layer_percent =
        static_cast<int>(num_rounded_corner_mask_layers /
                         static_cast<float>(num_mask_layers) * 100);
    int rc_area_percent =
        static_cast<int>(visible_rounded_corner_mask_layer_area /
                         static_cast<float>(total_visible_area) * 100);

    UMA_HISTOGRAM_PERCENTAGE(
        "Compositing.RenderPass.AppendQuadData.MaskLayerPercent",
        mask_layer_percent);
    if (num_mask_layers > 0) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Compositing.RenderPass.AppendQuadData.RCMaskLayerPercent",
          rc_mask_layer_percent);
      UMA_HISTOGRAM_PERCENTAGE(
          "Compositing.RenderPass.AppendQuadData.RCMaskAreaPercent",
          rc_area_percent);
      UMA_HISTOGRAM_COUNTS_10M(
          "Compositing.RenderPass.AppendQuadData.RCMaskArea",
          visible_rounded_corner_mask_layer_area);
    }
  }

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
  DCHECK(!have_copy_request || draw_result == DRAW_SUCCESS);

  // TODO(crbug.com/564832): This workaround to prevent creating unnecessarily
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
  DCHECK(!pending_tree_);
  // Invalidation should never be ran outside the impl frame for non
  // synchronous compositor mode. For devices that use synchronous compositor,
  // e.g. Android Webview, the assertion is not guaranteed because it may ask
  // for a frame at any time.
  DCHECK(impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME ||
         settings_.using_synchronous_renderer_compositor);

  if (!CommitToActiveTree())
    CreatePendingTree();

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
}

void LayerTreeHostImpl::InvalidateLayerTreeFrameSink(bool needs_redraw) {
  DCHECK(layer_tree_frame_sink());

  layer_tree_frame_sink()->Invalidate(needs_redraw);
  skipped_frame_tracker_.DidProduceFrame();
}

DrawResult LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  TRACE_EVENT1("cc", "LayerTreeHostImpl::PrepareToDraw", "SourceFrameNumber",
               active_tree_->source_frame_number());
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(CurrentBeginFrameArgs().trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "GenerateRenderPass");
  if (input_handler_client_)
    input_handler_client_->ReconcileElasticOverscrollAndRootScroll();

  // |client_name| is used for various UMA histograms below.
  // GetClientNameForMetrics only returns one non-null value over the lifetime
  // of the process, so the histogram names are runtime constant.
  const char* client_name = GetClientNameForMetrics();
  if (client_name) {
    size_t total_memory_in_bytes = 0;
    size_t total_gpu_memory_for_tilings_in_bytes = 0;
    for (const PictureLayerImpl* layer : active_tree()->picture_layers()) {
      total_memory_in_bytes += layer->GetRasterSource()->GetMemoryUsage();
      total_gpu_memory_for_tilings_in_bytes += layer->GPUMemoryUsageInBytes();
    }
    if (total_memory_in_bytes != 0) {
      UMA_HISTOGRAM_COUNTS_1M(
          base::StringPrintf("Compositing.%s.PictureMemoryUsageKb",
                             client_name),
          base::saturated_cast<int>(total_memory_in_bytes / 1024));
    }

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.NumActiveLayers", client_name),
        base::saturated_cast<int>(active_tree_->NumLayers()), 1, 400, 20);

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.NumActivePictureLayers",
                           client_name),
        base::saturated_cast<int>(active_tree_->picture_layers().size()), 1,
        400, 20);

    // TODO(yigu): Maybe we should use the same check above. Need to figure out
    // why exactly we skip 0.
    if (!active_tree()->picture_layers().empty()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          base::StringPrintf("Compositing.%s.GPUMemoryForTilingsInKb",
                             client_name),
          base::saturated_cast<int>(total_gpu_memory_for_tilings_in_bytes /
                                    1024),
          1, kGPUMemoryForTilingsLargestBucketKb,
          kGPUMemoryForTilingsBucketCount);
    }
  }

  bool ok = active_tree_->UpdateDrawProperties();
  DCHECK(ok) << "UpdateDrawProperties failed during draw";

  // This will cause NotifyTileStateChanged() to be called for any tiles that
  // completed, which will add damage for visible tiles to the frame for them so
  // they appear as part of the current frame being drawn.
  tile_manager_.CheckForCompletedTasks();

  frame->render_surface_list = &active_tree_->GetRenderSurfaceList();
  frame->render_passes.clear();
  frame->will_draw_layers.clear();
  frame->has_no_damage = false;
  frame->may_contain_video = false;

  if (active_tree_->RootRenderSurface()) {
    gfx::Rect device_viewport_damage_rect = viewport_damage_rect_;
    viewport_damage_rect_ = gfx::Rect();

    active_tree_->RootRenderSurface()->damage_tracker()->AddDamageNextUpdate(
        device_viewport_damage_rect);
  }

  DrawResult draw_result = CalculateRenderPasses(frame);
  if (draw_result != DRAW_SUCCESS) {
    DCHECK(!resourceless_software_draw_);
    return draw_result;
  }

  // If we return DRAW_SUCCESS, then we expect DrawLayers() to be called before
  // this function is called again.
  return draw_result;
}

void LayerTreeHostImpl::RemoveRenderPasses(FrameData* frame) {
  // There is always at least a root RenderPass.
  DCHECK_GE(frame->render_passes.size(), 1u);

  // A set of RenderPasses that we have seen.
  base::flat_set<viz::RenderPassId> pass_exists;
  // A set of viz::RenderPassDrawQuads that we have seen (stored by the
  // RenderPasses they refer to).
  base::flat_map<viz::RenderPassId, int> pass_references;

  // Iterate RenderPasses in draw order, removing empty render passes (except
  // the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size(); ++i) {
    viz::RenderPass* pass = frame->render_passes[i].get();

    // Remove orphan viz::RenderPassDrawQuads.
    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end();) {
      if (it->material != viz::DrawQuad::RENDER_PASS) {
        ++it;
        continue;
      }
      const viz::RenderPassDrawQuad* quad =
          viz::RenderPassDrawQuad::MaterialCast(*it);
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
        pass->filters.IsEmpty() && pass->backdrop_filters.IsEmpty()) {
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
    viz::RenderPass* pass =
        frame->render_passes[frame->render_passes.size() - 2 - i].get();
    if (!pass->copy_requests.empty())
      continue;
    if (pass_references[pass->id])
      continue;

    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end(); ++it) {
      if (it->material != viz::DrawQuad::RENDER_PASS)
        continue;
      const viz::RenderPassDrawQuad* quad =
          viz::RenderPassDrawQuad::MaterialCast(*it);
      pass_references[quad->render_pass_id]--;
    }

    frame->render_passes.erase(frame->render_passes.end() - 2 - i);
    --i;
  }
}

void LayerTreeHostImpl::EvictTexturesForTesting() {
  UpdateTileManagerMemoryPolicy(ManagedMemoryPolicy(0));
}

void LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting(bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::BlockImplSideInvalidationRequestsForTesting(
    bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::ResetTreesForTesting() {
  if (active_tree_)
    active_tree_->DetachLayers();
  active_tree_ =
      std::make_unique<LayerTreeImpl>(this, active_tree()->page_scale_factor(),
                                      active_tree()->top_controls_shown_ratio(),
                                      active_tree()->elastic_overscroll());
  active_tree_->property_trees()->is_active = true;
  if (pending_tree_)
    pending_tree_->DetachLayers();
  pending_tree_ = nullptr;
  pending_tree_duration_timer_ = nullptr;
  if (recycle_tree_)
    recycle_tree_->DetachLayers();
  recycle_tree_ = nullptr;
}

size_t LayerTreeHostImpl::SourceAnimationFrameNumberForTesting() const {
  return fps_counter_->current_frame_number();
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
    if (image_decode_cache_)
      image_decode_cache_->SetShouldAggressivelyFreeResources(false);
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

  DidModifyTilePriorities();
}

void LayerTreeHostImpl::DidModifyTilePriorities() {
  // Mark priorities as dirty and schedule a PrepareTiles().
  tile_priorities_dirty_ = true;
  tile_manager_.DidModifyTilePriorities();
  client_->SetNeedsPrepareTilesOnImplThread();
}

std::unique_ptr<RasterTilePriorityQueue> LayerTreeHostImpl::BuildRasterQueue(
    TreePriority tree_priority,
    RasterTilePriorityQueue::Type type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildRasterQueue");

  return RasterTilePriorityQueue::Create(active_tree_->picture_layers(),
                                         pending_tree_
                                             ? pending_tree_->picture_layers()
                                             : std::vector<PictureLayerImpl*>(),
                                         tree_priority, type);
}

std::unique_ptr<EvictionTilePriorityQueue>
LayerTreeHostImpl::BuildEvictionQueue(TreePriority tree_priority) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildEvictionQueue");

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      new EvictionTilePriorityQueue);
  queue->Build(active_tree_->picture_layers(),
               pending_tree_ ? pending_tree_->picture_layers()
                             : std::vector<PictureLayerImpl*>(),
               tree_priority);
  return queue;
}

void LayerTreeHostImpl::SetIsLikelyToRequireADraw(
    bool is_likely_to_require_a_draw) {
  // Proactively tell the scheduler that we expect to draw within each vsync
  // until we get all the tiles ready to draw. If we happen to miss a required
  // for draw tile here, then we will miss telling the scheduler each frame that
  // we intend to draw so it may make worse scheduling decisions.
  is_likely_to_require_a_draw_ = is_likely_to_require_a_draw;
}

RasterColorSpace LayerTreeHostImpl::GetRasterColorSpace() const {
  RasterColorSpace result;
  // The pending tree will have the most recently updated color space, so
  // prefer that.
  if (pending_tree_) {
    result.color_space = pending_tree_->raster_color_space();
    result.color_space_id = pending_tree_->raster_color_space_id();
  } else if (active_tree_) {
    result.color_space = active_tree_->raster_color_space();
    result.color_space_id = active_tree_->raster_color_space_id();
  }

  // If we are likely to software composite the resource, we use sRGB because
  // software compositing is unable to perform color conversion. Also always
  // specify a color space if color correct rasterization is requested
  // (not specifying a color space indicates that no color conversion is
  // required).
  if (!layer_tree_frame_sink_ || !layer_tree_frame_sink_->context_provider() ||
      !result.color_space.IsValid()) {
    result.color_space = default_color_space_;
    result.color_space_id = default_color_space_id_;
  }
  return result;
}

void LayerTreeHostImpl::RequestImplSideInvalidationForCheckerImagedTiles() {
  // When using impl-side invalidation for checker-imaging, a pending tree does
  // not need to be flushed as an independent update through the pipeline.
  bool needs_first_draw_on_activation = false;
  client_->NeedsImplSideInvalidation(needs_first_draw_on_activation);
}

size_t LayerTreeHostImpl::GetFrameIndexForImage(const PaintImage& paint_image,
                                                WhichTree tree) const {
  if (!paint_image.ShouldAnimate())
    return PaintImage::kDefaultFrameIndex;

  return image_animation_controller_.GetFrameIndexForImage(
      paint_image.stable_id(), tree);
}

void LayerTreeHostImpl::NotifyReadyToActivate() {
  pending_tree_raster_duration_timer_.reset();
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
  // The tile tasks started by the most recent call to PrepareTiles have
  // completed. Now is a good time to free resources if necessary.
  if (global_tile_state_.hard_memory_limit_in_bytes == 0) {
    // Free image decode controller resources before notifying the
    // contexts of visibility change. This ensures that the imaged decode
    // controller has released all Skia refs at the time Skia's cleanup
    // executes (within worker context's cleanup).
    if (image_decode_cache_)
      image_decode_cache_->SetShouldAggressivelyFreeResources(true);
    SetContextVisibility(false);
  }
}

void LayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::NotifyTileStateChanged");

  if (active_tree_) {
    LayerImpl* layer_impl =
        active_tree_->FindActiveTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }

  if (pending_tree_) {
    LayerImpl* layer_impl =
        pending_tree_->FindPendingTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }

  // Check for a non-null active tree to avoid doing this during shutdown.
  if (active_tree_ && !client_->IsInsideDraw() && tile->required_for_draw()) {
    // The LayerImpl::NotifyTileStateChanged() should damage the layer, so this
    // redraw will make those tiles be displayed.
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  DCHECK(task_runner_provider_->IsImplThread());

  SetManagedMemoryPolicy(policy);

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
    const base::Closure& callback) {
  DCHECK(task_runner_provider_->IsImplThread());
  tree_activation_callback_ = callback;
}

void LayerTreeHostImpl::SetManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
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
  gfx::Rect viewport_rect_for_tile_priority_in_view_space;
  gfx::Transform screen_to_view(gfx::Transform::kSkipInitialization);
  if (transform.GetInverse(&screen_to_view)) {
    // Convert from screen space to view space.
    viewport_rect_for_tile_priority_in_view_space =
        MathUtil::ProjectEnclosingClippedRect(screen_to_view, viewport_rect);
  }

  const bool tile_priority_params_changed =
      viewport_rect_for_tile_priority_ !=
      viewport_rect_for_tile_priority_in_view_space;

  viewport_rect_for_tile_priority_ =
      viewport_rect_for_tile_priority_in_view_space;

  if (tile_priority_params_changed) {
    active_tree_->set_needs_update_draw_properties();
    if (pending_tree_)
      pending_tree_->set_needs_update_draw_properties();

    // Compositor, not LayerTreeFrameSink, is responsible for setting damage
    // and triggering redraw for constraint changes.
    SetFullViewportDamage();
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAckOnImplThread();
}

LayerTreeHostImpl::FrameTokenInfo::FrameTokenInfo(
    uint32_t token,
    base::TimeTicks cc_frame_time,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks)
    : token(token),
      cc_frame_time(cc_frame_time),
      callbacks(std::move(callbacks)) {}

LayerTreeHostImpl::FrameTokenInfo::FrameTokenInfo(FrameTokenInfo&&) = default;
LayerTreeHostImpl::FrameTokenInfo::~FrameTokenInfo() = default;

void LayerTreeHostImpl::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  std::vector<LayerTreeHost::PresentationTimeCallback> all_callbacks;
  while (!frame_token_infos_.empty()) {
    auto info = frame_token_infos_.begin();
    if (viz::FrameTokenGT(info->token, frame_token))
      break;

    // Update compositor frame latency and smoothness stats only for frames
    // that caused on-screen damage.
    if (info->token == frame_token)
      frame_metrics_.AddFrameDisplayed(info->cc_frame_time, feedback.timestamp);

    std::copy(std::make_move_iterator(info->callbacks.begin()),
              std::make_move_iterator(info->callbacks.end()),
              std::back_inserter(all_callbacks));
    frame_token_infos_.erase(info);
  }
  client_->DidPresentCompositorFrameOnImplThread(
      frame_token, std::move(all_callbacks), feedback);
}

void LayerTreeHostImpl::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  resource_provider_.ReceiveReturnsFromParent(resources);

  // In OOM, we now might be able to release more resources that were held
  // because they were exported.
  if (resource_pool_) {
    if (resource_pool_->memory_usage_bytes()) {
      const size_t kMegabyte = 1024 * 1024;

      // This is a good time to log memory usage. A chunk of work has just
      // completed but none of the memory used for that work has likely been
      // freed.
      UMA_HISTOGRAM_MEMORY_MB(
          "Renderer4.ResourcePoolMemoryUsage",
          static_cast<int>(resource_pool_->memory_usage_bytes() / kMegabyte));
    }

    resource_pool_->ReduceResourceUsage();
  }

  // If we're not visible, we likely released resources, so we want to
  // aggressively flush here to make sure those DeleteTextures make it to the
  // GPU process to free up the memory.
  if (!visible_ && layer_tree_frame_sink_->context_provider()) {
    auto* gl = layer_tree_frame_sink_->context_provider()->ContextGL();
    gl->ShallowFlushCHROMIUM();
  }
}

void LayerTreeHostImpl::OnDraw(const gfx::Transform& transform,
                               const gfx::Rect& viewport,
                               bool resourceless_software_draw,
                               bool skip_draw) {
  DCHECK(!resourceless_software_draw_);

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

    if (resourceless_software_draw) {
      client_->OnCanDrawStateChanged(CanDraw());
    }

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

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

viz::CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() {
  viz::CompositorFrameMetadata metadata;
  metadata.frame_token = next_frame_token_++;
  if (!next_frame_token_)
    next_frame_token_ = 1u;

  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.root_background_color = active_tree_->background_color();
  metadata.content_source_id = active_tree_->content_source_id();

  if (active_tree_->has_presentation_callbacks() ||
      settings_.always_request_presentation_time) {
    metadata.request_presentation_feedback = true;
    frame_token_infos_.emplace_back(metadata.frame_token,
                                    CurrentBeginFrameArgs().frame_time,
                                    active_tree_->TakePresentationCallbacks());

    DCHECK_LE(frame_token_infos_.size(), 25u);
  }

  if (GetDrawMode() == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    metadata.is_resourceless_software_draw_with_scroll_or_animation =
        IsActivelyScrolling() || mutator_host_->NeedsTickAnimations();
  }

  const base::flat_set<viz::SurfaceRange>& referenced_surfaces =
      active_tree_->SurfaceRanges();
  for (auto& surface_range : referenced_surfaces)
    metadata.referenced_surfaces.push_back(surface_range);

  if (last_draw_referenced_surfaces_ != referenced_surfaces)
    last_draw_referenced_surfaces_ = referenced_surfaces;

  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();

  metadata.top_controls_height =
      browser_controls_offset_manager_->TopControlsHeight();
  metadata.top_controls_shown_ratio =
      browser_controls_offset_manager_->TopControlsShownRatio();

  metadata.local_surface_id_allocation_time =
      child_local_surface_id_allocator_.allocation_time();

#if defined(OS_ANDROID)
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  metadata.root_layer_size = active_tree_->ScrollableSize();

  if (const auto* outer_viewport_scroll_node = OuterViewportScrollNode()) {
    metadata.root_overflow_y_hidden =
        !outer_viewport_scroll_node->user_scrollable_vertical;
  }

  metadata.bottom_controls_height =
      browser_controls_offset_manager_->BottomControlsHeight();
  metadata.bottom_controls_shown_ratio =
      browser_controls_offset_manager_->BottomControlsShownRatio();

  active_tree_->GetViewportSelection(&metadata.selection);
#endif

  const auto* inner_viewport_scroll_node = InnerViewportScrollNode();
  if (!inner_viewport_scroll_node)
    return metadata;

#if defined(OS_ANDROID)
  metadata.root_overflow_y_hidden |=
      !inner_viewport_scroll_node->user_scrollable_vertical;
#endif

  // TODO(miletus) : Change the metadata to hold ScrollOffset.
  metadata.root_scroll_offset =
      gfx::ScrollOffsetToVector2dF(active_tree_->TotalScrollOffset());

  return metadata;
}

RenderFrameMetadata LayerTreeHostImpl::MakeRenderFrameMetadata(
    FrameData* frame) {
  RenderFrameMetadata metadata;
  metadata.root_scroll_offset =
      gfx::ScrollOffsetToVector2dF(active_tree_->TotalScrollOffset());

  metadata.root_background_color = active_tree_->background_color();
  metadata.is_scroll_offset_at_top = active_tree_->TotalScrollOffset().y() == 0;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();
  active_tree_->GetViewportSelection(&metadata.selection);
  metadata.is_mobile_optimized = IsMobileOptimized(active_tree_.get());
  metadata.viewport_size_in_pixels = active_tree_->GetDeviceViewport().size();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();

  metadata.top_controls_height =
      browser_controls_offset_manager_->TopControlsHeight();
  metadata.top_controls_shown_ratio =
      browser_controls_offset_manager_->TopControlsShownRatio();
#if defined(OS_ANDROID)
  metadata.bottom_controls_height =
      browser_controls_offset_manager_->BottomControlsHeight();
  metadata.bottom_controls_shown_ratio =
      browser_controls_offset_manager_->BottomControlsShownRatio();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  if (const auto* outer_viewport_scroll_node = OuterViewportScrollNode()) {
    metadata.root_overflow_y_hidden =
        !outer_viewport_scroll_node->user_scrollable_vertical;
  }
  const auto* inner_viewport_scroll_node = InnerViewportScrollNode();
  if (inner_viewport_scroll_node) {
    metadata.root_overflow_y_hidden |=
        !inner_viewport_scroll_node->user_scrollable_vertical;
  }
  metadata.has_transparent_background =
      frame->render_passes.back()->has_transparent_background;
#endif

  bool allocate_new_local_surface_id =
#if !defined(OS_ANDROID)
      last_draw_render_frame_metadata_ &&
      (last_draw_render_frame_metadata_->top_controls_height !=
           metadata.top_controls_height ||
       last_draw_render_frame_metadata_->top_controls_shown_ratio !=
           metadata.top_controls_shown_ratio);
#else
      last_draw_render_frame_metadata_ &&
      (last_draw_render_frame_metadata_->top_controls_height !=
           metadata.top_controls_height ||
       last_draw_render_frame_metadata_->top_controls_shown_ratio !=
           metadata.top_controls_shown_ratio ||
       last_draw_render_frame_metadata_->bottom_controls_height !=
           metadata.bottom_controls_height ||
       last_draw_render_frame_metadata_->bottom_controls_shown_ratio !=
           metadata.bottom_controls_shown_ratio ||
       last_draw_render_frame_metadata_->selection != metadata.selection ||
       last_draw_render_frame_metadata_->has_transparent_background !=
           metadata.has_transparent_background);
#endif

  viz::LocalSurfaceId local_surface_id =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  if (local_surface_id.is_valid()) {
    if (allocate_new_local_surface_id)
      local_surface_id = child_local_surface_id_allocator_.GenerateId();
    metadata.local_surface_id = local_surface_id;
    metadata.local_surface_id_allocation_time_from_child =
        child_local_surface_id_allocator_.allocation_time();
  }

  return metadata;
}

bool LayerTreeHostImpl::DrawLayers(FrameData* frame) {
  DCHECK(CanDraw());
  DCHECK_EQ(frame->has_no_damage, frame->render_passes.empty());
  ResetRequiresHighResToDraw();
  skipped_frame_tracker_.DidProduceFrame();

  if (frame->has_no_damage) {
    DCHECK(!resourceless_software_draw_);

    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoDamage", TRACE_EVENT_SCOPE_THREAD);
    active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);
    return false;
  }

  auto compositor_frame = GenerateCompositorFrame(frame);
  layer_tree_frame_sink_->SubmitCompositorFrame(std::move(compositor_frame));

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

  active_tree_->set_has_ever_been_drawn(true);
  devtools_instrumentation::DidDrawFrame(id_);
  benchmark_instrumentation::IssueImplThreadRenderingStatsEvent(
      rendering_stats_instrumentation_->TakeImplThreadRenderingStats());
  return true;
}

viz::CompositorFrame LayerTreeHostImpl::GenerateCompositorFrame(
    FrameData* frame) {
  TRACE_EVENT0("cc,benchmark", "LayerTreeHostImpl::GenerateCompositorFrame");
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(CurrentBeginFrameArgs().trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "GenerateCompositorFrame");
  base::TimeTicks frame_time = CurrentBeginFrameArgs().frame_time;
  fps_counter_->SaveTimeStamp(frame_time,
                              !layer_tree_frame_sink_->context_provider());
  rendering_stats_instrumentation_->IncrementFrameCount(1);

  memory_history_->SaveEntry(tile_manager_.memory_stats_from_last_assign());

  if (debug_state_.ShowHudRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree(), active_tree_->hud_layer(), *frame->render_surface_list,
        debug_state_);
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace) {
    if (pending_tree_) {
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          pending_tree(), [](LayerImpl* layer) { layer->DidBeginTracing(); });
    }
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        active_tree(), [](LayerImpl* layer) { layer->DidBeginTracing(); });
  }

  {
    TRACE_EVENT0("cc", "DrawLayers.FrameViewerTracing");
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        frame_viewer_instrumentation::kCategoryLayerTree,
        "cc::LayerTreeHostImpl", id_, AsValueWithFrame(frame));
  }

  const DrawMode draw_mode = GetDrawMode();

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer()) {
    TRACE_EVENT0("cc", "DrawLayers.UpdateHudTexture");
    active_tree_->hud_layer()->UpdateHudTexture(
        draw_mode, layer_tree_frame_sink_, &resource_provider_,
        // The hud uses Gpu rasterization if the device is capable, not related
        // to the content of the web page.
        gpu_rasterization_status_ != GpuRasterizationStatus::OFF_DEVICE,
        frame->render_passes);
  }

  viz::CompositorFrameMetadata metadata = MakeCompositorFrameMetadata();
  metadata.may_contain_video = frame->may_contain_video;
  metadata.deadline = viz::FrameDeadline(
      CurrentBeginFrameArgs().frame_time,
      frame->deadline_in_frames.value_or(0u), CurrentBeginFrameArgs().interval,
      frame->use_default_lower_bound_deadline);

  metadata.activation_dependencies = std::move(frame->activation_dependencies);
  active_tree()->FinishSwapPromises(&metadata);
  // The swap-promises should not change the frame-token.
  DCHECK_EQ(metadata.frame_token + 1, next_frame_token_);

  if (render_frame_metadata_observer_) {
    last_draw_render_frame_metadata_ = MakeRenderFrameMetadata(frame);
    render_frame_metadata_observer_->OnRenderFrameSubmission(
        *last_draw_render_frame_metadata_, &metadata);
  }

  metadata.latency_info.emplace_back(ui::SourceEventType::FRAME);
  ui::LatencyInfo& new_latency_info = metadata.latency_info.back();
  if (CommitToActiveTree()) {
    new_latency_info.AddLatencyNumberWithTimestamp(
        ui::LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT, frame_time, 1);
  } else {
    new_latency_info.AddLatencyNumberWithTimestamp(
        ui::LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT, frame_time, 1);

    base::TimeTicks draw_time = base::TimeTicks::Now();
    for (auto& latency : metadata.latency_info) {
      latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, draw_time, 1);
    }
  }
  ui::LatencyInfo::TraceIntermediateFlowEvents(metadata.latency_info,
                                               "SwapBuffers");

  // Collect all resource ids in the render passes into a single array.
  std::vector<viz::ResourceId> resources;
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list) {
      for (viz::ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }

  DCHECK_LE(viz::BeginFrameArgs::kStartingFrameNumber,
            frame->begin_frame_ack.sequence_number);
  metadata.begin_frame_ack = frame->begin_frame_ack;

  viz::CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  resource_provider_.PrepareSendToParent(
      resources, &compositor_frame.resource_list,
      layer_tree_frame_sink_->context_provider());
  compositor_frame.render_pass_list = std::move(frame->render_passes);
  // TODO(fsamuel): Once all clients get their viz::LocalSurfaceId from their
  // parent, the viz::LocalSurfaceId should hang off CompositorFrameMetadata.
  if (settings_.enable_surface_synchronization) {
    // If surface synchronization is on, we should always have a valid
    // LocalSurfaceId in LayerTreeImpl unless we don't have a scheduler because
    // without a scheduler commits are not deferred and LayerTrees without valid
    // LocalSurfaceId might slip through, but single-thread-without-scheduler
    // mode is only used in tests so it doesn't matter.
    CHECK(!settings_.single_thread_proxy_scheduler ||
          active_tree()->local_surface_id_from_parent().is_valid());
    layer_tree_frame_sink_->SetLocalSurfaceId(
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }
  last_draw_local_surface_id_ =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  if (const char* client_name = GetClientNameForMetrics()) {
    size_t total_quad_count = 0;
    for (const auto& pass : compositor_frame.render_pass_list)
      total_quad_count += pass->quad_list.size();
    UMA_HISTOGRAM_COUNTS_1000(
        base::StringPrintf("Compositing.%s.CompositorFrame.Quads", client_name),
        total_quad_count);
  }

  return compositor_frame;
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  // TODO(lethalantidote): LayerImpl::DidDraw can be removed when
  // VideoLayerImpl is removed.
  for (size_t i = 0; i < frame.will_draw_layers.size(); ++i)
    frame.will_draw_layers[i]->DidDraw(&resource_provider_);

  for (auto* it : video_frame_controllers_)
    it->DidDrawFrame();
}

int LayerTreeHostImpl::RequestedMSAASampleCount() const {
  if (settings_.gpu_rasterization_msaa_sample_count == -1) {
    // Use the most up-to-date version of device_scale_factor that we have.
    float device_scale_factor = pending_tree_
                                    ? pending_tree_->device_scale_factor()
                                    : active_tree_->device_scale_factor();
    return device_scale_factor >= 2.0f ? 4 : 8;
  }

  return settings_.gpu_rasterization_msaa_sample_count;
}

void LayerTreeHostImpl::SetHasGpuRasterizationTrigger(bool flag) {
  if (has_gpu_rasterization_trigger_ != flag) {
    has_gpu_rasterization_trigger_ = flag;
    need_update_gpu_rasterization_status_ = true;
  }
}

void LayerTreeHostImpl::SetContentHasSlowPaths(bool flag) {
  if (content_has_slow_paths_ != flag) {
    content_has_slow_paths_ = flag;
    need_update_gpu_rasterization_status_ = true;
  }
}

void LayerTreeHostImpl::SetContentHasNonAAPaint(bool flag) {
  if (content_has_non_aa_paint_ != flag) {
    content_has_non_aa_paint_ = flag;
    need_update_gpu_rasterization_status_ = true;
  }
}

void LayerTreeHostImpl::GetGpuRasterizationCapabilities(
    bool* gpu_rasterization_enabled,
    bool* gpu_rasterization_supported,
    int* max_msaa_samples,
    bool* supports_disable_msaa) {
  *gpu_rasterization_enabled = false;
  *gpu_rasterization_supported = false;
  *max_msaa_samples = 0;
  *supports_disable_msaa = false;

  if (!(layer_tree_frame_sink_ && layer_tree_frame_sink_->context_provider() &&
        layer_tree_frame_sink_->worker_context_provider()))
    return;

  viz::RasterContextProvider* context_provider =
      layer_tree_frame_sink_->worker_context_provider();
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context_provider);

  const auto& caps = context_provider->ContextCapabilities();
  *gpu_rasterization_enabled = caps.gpu_rasterization;
  if (!*gpu_rasterization_enabled && !settings_.gpu_rasterization_forced)
    return;

  if (use_oop_rasterization_) {
    *gpu_rasterization_supported = true;
    *supports_disable_msaa = caps.multisample_compatibility;
    // For OOP raster, the gpu service side will disable msaa if the
    // requested samples are not enough.  GPU raster does this same
    // logic below client side.
    *max_msaa_samples = RequestedMSAASampleCount();
    return;
  }

  if (!context_provider->ContextSupport()->HasGrContextSupport())
    return;

  // Do not check GrContext above. It is lazy-created, and we only want to
  // create it if it might be used.
  GrContext* gr_context = context_provider->GrContext();
  *gpu_rasterization_supported = !!gr_context;
  if (!*gpu_rasterization_supported)
    return;

  *supports_disable_msaa = caps.multisample_compatibility;
  if (!caps.msaa_is_slow && !caps.avoid_stencil_buffers) {
    // Skia may blacklist MSAA independently of Chrome. Query Skia for its max
    // supported sample count. Assume gpu compositing + gpu raster for this, as
    // that is what we are hoping to use.
    viz::ResourceFormat tile_format = TileRasterBufferFormat(
        settings_, layer_tree_frame_sink_->context_provider(),
        /*use_gpu_rasterization=*/true);
    SkColorType color_type = ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, tile_format);
    *max_msaa_samples =
        gr_context->maxSurfaceSampleCountForColorType(color_type);
  }
}

bool LayerTreeHostImpl::UpdateGpuRasterizationStatus() {
  if (!need_update_gpu_rasterization_status_)
    return false;
  need_update_gpu_rasterization_status_ = false;

  // TODO(danakj): Can we avoid having this run when there's no
  // LayerTreeFrameSink?
  // For now just early out and leave things unchanged, we'll come back here
  // when we get a LayerTreeFrameSink.
  if (!layer_tree_frame_sink_)
    return false;

  int requested_msaa_samples = RequestedMSAASampleCount();
  int max_msaa_samples = 0;
  bool gpu_rasterization_enabled = false;
  bool gpu_rasterization_supported = false;
  bool supports_disable_msaa = false;
  GetGpuRasterizationCapabilities(&gpu_rasterization_enabled,
                                  &gpu_rasterization_supported,
                                  &max_msaa_samples, &supports_disable_msaa);

  bool use_gpu = false;
  bool use_msaa = false;
  bool using_msaa_for_slow_paths =
      requested_msaa_samples > 0 &&
      max_msaa_samples >= requested_msaa_samples &&
      (!content_has_non_aa_paint_ || supports_disable_msaa);
  if (settings_.gpu_rasterization_forced) {
    use_gpu = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::ON_FORCED;
    use_msaa = content_has_slow_paths_ && using_msaa_for_slow_paths;
    if (use_msaa) {
      gpu_rasterization_status_ = GpuRasterizationStatus::MSAA_CONTENT;
    }
  } else if (!gpu_rasterization_enabled) {
    gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
  } else if (!has_gpu_rasterization_trigger_) {
    gpu_rasterization_status_ = GpuRasterizationStatus::OFF_VIEWPORT;
  } else if (content_has_slow_paths_ && using_msaa_for_slow_paths) {
    use_gpu = use_msaa = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::MSAA_CONTENT;
  } else {
    use_gpu = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::ON;
  }

  if (use_gpu && !use_gpu_rasterization_) {
    if (!gpu_rasterization_supported) {
      // If GPU rasterization is unusable, e.g. if GlContext could not
      // be created due to losing the GL context, force use of software
      // raster.
      use_gpu = false;
      use_msaa = false;
      gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
    }
  }

  if (use_gpu == use_gpu_rasterization_ && use_msaa == use_msaa_)
    return false;

  // Note that this must happen first, in case the rest of the calls want to
  // query the new state of |use_gpu_rasterization_|.
  use_gpu_rasterization_ = use_gpu;
  use_msaa_ = use_msaa;
  return true;
}

void LayerTreeHostImpl::UpdateTreeResourcesForGpuRasterizationIfNeeded() {
  if (!UpdateGpuRasterizationStatus())
    return;

  // Clean up and replace existing tile manager with another one that uses
  // appropriate rasterizer. Only do this however if we already have a
  // resource pool, since otherwise we might not be able to create a new
  // one.
  ReleaseTileResources();
  if (resource_pool_) {
    CleanUpTileManagerResources();
    CreateTileManagerResources();
  }
  RecreateTileResources();

  // We have released tilings for both active and pending tree.
  // We would not have any content to draw until the pending tree is activated.
  // Prevent the active tree from drawing until activation.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();
}

bool LayerTreeHostImpl::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  impl_thread_phase_ = ImplThreadPhase::INSIDE_IMPL_FRAME;
  current_begin_frame_tracker_.Start(args);

  if (is_likely_to_require_a_draw_) {
    // Optimistically schedule a draw. This will let us expect the tile manager
    // to complete its work so that we can draw new tiles within the impl frame
    // we are beginning now.
    SetNeedsRedraw();
  }

  if (input_handler_client_)
    input_handler_client_->DeliverInputForBeginFrame();

  Animate();

  for (auto* it : video_frame_controllers_)
    it->OnBeginFrame(args);

  skipped_frame_tracker_.BeginFrame(args.frame_time, args.interval);

  bool recent_frame_had_no_damage =
      consecutive_frame_with_damage_count_ < settings_.damaged_frame_limit;
  // Check damage early if the setting is enabled and a recent frame had no
  // damage. HasDamage() expects CanDraw to be true. If we can't check damage,
  // return true to indicate that there might be damage in this frame.
  if (settings_.enable_early_damage_check && recent_frame_had_no_damage &&
      CanDraw()) {
    bool ok = active_tree()->UpdateDrawProperties();
    DCHECK(ok);
    DamageTracker::UpdateDamageTracking(active_tree_.get(),
                                        active_tree_->GetRenderSurfaceList());
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

void LayerTreeHostImpl::DidFinishImplFrame() {
  skipped_frame_tracker_.FinishFrame();
  impl_thread_phase_ = ImplThreadPhase::IDLE;
  current_begin_frame_tracker_.Finish();
}

void LayerTreeHostImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  if (layer_tree_frame_sink_)
    layer_tree_frame_sink_->DidNotProduceFrame(ack);
}

void LayerTreeHostImpl::UpdateViewportContainerSizes() {
  if (!InnerViewportScrollNode())
    return;

  ViewportAnchor anchor(InnerViewportScrollNode(), OuterViewportScrollLayer(),
                        active_tree_.get());

  float top_controls_layout_height =
      active_tree_->browser_controls_shrink_blink_size()
          ? active_tree_->top_controls_height()
          : 0.f;
  float delta_from_top_controls =
      top_controls_layout_height -
      browser_controls_offset_manager_->ContentTopOffset();
  float bottom_controls_layout_height =
      active_tree_->browser_controls_shrink_blink_size()
          ? active_tree_->bottom_controls_height()
          : 0.f;
  delta_from_top_controls +=
      bottom_controls_layout_height -
      browser_controls_offset_manager_->ContentBottomOffset();

  // Adjust the viewport layers by shrinking/expanding the container to account
  // for changes in the size (e.g. browser controls) since the last resize from
  // Blink.
  auto* property_trees = active_tree_->property_trees();
  gfx::Vector2dF inner_bounds_delta(0.f, delta_from_top_controls);
  if (property_trees->inner_viewport_container_bounds_delta() ==
      inner_bounds_delta)
    return;

  property_trees->SetInnerViewportContainerBoundsDelta(inner_bounds_delta);

  ClipNode* inner_clip_node = property_trees->clip_tree.Node(
      InnerViewportScrollLayer()->clip_tree_index());
  inner_clip_node->clip.set_height(
      InnerViewportScrollNode()->container_bounds.height() +
      inner_bounds_delta.y());

  // Adjust the outer viewport container as well, since adjusting only the
  // inner may cause its bounds to exceed those of the outer, causing scroll
  // clamping.
  if (OuterViewportScrollNode()) {
    gfx::Vector2dF outer_bounds_delta = gfx::ScaleVector2d(
        inner_bounds_delta, 1.f / active_tree_->min_page_scale_factor());

    property_trees->SetOuterViewportContainerBoundsDelta(outer_bounds_delta);
    property_trees->SetInnerViewportScrollBoundsDelta(outer_bounds_delta);

    ClipNode* outer_clip_node = property_trees->clip_tree.Node(
        OuterViewportScrollLayer()->clip_tree_index());
    outer_clip_node->clip.set_height(
        OuterViewportScrollNode()->container_bounds.height() +
        outer_bounds_delta.y());

    // Expand all clips between the outer viewport and the inner viewport.
    auto* outer_ancestor = property_trees->clip_tree.parent(outer_clip_node);
    while (outer_ancestor && outer_ancestor != inner_clip_node) {
      outer_ancestor->clip.Union(outer_clip_node->clip);
      outer_ancestor = property_trees->clip_tree.parent(outer_ancestor);
    }

    anchor.ResetViewportToAnchoredPosition();
  }

  property_trees->clip_tree.set_needs_update(true);
  property_trees->full_tree_damaged = true;
  active_tree_->set_needs_update_draw_properties();

  // Viewport scrollbar positions are determined using the viewport bounds
  // delta.
  active_tree_->SetScrollbarGeometriesNeedUpdate();
  active_tree_->set_needs_update_draw_properties();

  // For pre-BlinkGenPropertyTrees mode, we need to ensure the layers are
  // appropriately updated.
  if (!settings().use_layer_lists) {
    if (OuterViewportContainerLayer())
      OuterViewportContainerLayer()->NoteLayerPropertyChanged();
    if (InnerViewportScrollLayer())
      InnerViewportScrollLayer()->NoteLayerPropertyChanged();
    if (OuterViewportScrollLayer())
      OuterViewportScrollLayer()->NoteLayerPropertyChanged();
  }
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
                                  const gfx::Rect& rect,
                                  const viz::SurfaceId& surface_id,
                                  float device_scale_factor) {
  hit_test_region->frame_sink_id = surface_id.frame_sink_id();
  hit_test_region->flags = flags;

  hit_test_region->rect = rect;
  // The transform of hit test region maps a point from parent hit test region
  // to the local space. This is the inverse of screen space transform. Because
  // hit test query wants the point in target to be in Pixel space, we
  // counterscale the transform here. Note that the rect is scaled by dsf, so
  // the point and the rect are still in the same space.
  gfx::Transform surface_to_root_transform = layer->ScreenSpaceTransform();
  surface_to_root_transform.Scale(SK_MScalar1 / device_scale_factor,
                                  SK_MScalar1 / device_scale_factor);
  surface_to_root_transform.FlattenTo2d();
  // TODO(sunxd): Avoid losing precision by not using inverse if possible.
  bool ok = surface_to_root_transform.GetInverse(&hit_test_region->transform);
  // Note: If |ok| is false, the |transform| is set to the identity before
  // returning, which is what we want.
  ALLOW_UNUSED_LOCAL(ok);
}

base::Optional<viz::HitTestRegionList> LayerTreeHostImpl::BuildHitTestData() {
  if (!settings_.build_hit_test_data)
    return {};

  base::Optional<viz::HitTestRegionList> hit_test_region_list(base::in_place);
  hit_test_region_list->flags = viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds = active_tree_->GetDeviceViewport();
  hit_test_region_list->transform = DrawTransform();

  float device_scale_factor = active_tree()->device_scale_factor();

  Region overlapping_region;
  for (const auto* layer : base::Reversed(*active_tree())) {
    if (!layer->should_hit_test())
      continue;

    if (layer->is_surface_layer()) {
      const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
      // If a surface layer is created not by child frame compositor or the
      // frame owner has pointer-events: none property, the surface layer
      // becomes not hit testable. We should not generate data for it.
      if (!surface_layer->ShouldGenerateSurfaceHitTestData()) {
        // If a surface layer is created due to video or offscreen canvas, it
        // can still block overlapped surface layers from getting events, we
        // need to account for all layers that don't have pointer-events: none.
        if (!surface_layer->has_pointer_events_none()) {
          overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
              layer->ScreenSpaceTransform(),
              gfx::Rect(surface_layer->bounds())));
        }
        continue;
      }

      gfx::Rect content_rect(
          gfx::ScaleToEnclosingRect(gfx::Rect(surface_layer->bounds()),
                                    device_scale_factor, device_scale_factor));

      gfx::Rect layer_screen_space_rect = MathUtil::MapEnclosingClippedRect(
          surface_layer->ScreenSpaceTransform(),
          gfx::Rect(surface_layer->bounds()));
      if (overlapping_region.Contains(layer_screen_space_rect))
        continue;

      auto flag = GetFlagsForSurfaceLayer(surface_layer);
      if (overlapping_region.Intersects(layer_screen_space_rect))
        flag |= viz::HitTestRegionFlags::kHitTestAsk;
      if (surface_layer->is_clipped()) {
        bool layer_hit_test_region_is_rectangle =
            active_tree()
                ->property_trees()
                ->effect_tree.ClippedHitTestRegionIsRectangle(
                    surface_layer->effect_tree_index()) &&
            surface_layer->ScreenSpaceTransform().Preserves2dAxisAlignment();
        content_rect =
            gfx::ScaleToEnclosingRect(surface_layer->visible_layer_rect(),
                                      device_scale_factor, device_scale_factor);
        if (!layer_hit_test_region_is_rectangle)
          flag |= viz::HitTestRegionFlags::kHitTestAsk;
      }
      const auto& surface_id = surface_layer->range().end();
      hit_test_region_list->regions.emplace_back();
      PopulateHitTestRegion(&hit_test_region_list->regions.back(), layer, flag,
                            content_rect, surface_id, device_scale_factor);
      continue;
    }
    // TODO(sunxd): Submit all overlapping layer bounds as hit test regions.
    // Also investigate if we can use visible layer rect as overlapping regions.
    overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
        layer->ScreenSpaceTransform(), gfx::Rect(layer->bounds())));
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
}

bool LayerTreeHostImpl::HaveRootScrollNode() const {
  return InnerViewportScrollNode();
}

void LayerTreeHostImpl::SetNeedsCommit() {
  client_->SetNeedsCommitOnImplThread();
}

LayerImpl* LayerTreeHostImpl::InnerViewportContainerLayer() const {
  return active_tree_->InnerViewportContainerLayer();
}

LayerImpl* LayerTreeHostImpl::InnerViewportScrollLayer() const {
  return active_tree_->InnerViewportScrollLayer();
}

ScrollNode* LayerTreeHostImpl::InnerViewportScrollNode() const {
  return active_tree_->InnerViewportScrollNode();
}

LayerImpl* LayerTreeHostImpl::OuterViewportContainerLayer() const {
  return active_tree_->OuterViewportContainerLayer();
}

LayerImpl* LayerTreeHostImpl::OuterViewportScrollLayer() const {
  return active_tree_->OuterViewportScrollLayer();
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

bool LayerTreeHostImpl::IsActivelyScrolling() const {
  if (!CurrentlyScrollingNode())
    return false;
  // On Android WebView root flings are controlled by the application,
  // so the compositor does not animate them and can't tell if they
  // are actually animating. So assume there are none.
  if (settings_.ignore_root_layer_flings && IsCurrentlyScrollingViewport())
    return false;
  return did_lock_scrolling_layer_;
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!pending_tree_);
  if (recycle_tree_) {
    recycle_tree_.swap(pending_tree_);
  } else {
    pending_tree_ = std::make_unique<LayerTreeImpl>(
        this, active_tree()->page_scale_factor(),
        active_tree()->top_controls_shown_ratio(),
        active_tree()->elastic_overscroll());
  }

  client_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_ASYNC_BEGIN0("cc", "PendingTree:waiting", pending_tree_.get());

  DCHECK(!pending_tree_duration_timer_);
  pending_tree_duration_timer_.reset(new PendingTreeDurationHistogramTimer());
}

void LayerTreeHostImpl::PushScrollbarOpacitiesFromActiveToPending() {
  if (!active_tree())
    return;
  for (auto& pair : scrollbar_animation_controllers_) {
    for (auto* scrollbar : pair.second->Scrollbars()) {
      if (const EffectNode* source_effect_node =
              active_tree()
                  ->property_trees()
                  ->effect_tree.FindNodeFromElementId(
                      scrollbar->element_id())) {
        if (EffectNode* target_effect_node =
                pending_tree()
                    ->property_trees()
                    ->effect_tree.FindNodeFromElementId(
                        scrollbar->element_id())) {
          DCHECK(target_effect_node);
          float source_opacity = source_effect_node->opacity;
          float target_opacity = target_effect_node->opacity;
          if (source_opacity == target_opacity)
            continue;
          target_effect_node->opacity = source_opacity;
          pending_tree()->property_trees()->effect_tree.set_needs_update(true);
        }
      }
    }
  }
}

void LayerTreeHostImpl::ActivateSyncTree() {
  TRACE_EVENT0("cc,benchmark", "LayerTreeHostImpl::ActivateSyncTree()");
  if (pending_tree_) {
    TRACE_EVENT_ASYNC_END0("cc", "PendingTree:waiting", pending_tree_.get());
    active_tree_->lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

    DCHECK(pending_tree_duration_timer_);
    // Reset will call the destructor and log the timer histogram.
    pending_tree_duration_timer_.reset();

    // In most cases, this will be reset in NotifyReadyToActivate, since we
    // activate the pending tree only when its ready. But an activation may be
    // forced, in the case of a context loss for instance, so reset it here as
    // well.
    pending_tree_raster_duration_timer_.reset();

    // Process any requests in the UI resource queue.  The request queue is
    // given in LayerTreeHost::FinishCommitOnImplThread.  This must take place
    // before the swap.
    pending_tree_->ProcessUIResourceRequestQueue();

    if (pending_tree_->needs_full_tree_sync()) {
      TreeSynchronizer::SynchronizeTrees(pending_tree_.get(),
                                         active_tree_.get());
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

  UpdateViewportContainerSizes();

  if (InnerViewportScrollNode()) {
    active_tree_->property_trees()->scroll_tree.ClampScrollToMaxScrollOffset(
        InnerViewportScrollNode(), active_tree_.get());
  }
  if (OuterViewportScrollNode()) {
    active_tree_->property_trees()->scroll_tree.ClampScrollToMaxScrollOffset(
        OuterViewportScrollNode(), active_tree_.get());
  }

  active_tree_->DidBecomeActive();
  client_->RenewTreePriority();

  // If we have any picture layers, then by activating we also modified tile
  // priorities.
  if (!active_tree_->picture_layers().empty())
    DidModifyTilePriorities();

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
  // Activation can change the root scroll offset, so inform the synchronous
  // input handler.
  UpdateRootLayerStateForSynchronousInputHandler();

  // Update the child's LocalSurfaceId.
  if (active_tree()->local_surface_id_from_parent().is_valid()) {
    child_local_surface_id_allocator_.UpdateFromParent(
        active_tree()->local_surface_id_from_parent(),
        active_tree()->local_surface_id_allocation_time_from_parent());
    if (active_tree()->TakeNewLocalSurfaceIdRequest())
      child_local_surface_id_allocator_.GenerateId();
  }

  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_host_impl=3
  if (VLOG_IS_ON(3)) {
    std::string property_trees;
    base::JSONWriter::WriteWithOptions(
        *active_tree_->property_trees()->AsTracedValue()->ToBaseValue(),
        base::JSONWriter::OPTIONS_PRETTY_PRINT, &property_trees);
    VLOG(3) << "After activating sync tree, the active tree:"
            << "\nproperty_trees:\n"
            << property_trees << "\nlayers:\n"
            << LayerListAsJson();
  }
}

void LayerTreeHostImpl::ActivateStateForImages() {
  image_animation_controller_.DidActivate();
  tile_manager_.DidActivateSyncTree();
}

void LayerTreeHostImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Only work for low-end devices for now.
  if (!base::SysInfo::IsLowEndDevice())
    return;

  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  ReleaseTileResources();
  active_tree_->OnPurgeMemory();
  if (pending_tree_)
    pending_tree_->OnPurgeMemory();
  if (recycle_tree_)
    recycle_tree_->OnPurgeMemory();

  EvictAllUIResources();
  if (image_decode_cache_) {
    image_decode_cache_->SetShouldAggressivelyFreeResources(true);
    image_decode_cache_->SetShouldAggressivelyFreeResources(false);
  }
  if (resource_pool_)
    resource_pool_->OnMemoryPressure(level);
  tile_manager_.decoded_image_tracker().UnlockAllImages();
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;
  DidVisibilityChange(this, visible_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());

  // If we just became visible, we have to ensure that we draw high res tiles,
  // to prevent checkerboard/low res flashes.
  if (visible_) {
    // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
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
      SetNeedsRedraw();
    }
    // If surface synchronization is off, force allocating a new LocalSurfaceId
    // because the previous LocalSurfaceId might have been evicted while we were
    // invisible. When surface synchronization is on, the embedder will pass us
    // a new LocalSurfaceID.
    if (layer_tree_frame_sink_ && !settings_.enable_surface_synchronization)
      layer_tree_frame_sink_->ForceAllocateNewId();
  } else {
    EvictAllUIResources();
    // Call PrepareTiles to evict tiles when we become invisible.
    PrepareTiles();
    tile_manager_.decoded_image_tracker().UnlockAllImages();
  }
}

void LayerTreeHostImpl::SetNeedsOneBeginImplFrame() {
  // TODO(miletus): This is just the compositor-thread-side call to the
  // SwapPromiseMonitor to say something happened that may cause a swap in the
  // future. The name should not refer to SetNeedsRedraw but it does for now.
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsOneBeginImplFrameOnImplThread();
}

void LayerTreeHostImpl::SetNeedsRedraw() {
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsRedrawOnImplThread();
  skipped_frame_tracker_.WillProduceFrame();
}

ManagedMemoryPolicy LayerTreeHostImpl::ActualManagedMemoryPolicy() const {
  ManagedMemoryPolicy actual = cached_managed_memory_policy_;
  if (debug_state_.rasterize_only_visible_content) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;
  } else if (use_gpu_rasterization()) {
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
  active_tree_->RecreateTileResources();
  if (pending_tree_)
    pending_tree_->RecreateTileResources();
  if (recycle_tree_)
    recycle_tree_->RecreateTileResources();
}

void LayerTreeHostImpl::CreateTileManagerResources() {
  raster_buffer_provider_ = CreateRasterBufferProvider();

  viz::ResourceFormat tile_format = TileRasterBufferFormat(
      settings_, layer_tree_frame_sink_->context_provider(),
      use_gpu_rasterization_);

  if (use_gpu_rasterization_) {
    image_decode_cache_ = std::make_unique<GpuImageDecodeCache>(
        layer_tree_frame_sink_->worker_context_provider(),
        use_oop_rasterization_,
        viz::ResourceFormatToClosestSkColorType(/*gpu_compositing=*/true,
                                                tile_format),
        settings_.decoded_image_working_set_budget_bytes, max_texture_size_,
        paint_image_generator_client_id_);
  } else {
    bool gpu_compositing = !!layer_tree_frame_sink_->context_provider();
    image_decode_cache_ = std::make_unique<SoftwareImageDecodeCache>(
        viz::ResourceFormatToClosestSkColorType(gpu_compositing, tile_format),
        settings_.decoded_image_working_set_budget_bytes,
        paint_image_generator_client_id_);
  }

  // Pass the single-threaded synchronous task graph runner to the worker pool
  // if we're in synchronous single-threaded mode.
  TaskGraphRunner* task_graph_runner = task_graph_runner_;
  if (is_synchronous_single_threaded_) {
    DCHECK(!single_thread_synchronous_task_graph_runner_);
    single_thread_synchronous_task_graph_runner_.reset(
        new SynchronousTaskGraphRunner);
    task_graph_runner = single_thread_synchronous_task_graph_runner_.get();
  }

  tile_manager_.SetResources(resource_pool_.get(), image_decode_cache_.get(),
                             task_graph_runner, raster_buffer_provider_.get(),
                             use_gpu_rasterization_);
  tile_manager_.SetCheckerImagingForceDisabled(
      settings_.only_checker_images_with_gpu_raster && !use_gpu_rasterization_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
}

std::unique_ptr<RasterBufferProvider>
LayerTreeHostImpl::CreateRasterBufferProvider() {
  DCHECK(GetTaskRunner());

  viz::ContextProvider* compositor_context_provider =
      layer_tree_frame_sink_->context_provider();
  if (!compositor_context_provider)
    return std::make_unique<BitmapRasterBufferProvider>(layer_tree_frame_sink_);

  const gpu::Capabilities& caps =
      compositor_context_provider->ContextCapabilities();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink_->worker_context_provider();

  viz::ResourceFormat tile_format = TileRasterBufferFormat(
      settings_, compositor_context_provider, use_gpu_rasterization_);

  if (use_gpu_rasterization_) {
    DCHECK(worker_context_provider);

    int msaa_sample_count = use_msaa_ ? RequestedMSAASampleCount() : 0;
    return std::make_unique<GpuRasterBufferProvider>(
        compositor_context_provider, worker_context_provider,
        settings_.resource_settings.use_gpu_memory_buffer_resources,
        msaa_sample_count, tile_format, settings_.max_gpu_raster_tile_size,
        settings_.unpremultiply_and_dither_low_bit_depth_tiles,
        use_oop_rasterization_);
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
        layer_tree_frame_sink_->gpu_memory_buffer_manager(),
        compositor_context_provider, tile_format);
  }

  const int max_copy_texture_chromium_size =
      caps.max_copy_texture_chromium_size;
  return std::make_unique<OneCopyRasterBufferProvider>(
      GetTaskRunner(), compositor_context_provider, worker_context_provider,
      layer_tree_frame_sink_->gpu_memory_buffer_manager(),
      max_copy_texture_chromium_size, settings_.use_partial_raster,
      settings_.resource_settings.use_gpu_memory_buffer_resources,
      settings_.max_staging_buffer_usage_in_bytes, tile_format);
}

void LayerTreeHostImpl::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  mutator_host_->SetLayerTreeMutator(std::move(mutator));
}

LayerImpl* LayerTreeHostImpl::ViewportMainScrollLayer() {
  return viewport()->MainScrollLayer();
}

ScrollNode* LayerTreeHostImpl::ViewportMainScrollNode() {
  if (!ViewportMainScrollLayer())
    return nullptr;

  return active_tree_->property_trees()->scroll_tree.Node(
      ViewportMainScrollLayer()->scroll_tree_index());
}

void LayerTreeHostImpl::QueueImageDecode(int request_id,
                                         const PaintImage& image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::QueueImageDecode", "frame_key",
               image.GetKeyForFrame(PaintImage::kDefaultFrameIndex).ToString());
  // Optimistically specify the current raster color space, since we assume that
  // it won't change.
  tile_manager_.decoded_image_tracker().QueueImageDecode(
      image, GetRasterColorSpace().color_space,
      base::Bind(&LayerTreeHostImpl::ImageDecodeFinished,
                 base::Unretained(this), request_id));
  tile_manager_.checker_image_tracker().DisallowCheckeringForImage(image);
}

void LayerTreeHostImpl::ImageDecodeFinished(int request_id,
                                            bool decode_succeeded) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::ImageDecodeFinished");
  completed_image_decode_requests_.emplace_back(request_id, decode_succeeded);
  client_->NotifyImageDecodeRequestFinished();
}

std::vector<std::pair<int, bool>>
LayerTreeHostImpl::TakeCompletedImageDecodeRequests() {
  auto result = std::move(completed_image_decode_requests_);
  completed_image_decode_requests_.clear();
  return result;
}

void LayerTreeHostImpl::ClearCaches() {
  // It is safe to clear the decode policy tracking on navigations since it
  // comes with an invalidation and the image ids are never re-used.
  bool can_clear_decode_policy_tracking = true;
  tile_manager_.ClearCheckerImageTracking(can_clear_decode_policy_tracking);
  if (image_decode_cache_)
    image_decode_cache_->ClearCache();
  image_animation_controller_.set_did_navigate();
}

void LayerTreeHostImpl::DidChangeScrollbarVisibility() {
  // Need a commit since input handling for scrollbars is handled in Blink so
  // we need to communicate to Blink when the compositor shows/hides the
  // scrollbars.
  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::CleanUpTileManagerResources() {
  tile_manager_.FinishTasksAndCleanUp();
  single_thread_synchronous_task_graph_runner_ = nullptr;
  image_decode_cache_ = nullptr;
  raster_buffer_provider_ = nullptr;
  // Any resources that were allocated previously should be considered not good
  // for reuse, as the RasterBufferProvider will be replaced and it may choose
  // to allocate future resources differently.
  resource_pool_->InvalidateResources();

  // We've potentially just freed a large number of resources on our various
  // contexts. Flushing now helps ensure these are cleaned up quickly
  // preventing driver cache growth. See crbug.com/643251
  if (layer_tree_frame_sink_) {
    if (auto* compositor_context = layer_tree_frame_sink_->context_provider())
      compositor_context->ContextGL()->ShallowFlushCHROMIUM();
    if (auto* worker_context =
            layer_tree_frame_sink_->worker_context_provider()) {
      viz::RasterContextProvider::ScopedRasterContextLock hold(worker_context);
      hold.RasterInterface()->ShallowFlushCHROMIUM();
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
  CleanUpTileManagerResources();
  resource_pool_ = nullptr;
  ClearUIResources();

  if (layer_tree_frame_sink_->context_provider()) {
    auto* gl = layer_tree_frame_sink_->context_provider()->ContextGL();
    gl->Finish();
  }

  // Release any context visibility before we destroy the LayerTreeFrameSink.
  SetContextVisibility(false);

  bool all_resources_are_lost = layer_tree_frame_sink_->context_provider();

  // Detach from the old LayerTreeFrameSink and reset |layer_tree_frame_sink_|
  // pointer as this surface is going to be destroyed independent of if binding
  // the new LayerTreeFrameSink succeeds or not.
  layer_tree_frame_sink_->DetachFromClient();
  layer_tree_frame_sink_ = nullptr;

  // If gpu compositing, then any resources created with the gpu context in the
  // LayerTreeFrameSink were exported to the display compositor may be modified
  // by it, and thus we would be unable to determine what state they are in, in
  // order to reuse them, so they must be lost. In software compositing, the
  // resources are not modified by the display compositor (there is no stateful
  // metadata for shared memory), so we do not need to consider them lost.
  //
  // In both cases, the resources that are exported to the display compositor
  // will have no means of being returned to this client without the
  // LayerTreeFrameSink, so they should no longer be considered as exported.
  // Do this *after* any interactions with the |layer_tree_frame_sink_| in case
  // it tries to return resources during destruction.
  //
  // The assumption being made here is that the display compositor WILL NOT use
  // any resources previously exported when the CompositorFrameSink is closed.
  // This should be true as the connection is closed when the display compositor
  // shuts down/crashes, or when it believes we are a malicious client in which
  // case it will not display content from the previous CompositorFrameSink. If
  // this assumption is violated, we may modify resources no longer considered
  // as exported while the display compositor is still making use of them,
  // leading to visual mistakes.
  resource_provider_.ReleaseAllExportedResources(all_resources_are_lost);

  // We don't know if the next LayerTreeFrameSink will support GPU
  // rasterization. Make sure to clear the flag so that we force a
  // re-computation.
  use_gpu_rasterization_ = false;
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

  auto* context_provider = layer_tree_frame_sink_->context_provider();
  if (context_provider) {
    max_texture_size_ =
        context_provider->ContextCapabilities().max_texture_size;
  } else {
    // Pick an arbitrary limit here similar to what hardware might.
    max_texture_size_ = 16 * 1024;
  }

  resource_pool_ = std::make_unique<ResourcePool>(
      &resource_provider_, context_provider, GetTaskRunner(),
      ResourcePool::kDefaultExpirationDelay,
      settings_.disallow_non_exact_resource_reuse);

  auto* context = layer_tree_frame_sink_->worker_context_provider();
  if (context) {
    viz::RasterContextProvider::ScopedRasterContextLock hold(context);
    use_oop_rasterization_ = context->ContextCapabilities().supports_oop_raster;
  } else {
    use_oop_rasterization_ = false;
  }

  // Since the new context may be capable of MSAA, update status here. We don't
  // need to check the return value since we are recreating all resources
  // already.
  SetNeedUpdateGpuRasterizationStatus();
  UpdateGpuRasterizationStatus();

  // See note in LayerTreeImpl::UpdateDrawProperties, new LayerTreeFrameSink
  // means a new max texture size which affects draw properties. Also, if the
  // draw properties were up to date, layers still lost resources and we need to
  // UpdateDrawProperties() after calling RecreateTreeResources().
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();

  CreateTileManagerResources();
  RecreateTileResources();

  client_->OnCanDrawStateChanged(CanDraw());
  SetFullViewportDamage();
  // There will not be anything to draw here, so set high res
  // to avoid checkerboards, typically when we are recovering
  // from lost context.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();

  // Always allocate a new viz::LocalSurfaceId when we get a new
  // LayerTreeFrameSink to ensure that we do not reuse the same surface after
  // it might have been garbage collected.
  if (settings_.enable_surface_synchronization) {
    const viz::LocalSurfaceId& local_surface_id =
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    if (local_surface_id.is_valid())
      child_local_surface_id_allocator_.GenerateId();
  } else {
    layer_tree_frame_sink_->ForceAllocateNewId();
  }

  return true;
}

void LayerTreeHostImpl::SetBeginFrameSource(viz::BeginFrameSource* source) {
  client_->SetBeginFrameSource(source);
}

const gfx::Transform& LayerTreeHostImpl::DrawTransform() const {
  return external_transform_;
}

void LayerTreeHostImpl::DidChangeBrowserControlsPosition() {
  UpdateViewportContainerSizes();
  SetNeedsRedraw();
  SetNeedsOneBeginImplFrame();
  active_tree_->set_needs_update_draw_properties();
  SetFullViewportDamage();
}

float LayerTreeHostImpl::TopControlsHeight() const {
  return active_tree_->top_controls_height();
}

float LayerTreeHostImpl::BottomControlsHeight() const {
  return active_tree_->bottom_controls_height();
}

void LayerTreeHostImpl::SetCurrentBrowserControlsShownRatio(float ratio) {
  if (active_tree_->SetCurrentBrowserControlsShownRatio(ratio))
    DidChangeBrowserControlsPosition();
}

float LayerTreeHostImpl::CurrentBrowserControlsShownRatio() const {
  return active_tree_->CurrentBrowserControlsShownRatio();
}

void LayerTreeHostImpl::BindToClient(InputHandlerClient* client) {
  DCHECK(input_handler_client_ == nullptr);
  input_handler_client_ = client;
}

InputHandler::ScrollStatus LayerTreeHostImpl::TryScroll(
    const gfx::PointF& screen_space_point,
    InputHandler::ScrollInputType type,
    const ScrollTree& scroll_tree,
    ScrollNode* scroll_node) const {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (scroll_node->main_thread_scrolling_reasons) {
    TRACE_EVENT1("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread",
                 "MainThreadScrollingReason",
                 scroll_node->main_thread_scrolling_reasons);
    scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        scroll_node->main_thread_scrolling_reasons;
    return scroll_status;
  }

  gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node->id);
  if (!screen_space_transform.IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNonInvertibleTransform;
    return scroll_status;
  }

  LayerImpl* layer =
      active_tree_->ScrollableLayerByElementId(scroll_node->element_id);

  // We may not find an associated layer for the root or secondary root node -
  // that's fine, they're not associated with any elements on the page. We also
  // won't find a layer for the inner viewport (in SPv2) since it doesn't
  // require hit testing.
  DCHECK(layer || scroll_node->id == ScrollTree::kRootNodeId ||
         scroll_node->id == ScrollTree::kSecondaryRootNodeId ||
         scroll_node->scrolls_inner_viewport);

  if (layer && !layer->non_fast_scrollable_region().IsEmpty()) {
    bool clipped = false;
    gfx::Transform inverse_screen_space_transform(
        gfx::Transform::kSkipInitialization);
    if (!screen_space_transform.GetInverse(&inverse_screen_space_transform)) {
      // TODO(shawnsingh): We shouldn't be applying a projection if screen space
      // transform is uninvertible here. Perhaps we should be returning
      // SCROLL_ON_MAIN_THREAD in this case?
    }

    gfx::PointF hit_test_point_in_layer_space = MathUtil::ProjectPoint(
        inverse_screen_space_transform, screen_space_point, &clipped);
    if (!clipped && layer->non_fast_scrollable_region().Contains(
                        gfx::ToRoundedPoint(hit_test_point_in_layer_space))) {
      TRACE_EVENT0("cc",
                   "LayerImpl::tryScroll: Failed NonFastScrollableRegion");
      scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNonFastScrollableRegion;
      return scroll_status;
    }
  }

  if (!scroll_node->scrollable) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  gfx::ScrollOffset max_scroll_offset =
      scroll_tree.MaxScrollOffset(scroll_node->id);
  if (max_scroll_offset.x() <= 0 && max_scroll_offset.y() <= 0) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  scroll_status.thread = InputHandler::SCROLL_ON_IMPL_THREAD;
  return scroll_status;
}

static bool IsMainThreadScrolling(const InputHandler::ScrollStatus& status,
                                  const ScrollNode* scroll_node) {
  if (status.thread == InputHandler::SCROLL_ON_MAIN_THREAD) {
    if (!!scroll_node->main_thread_scrolling_reasons) {
      DCHECK(MainThreadScrollingReason::MainThreadCanSetScrollReasons(
          status.main_thread_scrolling_reasons));
    } else {
      DCHECK(MainThreadScrollingReason::CompositorCanSetScrollReasons(
          status.main_thread_scrolling_reasons));
    }
    return true;
  }
  return false;
}

ScrollNode* LayerTreeHostImpl::FindScrollNodeForDeviceViewportPoint(
    const gfx::PointF& device_viewport_point,
    InputHandler::ScrollInputType type,
    LayerImpl* layer_impl,
    bool* scroll_on_main_thread,
    uint32_t* main_thread_scrolling_reasons) const {
  DCHECK(scroll_on_main_thread);
  DCHECK(main_thread_scrolling_reasons);
  *main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  // Walk up the hierarchy and look for a scrollable layer.
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* impl_scroll_node = nullptr;
  if (layer_impl) {
    ScrollNode* scroll_node = scroll_tree.Node(layer_impl->scroll_tree_index());
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      // The content layer can also block attempts to scroll outside the main
      // thread.
      ScrollStatus status =
          TryScroll(device_viewport_point, type, scroll_tree, scroll_node);
      if (IsMainThreadScrolling(status, scroll_node)) {
        *scroll_on_main_thread = true;
        *main_thread_scrolling_reasons = status.main_thread_scrolling_reasons;
        return scroll_node;
      }

      if (status.thread == InputHandler::SCROLL_ON_IMPL_THREAD &&
          !impl_scroll_node) {
        impl_scroll_node = scroll_node;
      }
    }
  }

  // Falling back to the viewport layer ensures generation of root overscroll
  // notifications. We use the viewport's main scroll layer to represent the
  // viewport in scrolling code.
  bool scrolls_inner_viewport =
      impl_scroll_node && impl_scroll_node->scrolls_inner_viewport;
  bool scrolls_outer_viewport =
      impl_scroll_node && impl_scroll_node->scrolls_outer_viewport;
  if (!impl_scroll_node || scrolls_inner_viewport || scrolls_outer_viewport)
    impl_scroll_node = OuterViewportScrollNode();

  if (impl_scroll_node) {
    // Ensure that final layer scrolls on impl thread (crbug.com/625100)
    ScrollStatus status =
        TryScroll(device_viewport_point, type, scroll_tree, impl_scroll_node);
    if (IsMainThreadScrolling(status, impl_scroll_node)) {
      *scroll_on_main_thread = true;
      *main_thread_scrolling_reasons = status.main_thread_scrolling_reasons;
    }
  }

  return impl_scroll_node;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBeginImpl(
    ScrollState* scroll_state,
    ScrollNode* scrolling_node,
    InputHandler::ScrollInputType type) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (!scrolling_node) {
    if (settings_.is_layer_tree_for_subframe) {
      TRACE_EVENT_INSTANT0("cc", "Ignored - No ScrollNode (OOPIF)",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_UNKNOWN;
    } else {
      TRACE_EVENT_INSTANT0("cc", "Ignroed - No ScrollNode",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_IGNORED;
    }
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNoScrollingLayer;
    return scroll_status;
  }
  scroll_status.thread = SCROLL_ON_IMPL_THREAD;
  mutator_host_->ScrollAnimationAbort();
  is_animating_for_snap_ = false;

  browser_controls_offset_manager_->ScrollBegin();

  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode ScrollBeginImpl",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       scrolling_node ? false : true);
  active_tree_->SetCurrentlyScrollingNode(scrolling_node);
  // TODO(majidvp): get rid of wheel_scrolling_ and set is_direct_manipulation
  // in input_handler_proxy instead.
  wheel_scrolling_ = IsWheelBasedScroll(type);
  scroll_state->set_is_direct_manipulation(!wheel_scrolling_);
  // Invoke |DistributeScrollDelta| even with zero delta and velocity to ensure
  // scroll customization callbacks are invoked.
  DistributeScrollDelta(scroll_state);

  // If the CurrentlyScrollingNode doesn't exist after distributing scroll
  // delta, no scroller can scroll in the given delta hint direction(s).
  if (!active_tree_->CurrentlyScrollingNode()) {
    TRACE_EVENT_INSTANT0("cc", "Ignored - Didnt Scroll",
                         TRACE_EVENT_SCOPE_THREAD);
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;
    return scroll_status;
  }

  // If the viewport is scrolling and it cannot consume any delta hints, the
  // scroll event will need to get bubbled if the viewport is for a guest or
  // oopif.
  if (active_tree_->CurrentlyScrollingNode() == ViewportMainScrollNode() &&
      !viewport()->CanScroll(*scroll_state)) {
    scroll_status.bubble = true;
  }

  client_->RenewTreePriority();
  RecordCompositorSlowScrollMetric(type, CC_THREAD);

  UpdateScrollSourceInfo(wheel_scrolling_);

  return scroll_status;
}

InputHandler::ScrollStatus LayerTreeHostImpl::RootScrollBegin(
    ScrollState* scroll_state,
    InputHandler::ScrollInputType type) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::RootScrollBegin");

  ClearCurrentlyScrollingNode();

  gfx::Point viewport_point(scroll_state->position_x(),
                            scroll_state->position_y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  LayerImpl* first_scrolling_layer_or_scrollbar =
      active_tree_->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
          device_viewport_point);

  if (IsTouchDraggingScrollbar(first_scrolling_layer_or_scrollbar, type)) {
    TRACE_EVENT_INSTANT0("cc", "Scrollbar Scrolling", TRACE_EVENT_SCOPE_THREAD);
    ScrollStatus scroll_status;
    scroll_status.thread = SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kScrollbarScrolling;
    return scroll_status;
  }

  return ScrollBeginImpl(scroll_state, OuterViewportScrollNode(), type);
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBegin(
    ScrollState* scroll_state,
    InputHandler::ScrollInputType type) {
  ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBegin");

  ScrollNode* scrolling_node = nullptr;
  bool scroll_on_main_thread = false;

  if (scroll_state->is_in_inertial_phase())
    scrolling_node = CurrentlyScrollingNode();

  if (!scrolling_node) {
    ClearCurrentlyScrollingNode();

    gfx::Point viewport_point(scroll_state->position_x(),
                              scroll_state->position_y());

    gfx::PointF device_viewport_point = gfx::ScalePoint(
        gfx::PointF(viewport_point), active_tree_->device_scale_factor());
    LayerImpl* layer_impl =
        active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

    if (layer_impl) {
      LayerImpl* first_scrolling_layer_or_scrollbar =
          active_tree_->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
              device_viewport_point);

      // Touch dragging the scrollbar requires falling back to main-thread
      // scrolling.
      if (IsTouchDraggingScrollbar(first_scrolling_layer_or_scrollbar, type)) {
        TRACE_EVENT_INSTANT0("cc", "Scrollbar Scrolling",
                             TRACE_EVENT_SCOPE_THREAD);
        scroll_status.thread = SCROLL_ON_MAIN_THREAD;
        scroll_status.main_thread_scrolling_reasons =
            MainThreadScrollingReason::kScrollbarScrolling;
        return scroll_status;
      } else if (!IsInitialScrollHitTestReliable(
                     layer_impl, first_scrolling_layer_or_scrollbar)) {
        TRACE_EVENT_INSTANT0("cc", "Failed Hit Test", TRACE_EVENT_SCOPE_THREAD);
        scroll_status.thread = SCROLL_UNKNOWN;
        scroll_status.main_thread_scrolling_reasons =
            MainThreadScrollingReason::kFailedHitTest;
        return scroll_status;
      }
    }

    scrolling_node = FindScrollNodeForDeviceViewportPoint(
        device_viewport_point, type, layer_impl, &scroll_on_main_thread,
        &scroll_status.main_thread_scrolling_reasons);
  }

  if (scroll_on_main_thread) {
    RecordCompositorSlowScrollMetric(type, MAIN_THREAD);

    scroll_status.thread = SCROLL_ON_MAIN_THREAD;
    return scroll_status;
  } else if (scrolling_node) {
    scroll_affects_scroll_handler_ = active_tree_->have_scroll_event_handlers();
  }

  return ScrollBeginImpl(scroll_state, scrolling_node, type);
}

// Requires falling back to main thread scrolling when it hit tests in scrollbar
// from touch.
bool LayerTreeHostImpl::IsTouchDraggingScrollbar(
    LayerImpl* first_scrolling_layer_or_scrollbar,
    InputHandler::ScrollInputType type) {
  return first_scrolling_layer_or_scrollbar &&
         first_scrolling_layer_or_scrollbar->is_scrollbar() &&
         type == InputHandler::TOUCHSCREEN;
}

// Initial scroll hit testing can be unreliable in the presence of squashed
// layers. In this case, we fall back to main thread scrolling.
bool LayerTreeHostImpl::IsInitialScrollHitTestReliable(
    LayerImpl* layer_impl,
    LayerImpl* first_scrolling_layer_or_scrollbar) {
  if (!first_scrolling_layer_or_scrollbar)
    return true;
  ScrollNode* closest_scroll_node = nullptr;
  auto& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.Node(layer_impl->scroll_tree_index());
  for (; scroll_tree.parent(scroll_node);
       scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->scrollable) {
      closest_scroll_node = scroll_node;
      break;
    }
  }
  if (!closest_scroll_node)
    return false;

  // If |first_scrolling_layer_or_scrollbar| is scrollable, it will
  // create a scroll node. If this scroll node corresponds to first scrollable
  // ancestor along the scroll tree for |layer_impl|, the hit test has not
  // escaped to other areas of the scroll tree and is reliable.
  if (first_scrolling_layer_or_scrollbar->scrollable()) {
    return closest_scroll_node->id ==
           first_scrolling_layer_or_scrollbar->scroll_tree_index();
  }

  // If |first_scrolling_layer_or_scrollbar| is not scrollable, it must
  // be a drawn scrollbar. It may hit the squashing layer at the same time.
  // These hit tests require falling back to main-thread scrolling.
  DCHECK(first_scrolling_layer_or_scrollbar->is_scrollbar());
  return false;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollAnimatedBegin(
    ScrollState* scroll_state) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollAnimatedBegin");
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    gfx::Vector2dF delta;

    if (ScrollAnimationUpdateTarget(scroll_node, delta, base::TimeDelta())) {
      scroll_status.thread = SCROLL_ON_IMPL_THREAD;
    } else {
      TRACE_EVENT_INSTANT0("cc", "Failed to create animation",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_IGNORED;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNotScrollable;
    }
    return scroll_status;
  }

  // ScrollAnimated is used for animated wheel scrolls. We find the first layer
  // that can scroll and set up an animation of its scroll offset. Note that
  // this does not currently go through the scroll customization machinery
  // that ScrollBy uses for non-animated wheel scrolls.
  scroll_status = ScrollBegin(scroll_state, WHEEL);
  if (scroll_status.thread == SCROLL_ON_IMPL_THREAD) {
    scroll_animating_latched_element_id_ = ElementId();
    ScrollStateData scroll_state_end_data;
    scroll_state_end_data.is_ending = true;
    ScrollState scroll_state_end(scroll_state_end_data);
    ScrollEndImpl(&scroll_state_end);
  }
  return scroll_status;
}

gfx::Vector2dF LayerTreeHostImpl::ComputeScrollDelta(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& delta) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  float scale_factor = active_tree()->current_page_scale_factor();

  gfx::Vector2dF adjusted_scroll(delta);
  adjusted_scroll.Scale(1.f / scale_factor);
  if (!scroll_node.user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node.user_scrollable_vertical)
    adjusted_scroll.set_y(0);

  gfx::ScrollOffset old_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::ScrollOffset new_offset = scroll_tree.ClampScrollOffsetToLimits(
      old_offset + gfx::ScrollOffset(adjusted_scroll), scroll_node);

  gfx::ScrollOffset scrolled = new_offset - old_offset;
  return gfx::Vector2dF(scrolled.x(), scrolled.y());
}

bool LayerTreeHostImpl::ScrollAnimationCreate(ScrollNode* scroll_node,
                                              const gfx::Vector2dF& delta,
                                              base::TimeDelta delayed_by) {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;

  const float kEpsilon = 0.1f;
  bool scroll_animated =
      (std::abs(delta.x()) > kEpsilon || std::abs(delta.y()) > kEpsilon);
  if (!scroll_animated) {
    scroll_tree.ScrollBy(scroll_node, delta, active_tree());
    TRACE_EVENT_INSTANT0("cc", "no scroll animation due to small delta",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  scroll_tree.set_currently_scrolling_node(scroll_node->id);

  gfx::ScrollOffset current_offset =
      scroll_tree.current_scroll_offset(scroll_node->element_id);
  gfx::ScrollOffset target_offset = scroll_tree.ClampScrollOffsetToLimits(
      current_offset + gfx::ScrollOffset(delta), *scroll_node);

  // Start the animation one full frame in. Without any offset, the animation
  // doesn't start until next frame, increasing latency, and preventing our
  // input latency tracking architecture from working.
  base::TimeDelta animation_start_offset = CurrentBeginFrameArgs().interval;

  mutator_host_->ImplOnlyScrollAnimationCreate(
      scroll_node->element_id, target_offset, current_offset, delayed_by,
      animation_start_offset);

  SetNeedsOneBeginImplFrame();

  return true;
}

static bool CanPropagate(ScrollNode* scroll_node, float x, float y) {
  return (x == 0 || scroll_node->overscroll_behavior.x ==
                        OverscrollBehavior::kOverscrollBehaviorTypeAuto) &&
         (y == 0 || scroll_node->overscroll_behavior.y ==
                        OverscrollBehavior::kOverscrollBehaviorTypeAuto);
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollAnimated(
    const gfx::Point& viewport_point,
    const gfx::Vector2dF& scroll_delta,
    base::TimeDelta delayed_by) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollAnimated");
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();

  if (scroll_node) {
    // Flash the overlay scrollbar even if the scroll dalta is 0.
    if (settings_.scrollbar_flash_after_any_scroll_update) {
      FlashAllScrollbars(false);
    } else {
      ScrollbarAnimationController* animation_controller =
          ScrollbarAnimationControllerForElementId(scroll_node->element_id);
      if (animation_controller)
        animation_controller->WillUpdateScroll();
    }

    gfx::Vector2dF delta = scroll_delta;
    if (!scroll_node->user_scrollable_horizontal)
      delta.set_x(0);
    if (!scroll_node->user_scrollable_vertical)
      delta.set_y(0);

    if (ScrollAnimationUpdateTarget(scroll_node, delta, delayed_by)) {
      scroll_status.thread = SCROLL_ON_IMPL_THREAD;
    } else {
      TRACE_EVENT_INSTANT0("cc", "Failed to update animation",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_IGNORED;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNotScrollable;
      // Adding NOTREACHED to debug https://crbug.com/797708, based on the
      // traces on the bug scrolling gets stuck in a situation where the
      // layout_tree_host_impl assumes that there is an ongoing scroll animation
      // since scroll_node exists but the
      // ScrollOffsetAnimationsImpl::ScrollAnimationUpdateTarget returns false
      // since no keyframe_model exists. TODO(sahel):remove this once the issue
      // is fixed.
      NOTREACHED();
    }
    return scroll_status;
  }

  ScrollStateData scroll_state_data;
  scroll_state_data.position_x = viewport_point.x();
  scroll_state_data.position_y = viewport_point.y();
  ScrollState scroll_state(scroll_state_data);

  // ScrollAnimated is used for animated wheel scrolls. We find the first layer
  // that can scroll and set up an animation of its scroll offset. Note that
  // this does not currently go through the scroll customization machinery
  // that ScrollBy uses for non-animated wheel scrolls.
  scroll_status = ScrollBegin(&scroll_state, WHEEL);
  scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_status.thread == SCROLL_ON_IMPL_THREAD && scroll_node) {
    gfx::Vector2dF pending_delta = scroll_delta;
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      if (!scroll_node->scrollable)
        continue;

      // For the rest of the current scroll sequence, latch to the first node
      // that scrolled while it still exists.
      if (scroll_tree.FindNodeFromElementId(
              scroll_animating_latched_element_id_) &&
          scroll_node->element_id != scroll_animating_latched_element_id_) {
        continue;
      }

      bool scrolls_main_viewport_scroll_layer =
          viewport()->MainScrollLayer() &&
          viewport()->MainScrollLayer()->scroll_tree_index() == scroll_node->id;
      if (scrolls_main_viewport_scroll_layer) {
        // Flash the overlay scrollbar even if the scroll dalta is 0.
        if (settings_.scrollbar_flash_after_any_scroll_update) {
          FlashAllScrollbars(false);
        } else {
          ScrollbarAnimationController* animation_controller =
              ScrollbarAnimationControllerForElementId(scroll_node->element_id);
          if (animation_controller)
            animation_controller->WillUpdateScroll();
        }

        gfx::Vector2dF scrolled =
            viewport()->ScrollAnimated(pending_delta, delayed_by);
        // Viewport::ScrollAnimated returns pending_delta as long as it starts
        // an animation.
        did_scroll_x_for_scroll_gesture_ |= scrolled.x() != 0;
        did_scroll_y_for_scroll_gesture_ |= scrolled.y() != 0;
        if (scrolled == pending_delta) {
          scroll_animating_latched_element_id_ = scroll_node->element_id;
          TRACE_EVENT_INSTANT0("cc", "Viewport scroll animated",
                               TRACE_EVENT_SCOPE_THREAD);
          return scroll_status;
        }
        break;
      }

      gfx::Vector2dF scroll_delta =
          ComputeScrollDelta(*scroll_node, pending_delta);
      if (ScrollAnimationCreate(scroll_node, scroll_delta, delayed_by)) {
        did_scroll_x_for_scroll_gesture_ |= scroll_delta.x() != 0;
        did_scroll_y_for_scroll_gesture_ |= scroll_delta.y() != 0;
        scroll_animating_latched_element_id_ = scroll_node->element_id;
        TRACE_EVENT_INSTANT0("cc", "created scroll animation",
                             TRACE_EVENT_SCOPE_THREAD);
        // Flash the overlay scrollbar even if the scroll dalta is 0.
        if (settings_.scrollbar_flash_after_any_scroll_update) {
          FlashAllScrollbars(false);
        } else {
          ScrollbarAnimationController* animation_controller =
              ScrollbarAnimationControllerForElementId(scroll_node->element_id);
          if (animation_controller)
            animation_controller->WillUpdateScroll();
        }
        return scroll_status;
      }

      pending_delta -= scroll_delta;

      if (!CanPropagate(scroll_node, pending_delta.x(), pending_delta.y())) {
        scroll_state.set_is_scroll_chain_cut(true);
        break;
      }
    }
  }
  scroll_state.set_is_ending(true);
  ScrollEndImpl(&scroll_state);
  if (scroll_status.thread == SCROLL_ON_IMPL_THREAD) {
    // Update scroll_status.thread to SCROLL_IGNORED when there is no ongoing
    // scroll animation, we can scroll on impl thread and yet, we couldn't
    // create a new scroll animation. This happens when the scroller has hit its
    // extent.
    TRACE_EVENT_INSTANT0("cc", "Ignored - Scroller at extent",
                         TRACE_EVENT_SCOPE_THREAD);
    scroll_status.thread = SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
  }
  return scroll_status;
}

bool LayerTreeHostImpl::CalculateLocalScrollDeltaAndStartPoint(
    const ScrollNode& scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    const ScrollTree& scroll_tree,
    gfx::Vector2dF* out_local_scroll_delta,
    gfx::PointF* out_local_start_point /*= nullptr*/) {
  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  const gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node.id);
  DCHECK(screen_space_transform.IsInvertible());
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert =
      screen_space_transform.GetInverse(&inverse_screen_space_transform);
  // TODO(shawnsingh): With the advent of impl-side scrolling for non-root
  // layers, we may need to explicitly handle uninvertible transforms here.
  DCHECK(did_invert);

  float scale_from_viewport_to_screen_space =
      active_tree_->device_scale_factor();
  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // Project the scroll start and end points to local layer space to find the
  // scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &start_clipped);
  gfx::PointF local_end_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_end_point, &end_clipped);
  DCHECK(out_local_scroll_delta);
  *out_local_scroll_delta = local_end_point - local_start_point;

  if (out_local_start_point)
    *out_local_start_point = local_start_point;

  if (start_clipped || end_clipped)
    return false;

  return true;
}

gfx::Vector2dF LayerTreeHostImpl::ScrollNodeWithViewportSpaceDelta(
    ScrollNode* scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    ScrollTree* scroll_tree) {
  gfx::PointF local_start_point;
  gfx::Vector2dF local_scroll_delta;
  if (!CalculateLocalScrollDeltaAndStartPoint(
          *scroll_node, viewport_point, viewport_delta, *scroll_tree,
          &local_scroll_delta, &local_start_point)) {
    return gfx::Vector2dF();
  }

  // Apply the scroll delta.
  gfx::ScrollOffset previous_offset =
      scroll_tree->current_scroll_offset(scroll_node->element_id);
  scroll_tree->ScrollBy(scroll_node, local_scroll_delta, active_tree());
  gfx::ScrollOffset scrolled =
      scroll_tree->current_scroll_offset(scroll_node->element_id) -
      previous_offset;

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point =
      local_start_point + gfx::Vector2dF(scrolled.x(), scrolled.y());

  // Calculate the applied scroll delta in viewport space coordinates.
  bool end_clipped;
  const gfx::Transform screen_space_transform =
      scroll_tree->ScreenSpaceTransform(scroll_node->id);
  gfx::PointF actual_screen_space_end_point = MathUtil::MapPoint(
      screen_space_transform, actual_local_end_point, &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();

  float scale_from_viewport_to_screen_space =
      active_tree_->device_scale_factor();
  gfx::PointF actual_viewport_end_point = gfx::ScalePoint(
      actual_screen_space_end_point, 1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

static gfx::Vector2dF ScrollNodeWithLocalDelta(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& local_delta,
    float page_scale_factor,
    LayerTreeImpl* layer_tree_impl) {
  ScrollTree& scroll_tree = layer_tree_impl->property_trees()->scroll_tree;
  gfx::ScrollOffset previous_offset =
      scroll_tree.current_scroll_offset(scroll_node->element_id);
  gfx::Vector2dF delta = local_delta;
  delta.Scale(1.f / page_scale_factor);
  scroll_tree.ScrollBy(scroll_node, delta, layer_tree_impl);
  gfx::ScrollOffset scrolled =
      scroll_tree.current_scroll_offset(scroll_node->element_id) -
      previous_offset;
  gfx::Vector2dF consumed_scroll(scrolled.x(), scrolled.y());
  consumed_scroll.Scale(page_scale_factor);

  return consumed_scroll;
}

// TODO(danakj): Make this into two functions, one with delta, one with
// viewport_point, no bool required.
gfx::Vector2dF LayerTreeHostImpl::ScrollSingleNode(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& delta,
    const gfx::Point& viewport_point,
    bool is_direct_manipulation,
    ScrollTree* scroll_tree) {
  // Events representing direct manipulation of the screen (such as gesture
  // events) need to be transformed from viewport coordinates to local layer
  // coordinates so that the scrolling contents exactly follow the user's
  // finger. In contrast, events not representing direct manipulation of the
  // screen (such as wheel events) represent a fixed amount of scrolling so we
  // can just apply them directly, but the page scale factor is applied to the
  // scroll delta.
  if (is_direct_manipulation) {
    return ScrollNodeWithViewportSpaceDelta(
        scroll_node, gfx::PointF(viewport_point), delta, scroll_tree);
  }
  float scale_factor = active_tree()->current_page_scale_factor();
  return ScrollNodeWithLocalDelta(scroll_node, delta, scale_factor,
                                  active_tree());
}

void LayerTreeHostImpl::ApplyScroll(ScrollNode* scroll_node,
                                    ScrollState* scroll_state) {
  DCHECK(scroll_node && scroll_state);
  gfx::Point viewport_point(scroll_state->position_x(),
                            scroll_state->position_y());
  const gfx::Vector2dF delta(scroll_state->delta_x(), scroll_state->delta_y());
  gfx::Vector2dF applied_delta;
  gfx::Vector2dF delta_applied_to_content;
  // TODO(tdresser): Use a more rational epsilon. See crbug.com/510550 for
  // details.
  const float kEpsilon = 0.1f;

  bool scrolls_main_viewport_scroll_layer =
      viewport()->MainScrollLayer() &&
      viewport()->MainScrollLayer()->scroll_tree_index() == scroll_node->id;

  // This is needed if the scroll chains up to the viewport without going
  // through the outer viewport scroll node. This can happen if we scroll an
  // element that's not a descendant of the document.rootScroller. In that case
  // we want to scroll the inner viewport -- to allow panning while zoomed --
  // but also move browser controls if needed.
  bool scrolls_inner_viewport = scroll_node->scrolls_inner_viewport;

  if (scrolls_main_viewport_scroll_layer || scrolls_inner_viewport) {
    Viewport::ScrollResult result = viewport()->ScrollBy(
        delta, viewport_point, scroll_state->is_direct_manipulation(),
        !wheel_scrolling_, scrolls_main_viewport_scroll_layer);

    applied_delta = result.consumed_delta;
    delta_applied_to_content = result.content_scrolled_delta;
  } else {
    applied_delta = ScrollSingleNode(
        scroll_node, delta, viewport_point,
        scroll_state->is_direct_manipulation(),
        &scroll_state->layer_tree_impl()->property_trees()->scroll_tree);
  }

  // If the layer wasn't able to move, try the next one in the hierarchy.
  bool scrolled = std::abs(applied_delta.x()) > kEpsilon;
  scrolled = scrolled || std::abs(applied_delta.y()) > kEpsilon;
  if (!scrolled) {
    // TODO(bokan): This preserves existing behavior by not allowing tiny
    // scrolls to produce overscroll but is inconsistent in how delta gets
    // chained up. We need to clean this up.
    if (scrolls_main_viewport_scroll_layer)
      scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());
    return;
  }

  if (!scrolls_main_viewport_scroll_layer && !scrolls_inner_viewport) {
    // If the applied delta is within 45 degrees of the input
    // delta, bail out to make it easier to scroll just one layer
    // in one direction without affecting any of its parents.
    float angle_threshold = 45;
    if (MathUtil::SmallestAngleBetweenVectors(applied_delta, delta) <
        angle_threshold) {
      applied_delta = delta;
    } else {
      // Allow further movement only on an axis perpendicular to the direction
      // in which the layer moved.
      applied_delta = MathUtil::ProjectVector(delta, applied_delta);
    }
    delta_applied_to_content = applied_delta;
  }

  scroll_state->set_caused_scroll(
      std::abs(delta_applied_to_content.x()) > kEpsilon,
      std::abs(delta_applied_to_content.y()) > kEpsilon);
  scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());

  scroll_state->set_current_native_scrolling_node(scroll_node);
}

void LayerTreeHostImpl::DistributeScrollDelta(ScrollState* scroll_state) {
  // TODO(majidvp): in Blink we compute scroll chain only at scroll begin which
  // is not the case here. We eventually want to have the same behaviour on both
  // sides but it may become a non issue if we get rid of scroll chaining (see
  // crbug.com/526462)
  std::list<ScrollNode*> current_scroll_chain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  ScrollNode* viewport_scroll_node = ViewportMainScrollNode();
  if (scroll_node) {
    // TODO(bokan): The loop checks for a null parent but don't we still want to
    // distribute to the root scroll node?
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      if (scroll_node == viewport_scroll_node) {
        // Don't chain scrolls past the outer viewport scroll layer. Once we
        // reach that, we should scroll the viewport which is represented by the
        // main viewport scroll layer.
        DCHECK(viewport_scroll_node);
        current_scroll_chain.push_front(viewport_scroll_node);
        break;
      }

      if (!scroll_node->scrollable)
        continue;

      if (CanConsumeDelta(*scroll_node, *scroll_state))
        current_scroll_chain.push_front(scroll_node);

      float delta_x = scroll_state->is_beginning()
                          ? scroll_state->delta_x_hint()
                          : scroll_state->delta_x();
      float delta_y = scroll_state->is_beginning()
                          ? scroll_state->delta_y_hint()
                          : scroll_state->delta_y();

      if (!CanPropagate(scroll_node, delta_x, delta_y)) {
        // We should add the first node with non-auto overscroll-behavior to
        // the scroll chain regardlessly, as it's the only node we can latch to.
        if (current_scroll_chain.empty() ||
            current_scroll_chain.front() != scroll_node) {
          current_scroll_chain.push_front(scroll_node);
        }
        scroll_state->set_is_scroll_chain_cut(true);
        break;
      }
    }
  }

  scroll_node =
      current_scroll_chain.empty() ? nullptr : current_scroll_chain.back();
  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode DistributeScrollDelta",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       scroll_node ? false : true);
  active_tree_->SetCurrentlyScrollingNode(scroll_node);
  scroll_state->set_scroll_chain_and_layer_tree(current_scroll_chain,
                                                active_tree());
  scroll_state->DistributeToScrollChainDescendant();
}

bool LayerTreeHostImpl::CanConsumeDelta(const ScrollNode& scroll_node,
                                        const ScrollState& scroll_state) {
  gfx::Vector2dF delta_to_scroll;
  if (scroll_state.is_beginning()) {
    delta_to_scroll = gfx::Vector2dF(scroll_state.delta_x_hint(),
                                     scroll_state.delta_y_hint());
  } else {
    delta_to_scroll =
        gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y());
  }

  if (delta_to_scroll == gfx::Vector2dF())
    return true;

  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  if (scroll_state.is_direct_manipulation()) {
    gfx::Vector2dF local_scroll_delta;
    if (!CalculateLocalScrollDeltaAndStartPoint(
            scroll_node,
            gfx::PointF(scroll_state.position_x(), scroll_state.position_y()),
            delta_to_scroll, scroll_tree, &local_scroll_delta)) {
      return false;
    }
    delta_to_scroll = local_scroll_delta;
  }

  if (ComputeScrollDelta(scroll_node, delta_to_scroll) != gfx::Vector2dF())
    return true;

  return false;
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
  render_frame_metadata_observer_->BindToCurrentThread();
}

InputHandlerScrollResult LayerTreeHostImpl::ScrollBy(
    ScrollState* scroll_state) {
  DCHECK(scroll_state);

  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBy");
  auto& scroll_tree = active_tree_->property_trees()->scroll_tree;

  ElementId provided_element =
      scroll_state->data()->current_native_scrolling_element();
  const auto* provided_scroll_node =
      scroll_tree.FindNodeFromElementId(provided_element);

  // If the currently scrolling node is not set, set it with
  // |provided_scroll_node|.
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    // If |provided_scroll_node| is not null, make sure it matches
    // |scroll_node|.
    DCHECK(!provided_scroll_node || scroll_node == provided_scroll_node);
  } else {
    TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode ScrollBy",
                         TRACE_EVENT_SCOPE_THREAD, "isNull",
                         provided_scroll_node ? false : true);
    active_tree_->SetCurrentlyScrollingNode(provided_scroll_node);
    scroll_node = scroll_tree.CurrentlyScrollingNode();
  }

  if (!scroll_node)
    return InputHandlerScrollResult();

  // Flash the overlay scrollbar even if the scroll dalta is 0.
  if (settings_.scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(false);
  } else {
    ScrollbarAnimationController* animation_controller =
        ScrollbarAnimationControllerForElementId(scroll_node->element_id);
    if (animation_controller)
      animation_controller->WillUpdateScroll();
  }

  float initial_top_controls_offset =
      browser_controls_offset_manager_->ControlsTopOffset();

  scroll_state->set_delta_consumed_for_scroll_sequence(
      did_lock_scrolling_layer_);
  scroll_state->set_is_direct_manipulation(!wheel_scrolling_);
  scroll_state->set_current_native_scrolling_node(scroll_node);

  DistributeScrollDelta(scroll_state);

  ScrollNode* current_scrolling_node =
      scroll_state->current_native_scrolling_node();
  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode ApplyDelta",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       current_scrolling_node ? false : true);
  active_tree_->SetCurrentlyScrollingNode(current_scrolling_node);
  did_lock_scrolling_layer_ =
      scroll_state->delta_consumed_for_scroll_sequence();

  bool did_scroll_x = scroll_state->caused_scroll_x();
  bool did_scroll_y = scroll_state->caused_scroll_y();
  did_scroll_x_for_scroll_gesture_ |= did_scroll_x;
  did_scroll_y_for_scroll_gesture_ |= did_scroll_y;
  bool did_scroll_content = did_scroll_x || did_scroll_y;
  if (did_scroll_content) {
    ShowScrollbarsForImplScroll(current_scrolling_node->element_id);

    // If we are scrolling with an active scroll handler, forward latency
    // tracking information to the main thread so the delay introduced by the
    // handler is accounted for.
    if (scroll_affects_scroll_handler())
      NotifySwapPromiseMonitorsOfForwardingToMainThread();
    client_->SetNeedsCommitOnImplThread();
    SetNeedsRedraw();
    client_->RenewTreePriority();
  }

  // Scrolling along an axis resets accumulated root overscroll for that axis.
  if (did_scroll_x)
    accumulated_root_overscroll_.set_x(0);
  if (did_scroll_y)
    accumulated_root_overscroll_.set_y(0);

  gfx::Vector2dF unused_root_delta;
  if (current_scrolling_node &&
      current_scrolling_node == ViewportMainScrollNode()) {
    unused_root_delta =
        gfx::Vector2dF(scroll_state->delta_x(), scroll_state->delta_y());
  }

  // When inner viewport is unscrollable, disable overscrolls.
  if (const auto* inner_viewport_scroll_node = InnerViewportScrollNode()) {
    if (!inner_viewport_scroll_node->user_scrollable_horizontal)
      unused_root_delta.set_x(0);
    if (!inner_viewport_scroll_node->user_scrollable_vertical)
      unused_root_delta.set_y(0);
  }

  accumulated_root_overscroll_ += unused_root_delta;

  bool did_scroll_top_controls =
      initial_top_controls_offset !=
      browser_controls_offset_manager_->ControlsTopOffset();

  InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = did_scroll_content || did_scroll_top_controls;
  scroll_result.did_overscroll_root = !unused_root_delta.IsZero();
  scroll_result.accumulated_root_overscroll = accumulated_root_overscroll_;
  scroll_result.unused_scroll_delta = unused_root_delta;
  scroll_result.overscroll_behavior =
      scroll_state->is_scroll_chain_cut()
          ? OverscrollBehavior(OverscrollBehavior::OverscrollBehaviorType::
                                   kOverscrollBehaviorTypeNone)
          : active_tree()->overscroll_behavior();

  if (scroll_result.did_scroll) {
    // Scrolling can change the root scroll offset, so inform the synchronous
    // input handler.
    UpdateRootLayerStateForSynchronousInputHandler();
  }

  scroll_result.current_visual_offset =
      ScrollOffsetToVector2dF(GetVisualScrollOffset(*scroll_node));
  float scale_factor = active_tree()->current_page_scale_factor();
  scroll_result.current_visual_offset.Scale(scale_factor);

  // Run animations which need to respond to updated scroll offset.
  mutator_host_->TickScrollAnimations(
      CurrentBeginFrameArgs().frame_time,
      active_tree_->property_trees()->scroll_tree);

  return scroll_result;
}

void LayerTreeHostImpl::RequestUpdateForSynchronousInputHandler() {
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::SetSynchronousInputHandlerRootScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  TRACE_EVENT2("cc",
               "LayerTreeHostImpl::SetSynchronousInputHandlerRootScrollOffset",
               "offset_x", root_offset.x(), "offset_y", root_offset.y());
  bool changed = active_tree_->DistributeRootScrollOffset(root_offset);
  if (!changed)
    return;

  ShowScrollbarsForImplScroll(OuterViewportScrollLayer()->element_id());
  client_->SetNeedsCommitOnImplThread();
  // After applying the synchronous input handler's scroll offset, tell it what
  // we ended up with.
  UpdateRootLayerStateForSynchronousInputHandler();
  SetFullViewportDamage();
  SetNeedsRedraw();
}

bool LayerTreeHostImpl::SnapAtScrollEnd() {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value())
    return false;

  const SnapContainerData& data = scroll_node->snap_container_data.value();
  gfx::ScrollOffset current_position = GetVisualScrollOffset(*scroll_node);

  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(
          current_position, did_scroll_x_for_scroll_gesture_,
          did_scroll_y_for_scroll_gesture_);
  gfx::ScrollOffset snap_position;
  if (!data.FindSnapPosition(*strategy, &snap_position))
    return false;

  gfx::Vector2dF delta =
      ScrollOffsetToVector2dF(snap_position - current_position);
  bool scrolls_main_viewport_scroll_layer =
      scroll_node == ViewportMainScrollNode();
  if (scrolls_main_viewport_scroll_layer) {
    // Flash the overlay scrollbar even if the scroll dalta is 0.
    if (settings_.scrollbar_flash_after_any_scroll_update) {
      FlashAllScrollbars(false);
    } else {
      ScrollbarAnimationController* animation_controller =
          ScrollbarAnimationControllerForElementId(scroll_node->element_id);
      if (animation_controller)
        animation_controller->WillUpdateScroll();
    }
    gfx::Vector2dF scaled_delta(delta);
    scaled_delta.Scale(active_tree()->current_page_scale_factor());
    viewport()->ScrollAnimated(scaled_delta, base::TimeDelta());
  } else {
    ScrollAnimationCreate(scroll_node, delta, base::TimeDelta());
  }
  DCHECK(!is_animating_for_snap_);
  is_animating_for_snap_ = true;
  return true;
}

gfx::ScrollOffset LayerTreeHostImpl::GetVisualScrollOffset(
    const ScrollNode& scroll_node) const {
  const ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;

  bool scrolls_main_viewport_scroll_layer =
      viewport()->MainScrollLayer() &&
      viewport()->MainScrollLayer()->scroll_tree_index() == scroll_node.id;

  if (scrolls_main_viewport_scroll_layer)
    return viewport()->TotalScrollOffset();
  else
    return scroll_tree.current_scroll_offset(scroll_node.element_id);
}

bool LayerTreeHostImpl::GetSnapFlingInfo(
    const gfx::Vector2dF& natural_displacement_in_viewport,
    gfx::Vector2dF* out_initial_position,
    gfx::Vector2dF* out_target_position) const {
  const ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value())
    return false;

  const SnapContainerData& data = scroll_node->snap_container_data.value();
  float scale_factor = active_tree()->current_page_scale_factor();
  gfx::Vector2dF natural_displacement_in_content =
      gfx::ScaleVector2d(natural_displacement_in_viewport, 1.f / scale_factor);

  gfx::ScrollOffset current_offset = GetVisualScrollOffset(*scroll_node);
  *out_initial_position = ScrollOffsetToVector2dF(current_offset);

  gfx::ScrollOffset snap_offset;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndAndDirection(
          current_offset, gfx::ScrollOffset(natural_displacement_in_content));
  if (!data.FindSnapPosition(*strategy, &snap_offset))
    return false;

  *out_target_position = ScrollOffsetToVector2dF(snap_offset);
  out_target_position->Scale(scale_factor);
  out_initial_position->Scale(scale_factor);
  return true;
}

void LayerTreeHostImpl::ClearCurrentlyScrollingNode() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ClearCurrentlyScrollingNode");
  active_tree_->ClearCurrentlyScrollingNode();
  did_lock_scrolling_layer_ = false;
  scroll_affects_scroll_handler_ = false;
  accumulated_root_overscroll_ = gfx::Vector2dF();
  did_scroll_x_for_scroll_gesture_ = false;
  did_scroll_y_for_scroll_gesture_ = false;
  is_animating_for_snap_ = false;
}

void LayerTreeHostImpl::ScrollEndImpl(ScrollState* scroll_state) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  DistributeScrollDelta(scroll_state);
  browser_controls_offset_manager_->ScrollEnd();
  ClearCurrentlyScrollingNode();
}

void LayerTreeHostImpl::ScrollEnd(ScrollState* scroll_state, bool should_snap) {
  if (should_snap && SnapAtScrollEnd())
    return;

  ScrollEndImpl(scroll_state);
}

void LayerTreeHostImpl::MouseDown() {
  ScrollbarAnimationController* animation_controller =
      ScrollbarAnimationControllerForElementId(
          scroll_element_id_mouse_currently_over_);
  if (animation_controller) {
    animation_controller->DidMouseDown();
    scroll_element_id_mouse_currently_captured_ =
        scroll_element_id_mouse_currently_over_;
  }
}

void LayerTreeHostImpl::MouseUp() {
  if (scroll_element_id_mouse_currently_captured_) {
    ScrollbarAnimationController* animation_controller =
        ScrollbarAnimationControllerForElementId(
            scroll_element_id_mouse_currently_captured_);

    scroll_element_id_mouse_currently_captured_ = ElementId();

    if (animation_controller)
      animation_controller->DidMouseUp();
  }
}

void LayerTreeHostImpl::MouseMoveAt(const gfx::Point& viewport_point) {
  // Early out if there are no animation controllers and avoid the hit test.
  // This happens on platforms without animated scrollbars.
  if (scrollbar_animation_controllers_.empty())
    return;

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  // Check if mouse is over a scrollbar or not.
  // TODO(sahel): get rid of this extera checking when
  // FindScrollNodeForDeviceViewportPoint finds the proper node for scrolling on
  // the main thread when the mouse is over a scrollbar as well.
  ElementId scroll_element_id;
  if (layer_impl && layer_impl->ToScrollbarLayer())
    scroll_element_id = layer_impl->ToScrollbarLayer()->scroll_element_id();
  if (!scroll_element_id) {
    bool scroll_on_main_thread = false;
    uint32_t main_thread_scrolling_reasons;
    auto* scroll_node = FindScrollNodeForDeviceViewportPoint(
        device_viewport_point, InputHandler::TOUCHSCREEN, layer_impl,
        &scroll_on_main_thread, &main_thread_scrolling_reasons);
    if (scroll_node)
      scroll_element_id = scroll_node->element_id;

    // Scrollbars for the viewport are registered with the outer viewport layer.
    if (InnerViewportScrollNode() && OuterViewportScrollLayer() &&
        scroll_element_id == InnerViewportScrollNode()->element_id)
      scroll_element_id = OuterViewportScrollLayer()->element_id();
  }

  ScrollbarAnimationController* new_animation_controller =
      ScrollbarAnimationControllerForElementId(scroll_element_id);
  if (scroll_element_id != scroll_element_id_mouse_currently_over_) {
    ScrollbarAnimationController* old_animation_controller =
        ScrollbarAnimationControllerForElementId(
            scroll_element_id_mouse_currently_over_);
    if (old_animation_controller)
      old_animation_controller->DidMouseLeave();

    scroll_element_id_mouse_currently_over_ = scroll_element_id;

    // Experiment: Enables will flash scorllbar when user move mouse enter a
    // scrollable area.
    if (settings_.scrollbar_flash_when_mouse_enter && new_animation_controller)
      new_animation_controller->DidScrollUpdate();
  }

  if (!new_animation_controller)
    return;

  new_animation_controller->DidMouseMove(device_viewport_point);
}

void LayerTreeHostImpl::MouseLeave() {
  for (auto& pair : scrollbar_animation_controllers_)
    pair.second->DidMouseLeave();
  scroll_element_id_mouse_currently_over_ = ElementId();
}

void LayerTreeHostImpl::PinchGestureBegin() {
  pinch_gesture_active_ = true;
  client_->RenewTreePriority();
  pinch_gesture_end_should_clear_scrolling_node_ = !CurrentlyScrollingNode();

  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode PinchGestureBegin",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       OuterViewportScrollNode() ? false : true);
  active_tree_->SetCurrentlyScrollingNode(OuterViewportScrollNode());
  browser_controls_offset_manager_->PinchBegin();
}

void LayerTreeHostImpl::PinchGestureUpdate(float magnify_delta,
                                           const gfx::Point& anchor) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::PinchGestureUpdate");
  if (!InnerViewportScrollNode())
    return;
  viewport()->PinchUpdate(magnify_delta, anchor);
  client_->SetNeedsCommitOnImplThread();
  SetNeedsRedraw();
  client_->RenewTreePriority();
  // Pinching can change the root scroll offset, so inform the synchronous input
  // handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::PinchGestureEnd(const gfx::Point& anchor,
                                        bool snap_to_min) {
  pinch_gesture_active_ = false;
  if (pinch_gesture_end_should_clear_scrolling_node_) {
    pinch_gesture_end_should_clear_scrolling_node_ = false;
    ClearCurrentlyScrollingNode();
  }
  viewport()->PinchEnd(anchor, snap_to_min);
  browser_controls_offset_manager_->PinchEnd();
  client_->SetNeedsCommitOnImplThread();
  // When a pinch ends, we may be displaying content cached at incorrect scales,
  // so updating draw properties and drawing will ensure we are using the right
  // scales that we want when we're not inside a pinch.
  active_tree_->set_needs_update_draw_properties();
  SetNeedsRedraw();
}

void LayerTreeHostImpl::CollectScrollDeltas(
    ScrollAndScaleSet* scroll_info) const {
  if (active_tree_->LayerListIsEmpty())
    return;

  ElementId inner_viewport_scroll_element_id =
      InnerViewportScrollNode() ? InnerViewportScrollNode()->element_id
                                : ElementId();

  active_tree_->property_trees()->scroll_tree.CollectScrollDeltas(
      scroll_info, inner_viewport_scroll_element_id,
      active_tree_->settings().commit_fractional_scroll_deltas);
}

void LayerTreeHostImpl::CollectScrollbarUpdates(
    ScrollAndScaleSet* scroll_info) const {
  scroll_info->scrollbars.reserve(scrollbar_animation_controllers_.size());
  for (auto& pair : scrollbar_animation_controllers_) {
    scroll_info->scrollbars.push_back(LayerTreeHostCommon::ScrollbarsUpdateInfo(
        pair.first, pair.second->ScrollbarsHidden()));
  }
}

std::unique_ptr<ScrollAndScaleSet> LayerTreeHostImpl::ProcessScrollDeltas() {
  std::unique_ptr<ScrollAndScaleSet> scroll_info(new ScrollAndScaleSet());

  CollectScrollDeltas(scroll_info.get());
  CollectScrollbarUpdates(scroll_info.get());
  scroll_info->page_scale_delta =
      active_tree_->page_scale_factor()->PullDeltaForMainThread();
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this DCHECK as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(!settings().is_layer_tree_for_subframe ||
         scroll_info->page_scale_delta == 1.f);
  scroll_info->top_controls_delta =
      active_tree()->top_controls_shown_ratio()->PullDeltaForMainThread();
  scroll_info->elastic_overscroll_delta =
      active_tree_->elastic_overscroll()->PullDeltaForMainThread();
  scroll_info->swap_promises.swap(swap_promises_for_main_thread_scroll_update_);

  // Record and reset scroll source flags.
  scroll_info->has_scrolled_by_wheel = has_scrolled_by_wheel_;
  scroll_info->has_scrolled_by_touch = has_scrolled_by_touch_;
  has_scrolled_by_wheel_ = has_scrolled_by_touch_ = false;

  if (browser_controls_manager()) {
    scroll_info->browser_controls_constraint =
        browser_controls_manager()->PullConstraintForMainThread(
            &scroll_info->browser_controls_constraint_changed);
  }

  return scroll_info;
}

void LayerTreeHostImpl::SetFullViewportDamage() {
  SetViewportDamage(gfx::Rect(active_tree_->GetDeviceViewport().size()));
}

bool LayerTreeHostImpl::AnimatePageScale(base::TimeTicks monotonic_time) {
  if (!page_scale_animation_)
    return false;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();

  if (!page_scale_animation_->IsAnimationStarted())
    page_scale_animation_->StartAnimation(monotonic_time);

  active_tree_->SetPageScaleOnActiveTree(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time));
  gfx::ScrollOffset next_scroll = gfx::ScrollOffset(
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time));

  DCHECK(viewport());
  viewport()->ScrollByInnerFirst(next_scroll.DeltaFrom(scroll_total));

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
  if (!browser_controls_offset_manager_->has_animation())
    return false;

  gfx::Vector2dF scroll = browser_controls_offset_manager_->Animate(time);

  if (browser_controls_offset_manager_->has_animation())
    SetNeedsOneBeginImplFrame();

  if (active_tree_->TotalScrollOffset().y() == 0.f)
    return false;

  if (scroll.IsZero())
    return false;

  DCHECK(viewport());
  viewport()->ScrollBy(scroll, gfx::Point(), false, false, true);
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
  return true;
}

bool LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks monotonic_time) {
  bool animated = false;
  for (auto& pair : scrollbar_animation_controllers_)
    animated |= pair.second->Animate(monotonic_time);
  return animated;
}

bool LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time,
                                      bool is_active_tree) {
  const ScrollTree& scroll_tree =
      is_active_tree ? active_tree_->property_trees()->scroll_tree
                     : pending_tree_->property_trees()->scroll_tree;
  const bool animated = mutator_host_->TickAnimations(
      monotonic_time, scroll_tree, is_active_tree);

  // TODO(crbug.com/551134): Only do this if the animations are on the active
  // tree, or if they are on the pending tree waiting for some future time to
  // start.
  // TODO(crbug.com/551138): We currently have a single signal from the
  // animation_host, so on the last frame of an animation we will
  // still request an extra SetNeedsAnimate here.
  if (animated)
    SetNeedsOneBeginImplFrame();
  // TODO(crbug.com/551138): We could return true only if the animaitons are on
  // the active tree. There's no need to cause a draw to take place from
  // animations starting/ticking on the pending tree.
  return animated;
}

void LayerTreeHostImpl::UpdateAnimationState(bool start_ready_animations) {
  std::unique_ptr<MutatorEvents> events = mutator_host_->CreateEvents();

  const bool has_active_animations =
      mutator_host_->UpdateAnimationState(start_ready_animations, events.get());

  if (!events->IsEmpty())
    client_->PostAnimationEventsToMainThreadOnImplThread(std::move(events));

  if (has_active_animations)
    SetNeedsOneBeginImplFrame();
}

void LayerTreeHostImpl::ActivateAnimations() {
  const bool activated = mutator_host_->ActivateAnimations();
  if (activated) {
    // Activating an animation changes layer draw properties, such as
    // screen_space_transform_is_animating. So when we see a new animation get
    // activated, we need to update the draw properties on the active tree.
    active_tree()->set_needs_update_draw_properties();
    // Request another frame to run the next tick of the animation.
    SetNeedsOneBeginImplFrame();
  }
}

std::string LayerTreeHostImpl::LayerListAsJson() const {
  auto list = std::make_unique<base::ListValue>();
  for (auto* layer : *active_tree_) {
    list->Append(layer->LayerAsJson());
  }
  std::string str;
  base::JSONWriter::WriteWithOptions(
      *list, base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
  return str;
}

std::string LayerTreeHostImpl::LayerTreeAsJson() const {
  std::string str;
  if (active_tree_->root_layer_for_testing()) {
    std::unique_ptr<base::Value> json(
        active_tree_->root_layer_for_testing()->LayerTreeAsJson());
    base::JSONWriter::WriteWithOptions(
        *json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
  }
  return str;
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

void LayerTreeHostImpl::UnregisterScrollbarAnimationController(
    ElementId scroll_element_id) {
  scrollbar_animation_controllers_.erase(scroll_element_id);
}

ScrollbarAnimationController*
LayerTreeHostImpl::ScrollbarAnimationControllerForElementId(
    ElementId scroll_element_id) const {
  // The viewport layers have only one set of scrollbars. On Android, these are
  // registered with the inner viewport, otherwise they're registered with the
  // outer viewport. If a controller for one exists, the other shouldn't.
  if (InnerViewportScrollNode() && OuterViewportScrollLayer()) {
    if (scroll_element_id == InnerViewportScrollNode()->element_id ||
        scroll_element_id == OuterViewportScrollLayer()->element_id()) {
      auto itr = scrollbar_animation_controllers_.find(
          InnerViewportScrollNode()->element_id);
      if (itr != scrollbar_animation_controllers_.end())
        return itr->second.get();

      itr = scrollbar_animation_controllers_.find(
          OuterViewportScrollLayer()->element_id());
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
    const base::Closure& task,
    base::TimeDelta delay) {
  client_->PostDelayedAnimationTaskOnImplThread(task, delay);
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
  DidModifyTilePriorities();
}

TreePriority LayerTreeHostImpl::GetTreePriority() const {
  return global_tile_state_.tree_priority;
}

viz::BeginFrameArgs LayerTreeHostImpl::CurrentBeginFrameArgs() const {
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
  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  SetFullViewportDamage();
}

void LayerTreeHostImpl::CreateUIResource(UIResourceId uid,
                                         const UIResourceBitmap& bitmap) {
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

  viz::ResourceFormat format;
  switch (bitmap.GetFormat()) {
    case UIResourceBitmap::RGBA8:
      if (layer_tree_frame_sink_->context_provider()) {
        const gpu::Capabilities& caps =
            layer_tree_frame_sink_->context_provider()->ContextCapabilities();
        format = viz::PlatformColor::BestSupportedTextureFormat(caps);
      } else {
        format = viz::RGBA_8888;
      }
      break;
    case UIResourceBitmap::ALPHA_8:
      format = viz::ALPHA_8;
      break;
    case UIResourceBitmap::ETC1:
      format = viz::ETC1;
      break;
  }

  const gfx::Size source_size = bitmap.GetSize();
  gfx::Size upload_size = bitmap.GetSize();
  bool scaled = false;
  // UIResources are assumed to be rastered in SRGB.
  const gfx::ColorSpace& color_space = gfx::ColorSpace::CreateSRGB();

  if (source_size.width() > max_texture_size_ ||
      source_size.height() > max_texture_size_) {
    // Must resize the bitmap to fit within the max texture size.
    scaled = true;
    int edge = std::max(source_size.width(), source_size.height());
    float scale = static_cast<float>(max_texture_size_ - 1) / edge;
    DCHECK_LT(scale, 1.f);
    upload_size = gfx::ScaleToCeiledSize(source_size, scale, scale);
  }

  // For gpu compositing, a texture will be allocated and the UIResource
  // will be uploaded into it.
  viz::TextureAllocation texture_alloc;
  // For software compositing, shared memory will be allocated and the
  // UIResource will be copied into it.
  std::unique_ptr<base::SharedMemory> shared_memory;
  viz::SharedBitmapId shared_bitmap_id;

  if (layer_tree_frame_sink_->context_provider()) {
    viz::ContextProvider* context_provider =
        layer_tree_frame_sink_->context_provider();
    texture_alloc = viz::TextureAllocation::MakeTextureId(
        context_provider->ContextGL(), context_provider->ContextCapabilities(),
        format, settings_.resource_settings.use_gpu_memory_buffer_resources,
        /*for_framebuffer_attachment=*/false);
  } else {
    shared_memory =
        viz::bitmap_allocation::AllocateMappedBitmap(upload_size, format);
    shared_bitmap_id = viz::SharedBitmap::GenerateId();
  }

  if (!scaled) {
    // If not scaled, we can copy the pixels 1:1 from the source bitmap to our
    // destination backing of a texture or shared bitmap.
    if (layer_tree_frame_sink_->context_provider()) {
      viz::TextureAllocation::UploadStorage(
          layer_tree_frame_sink_->context_provider()->ContextGL(),
          layer_tree_frame_sink_->context_provider()->ContextCapabilities(),
          format, upload_size, texture_alloc, color_space, bitmap.GetPixels());
    } else {
      DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);
      SkImageInfo src_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(source_size));
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));

      sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(
          dst_info, shared_memory->memory(), dst_info.minRowBytes());
      surface->getCanvas()->writePixels(
          src_info, const_cast<uint8_t*>(bitmap.GetPixels()),
          src_info.minRowBytes(), 0, 0);
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
    source_bitmap.setInfo(info);
    source_bitmap.setPixels(const_cast<uint8_t*>(bitmap.GetPixels()));

    // This applies the scale to draw the |bitmap| into |scaled_surface|. For
    // gpu compositing, we scale into a software bitmap-backed SkSurface here,
    // then upload from there into a texture. For software compositing, we scale
    // directly into the shared memory backing.
    sk_sp<SkSurface> scaled_surface;
    if (layer_tree_frame_sink_->context_provider()) {
      scaled_surface = SkSurface::MakeRasterN32Premul(upload_size.width(),
                                                      upload_size.height());
    } else {
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));
      scaled_surface = SkSurface::MakeRasterDirect(
          dst_info, shared_memory->memory(), dst_info.minRowBytes());
    }
    SkCanvas* scaled_canvas = scaled_surface->getCanvas();
    scaled_canvas->scale(canvas_scale_x, canvas_scale_y);
    // The |canvas_scale_x| and |canvas_scale_y| may have some floating point
    // error for large enough values, causing pixels on the edge to be not
    // fully filled by drawBitmap(), so we ensure they start empty. (See
    // crbug.com/642011 for an example.)
    scaled_canvas->clear(SK_ColorTRANSPARENT);
    scaled_canvas->drawBitmap(source_bitmap, 0, 0);

    if (layer_tree_frame_sink_->context_provider()) {
      SkPixmap pixmap;
      scaled_surface->peekPixels(&pixmap);
      viz::TextureAllocation::UploadStorage(
          layer_tree_frame_sink_->context_provider()->ContextGL(),
          layer_tree_frame_sink_->context_provider()->ContextCapabilities(),
          format, upload_size, texture_alloc, color_space, pixmap.addr());
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
    gpu::gles2::GLES2Interface* gl =
        layer_tree_frame_sink_->context_provider()->ContextGL();
    gpu::Mailbox mailbox;
    gl->ProduceTextureDirectCHROMIUM(texture_alloc.texture_id, mailbox.name);
    gpu::SyncToken sync_token =
        viz::ClientResourceProvider::GenerateSyncTokenHelper(gl);

    transferable = viz::TransferableResource::MakeGLOverlay(
        mailbox, GL_LINEAR, texture_alloc.texture_target, sync_token,
        upload_size, texture_alloc.overlay_candidate);
    transferable.format = format;
  } else {
    mojo::ScopedSharedBufferHandle memory_handle =
        viz::bitmap_allocation::DuplicateAndCloseMappedBitmap(
            shared_memory.get(), upload_size, format);
    layer_tree_frame_sink_->DidAllocateSharedBitmap(std::move(memory_handle),
                                                    shared_bitmap_id);
    transferable = viz::TransferableResource::MakeSoftware(shared_bitmap_id,
                                                           upload_size, format);
  }
  transferable.color_space = color_space;
  id = resource_provider_.ImportResource(
      transferable,
      // The OnUIResourceReleased method is bound with a WeakPtr, but the
      // resource backing will be deleted when the LayerTreeFrameSink is
      // removed before shutdown, so nothing leaks if the WeakPtr is
      // invalidated.
      viz::SingleReleaseCallback::Create(base::BindOnce(
          &LayerTreeHostImpl::OnUIResourceReleased, AsWeakPtr(), uid)));

  UIResourceData data;
  data.opaque = bitmap.GetOpaque();
  data.format = format;
  data.shared_bitmap_id = shared_bitmap_id;
  data.shared_memory = std::move(shared_memory);
  data.texture_id = texture_alloc.texture_id;
  data.resource_id_for_export = id;
  ui_resource_map_[uid] = std::move(data);

  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  auto it = ui_resource_map_.find(uid);
  if (it != ui_resource_map_.end()) {
    UIResourceData& data = it->second;
    viz::ResourceId id = data.resource_id_for_export;
    // Move the |data| to |deleted_ui_resources_| before removing it from the
    // viz::ClientResourceProvider, so that the ReleaseCallback can see it
    // there.
    deleted_ui_resources_[uid] = std::move(data);
    ui_resource_map_.erase(it);

    resource_provider_.RemoveImportedResource(id);
  }
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResourceBacking(
    UIResourceData data,
    const gpu::SyncToken& sync_token) {
  // Resources are either software or gpu backed, not both.
  DCHECK(!(data.shared_memory && data.texture_id));
  if (data.shared_memory)
    layer_tree_frame_sink_->DidDeleteSharedBitmap(data.shared_bitmap_id);
  if (data.texture_id) {
    gpu::gles2::GLES2Interface* gl =
        layer_tree_frame_sink_->context_provider()->ContextGL();
    if (sync_token.HasData())
      gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    gl->DeleteTextures(1, &data.texture_id);
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
    resource_provider_.RemoveImportedResource(data.resource_id_for_export);
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
  DCHECK(iter != ui_resource_map_.end());
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

void LayerTreeHostImpl::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHostImpl::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfSetNeedsRedraw() {
  auto it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsRedrawOnImpl();
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfForwardingToMainThread() {
  auto it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnForwardScrollUpdateToMainThreadOnImpl();
}

void LayerTreeHostImpl::UpdateRootLayerStateForSynchronousInputHandler() {
  if (!input_handler_client_)
    return;
  input_handler_client_->UpdateRootLayerStateForSynchronousInputHandler(
      active_tree_->TotalScrollOffset(), active_tree_->TotalMaxScrollOffset(),
      active_tree_->ScrollableSize(), active_tree_->current_page_scale_factor(),
      active_tree_->min_page_scale_factor(),
      active_tree_->max_page_scale_factor());
}

bool LayerTreeHostImpl::ScrollAnimationUpdateTarget(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& scroll_delta,
    base::TimeDelta delayed_by) {
  float scale_factor = active_tree()->current_page_scale_factor();
  gfx::Vector2dF scaled_delta =
      gfx::ScaleVector2d(scroll_delta, 1.f / scale_factor);
  return mutator_host_->ImplOnlyScrollAnimationUpdateTarget(
      scroll_node->element_id, scaled_delta,
      active_tree_->property_trees()->scroll_tree.MaxScrollOffset(
          scroll_node->id),
      CurrentBeginFrameArgs().frame_time, delayed_by);
}

bool LayerTreeHostImpl::IsElementInList(ElementId element_id,
                                        ElementListType list_type) const {
  if (list_type == ElementListType::ACTIVE)
    return active_tree() && active_tree()->IsElementInLayerList(element_id);

  return (pending_tree() && pending_tree()->IsElementInLayerList(element_id)) ||
         (recycle_tree() && recycle_tree()->IsElementInLayerList(element_id));
}

void LayerTreeHostImpl::SetMutatorsNeedCommit() {}

void LayerTreeHostImpl::SetMutatorsNeedRebuildPropertyTrees() {}

void LayerTreeHostImpl::SetTreeLayerScrollOffsetMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const gfx::ScrollOffset& scroll_offset) {
  if (!tree)
    return;

  PropertyTrees* property_trees = tree->property_trees();
  DCHECK_EQ(1u,
            property_trees->element_id_to_scroll_node_index.count(element_id));
  const int scroll_node_index =
      property_trees->element_id_to_scroll_node_index[element_id];
  property_trees->scroll_tree.OnScrollOffsetAnimated(
      element_id, scroll_node_index, scroll_offset, tree);
}

void LayerTreeHostImpl::SetNeedUpdateGpuRasterizationStatus() {
  need_update_gpu_rasterization_status_ = true;
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
    const gfx::ScrollOffset& scroll_offset) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerScrollOffsetMutated(element_id, active_tree(), scroll_offset);
    ShowScrollbarsForImplScroll(element_id);
  } else {
    SetTreeLayerScrollOffsetMutated(element_id, pending_tree(), scroll_offset);
    SetTreeLayerScrollOffsetMutated(element_id, recycle_tree(), scroll_offset);
  }
}

void LayerTreeHostImpl::ElementIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  LayerTreeImpl* tree =
      list_type == ElementListType::ACTIVE ? active_tree() : pending_tree();
  // TODO(wkorman): Explore enabling DCHECK in ElementIsAnimatingChanged()
  // below. Currently enabling causes batch of unit test failures.
  if (tree && tree->property_trees()->ElementIsAnimatingChanged(
                  mutator_host(), element_id, list_type, mask, state, false))
    tree->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::ScrollOffsetAnimationFinished() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollOffsetAnimationFinished");
  // TODO(majidvp): We should pass in the original starting scroll position here
  ScrollStateData scroll_state_data;
  ScrollState scroll_state(scroll_state_data);
  ScrollEnd(&scroll_state, !is_animating_for_snap_);
}

gfx::ScrollOffset LayerTreeHostImpl::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  if (active_tree()) {
    return active_tree()->property_trees()->scroll_tree.current_scroll_offset(
        element_id);
  }

  return gfx::ScrollOffset();
}

bool LayerTreeHostImpl::SupportsImplScrolling() const {
  // Supported in threaded mode.
  return task_runner_provider_->HasImplThread();
}

bool LayerTreeHostImpl::CommitToActiveTree() const {
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

void LayerTreeHostImpl::UpdateScrollSourceInfo(bool is_wheel_scroll) {
  if (is_wheel_scroll)
    has_scrolled_by_wheel_ = true;
  else
    has_scrolled_by_touch_ = true;
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

void LayerTreeHostImpl::RequestInvalidationForAnimatedImages() {
  // If we are animating an image, we want at least one draw of the active tree
  // before a new tree is activated.
  bool needs_first_draw_on_activation = true;
  client_->NeedsImplSideInvalidation(needs_first_draw_on_activation);
}

void LayerTreeHostImpl::InitializeUkm(
    std::unique_ptr<ukm::UkmRecorder> recorder) {
  DCHECK(!ukm_manager_);
  ukm_manager_ = std::make_unique<UkmManager>(std::move(recorder));
}

void LayerTreeHostImpl::SetActiveURL(const GURL& url) {
  tile_manager_.set_active_url(url);

  // The active tree might still be from content for the previous page when the
  // recorder is updated here, since new content will be pushed with the next
  // main frame. But we should only get a few impl frames wrong here in that
  // case. Also, since checkerboard stats are only recorded with user
  // interaction, it must be in progress when the navigation commits for this
  // case to occur.
  if (ukm_manager_)
    ukm_manager_->SetSourceURL(url);
}

}  // namespace cc
