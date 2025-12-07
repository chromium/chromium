// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/benchmarks/micro_benchmark_controller_impl.h"
#include "cc/cc_export.h"
#include "cc/input/actively_scrolling_type.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/input_handler.h"
#include "cc/input/progress_bar_offset_manager.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/layer_collections.h"
#include "cc/metrics/average_lag_tracking_manager.h"
#include "cc/metrics/event_latency_tracker.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/metrics/submit_info.h"
#include "cc/paint/paint_worklet_job.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/video_frame_controller.h"
#include "cc/tiles/tile_manager.h"
#include "cc/tiles/tile_manager_client.h"
#include "cc/trees/animated_paint_worklet_tracker.h"
#include "cc/trees/frame_data.h"
#include "cc/trees/image_animation_controller.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/raster_capabilities.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/task_runner_provider.h"
#include "cc/trees/throttle_decider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/trees_in_viz_timing.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class PointF;
}

namespace ukm {
class UkmRecorder;
}

namespace viz {
class ClientResourceProvider;
}

namespace cc {

class BrowserControlsOffsetManager;
class CompositorFrameReportingController;
class RasterDarkModeFilter;
class DebugRectHistory;
class EvictionTilePriorityQueue;
class ImageAnimationController;
class ImageDecodeCache;
class LCDTextMetricsReporter;
class LatencyInfoSwapPromiseMonitor;
class LayerContext;
class LayerImpl;
class LayerTreeFrameSink;
class LayerTreeHostImplClient;
class LayerTreeImpl;
class PaintWorkletLayerPainter;
class MemoryHistory;
class MutatorEvents;
class MutatorHost;
class PageScaleAnimation;
class RasterTilePriorityQueue;
class RasterBufferProvider;
class RasterQueryQueue;
class RenderFrameMetadataObserver;
class RenderingStatsInstrumentation;
class ResourcePool;
class SwapPromise;
class SynchronousTaskGraphRunner;
class TaskGraphRunner;
class UIResourceBitmap;
class Viewport;

struct UIResourceChange {
  bool resource_created : 1 = false;
  bool resource_deleted : 1 = false;
};

using UIResourceChangeMap = std::unordered_map<UIResourceId, UIResourceChange>;

// LayerTreeHostImpl owns the LayerImpl trees as well as associated rendering
// state.
class CC_EXPORT LayerTreeHostImpl : public TileManagerClient,
                                    public LayerTreeFrameSinkClient,
                                    public BrowserControlsOffsetManagerClient,
                                    public ScrollbarAnimationControllerClient,
                                    public VideoFrameControllerClient,
                                    public MutatorHostClient,
                                    public ImageAnimationController::Client,
                                    public CompositorDelegateForInput,
                                    public EventLatencyTracker,
                                    public base::MemoryPressureListener {
 public:
  // A struct of data for a single UIResource, including the backing
  // pixels, and metadata about it.
  struct CC_EXPORT UIResourceData {
    UIResourceData();
    UIResourceData(const UIResourceData&) = delete;
    UIResourceData(UIResourceData&&) noexcept;
    ~UIResourceData();

    UIResourceData& operator=(const UIResourceData&) = delete;
    UIResourceData& operator=(UIResourceData&&);

    bool opaque;

    base::WritableSharedMemoryMapping shared_mapping;
    // Backing for gpu compositing.
    scoped_refptr<gpu::ClientSharedImage> shared_image;

    // The name with which to refer to the resource in frames submitted to the
    // display compositor.
    viz::ResourceId resource_id_for_export;
  };

  static std::unique_ptr<LayerTreeHostImpl> Create(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      RasterDarkModeFilter* dark_mode_filter,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client);
  LayerTreeHostImpl(const LayerTreeHostImpl&) = delete;
  ~LayerTreeHostImpl() override;

  LayerTreeHostImpl& operator=(const LayerTreeHostImpl&) = delete;

  // TODO(crbug.com/404586886): This getter is an escape-hatch for code that
  // hasn't yet been cleaned up to decouple input from graphics. Callers should
  // be cleaned up to avoid calling it and it should be removed.
  InputHandler& GetInputHandler();
  const InputHandler& GetInputHandler() const;

  void StartPageScaleAnimation(const gfx::Point& target_offset,
                               bool anchor_point,
                               float page_scale,
                               base::TimeDelta duration);

  // BrowserControlsOffsetManagerClient implementation.
  float TopControlsHeight() const override;
  float TopControlsMinHeight() const override;
  float BottomControlsHeight() const override;
  float BottomControlsMinHeight() const override;
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override;
  float CurrentTopControlsShownRatio() const override;
  float CurrentBottomControlsShownRatio() const override;
  gfx::PointF ViewportScrollOffset() const override;
  void DidChangeBrowserControlsPosition() override;
  void DidObserveScrollDelay(int source_frame_number,
                             base::TimeDelta scroll_delay,
                             base::TimeTicks scroll_timestamp);
  bool OnlyExpandTopControlsAtPageTop() const override;
  bool HaveRootScrollNode() const override;
  void SetNeedsCommit() override;

  // ImageAnimationController::Client implementation.
  void RequestBeginFrameForAnimatedImages() override;
  void RequestInvalidationForAnimatedImages() override;

  base::WeakPtr<LayerTreeHostImpl> AsWeakPtr();

  void set_resourceless_software_draw_for_testing() {
    resourceless_software_draw_ = true;
  }

  const gfx::Rect& viewport_damage_rect_for_testing() const {
    return viewport_damage_rect_;
  }

  void ResetViewportDamageRectForTesting() {
    viewport_damage_rect_ = gfx::Rect();
  }

  bool HasPendingRasterInvalidationScrollForTesting(ElementId id) const {
    return pending_invalidation_raster_inducing_scrolls_.contains(id);
  }

  virtual void WillSendBeginMainFrame() {}
  virtual void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      const viz::BeginFrameArgs& args,
      bool next_bmf,
      bool scroll_and_viewport_changes_synced);
  virtual void ReadyToCommit(
      bool scroll_and_viewport_changes_synced,
      const BeginMainFrameMetrics* begin_main_frame_metrics,
      bool commit_timeout);
  virtual void BeginCommit(int source_frame_number,
                           BeginMainFrameTraceId trace_id);
  virtual void FinishCommit(CommitState& commit_state,
                            const ThreadUnsafeCommitState& unsafe_state);
  virtual void CommitComplete();
  virtual void UpdateAnimationState(bool start_ready_animations);
  void PullLayerTreeHostPropertiesFrom(const CommitState&);
  void RecordGpuRasterizationHistogram();
  bool Mutate(base::TimeTicks monotonic_time);
  void ActivateAnimations();
  void Animate();
  void AnimatePendingTreeAfterCommit();
  void DidAnimateScrollOffset();
  void SetFullViewportDamage();
  void SetViewportDamage(const gfx::Rect& damage_rect);

  // Interface for InputHandler
  void BindToInputHandler(
      std::unique_ptr<InputDelegateForCompositor> delegate) override;
  ScrollTree& GetScrollTree() const override;
  void ScrollAnimationAbort(ElementId element_id) const override;
  float GetBrowserControlsTopOffset() const override;
  void ScrollBegin() const override;
  void ScrollEnd() const override;
  void StartScrollSequence(
      FrameSequenceTrackerType type,
      FrameInfo::SmoothEffectDrivingThread scrolling_thread) override;
  void StopSequence(FrameSequenceTrackerType type) override;
  void PinchBegin() const override;
  void PinchEnd() const override;
  void TickScrollAnimations() const override;
  void ScrollbarAnimationMouseLeave(ElementId element_id) const override;
  void ScrollbarAnimationMouseMove(
      ElementId element_id,
      gfx::PointF device_viewport_point) const override;
  bool ScrollbarAnimationMouseDown(ElementId element_id) const override;
  bool ScrollbarAnimationMouseUp(ElementId element_id) const override;
  double PredictViewportBoundsDelta(
      double current_bounds_delta,
      gfx::Vector2dF scroll_distance) const override;
  bool ElementHasImplOnlyScrollAnimation(ElementId) const override;
  std::optional<gfx::PointF> UpdateImplAnimationScrollTargetWithDelta(
      gfx::Vector2dF adjusted_delta,
      int scroll_node_id,
      base::TimeDelta delayed_by,
      ElementId element_id) const override;
  void SetNeedsAnimateInput() override;
  std::unique_ptr<LatencyInfoSwapPromiseMonitor>
  CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) override;
  std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) override;
  void DidScrollForMetrics() override;
  void NotifyInputEvent(bool is_fling) override;
  bool HasAnimatedScrollbars() const override;
  // Already overridden for BrowserControlsOffsetManagerClient which declares a
  // method of the same name.
  // void SetNeedsCommit();
  void SetNeedsFullViewportRedraw() override;
  void DidUpdateScrollAnimationCurve() override;
  void DidStartPinchZoom() override;
  void DidUpdatePinchZoom() override;
  void DidEndPinchZoom() override;
  void DidStartScroll() override;
  void DidEndScroll() override;
  void DidMouseEnterNonViewportScroller(ElementId element_id) override;
  void DidMouseLeave() override;
  bool IsInHighLatencyMode() const override;
  void WillScrollContent(ElementId element_id) override;
  void DidScrollContent(ElementId element_id,
                        bool animated,
                        const gfx::Vector2dF& scroll_delta) override;
  float DeviceScaleFactor() const override;
  float PageScaleFactor() const override;
  gfx::Size VisualDeviceViewportSize() const override;
  const LayerTreeSettings& GetSettings() const override;
  LayerTreeHostImpl& GetImplDeprecated() override;
  const LayerTreeHostImpl& GetImplDeprecated() const override;
  void SetDeferBeginMainFrame(bool defer_begin_main_frame) const override;
  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagModifications>
          offset_tag_modifications) override;
  bool HasScrollLinkedAnimation(ElementId for_scroller) const override;

  void DetachInputDelegateAndRenderFrameObserver();

  FrameSequenceTrackerCollection& frame_trackers() { return frame_trackers_; }

  // VisualDeviceViewportSize is the size of the global viewport across all
  // compositors that are part of the scene that this compositor contributes to
  // (i.e. the visual viewport), allowing for that scene to be broken up into
  // multiple compositors that each contribute to the whole (e.g. cross-origin
  // iframes are isolated from each other). This is a size instead of a rect
  // because each compositor doesn't know its position relative to other
  // compositors. This is specified in device viewport coordinate space.
  void SetVisualDeviceViewportSize(const gfx::Size&);

  void set_viewport_mobile_optimized(bool viewport_mobile_optimized) {
    is_viewport_mobile_optimized_ = viewport_mobile_optimized;
  }

  bool viewport_mobile_optimized() const {
    return is_viewport_mobile_optimized_;
  }

  void SetPrefersReducedMotion(bool prefers_reduced_motion);

  void SetMayThrottleIfUndrawnFrames(bool may_throttle_if_undrawn_frames);

  // Analogous to a commit, this function is used to create a sync tree and
  // add impl-side invalidations to it.
  // virtual for testing.
  virtual void InvalidateContentOnImplSide();
  virtual void InvalidateLayerTreeFrameSink(bool needs_redraw);

  void SetTreeLayerScrollOffsetMutated(ElementId element_id,
                                       LayerTreeImpl* tree,
                                       const gfx::PointF& scroll_offset);

  // ProtectedSequenceSynchronizer implementation.
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

  // MutatorHostClient implementation.
  bool IsElementInPropertyTrees(ElementId element_id,
                                ElementListType list_type) const override;
  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;
  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;
  void SetElementBackdropFilterMutated(
      ElementId element_id,
      ElementListType list_type,
      const FilterOperations& backdrop_filters) override;
  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;
  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;
  void SetElementScrollOffsetMutated(ElementId element_id,
                                     ElementListType list_type,
                                     const gfx::PointF& scroll_offset) override;
  void ElementIsAnimatingChanged(const PropertyToElementIdMap& element_id_map,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;
  void MaximumScaleChanged(ElementId element_id,
                           ElementListType list_type,
                           float maximum_scale) override;
  void OnCustomPropertyMutated(
      PaintWorkletInput::PropertyKey property_key,
      PaintWorkletInput::PropertyValue property_value) override;

  bool RunsOnCurrentThread() const override;

  void ScrollOffsetAnimationFinished(ElementId element_id) override;

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override;

  virtual bool PrepareTiles();

  // Returns `DrawResult::kSuccess` unless problems occurred preparing the
  // frame, and we should try to avoid displaying the frame. If
  // `PrepareToDraw()` is called, `DidDrawAllLayers()` must also be called,
  // regardless of whether `DrawLayers()` is called between the two.
  //
  // |expects_to_draw| will force DrawResult::kSuccess state, and damage to be
  // set for this frame. This is only used in the trees_in_viz_in_viz_process
  // mode, internally, CalculateRenderPasses will DCHECK if |expects_to_draw|
  // does not match the actual behavior.
  virtual DrawResult PrepareToDraw(FrameData* frame,
                                   bool expects_to_draw = false);

  // If there is no damage, returns `std::nullopt`; otherwise, returns
  // information about the submitted frame including submit time and a set of
  // `EventMetrics` for the frame.
  virtual std::optional<SubmitInfo> DrawLayers(FrameData* frame);

  // Must be called if and only if PrepareToDraw was called.
  void DidDrawAllLayers(const FrameData& frame);

  // Pushes differential updates to the display tree via a LayerContext.
  base::TimeTicks UpdateDisplayTree(FrameData& frame);

  const LayerTreeSettings& settings() const { return settings_; }

  // Evict all textures by enforcing a memory policy with an allocation of 0.
  void EvictTexturesForTesting();

  // When blocking, this prevents client_->NotifyReadyToActivate() from being
  // called. When disabled, it calls client_->NotifyReadyToActivate()
  // immediately if any notifications had been blocked while blocking and
  // notify_if_blocked is true.
  virtual void BlockNotifyReadyToActivateForTesting(
      bool block,
      bool notify_if_blocked = true);

  // Prevents notifying the |client_| when an impl side invalidation request is
  // made. When unblocked, the disabled request will immediately be called.
  virtual void BlockImplSideInvalidationRequestsForTesting(bool block);

  // Resets all of the trees to an empty state.
  void ResetTreesForTesting();

  size_t SourceAnimationFrameNumberForTesting() const;

  void RegisterScrollbarAnimationController(ElementId scroll_element_id,
                                            float initial_opacity);
  void DidRegisterScrollbarLayer(ElementId scroll_element_id,
                                 ScrollbarOrientation orientation);
  void DidUnregisterScrollbarLayer(ElementId scroll_element_id,
                                   ScrollbarOrientation orientation);
  ScrollbarAnimationController* ScrollbarAnimationControllerForElementId(
      ElementId scroll_element_id) const;

  // Flashes scrollbars when the page scale is updated.
  void OnPageScaleUpdated();

  DrawMode GetDrawMode() const;

  void DidNotNeedBeginFrame();

  bool PrioritizeNewContentDueToCheckerboarding() const {
    return prioritize_new_content_due_to_checkerboarding_;
  }

  // TileManagerClient implementation.
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void NotifyAllTileTasksCompleted() override;
  void NotifyTileStateChanged(const Tile* tile,
                              bool update_damage,
                              bool set_needs_redraw) override;
  std::unique_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) override;
  std::unique_ptr<EvictionTilePriorityQueue> BuildEvictionQueue() override;
  void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) override;
  std::unique_ptr<TilesWithResourceIterator> CreateTilesWithResourceIterator()
      override;
  viz::SharedImageFormat GetTileFormat() const override;
  TargetColorParams GetTargetColorParams(
      gfx::ContentColorUsage content_color_usage) const override;
  void RequestImplSideInvalidationForCheckerImagedTiles() override;
  size_t GetFrameIndexForImage(const PaintImage& paint_image,
                               WhichTree tree) const override;
  int GetMSAASampleCountForRaster(
      const DisplayItemList& display_list) const override;

  bool HasPendingTree() override;

  // ScrollbarAnimationControllerClient implementation.
  void PostDelayedScrollbarAnimationTask(base::OnceClosure task,
                                         base::TimeDelta delay) override;
  void SetNeedsAnimateForScrollbarAnimation() override;
  void SetNeedsRedrawForScrollbarAnimation() override;
  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override;
  void DidChangeScrollbarVisibility() override;
  bool IsFluentOverlayScrollbar() const override;

  // VideoBeginFrameSource implementation.
  void AddVideoFrameController(VideoFrameController* controller) override;
  void RemoveVideoFrameController(VideoFrameController* controller) override;

  // LayerTreeFrameSinkClient implementation.
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  std::optional<viz::HitTestRegionList> BuildHitTestData() override;
  void DidLoseLayerTreeFrameSink() override;
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override;
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override;

  // EventLatencyTracker implementation.
  void ReportEventLatency(
      const viz::BeginFrameArgs& args,
      std::vector<EventLatencyTracker::LatencyData> latencies) override;

  // Called from LayerTreeImpl.
  void OnCanDrawStateChangedForTree();

  // Implementation.
  int id() const { return id_; }
  bool CanDraw() const;
  LayerTreeFrameSink* layer_tree_frame_sink() const {
    return layer_tree_frame_sink_;
  }
  int max_texture_size() const { return raster_caps().max_texture_size; }
  void ReleaseLayerTreeFrameSink();

  int RequestedMSAASampleCount() const;

  virtual bool InitializeFrameSink(LayerTreeFrameSink* layer_tree_frame_sink);
  TileManager* tile_manager() { return &tile_manager_; }

  const RasterCapabilities& raster_caps() const { return raster_caps_; }
  bool use_gpu_rasterization() const {
    return raster_caps().use_gpu_rasterization;
  }

  ResourcePool* resource_pool() { return resource_pool_.get(); }
  ImageAnimationController* image_animation_controller() {
    return &image_animation_controller_;
  }

  ImageDecodeCache* GetImageDecodeCache() const;

  uint32_t next_frame_token() const;
  void set_next_frame_token_from_client(uint32_t frame_token);

  // Buffers `callback` until a relevant presentation feedback arrives, at which
  // point the callback will be posted to run on the main thread. A presentation
  // feedback is considered relevant if the frame's token is greater than or
  // equal to `frame_token`.
  void RegisterMainThreadPresentationTimeCallbackForTesting(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::Callback callback);

  // Buffers `callback` until a relevant successful presentation occurs, at
  // which point the callback will be posted to run on the main thread. A
  // successful presentation is considered relevant if the presented frame's
  // token is greater than or equal to `frame_token`.
  void RegisterMainThreadSuccessfulPresentationTimeCallbackForTesting(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails callback);

  // Buffers `callback` until a relevant successful presentation occurs, at
  // which point the callback will be run on the compositor thread. A successful
  // presentation is considered relevant if the presented frame's token is
  // greater than or equal to `frame_token`.
  void RegisterCompositorThreadSuccessfulPresentationTimeCallbackForTesting(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::SuccessfulCallback callback);

  virtual bool WillBeginImplFrame(const viz::BeginFrameArgs& args);
  virtual void DidFinishImplFrame(const viz::BeginFrameArgs& args);
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason);
  void DidChangeBeginFrameSourcePaused(bool paused);
  void OnBeginImplFrameDeadline();
  void DidModifyTilePriorities(bool pending_update_tiles);
  // Requests that we do not produce frames until the new viz::LocalSurfaceId
  // has been activated.
  void SetTargetLocalSurfaceId(
      const viz::LocalSurfaceId& target_local_surface_id);
  const viz::LocalSurfaceId& last_draw_local_surface_id() const {
    return last_draw_local_surface_id_;
  }
  // Returns the current local surface id.
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const {
    if (settings().trees_in_viz_in_viz_process) {
      return current_local_surface_id_from_client_;
    }
    return child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  }
  const viz::LocalSurfaceId& target_local_surface_id() const {
    return target_local_surface_id_;
  }
  void set_current_local_surface_id_from_client(
      const viz::LocalSurfaceId& local_surface_id_from_client) {
    DCHECK(settings().trees_in_viz_in_viz_process);
    current_local_surface_id_from_client_ = local_surface_id_from_client;
  }

  LayerTreeImpl* active_tree() { return active_tree_.get(); }
  const LayerTreeImpl* active_tree() const { return active_tree_.get(); }
  LayerTreeImpl* pending_tree() { return pending_tree_.get(); }
  const LayerTreeImpl* pending_tree() const { return pending_tree_.get(); }
  LayerTreeImpl* recycle_tree() { return recycle_tree_.get(); }
  const LayerTreeImpl* recycle_tree() const { return recycle_tree_.get(); }
  // Returns the tree LTH synchronizes with.
  LayerTreeImpl* sync_tree() const {
    return CommitsToActiveTree() ? active_tree_.get() : pending_tree_.get();
  }
  virtual void CreatePendingTree();
  virtual void ActivateSyncTree();

  // Shortcuts to layers/nodes on the active tree.
  ScrollNode* InnerViewportScrollNode() const;
  ScrollNode* OuterViewportScrollNode() const;
  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;

  void QueueSwapPromiseForMainThreadScrollUpdate(
      std::unique_ptr<SwapPromise> swap_promise);

  // TODO(bokan): These input-related methods shouldn't be part of
  // LayerTreeHostImpl's interface.
  bool IsPinchGestureActive() const;
  // See comment in equivalent InputHandler method for what this means.
  ActivelyScrollingType GetActivelyScrollingType() const;
  bool IsCurrentScrollMainRepainted() const;
  bool ScrollAffectsScrollHandler() const;
  void SetExternalPinchGestureActive(bool active);
  void set_force_smooth_wheel_scrolling_for_testing(bool enabled) {
    GetInputHandler().set_force_smooth_wheel_scrolling_for_testing(enabled);
  }

  virtual void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void SetNeedsOneBeginImplFrame();
  void SetNeedsRedraw(bool animation_only, bool skip_if_inside_draw) override;

  ManagedMemoryPolicy ActualManagedMemoryPolicy() const;

  const gfx::Transform& DrawTransform() const;

  // During commit, processes and returns changes in the compositor since the
  // last commit.
  std::unique_ptr<CompositorCommitData> ProcessCompositorDeltas(
      const MutatorHost* main_thread_mutator_host);

  FrameSorter* frame_sorter() { return &frame_sorter_; }
  MemoryHistory* memory_history() { return memory_history_.get(); }
  DebugRectHistory* debug_rect_history() { return debug_rect_history_.get(); }
  viz::ClientResourceProvider* resource_provider() {
    return resource_provider_.get();
  }
  BrowserControlsOffsetManager* browser_controls_manager() {
    return browser_controls_offset_manager_.get();
  }
  ProgressBarOffsetManager* progress_bar_manager() {
    return progress_bar_offset_manager_.get();
  }
  const GlobalStateThatImpactsTilePriority& global_tile_state() {
    return global_tile_state_;
  }

  TaskRunnerProvider* task_runner_provider() const {
    return task_runner_provider_;
  }

  MutatorHost* mutator_host() const { return mutator_host_.get(); }

  void SetDebugState(const LayerTreeDebugState& new_debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  void SetTreePriority(TreePriority priority);
  TreePriority GetTreePriority() const;

  // TODO(mithro): Remove this methods which exposes the internal
  // viz::BeginFrameArgs to external callers.
  virtual const viz::BeginFrameArgs& CurrentBeginFrameArgs() const;

  // Expected time between two begin impl frame calls.
  base::TimeDelta CurrentBeginFrameInterval() const;

  void AsValueWithFrameInto(FrameData* frame,
                            base::trace_event::TracedValue* value) const;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValueWithFrame(
      FrameData* frame) const;
  void ActivationStateAsValueInto(base::trace_event::TracedValue* value) const;

  bool page_scale_animation_active() const { return !!page_scale_animation_; }

  virtual void CreateUIResource(UIResourceId uid,
                                const UIResourceBitmap& bitmap);
  virtual void CreateUIResourceFromImportedResource(UIResourceId uid,
                                                    viz::ResourceId resource_id,
                                                    bool is_opaque);

  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId uid);
  // Evict all UI resources. This differs from ClearUIResources in that this
  // will not immediately delete the resources' backing textures.
  void EvictAllUIResources();
  bool EvictedUIResourcesExist() const;

  virtual viz::ResourceId ResourceIdForUIResource(UIResourceId uid) const;

  virtual bool IsUIResourceOpaque(UIResourceId uid) const;

  void ScheduleMicroBenchmark(std::unique_ptr<MicroBenchmarkImpl> benchmark);

  viz::RegionCaptureBounds CollectRegionCaptureBounds();

  viz::CompositorFrameMetadata MakeCompositorFrameMetadata();
  RenderFrameMetadata MakeRenderFrameMetadata(FrameData* frame);

  const gfx::Rect& external_viewport() const { return external_viewport_; }

  // Viewport rect to be used for tiling prioritization instead of the
  // DeviceViewport().
  const gfx::Rect& viewport_rect_for_tile_priority() const {
    return viewport_rect_for_tile_priority_;
  }

  // When a `LatencyInfoSwapPromiseMonitor` is created on the impl thread, it
  // calls `InsertLatencyInfoSwapPromiseMonitor()` to register itself with
  // `LayerTreeHostImpl`. When the monitor is destroyed, it calls
  // `RemoveLatencyInfoSwapPromiseMonitor()` to unregister itself.
  void InsertLatencyInfoSwapPromiseMonitor(
      LatencyInfoSwapPromiseMonitor* monitor);
  void RemoveLatencyInfoSwapPromiseMonitor(
      LatencyInfoSwapPromiseMonitor* monitor);

  // TODO(weiliangc): Replace RequiresHighResToDraw with scheduler waits for
  // ReadyToDraw. crbug.com/469175
  void SetRequiresHighResToDraw() { requires_high_res_to_draw_ = true; }
  void ResetRequiresHighResToDraw() { requires_high_res_to_draw_ = false; }
  bool RequiresHighResToDraw() const { return requires_high_res_to_draw_; }

  // Only valid for synchronous (non-scheduled) single-threaded case.
  void SynchronouslyInitializeAllTiles();

  bool CommitsToActiveTree() const;

  // Virtual so tests can inject their own.
  virtual std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider();

  bool prepare_tiles_needed() const { return tile_priorities_dirty_; }

  base::SingleThreadTaskRunner* GetTaskRunner() const {
    DCHECK(task_runner_provider_);
    return task_runner_provider_->HasImplThread()
               ? task_runner_provider_->ImplThreadTaskRunner()
               : task_runner_provider_->MainThreadTaskRunner();
  }

  // Returns true if a scroll offset animation is created and false if we scroll
  // by the desired amount without an animation.
  bool ScrollAnimationCreate(const ScrollNode& scroll_node,
                             const gfx::Vector2dF& scroll_amount,
                             base::TimeDelta delayed_by) override;
  void AutoScrollAnimationCreate(const ScrollNode& scroll_node,
                                 const gfx::PointF& target_offset,
                                 float autoscroll_velocity);

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter);
  PaintWorkletLayerPainter* GetPaintWorkletLayerPainterForTesting() const {
    return paint_worklet_painter_.get();
  }

  void QueueImageDecode(int request_id,
                        const DrawImage& image,
                        bool speculative);
  std::vector<std::pair<int, bool>> TakeCompletedImageDecodeRequests();
  // Returns mutator events to be handled by BeginMainFrame.
  std::unique_ptr<MutatorEvents> TakeMutatorEvents();
  UIResourceChangeMap TakeUIResourceChanges(bool require_full_sync);

  void ClearHistory();
  size_t CommitDurationSampleCountForTesting() const;
  void ClearCaches();

  void UpdateImageDecodingHints(
      base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
          decoding_mode_map);

  void InitializeUkm(std::unique_ptr<ukm::UkmRecorder> recorder);

  ActiveFrameSequenceTrackers FrameSequenceTrackerActiveTypes() {
    return frame_trackers_.FrameSequenceTrackerActiveTypes();
  }

  void RenewTreePriorityForTesting() { RenewTreePriority(); }

  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer);

  void SetActiveURL(const GURL& url, ukm::SourceId source_id);

  void SetUkmDroppedFramesDestination(
      base::WritableSharedMemoryMapping ukm_dropped_frames_data);

  // Notifies FrameTrackers, impl side callbacks that the compsitor frame
  // was presented.
  void NotifyDidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<PresentationTimeCallbackBuffer::SuccessfulCallback> callbacks,
      const viz::FrameTimingDetails& details);

  CompositorFrameReportingController* compositor_frame_reporting_controller()
      const {
    return compositor_frame_reporting_controller_.get();
  }

  void set_pending_tree_fully_painted_for_testing(bool painted) {
    pending_tree_fully_painted_ = painted;
  }
  AnimatedPaintWorkletTracker& paint_worklet_tracker() {
    return paint_worklet_tracker_;
  }

  bool can_use_msaa() const { return raster_caps().can_use_msaa; }

  Viewport& viewport() const { return *viewport_.get(); }

  FrameSorter* frame_sorter_for_testing() { return &frame_sorter_; }

  // Returns true if the client is currently compositing synchronously.
  bool IsInSynchronousComposite() const;

  RasterQueryQueue* GetRasterQueryQueueForTesting() const {
    return pending_raster_queries_.get();
  }

  base::flat_set<viz::FrameSinkId> GetFrameSinksToThrottleForTesting() const {
    return throttle_decider_.ids();
  }

  bool IsReadyToActivate() const;

  void RequestImplSideInvalidationForRerasterTiling();
  void RequestImplSideInvalidationForRasterInducingScroll(
      ElementId scroll_element_id);

  void SetDownsampleMetricsForTesting(bool value) {
    downsample_metrics_ = value;
  }
  const LayerTreeHostImplClient* client_for_testing() const { return client_; }

  void SetViewTransitionContentRect(
      uint32_t sequence_id,
      const viz::ViewTransitionElementResourceId& id,
      const gfx::RectF& rect);

  void UpdateChildLocalSurfaceId();

  void ReturnResource(viz::ReturnedResource returned_resource);

  void NotifyNewLocalSurfaceIdExpectedWhilePaused();

  void ElasticOverscrollAnimationFinished(ElementId finished_id);

  void set_send_frame_token_to_embedder(bool send_frame_token_to_embedder) {
    send_frame_token_to_embedder_ = send_frame_token_to_embedder;
  }
  bool send_frame_token_to_embedder() const {
    return send_frame_token_to_embedder_;
  }

 protected:
  LayerTreeHostImpl(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      RasterDarkModeFilter* dark_mode_filter,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client);

  // Virtual for testing.
  virtual bool AnimateLayers(base::TimeTicks monotonic_time,
                             bool is_active_tree);
  virtual viz::CompositorFrame GenerateCompositorFrame(FrameData* frame);
  void ImageDecodeFinished(int request_id,
                           bool speculative,
                           bool decode_succeeded);

  bool is_likely_to_require_a_draw() const {
    return is_likely_to_require_a_draw_;
  }

  // Removes empty or orphan RenderPasses from the frame.
  static void RemoveRenderPasses(FrameData* frame);

  const raw_ptr<LayerTreeHostImplClient> client_;
  const raw_ptr<LayerTreeHostSchedulingClient> scheduling_client_;
  const raw_ptr<TaskRunnerProvider> task_runner_provider_;

  BeginFrameTracker current_begin_frame_tracker_;

 private:
  // Holds image decode cache instance. It can either be a shared cache or
  // a cache create by this instance. Which is used depends on the settings.
  class ImageDecodeCacheHolder;

  void CollectScrollbarUpdatesForCommit(
      CompositorCommitData* commit_data) const;

  void CleanUpTileManagerResources();
  void CreateTileManagerResources();
  void ReleaseTreeResources();
  void ReleaseTileResources();
  void RecreateTileResources();

  void AnimateInternal();

  void RenewTreePriority();

  // The function is called to update state on the sync tree after a commit
  // finishes or after the sync tree was created to invalidate content on the
  // impl thread.
  void UpdateSyncTreeAfterCommitOrImplSideInvalidation();

  // Returns a job map for all 'dirty' PaintWorklets, e.g. PaintWorkletInputs
  // that do not map to a PaintRecord.
  PaintWorkletJobMap GatherDirtyPaintWorklets(PaintImageIdFlatSet*) const;

  // Called when all PaintWorklet results are ready (i.e. have been painted) for
  // the current pending tree.
  void OnPaintWorkletResultsReady(PaintWorkletJobMap results);

  // Called when the pending tree has been fully painted, i.e. all required data
  // is available to raster the tree.
  void NotifyPendingTreeFullyPainted();

  void UpdateRasterCapabilities();

  bool AnimatePageScale(base::TimeTicks monotonic_time);
  bool AnimateScrollbars(base::TimeTicks monotonic_time);
  bool AnimateBrowserControls(base::TimeTicks monotonic_time);

  void UpdateTileManagerMemoryPolicy(const ManagedMemoryPolicy& policy);

  // Returns true if the damage rect is non-empty. This check includes damage
  // from the HUD. Should only be called when the active tree's draw properties
  // are valid and after updating the damage.
  bool HasDamage() const;

  // This function should only be called from PrepareToDraw, as DidDrawAllLayers
  // must be called if this helper function is called.  Returns
  // DrawResult::kSuccess if the frame should be drawn.
  //
  // |expects_to_draw| will force DrawResult::kSuccess state, and damage to be
  // set for this frame. This is only used in the trees_in_viz_in_viz_process
  // mode, and CalculateRenderPasses will DCHECK if |expects_to_draw| does not
  // match the actual behavior.
  DrawResult CalculateRenderPasses(FrameData* frame,
                                   bool expects_to_draw = false);

  // Once a resource is uploaded or deleted, it is no longer an evicted id, this
  // removes it from the evicted set, and updates if we're able to draw now that
  // all UIResources are valid.
  void MarkUIResourceNotEvicted(UIResourceId uid);
  // Deletes all UIResource backings, and marks all the ids as evicted.
  void ClearUIResources();
  // Frees the textures/bitmaps backing the UIResource, held in the
  // UIResourceData.
  void DeleteUIResourceBacking(UIResourceData data,
                               const gpu::SyncToken& sync_token);
  // Callback for when a UIResource is deleted *and* no longer in use by the
  // display compositor. It will DeleteUIResourceBacking() if the backing was
  // not already deleted preemptively.
  void OnUIResourceReleased(UIResourceId uid,
                            const gpu::SyncToken& sync_token,
                            bool lost);

  void NotifyLatencyInfoSwapPromiseMonitors();

  void SetMemoryPolicyImpl(const ManagedMemoryPolicy& policy);
  void SetContextVisibility(bool is_visible);

  void ShowScrollbarsForImplScroll(ElementId element_id);

  // Copy any opacity values already in the active tree to the pending
  // tree, because the active tree value always takes precedence for scrollbars.
  void PushScrollbarOpacitiesFromActiveToPending();

  // Pushes state for image animations and checkerboarded images from the
  // pending to active tree. This is called during activation when a pending
  // tree exists, and during the commit if we are committing directly to the
  // active tree.
  void ActivateStateForImages();

  void OnMemoryPressure(base::MemoryPressureLevel level) override;

  void AllocateLocalSurfaceId();

  // Log the AverageLag events from the frame identified by |frame_token| and
  // the information in |details|.
  void LogAverageLagEvents(uint32_t frame_token,
                           const viz::FrameTimingDetails& details);

  // Notifies client about the custom tracker results.
  void NotifyCompositorMetricsTrackerResults(
      const CustomTrackerResults& results);

  // Wrapper for checking and updating |contains_srgb_cache_|.
  bool CheckColorSpaceContainsSrgb(const gfx::ColorSpace& color_space) const;

  // Registers callbacks, as needed, to track First Scroll Latency.
  void ApplyFirstScrollTracking(const ui::LatencyInfo& latency,
                                uint32_t frame_token);

  // Flush pending work if we are currently not visible.
  void MaybeFlushPendingWork();

  // Returns whether the LayerTreeHostImpl is running on a renderer process.
  bool RunningOnRendererProcess() const;

  // Returns the most up to date display color spaces.
  gfx::DisplayColorSpaces GetDisplayColorSpaces() const;

  void ResetHasInputForFrameInterval();

  // Requests scrollbars' flashes. Returns true if a scrollbar with
  // |tracking_element_id| has been flashed as part of the bulk flash. If
  // `settings.scrollbar_flash_once_after_scroll_update` is true, invokes
  // `MaybeFlashAllScrollbarsOnce`. If
  // `settings.scrollbar_flash_after_any_scroll_update` is true, invokes
  // FlashAllScrollbars.
  bool MaybeFlashAllScrollbars(ElementId tracking_element_id, bool did_scroll);

  // Flashes all scrollbars. Can be used when
  // `settings.scrollbar_flash_after_any_scroll_update` is true.
  void FlashAllScrollbars(bool did_scroll);

  // Erases track of flashed scrollbars. Can be used when
  // `settings.scrollbar_flash_once_after_scroll_update` is true.
  void EraseFlashedScrollbars();

  // Flashes all scrollbars if they haven't been flashed yet. Scrollbars that
  // have been flashed are stored in |flashed_scrollbars_|. Returns true if a
  // scrollbar with |tracking_element_id| has been flashed.
  // Can used when `settings.scrollbar_flash_once_after_scroll_update` is true.
  bool MaybeFlashAllScrollbarsOnce(ElementId tracking_element_id,
                                   bool did_scroll);

  // If `settings.scrollbar_flash_once_visible_on_viewport` is true, flashes
  // scrollbars that become visible on the viewport. |element_id| is treated as
  // the scrolling element and is ignored. |scroll_delta| is used to determine
  // total amount of scroll, which is tested against a threshold to reduce
  // expensiveness of this function.
  void MaybeFlashEnteredViewportScrollbars(ElementId element_id,
                                           const gfx::Vector2dF& scroll_delta);

  // Once bound, this instance owns the InputHandler. However, an InputHandler
  // need not be bound so this should be null-checked before dereferencing.
  std::unique_ptr<InputDelegateForCompositor> input_delegate_;

  const LayerTreeSettings settings_;

  // This is set to true only if:
  //  . The compositor is running single-threaded (i.e. there is no separate
  //    compositor/impl thread).
  //  . There is no scheduler (which means layer-update, composite, etc. steps
  //    happen explicitly via. synchronous calls to appropriate functions).
  // This is usually turned on only in some tests (e.g. web-tests).
  const bool is_synchronous_single_threaded_;

  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;

  std::unordered_map<UIResourceId, UIResourceData> ui_resource_map_;
  // UIResources are held here once requested to be deleted until they are
  // released from the display compositor, then the backing can be deleted.
  std::unordered_map<UIResourceId, UIResourceData> deleted_ui_resources_;
  // Resources that were evicted by EvictAllUIResources. Resources are removed
  // from this when they are touched by a create or destroy from the UI resource
  // request queue. The resource IDs held in here do not have any backing
  // associated with them anymore, as that is freed at the time of eviction.
  std::set<UIResourceId> evicted_ui_resources_;

  // When using a layer context for display, this tracks changes to UIResources
  // that should be synchronized to the layer context.
  UIResourceChangeMap ui_resource_changes_;

  // These are valid when has_valid_layer_tree_frame_sink_ is true.
  //
  // A pointer used for communicating with and submitting output to the display
  // compositor.
  raw_ptr<LayerTreeFrameSink> layer_tree_frame_sink_ = nullptr;

  // Valid when we have a LayerTreeFrameSink and
  // `trees_in_viz_in_client_process_` is true. This object pushes updates to a
  // remote display tree.
  std::unique_ptr<LayerContext> layer_context_;

  // The following scoped variables must not outlive the
  // |layer_tree_frame_sink_|.
  // These should be transferred to viz::ContextCacheController's
  // ClientBecameNotVisible() before the output surface is destroyed.
  std::unique_ptr<viz::ContextCacheController::ScopedVisibility>
      compositor_context_visibility_;
  std::unique_ptr<viz::ContextCacheController::ScopedVisibility>
      worker_context_visibility_;

  RasterCapabilities raster_caps_;

  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<RasterQueryQueue> pending_raster_queries_;
  std::unique_ptr<ImageDecodeCacheHolder> image_decode_cache_holder_;

  GlobalStateThatImpactsTilePriority global_tile_state_;

  // Tree currently being drawn.
  std::unique_ptr<LayerTreeImpl> active_tree_;

  // In impl-side painting mode, tree with possibly incomplete rasterized
  // content. May be promoted to active by ActivateSyncTree().
  std::unique_ptr<LayerTreeImpl> pending_tree_;

  // In impl-side painting mode, inert tree with layers that can be recycled
  // by the next sync from the main thread.
  std::unique_ptr<LayerTreeImpl> recycle_tree_;

  // Tracks, for debugging purposes, the amount of scroll received (not
  // necessarily applied) in this compositor frame. This will be reset each
  // time a CompositorFrame is generated.
  gfx::Vector2dF scroll_accumulated_this_frame_;

  // This is used to track the accumulated scroll deltas for each element.
  // See `MaybeFlashEnteredViewportScrollbars` for more details.
  std::unordered_map<ElementId, gfx::Vector2dF, ElementIdHash>
      accumulated_scroll_deltas_by_element_id_;

  std::vector<std::unique_ptr<SwapPromise>>
      swap_promises_for_main_thread_scroll_update_;

  bool tile_priorities_dirty_ = false;

  LayerTreeDebugState debug_state_;
  bool visible_ = false;
  ManagedMemoryPolicy cached_managed_memory_policy_;

  TileManager tile_manager_;

  std::unique_ptr<BrowserControlsOffsetManager>
      browser_controls_offset_manager_;
  std::unique_ptr<ProgressBarOffsetManager> progress_bar_offset_manager_;

  std::unique_ptr<PageScaleAnimation> page_scale_animation_;

  base::WritableSharedMemoryMapping ukm_smoothness_mapping_;
  base::WritableSharedMemoryMapping ukm_dropped_frames_mapping_;

  std::unique_ptr<MemoryHistory> memory_history_;
  std::unique_ptr<DebugRectHistory> debug_rect_history_;

  // The maximum memory that would be used by the prioritized resource
  // manager, if there were no limit on memory usage.
  size_t max_memory_needed_bytes_ = 0;

  // Optional top-level constraints that can be set by the LayerTreeFrameSink.
  // - external_transform_ applies a transform above the root layer
  // - external_viewport_ is used DrawProperties, tile management and
  // glViewport/window projection matrix.
  // - viewport_rect_for_tile_priority_ is the rect in view space used for
  // tiling priority.
  gfx::Transform external_transform_;
  gfx::Rect external_viewport_;
  gfx::Rect viewport_rect_for_tile_priority_;
  bool resourceless_software_draw_ = false;

  gfx::Rect viewport_damage_rect_;
  std::optional<base::CheckedNumeric<uint32_t>> total_invalidated_area_ = 0;

  std::unique_ptr<MutatorHost> mutator_host_;
  std::unique_ptr<MutatorEvents> mutator_events_;
  std::set<raw_ptr<VideoFrameController, SetExperimental>>
      video_frame_controllers_;
  const raw_ptr<RasterDarkModeFilter> dark_mode_filter_;

  // Map from scroll element ID to scrollbar animation controller.
  // There is one animation controller per pair of overlay scrollbars.
  std::unordered_map<ElementId,
                     std::unique_ptr<ScrollbarAnimationController>,
                     ElementIdHash>
      scrollbar_animation_controllers_;

  // Set of scrollbars that have already been flashed (for flash-once behavior).
  std::unordered_set<ElementId, ElementIdHash> flashed_scrollbars_;

  raw_ptr<RenderingStatsInstrumentation> rendering_stats_instrumentation_;
  MicroBenchmarkControllerImpl micro_benchmark_controller_;
  std::unique_ptr<SynchronousTaskGraphRunner>
      single_thread_synchronous_task_graph_runner_;

  // Optional callback to notify of new tree activations.
  base::RepeatingClosure tree_activation_callback_;

  raw_ptr<TaskGraphRunner> task_graph_runner_;
  int id_;

  std::set<raw_ptr<LatencyInfoSwapPromiseMonitor, SetExperimental>>
      latency_info_swap_promise_monitor_;

  bool requires_high_res_to_draw_ = false;
  bool is_likely_to_require_a_draw_ = false;

  // TODO(danakj): Delete the LayerTreeFrameSink and all resources when
  // it's lost instead of having this bool.
  bool has_valid_layer_tree_frame_sink_ = false;

  bool prioritize_new_content_due_to_checkerboarding_ = false;

  // If it is enabled in the LayerTreeSettings, we can check damage in
  // WillBeginImplFrame and abort early if there is no damage. We only check
  // damage in WillBeginImplFrame if a recent frame had no damage. We keep
  // track of this with |consecutive_frame_with_damage_count_|.
  int consecutive_frame_with_damage_count_;

  std::unique_ptr<Viewport> viewport_;

  gfx::Size visual_device_viewport_size_;

  // Set to true if viewport is mobile optimized by using meta tag
  // <meta name="viewport" content="width=device-width">
  // or
  // <meta name="viewport" content="initial-scale=1.0">
  bool is_viewport_mobile_optimized_ = false;

  bool prefers_reduced_motion_ = false;

  bool may_throttle_if_undrawn_frames_ = true;

  // These completion states to be transferred to the main thread when we
  // begin main frame. The pair represents a request id and the completion (ie
  // success) state.
  std::vector<std::pair<int, bool>> completed_image_decode_requests_;

  enum class ImplThreadPhase {
    IDLE,
    INSIDE_IMPL_FRAME,
  };
  ImplThreadPhase impl_thread_phase_ = ImplThreadPhase::IDLE;

  ImageAnimationController image_animation_controller_;

  // Provides RenderFrameMetadata to the Browser process upon the submission of
  // each CompositorFrame.
  std::unique_ptr<RenderFrameMetadataObserver> render_frame_metadata_observer_;

  viz::FrameTokenGenerator next_frame_token_;
  uint32_t next_frame_token_from_client_ = viz::kInvalidFrameToken;

  viz::LocalSurfaceId last_draw_local_surface_id_;
  base::flat_set<viz::SurfaceRange> last_draw_referenced_surfaces_;
  std::optional<RenderFrameMetadata> last_draw_render_frame_metadata_;
  // The viz::LocalSurfaceId to unthrottle drawing for.
  viz::LocalSurfaceId target_local_surface_id_;
  viz::LocalSurfaceId evicted_local_surface_id_;
  viz::ChildLocalSurfaceIdAllocator child_local_surface_id_allocator_;
  viz::LocalSurfaceId current_local_surface_id_from_client_;

  // Indicates the direction of the last vertical scroll of the root layer.
  // Until the first vertical scroll occurs, this value is |kNull|. Note that
  // once this value is updated, it will never return to |kNull|.
  viz::VerticalScrollDirection last_vertical_scroll_direction_ =
      viz::VerticalScrollDirection::kNull;

  std::unique_ptr<base::AsyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;

  PresentationTimeCallbackBuffer presentation_time_callbacks_;

  // `compositor_frame_reporting_controller_` is an observer of
  // `frame_sorter_` so it must be declared last and deleted first.
  FrameSorter frame_sorter_;
  std::unique_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;
  FrameSequenceTrackerCollection frame_trackers_;

  // PaintWorklet painting is controlled from the LayerTreeHostImpl, dispatched
  // to the worklet thread via |paint_worklet_painter_|.
  std::unique_ptr<PaintWorkletLayerPainter> paint_worklet_painter_;

  // While PaintWorklet painting is ongoing the PendingTree is not yet fully
  // painted and cannot be rastered or activated. This boolean tracks whether or
  // not we are in that state.
  bool pending_tree_fully_painted_ = false;

#if DCHECK_IS_ON()
  // Use to track when doing a synchronous draw.
  bool doing_sync_draw_ = false;
#endif

  // This is used to tell the scheduler there are active scroll handlers on the
  // page so we should prioritize latency during a scroll to try to keep
  // scroll-linked effects up to data.
  // TODO(bokan): This is quite old and scheduling has become much more
  // sophisticated since so it's not clear how much value it's still providing.
  bool scroll_affects_scroll_handler_ = false;

  // Provides support for PaintWorklets which depend on input properties that
  // are being animated by the compositor (aka 'animated' PaintWorklets).
  // Responsible for storing animated custom property values and for
  // invalidating PaintWorklets as the property values change.
  AnimatedPaintWorkletTracker paint_worklet_tracker_;

  AverageLagTrackingManager lag_tracking_manager_;

  EventsMetricsManager events_metrics_manager_;

  std::unique_ptr<LCDTextMetricsReporter> lcd_text_metrics_reporter_;

  bool has_input_for_frame_interval_ = false;
  DelayedUniqueNotifier has_input_resetter_;
  bool has_non_fling_input_since_last_frame_ = false;
  bool has_observed_first_scroll_delay_ = false;

  // Cache for the results of calls to gfx::ColorSpace::Contains() on sRGB. This
  // computation is deterministic for a given color space, can be called
  // multiple times per frame, and incurs a non-trivial cost.
  // mutable because |contains_srgb_cache_| is accessed in a const method.
  mutable base::LRUCache<gfx::ColorSpace, bool> contains_srgb_cache_;

  // When enabled, calculates which frame sinks can be throttled based on
  // some pre-defined criteria.
  ThrottleDecider throttle_decider_;

  bool downsample_metrics_ = true;
  base::MetricsSubSampler metrics_subsampler_;

  // See `CommitState::screenshot_destination_token`.
  base::UnguessableToken screenshot_destination_;

  float top_controls_visible_height_ = 0.f;

  // Maximum scroll delta update in x or y direction since last begin impl
  // frame.
  float frame_max_scroll_delta_ = 0.f;

  // Time delta between last and current begin impl frame.
  base::TimeDelta begin_frame_time_delta_;

  base::flat_set<ElementId> pending_invalidation_raster_inducing_scrolls_;

  std::unordered_map<uint32_t, viz::ViewTransitionElementResourceRects>
      view_transition_content_rects_;

  // Last drawn frame's BeginFrameArgs, used to check whether the active tree
  // was reused.
  viz::BeginFrameArgs last_draw_active_tree_begin_frame_args_;

  // When true, we are expected to get a new local surface id with the next
  // commit.
  bool new_local_surface_id_expected_ = false;

  // Track previously visible scrollable elements for viewport visibility
  // detection.
  base::flat_set<ElementId> previously_visible_scrollable_elements_;

  // Only used in TreesInViz mode, whether to call
  // CompositorFrameSinkSupport::OnFrameTokenChanged(). In TreesInViz
  // mode, it's computed when calling GenerateCompositorFrame() in
  // renderer process, and its computation when calling
  // GenerateCompositorFrame() in viz is skipped, therefore, we need to
  // pass it from renderer to viz.
  bool send_frame_token_to_embedder_ = false;

  // Settings whether we dump generated compositor frame during DrawLayers.
  // They are for debug purposes for TreesInViz and TreeAnimationsInViz.
  bool dump_compositor_frame_ = false;
  uint32_t dump_compositor_frame_begin_ = 0;
  uint32_t dump_compositor_frame_end_ = 0;

  // Must be the last member to ensure this is destroyed first in the
  // destruction order and invalidates all weak pointers.
  base::WeakPtrFactory<LayerTreeHostImpl> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_H_
