// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/base/synced_property.h"
#include "cc/benchmarks/micro_benchmark_controller_impl.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/layers/layer_collections.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/paint_worklet_job.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler.h"
#include "cc/scheduler/video_frame_controller.h"
#include "cc/tiles/decoded_image_tracker.h"
#include "cc/tiles/image_decode_cache.h"
#include "cc/tiles/tile_manager.h"
#include "cc/trees/animated_paint_worklet_tracker.h"
#include "cc/trees/de_jelly_state.h"
#include "cc/trees/frame_rate_estimator.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/task_runner_provider.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class ScrollOffset;
}

namespace viz {
class CompositorFrame;
class CompositorFrameMetadata;
struct FrameTimingDetails;
}  // namespace viz

namespace cc {

class BrowserControlsOffsetManager;
class CompositorFrameReportingController;
class DebugRectHistory;
class EvictionTilePriorityQueue;
class ImageAnimationController;
class LCDTextMetricsReporter;
class LayerImpl;
class LayerTreeFrameSink;
class LayerTreeImpl;
class PaintWorkletLayerPainter;
class MemoryHistory;
class MutatorEvents;
class MutatorHost;
class PageScaleAnimation;
class PendingTreeRasterDurationHistogramTimer;
class RasterTilePriorityQueue;
class RasterBufferProvider;
class RenderFrameMetadataObserver;
class RenderingStatsInstrumentation;
class ResourcePool;
class ScrollElasticityHelper;
class SwapPromise;
class SwapPromiseMonitor;
class SynchronousTaskGraphRunner;
class TaskGraphRunner;
class UIResourceBitmap;
class Viewport;

enum class GpuRasterizationStatus {
  ON,
  OFF_FORCED,
  OFF_DEVICE,
};

enum class ImplThreadPhase {
  IDLE,
  INSIDE_IMPL_FRAME,
};

// LayerTreeHost->Proxy callback interface.
class LayerTreeHostImplClient {
 public:
  virtual void DidLoseLayerTreeFrameSinkOnImplThread() = 0;
  virtual void SetBeginFrameSource(viz::BeginFrameSource* source) = 0;
  virtual void DidReceiveCompositorFrameAckOnImplThread() = 0;
  virtual void OnCanDrawStateChanged(bool can_draw) = 0;
  virtual void NotifyReadyToActivate() = 0;
  virtual void NotifyReadyToDraw() = 0;
  // Please call these 2 functions through
  // LayerTreeHostImpl's SetNeedsRedraw() and SetNeedsOneBeginImplFrame().
  virtual void SetNeedsRedrawOnImplThread() = 0;
  virtual void SetNeedsOneBeginImplFrameOnImplThread() = 0;
  virtual void SetNeedsCommitOnImplThread() = 0;
  virtual void SetNeedsPrepareTilesOnImplThread() = 0;
  virtual void SetVideoNeedsBeginFrames(bool needs_begin_frames) = 0;
  virtual bool IsInsideDraw() = 0;
  virtual void RenewTreePriority() = 0;
  virtual void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                                    base::TimeDelta delay) = 0;
  virtual void DidActivateSyncTree() = 0;
  virtual void WillPrepareTiles() = 0;
  virtual void DidPrepareTiles() = 0;

  // Called when page scale animation has completed on the impl thread.
  virtual void DidCompletePageScaleAnimationOnImplThread() = 0;

  // Called when output surface asks for a draw.
  virtual void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                           bool skip_draw) = 0;

  virtual void NeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) = 0;
  // Called when a requested image decode completes.
  virtual void NotifyImageDecodeRequestFinished() = 0;

  virtual void RequestBeginMainFrameNotExpected(bool new_state) = 0;

  // Called when a presentation time is requested. |frame_token| identifies
  // the frame that was presented.
  virtual void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const viz::FrameTimingDetails& details) = 0;

  // Returns whether the main-thread is expected to receive a BeginMainFrame.
  virtual bool IsBeginMainFrameExpected() = 0;

  virtual void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType tree_type) = 0;

  virtual void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) = 0;

  virtual void NotifyThroughputTrackerResults(CustomTrackerResults results) = 0;

  // Send the throughput data to the main thread's LayerTreeHostClient, which
  // then send the data to the browser process and eventually report to UKM.
  virtual void SubmitThroughputData(ukm::SourceId source_id,
                                    int aggregated_percent,
                                    int impl_percent,
                                    base::Optional<int> main_percent) = 0;

  virtual void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) = 0;

 protected:
  virtual ~LayerTreeHostImplClient() = default;
};

// LayerTreeHostImpl owns the LayerImpl trees as well as associated rendering
// state.
class CC_EXPORT LayerTreeHostImpl : public InputHandler,
                                    public TileManagerClient,
                                    public LayerTreeFrameSinkClient,
                                    public BrowserControlsOffsetManagerClient,
                                    public ScrollbarAnimationControllerClient,
                                    public VideoFrameControllerClient,
                                    public MutatorHostClient,
                                    public ImageAnimationController::Client {
 public:
  // This structure is used to build all the state required for producing a
  // single CompositorFrame. The |render_passes| list becomes the set of
  // RenderPasses in the quad, and the other fields are used for computation
  // or become part of the CompositorFrameMetadata.
  struct CC_EXPORT FrameData {
    FrameData();
    FrameData(const FrameData&) = delete;
    ~FrameData();

    FrameData& operator=(const FrameData&) = delete;
    void AsValueInto(base::trace_event::TracedValue* value) const;
    std::string ToString() const;

    // frame_token is populated by the LayerTreeHostImpl when submitted.
    uint32_t frame_token = 0;

    bool has_missing_content = false;

    std::vector<viz::SurfaceId> activation_dependencies;
    base::Optional<uint32_t> deadline_in_frames;
    bool use_default_lower_bound_deadline = false;
    viz::RenderPassList render_passes;
    const RenderSurfaceList* render_surface_list = nullptr;
    LayerImplList will_draw_layers;
    bool has_no_damage = false;
    bool may_contain_video = false;
    viz::BeginFrameAck begin_frame_ack;
    // The original BeginFrameArgs that triggered the latest update from the
    // main thread.
    viz::BeginFrameArgs origin_begin_main_frame_args;
  };

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
    viz::ResourceFormat format;

    // Backing for software compositing.
    viz::SharedBitmapId shared_bitmap_id;
    base::WritableSharedMemoryMapping shared_mapping;
    // Backing for gpu compositing.
    gpu::Mailbox mailbox;

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
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client);
  LayerTreeHostImpl(const LayerTreeHostImpl&) = delete;
  ~LayerTreeHostImpl() override;

  LayerTreeHostImpl& operator=(const LayerTreeHostImpl&) = delete;

  // InputHandler implementation
  void BindToClient(InputHandlerClient* client) override;
  InputHandler::ScrollStatus ScrollBegin(ScrollState* scroll_state,
                                         ui::ScrollInputType type) override;
  InputHandler::ScrollStatus RootScrollBegin(ScrollState* scroll_state,
                                             ui::ScrollInputType type) override;
  InputHandlerScrollResult ScrollUpdate(
      ScrollState* scroll_state,
      base::TimeDelta delayed_by = base::TimeDelta()) override;
  void RequestUpdateForSynchronousInputHandler() override;
  void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::ScrollOffset& root_content_offset) override;
  void ScrollEnd(bool should_snap = false) override;
  void RecordScrollBegin(ui::ScrollInputType input_type,
                         ScrollBeginThreadState scroll_start_state) override;
  void RecordScrollEnd(ui::ScrollInputType input_type) override;

  InputHandlerPointerResult MouseDown(const gfx::PointF& viewport_point,
                                      bool shift_modifier) override;
  InputHandlerPointerResult MouseUp(const gfx::PointF& viewport_point) override;
  InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& viewport_point) override;
  void MouseLeave() override;

  // Returns frame_element_id from the layer hit by the given point.
  // If the hit test failed, an invalid element ID is returned.
  ElementId FindFrameElementIdAtPoint(
      const gfx::PointF& viewport_point) override;

  void PinchGestureBegin() override;
  void PinchGestureUpdate(float magnify_delta,
                          const gfx::Point& anchor) override;
  void PinchGestureEnd(const gfx::Point& anchor, bool snap_to_min) override;
  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool anchor_point,
                               float page_scale,
                               base::TimeDelta duration);
  void SetNeedsAnimateInput() override;
  bool IsCurrentlyScrollingViewport() const override;
  EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const override;
  InputHandler::TouchStartOrMoveEventListenerType
  EventListenerTypeForTouchStartOrMoveAt(
      const gfx::Point& viewport_port,
      TouchAction* out_touch_action) override;
  bool HasBlockingWheelEventHandlerAt(
      const gfx::Point& viewport_point) const override;
  std::unique_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override;
  std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      std::unique_ptr<EventMetrics> event_metrics) override;
  ScrollElasticityHelper* CreateScrollElasticityHelper() override;
  bool GetScrollOffsetForLayer(ElementId element_id,
                               gfx::ScrollOffset* offset) override;
  bool ScrollLayerTo(ElementId element_id,
                     const gfx::ScrollOffset& offset) override;
  bool ScrollingShouldSwitchtoMainThread() override;
  void NotifyInputEvent() override;

  // BrowserControlsOffsetManagerClient implementation.
  float TopControlsHeight() const override;
  float TopControlsMinHeight() const override;
  float BottomControlsHeight() const override;
  float BottomControlsMinHeight() const override;
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override;
  float CurrentTopControlsShownRatio() const override;
  float CurrentBottomControlsShownRatio() const override;
  void DidChangeBrowserControlsPosition() override;
  void DidObserveScrollDelay(base::TimeDelta scroll_delay,
                             base::TimeTicks scroll_timestamp);
  bool HaveRootScrollNode() const override;
  void SetNeedsCommit() override;

  // ImageAnimationController::Client implementation.
  void RequestBeginFrameForAnimatedImages() override;
  void RequestInvalidationForAnimatedImages() override;

  EventMetricsSet TakeEventsMetrics();
  void AppendEventsMetricsFromMainThread(
      std::vector<EventMetrics> events_metrics);

  base::WeakPtr<LayerTreeHostImpl> AsWeakPtr();

  void set_resourceless_software_draw_for_testing() {
    resourceless_software_draw_ = true;
  }

  const gfx::Rect& viewport_damage_rect_for_testing() const {
    return viewport_damage_rect_;
  }

  virtual void WillSendBeginMainFrame();
  virtual void DidSendBeginMainFrame(const viz::BeginFrameArgs& args);
  virtual void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      const viz::BeginFrameArgs& args);
  virtual void ReadyToCommit(const viz::BeginFrameArgs& commit_args);
  virtual void BeginCommit();
  virtual void CommitComplete();
  virtual void UpdateAnimationState(bool start_ready_animations);
  bool Mutate(base::TimeTicks monotonic_time);
  void ActivateAnimations();
  void Animate();
  void AnimatePendingTreeAfterCommit();
  void DidAnimateScrollOffset();
  void SetFullViewportDamage();
  void SetViewportDamage(const gfx::Rect& damage_rect);

  // Updates registered ElementIds present in |changed_list|. Call this after
  // changing the property trees for the |changed_list| trees.
  void UpdateElements(ElementListType changed_list);

  // Analogous to a commit, this function is used to create a sync tree and
  // add impl-side invalidations to it.
  // virtual for testing.
  virtual void InvalidateContentOnImplSide();
  virtual void InvalidateLayerTreeFrameSink(bool needs_redraw);

  void SetTreeLayerScrollOffsetMutated(ElementId element_id,
                                       LayerTreeImpl* tree,
                                       const gfx::ScrollOffset& scroll_offset);
  void SetNeedUpdateGpuRasterizationStatus();
  bool NeedUpdateGpuRasterizationStatusForTesting() const {
    return need_update_gpu_rasterization_status_;
  }

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
  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;
  void ElementIsAnimatingChanged(const PropertyToElementIdMap& element_id_map,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;
  void AnimationScalesChanged(ElementId element_id,
                              ElementListType list_type,
                              float maximum_scale,
                              float starting_scale) override;
  void OnCustomPropertyMutated(
      ElementId element_id,
      const std::string& custom_property_name,
      PaintWorkletInput::PropertyValue custom_property_value) override;

  void ScrollOffsetAnimationFinished() override;
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override;

  virtual bool PrepareTiles();

  // Returns DRAW_SUCCESS unless problems occured preparing the frame, and we
  // should try to avoid displaying the frame. If PrepareToDraw is called,
  // DidDrawAllLayers must also be called, regardless of whether DrawLayers is
  // called between the two.
  virtual DrawResult PrepareToDraw(FrameData* frame);
  virtual bool DrawLayers(FrameData* frame);
  viz::CompositorFrame GenerateCompositorFrame(FrameData* frame);
  // Must be called if and only if PrepareToDraw was called.
  void DidDrawAllLayers(const FrameData& frame);

  const LayerTreeSettings& settings() const { return settings_; }

  // Evict all textures by enforcing a memory policy with an allocation of 0.
  void EvictTexturesForTesting();

  // When blocking, this prevents client_->NotifyReadyToActivate() from being
  // called. When disabled, it calls client_->NotifyReadyToActivate()
  // immediately if any notifications had been blocked while blocking.
  virtual void BlockNotifyReadyToActivateForTesting(bool block);

  // Prevents notifying the |client_| when an impl side invalidation request is
  // made. When unblocked, the disabled request will immediately be called.
  virtual void BlockImplSideInvalidationRequestsForTesting(bool block);

  // Resets all of the trees to an empty state.
  void ResetTreesForTesting();

  void set_force_smooth_wheel_scrolling_for_testing(bool enabled) {
    force_smooth_wheel_scrolling_for_testing_ = enabled;
  }

  size_t SourceAnimationFrameNumberForTesting() const;

  void RegisterScrollbarAnimationController(ElementId scroll_element_id,
                                            float initial_opacity);
  void DidUnregisterScrollbarLayer(ElementId scroll_element_id,
                                   ScrollbarOrientation orientation);
  ScrollbarAnimationController* ScrollbarAnimationControllerForElementId(
      ElementId scroll_element_id) const;
  void FlashAllScrollbars(bool did_scroll);

  DrawMode GetDrawMode() const;

  void DidNotNeedBeginFrame();

  // TileManagerClient implementation.
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void NotifyAllTileTasksCompleted() override;
  void NotifyTileStateChanged(const Tile* tile) override;
  std::unique_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) override;
  std::unique_ptr<EvictionTilePriorityQueue> BuildEvictionQueue(
      TreePriority tree_priority) override;
  void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) override;
  gfx::ColorSpace GetRasterColorSpace(
      gfx::ContentColorUsage content_color_usage) const override;
  void RequestImplSideInvalidationForCheckerImagedTiles() override;
  size_t GetFrameIndexForImage(const PaintImage& paint_image,
                               WhichTree tree) const override;
  int GetMSAASampleCountForRaster(
      const scoped_refptr<DisplayItemList>& display_list) override;

  // ScrollbarAnimationControllerClient implementation.
  void PostDelayedScrollbarAnimationTask(base::OnceClosure task,
                                         base::TimeDelta delay) override;
  void SetNeedsAnimateForScrollbarAnimation() override;
  void SetNeedsRedrawForScrollbarAnimation() override;
  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override;
  void DidChangeScrollbarVisibility() override;

  // VideoBeginFrameSource implementation.
  void AddVideoFrameController(VideoFrameController* controller) override;
  void RemoveVideoFrameController(VideoFrameController* controller) override;

  // LayerTreeFrameSinkClient implementation.
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  base::Optional<viz::HitTestRegionList> BuildHitTestData() override;
  void DidLoseLayerTreeFrameSink() override;
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override;

  // Called from LayerTreeImpl.
  void OnCanDrawStateChangedForTree();

  // Implementation.
  int id() const { return id_; }
  bool CanDraw() const;
  LayerTreeFrameSink* layer_tree_frame_sink() const {
    return layer_tree_frame_sink_;
  }
  int max_texture_size() const { return max_texture_size_; }
  void ReleaseLayerTreeFrameSink();

  int RequestedMSAASampleCount() const;

  virtual bool InitializeFrameSink(LayerTreeFrameSink* layer_tree_frame_sink);
  TileManager* tile_manager() { return &tile_manager_; }

  void GetGpuRasterizationCapabilities(bool* gpu_rasterization_enabled,
                                       bool* gpu_rasterization_supported,
                                       int* max_msaa_samples,
                                       bool* supports_disable_msaa);
  bool use_gpu_rasterization() const { return use_gpu_rasterization_; }
  bool use_oop_rasterization() const { return use_oop_rasterization_; }

  GpuRasterizationStatus gpu_rasterization_status() const {
    return gpu_rasterization_status_;
  }

  bool create_low_res_tiling() const {
    return settings_.create_low_res_tiling && !use_gpu_rasterization_;
  }
  ResourcePool* resource_pool() { return resource_pool_.get(); }
  ImageDecodeCache* image_decode_cache() { return image_decode_cache_.get(); }
  ImageAnimationController* image_animation_controller() {
    return &image_animation_controller_;
  }

  uint32_t next_frame_token() const { return *next_frame_token_; }

  // Buffers |callback| until a relevant frame swap ocurrs, at which point the
  // callback will be posted to run on the main thread. A frame swap is
  // considered relevant if the swapped frame's token is greater than or equal
  // to |frame_token|.
  void RegisterMainThreadPresentationTimeCallback(
      uint32_t frame_token,
      LayerTreeHost::PresentationTimeCallback callback);

  // Buffers |callback| until a relevant frame swap ocurrs, at which point the
  // callback will be run on the compositor thread. A frame swap is considered
  // relevant if the swapped frame's token is greater than or equal to
  // |frame_token|.
  void RegisterCompositorPresentationTimeCallback(
      uint32_t frame_token,
      LayerTreeHost::PresentationTimeCallback callback);

  virtual bool WillBeginImplFrame(const viz::BeginFrameArgs& args);
  virtual void DidFinishImplFrame(const viz::BeginFrameArgs& args);
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason);
  void DidModifyTilePriorities();

  LayerTreeImpl* active_tree() { return active_tree_.get(); }
  const LayerTreeImpl* active_tree() const { return active_tree_.get(); }
  LayerTreeImpl* pending_tree() { return pending_tree_.get(); }
  const LayerTreeImpl* pending_tree() const { return pending_tree_.get(); }
  LayerTreeImpl* recycle_tree() { return recycle_tree_.get(); }
  const LayerTreeImpl* recycle_tree() const { return recycle_tree_.get(); }
  // Returns the tree LTH synchronizes with.
  LayerTreeImpl* sync_tree() const {
    return CommitToActiveTree() ? active_tree_.get() : pending_tree_.get();
  }
  virtual void CreatePendingTree();
  virtual void ActivateSyncTree();

  // Shortcuts to layers/nodes on the active tree.
  ScrollNode* InnerViewportScrollNode() const;
  ScrollNode* OuterViewportScrollNode() const;
  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;

  bool scroll_affects_scroll_handler() const {
    return settings_.enable_synchronized_scrolling &&
           scroll_affects_scroll_handler_;
  }
  void QueueSwapPromiseForMainThreadScrollUpdate(
      std::unique_ptr<SwapPromise> swap_promise);

  // Returns true if there is an active scroll in progress.  "Active" here
  // means that it's been latched (i.e. we have a CurrentlyScrollingNode()) but
  // also that some ScrollUpdates have been received and their delta consumed
  // for scrolling. These can differ significantly e.g. the page allows the
  // touchstart but preventDefaults all the touchmoves. In that case, we latch
  // and have a CurrentlyScrollingNode() but will never receive a ScrollUpdate.
  //
  // "Precision" means it's a non-animated scroll like a touchscreen or
  // high-precision touchpad. The latter distinction is important for things
  // like scheduling decisions which might schedule a wheel and a touch
  // scrolling differently due to user perception.
  bool IsActivelyPrecisionScrolling() const;

  virtual void SetVisible(bool visible);
  bool visible() const { return visible_; }

  bool IsAnimatingForSnap() const;

  void SetNeedsOneBeginImplFrame();
  void SetNeedsRedraw();

  ManagedMemoryPolicy ActualManagedMemoryPolicy() const;

  const gfx::Transform& DrawTransform() const;

  std::unique_ptr<ScrollAndScaleSet> ProcessScrollDeltas();
  DroppedFrameCounter* dropped_frame_counter() {
    return &dropped_frame_counter_;
  }
  MemoryHistory* memory_history() { return memory_history_.get(); }
  DebugRectHistory* debug_rect_history() { return debug_rect_history_.get(); }
  viz::ClientResourceProvider* resource_provider() {
    return &resource_provider_;
  }
  BrowserControlsOffsetManager* browser_controls_manager() {
    return browser_controls_offset_manager_.get();
  }
  const GlobalStateThatImpactsTilePriority& global_tile_state() {
    return global_tile_state_;
  }

  TaskRunnerProvider* task_runner_provider() const {
    return task_runner_provider_;
  }

  MutatorHost* mutator_host() const { return mutator_host_.get(); }
  ScrollbarController* scrollbar_controller_for_testing() const {
    return scrollbar_controller_.get();
  }

  void SetDebugState(const LayerTreeDebugState& new_debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  gfx::Vector2dF accumulated_root_overscroll() const {
    return accumulated_root_overscroll_;
  }

  bool pinch_gesture_active() const {
    return pinch_gesture_active_ || external_pinch_gesture_active_;
  }
  // Used to set the pinch gesture active state when the pinch gesture is
  // handled on another layer tree. In a page with OOPIFs, only the main
  // frame's layer tree directly handles pinch events. But layer trees for
  // sub-frames need to know when pinch gestures are active so they can
  // throttle the re-rastering. This function allows setting this flag on
  // OOPIF layer trees using information sent (initially) from the main-frame.
  void set_external_pinch_gesture_active(bool external_pinch_gesture_active) {
    external_pinch_gesture_active_ = external_pinch_gesture_active;
    // Only one of the flags should ever be true at any given time.
    DCHECK(!pinch_gesture_active_ || !external_pinch_gesture_active_);
  }

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
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId uid);
  // Evict all UI resources. This differs from ClearUIResources in that this
  // will not immediately delete the resources' backing textures.
  void EvictAllUIResources();
  bool EvictedUIResourcesExist() const;

  virtual viz::ResourceId ResourceIdForUIResource(UIResourceId uid) const;

  virtual bool IsUIResourceOpaque(UIResourceId uid) const;

  // This method gets the scroll offset for a regular scroller, or the combined
  // visual and layout offsets of the viewport.
  gfx::ScrollOffset GetVisualScrollOffset(const ScrollNode& scroll_node) const;

  bool GetSnapFlingInfoAndSetAnimatingSnapTarget(
      const gfx::Vector2dF& natural_displacement_in_viewport,
      gfx::Vector2dF* out_initial_position,
      gfx::Vector2dF* out_target_position) override;

  void ScrollEndForSnapFling(bool did_finish) override;

  // Returns the amount of delta that can be applied to scroll_node, taking
  // page scale into account.
  gfx::Vector2dF ComputeScrollDelta(const ScrollNode& scroll_node,
                                    const gfx::Vector2dF& delta);

  // Resolves a delta in the given granularity for the |scroll_node| into
  // physical pixels to scroll.
  gfx::Vector2dF ResolveScrollGranularityToPixels(
      const ScrollNode& scroll_node,
      const gfx::Vector2dF& scroll_delta,
      ui::ScrollGranularity granularity);

  void ScheduleMicroBenchmark(std::unique_ptr<MicroBenchmarkImpl> benchmark);

  viz::CompositorFrameMetadata MakeCompositorFrameMetadata();
  RenderFrameMetadata MakeRenderFrameMetadata(FrameData* frame);

  const gfx::Rect& external_viewport() const { return external_viewport_; }

  // Viewport rect to be used for tiling prioritization instead of the
  // DeviceViewport().
  const gfx::Rect& viewport_rect_for_tile_priority() const {
    return viewport_rect_for_tile_priority_;
  }

  // When a SwapPromiseMonitor is created on the impl thread, it calls
  // InsertSwapPromiseMonitor() to register itself with LayerTreeHostImpl.
  // When the monitor is destroyed, it calls RemoveSwapPromiseMonitor()
  // to unregister itself.
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor);
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor);

  // TODO(weiliangc): Replace RequiresHighResToDraw with scheduler waits for
  // ReadyToDraw. crbug.com/469175
  void SetRequiresHighResToDraw() { requires_high_res_to_draw_ = true; }
  void ResetRequiresHighResToDraw() { requires_high_res_to_draw_ = false; }
  bool RequiresHighResToDraw() const { return requires_high_res_to_draw_; }

  // Only valid for synchronous (non-scheduled) single-threaded case.
  void SynchronouslyInitializeAllTiles();

  bool SupportsImplScrolling() const;
  bool CommitToActiveTree() const;

  // Virtual so tests can inject their own.
  virtual std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider();

  bool prepare_tiles_needed() const { return tile_priorities_dirty_; }

  gfx::Vector2dF ScrollSingleNode(const ScrollNode& scroll_node,
                                  const gfx::Vector2dF& delta,
                                  const gfx::Point& viewport_point,
                                  bool is_direct_manipulation,
                                  ScrollTree* scroll_tree);

  base::SingleThreadTaskRunner* GetTaskRunner() const {
    DCHECK(task_runner_provider_);
    return task_runner_provider_->HasImplThread()
               ? task_runner_provider_->ImplThreadTaskRunner()
               : task_runner_provider_->MainThreadTaskRunner();
  }

  // Return all ScrollNode indices that have an associated layer with a non-fast
  // region that intersects the point.
  base::flat_set<int> NonFastScrollableNodes(
      const gfx::PointF& device_viewport_point) const;

  // Returns true if a scroll offset animation is created and false if we scroll
  // by the desired amount without an animation.
  bool ScrollAnimationCreate(const ScrollNode& scroll_node,
                             const gfx::Vector2dF& scroll_amount,
                             base::TimeDelta delayed_by);
  bool AutoScrollAnimationCreate(const ScrollNode& scroll_node,
                                 const gfx::Vector2dF& scroll_amount,
                                 float autoscroll_velocity);

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter);
  PaintWorkletLayerPainter* GetPaintWorkletLayerPainterForTesting() const {
    return paint_worklet_painter_.get();
  }

  void QueueImageDecode(int request_id, const PaintImage& image);
  std::vector<std::pair<int, bool>> TakeCompletedImageDecodeRequests();
  // Returns mutator events to be handled by BeginMainFrame.
  std::unique_ptr<MutatorEvents> TakeMutatorEvents();

  void ClearCaches();

  bool CanConsumeDelta(const ScrollState& scroll_state,
                       const ScrollNode& scroll_node);

  void UpdateImageDecodingHints(
      base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
          decoding_mode_map);

  void InitializeUkm(std::unique_ptr<ukm::UkmRecorder> recorder);
  UkmManager* ukm_manager() { return ukm_manager_.get(); }

  ActiveFrameSequenceTrackers FrameSequenceTrackerActiveTypes() {
    return frame_trackers_.FrameSequenceTrackerActiveTypes();
  }

  void RenewTreePriorityForTesting() { client_->RenewTreePriority(); }

  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer);

  void SetActiveURL(const GURL& url, ukm::SourceId source_id);

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

  bool can_use_msaa() const { return can_use_msaa_; }

 protected:
  LayerTreeHostImpl(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client);

  // Virtual for testing.
  virtual bool AnimateLayers(base::TimeTicks monotonic_time,
                             bool is_active_tree);

  bool is_likely_to_require_a_draw() const {
    return is_likely_to_require_a_draw_;
  }

  // Removes empty or orphan RenderPasses from the frame.
  static void RemoveRenderPasses(FrameData* frame);

  LayerTreeHostImplClient* const client_;
  LayerTreeHostSchedulingClient* const scheduling_client_;
  TaskRunnerProvider* const task_runner_provider_;

  BeginFrameTracker current_begin_frame_tracker_;

  std::unique_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;

 private:
  void CollectScrollDeltas(ScrollAndScaleSet* scroll_info);
  void CollectScrollbarUpdates(ScrollAndScaleSet* scroll_info) const;

  // Returns the ScrollNode we should use to scroll, accounting for viewport
  // scroll chaining rules.
  ScrollNode* GetNodeToScroll(ScrollNode* node) const;

  // Determines whether the given scroll node can scroll on the compositor
  // thread or if there are any reasons it must be scrolled on the main thread
  // or not at all. Note: in general, this is not sufficient to determine if a
  // scroll can occur on the compositor thread. If hit testing to a scroll
  // node, the caller must also check whether the hit point intersects a
  // non-fast-scrolling-region of any ancestor scrolling layers. Can be removed
  // after scroll unification https://crbug.com/476553.
  InputHandler::ScrollStatus TryScroll(const ScrollTree& scroll_tree,
                                       ScrollNode* scroll_node) const;

  // Transforms viewport start point and scroll delta to local start point and
  // local delta, respectively. If the transformation of either the start or end
  // point of a scroll is clipped, the function returns false.
  bool CalculateLocalScrollDeltaAndStartPoint(
      const ScrollNode& scroll_node,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta,
      const ScrollTree& scroll_tree,
      gfx::Vector2dF* out_local_scroll_delta,
      gfx::PointF* out_local_start_point = nullptr);
  gfx::Vector2dF ScrollNodeWithViewportSpaceDelta(
      const ScrollNode& scroll_node,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta,
      ScrollTree* scroll_tree);
  bool ScrollAnimationCreateInternal(const ScrollNode& scroll_node,
                                     const gfx::Vector2dF& delta,
                                     base::TimeDelta delayed_by,
                                     base::Optional<float> autoscroll_velocity);

  void CleanUpTileManagerResources();
  void CreateTileManagerResources();
  void ReleaseTreeResources();
  void ReleaseTileResources();
  void RecreateTileResources();

  void AnimateInternal();

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

  // Returns true if status changed.
  bool UpdateGpuRasterizationStatus();
  void UpdateTreeResourcesForGpuRasterizationIfNeeded();

  Viewport& viewport() const { return *viewport_.get(); }

  bool IsTouchDraggingScrollbar(
      LayerImpl* first_scrolling_layer_or_drawn_scrollbar,
      ui::ScrollInputType type);

  // |layer| is returned from a regular hit test, and
  // |first_scrolling_layer_or_drawn_scrollbar| is returned from a hit test
  // performed only on scrollers and scrollbars. Initial scroll hit testing can
  // be unreliable if the latter is not the direct scroll ancestor of the
  // former. In this case, we will fall back to main thread scrolling because
  // the compositor thread doesn't know which layer to scroll. This happens when
  // a layer covers a scroller that doesn't scroll the former, or a scroller is
  // masked by a mask layer for mask image, clip-path, rounded border, etc.
  //
  // Note, position: fixed layers use the inner viewport as their ScrollNode
  // (since they don't scroll with the outer viewport), however, scrolls from
  // the fixed layer still chain to the outer viewport. It's also possible for a
  // node to have the inner viewport as its ancestor without going through the
  // outer viewport; however, it may still scroll using the viewport(). Hence,
  // this method must use the same scroll chaining logic we use in ApplyScroll.
  bool IsInitialScrollHitTestReliable(
      LayerImpl* layer,
      LayerImpl* first_scrolling_layer_or_drawn_scrollbar) const;

  // Given a starting node (determined by hit-test), walks up the scroll tree
  // looking for the first node that can consume scroll from the given
  // scroll_state and returns the first such node. If none is found, or if
  // starting_node is nullptr, returns nullptr;
  ScrollNode* FindNodeToLatch(ScrollState* scroll_state,
                              ScrollNode* starting_node,
                              ui::ScrollInputType type);

  // Called during ScrollBegin once a scroller was successfully latched to
  // (i.e.  it can and will consume scroll delta on the compositor thread). The
  // latched scroller is now available in CurrentlyScrollingNode().
  // TODO(bokan): There's some debate about the name of this method. We should
  // get consensus on terminology to use and apply it consistently.
  // https://crrev.com/c/1981336/9/cc/trees/layer_tree_host_impl.cc#4520
  void DidLatchToScroller(const ScrollState& scroll_state,
                          ui::ScrollInputType type);

  // Applies the scroll_state to the currently latched scroller. See comment in
  // InputHandler::ScrollUpdate declaration for the meaning of |delayed_by|.
  void ScrollLatchedScroller(ScrollState* scroll_state,
                             base::TimeDelta delayed_by);

  bool ShouldAnimateScroll(const ScrollState& scroll_state) const;

  bool AnimatePageScale(base::TimeTicks monotonic_time);
  bool AnimateScrollbars(base::TimeTicks monotonic_time);
  bool AnimateBrowserControls(base::TimeTicks monotonic_time);

  void UpdateTileManagerMemoryPolicy(const ManagedMemoryPolicy& policy);

  // Returns true if the damage rect is non-empty. This check includes damage
  // from the HUD. Should only be called when the active tree's draw properties
  // are valid and after updating the damage.
  bool HasDamage() const;

  // This function should only be called from PrepareToDraw, as DidDrawAllLayers
  // must be called if this helper function is called.  Returns DRAW_SUCCESS if
  // the frame should be drawn.
  DrawResult CalculateRenderPasses(FrameData* frame);

  void ClearCurrentlyScrollingNode();

  // Performs a hit test to determine the ScrollNode to use when scrolling at
  // |viewport_point|. If no layer is hit, this falls back to the inner
  // viewport scroll node. Returns:
  // - If |hit_test_sucessful| is false, hit testing has failed and the
  //   compositor cannot determine the correct scroll node (e.g. see comments in
  //   IsInitialScrollHitTestReliable). |scroll_node| is always nullptr in this
  //   case.
  // - If |hit_test_successful| is true, returns the ScrollNode to use in
  //   |scroll_node|. This can be nullptr if no layer was hit and there are no
  //   viewport nodes (e.g. OOPIF, UI compositor).
  struct ScrollHitTestResult {
    ScrollNode* scroll_node;
    bool hit_test_successful;
  };
  ScrollHitTestResult HitTestScrollNode(
      const gfx::PointF& device_viewport_point) const;

  // Similar to above but includes complicated logic to determine whether the
  // ScrollNode is able to be scrolled on the compositor or requires main
  // thread scrolling. If main thread scrolling is required
  // |scroll_on_main_thread| is set to true and the reason is given in
  // |main_thread_scrolling_reason| to on of the enum values in
  // main_thread_scrolling_reason.h. Can be removed after scroll unification
  // https://crbug.com/476553.
  ScrollNode* FindScrollNodeForCompositedScrolling(
      const gfx::PointF& device_viewport_point,
      LayerImpl* layer_hit_by_point,
      bool* scroll_on_main_thread,
      uint32_t* main_thread_scrolling_reason) const;

  void StartScrollbarFadeRecursive(LayerImpl* layer);
  void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy);

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

  void NotifySwapPromiseMonitorsOfSetNeedsRedraw();

  void UpdateRootLayerStateForSynchronousInputHandler();

  bool ScrollAnimationUpdateTarget(const ScrollNode& scroll_node,
                                   const gfx::Vector2dF& scroll_delta,
                                   base::TimeDelta delayed_by);

  // Creates an animation curve and returns true if we need to update the
  // scroll position to a snap point. Otherwise returns false.
  bool SnapAtScrollEnd();

  void SetContextVisibility(bool is_visible);
  void ImageDecodeFinished(int request_id, bool decode_succeeded);

  // This function keeps track of sources of scrolls that are handled in the
  // compositor side. The information gets shared by the main thread as part of
  // the begin_main_frame_state. Finally Use counters are updated in the main
  // thread side to keep track of the frequency of scrolling with different
  // sources per page load. TODO(crbug.com/691886): Use GRC API to plumb the
  // scroll source info for Use Counters.
  void UpdateScrollSourceInfo(const ScrollState& scroll_state,
                              ui::ScrollInputType type);

  bool IsScrolledBy(LayerImpl* child, ScrollNode* ancestor);
  void ShowScrollbarsForImplScroll(ElementId element_id);

  // Copy any opacity values already in the active tree to the pending
  // tree, because the active tree value always takes precedence for scrollbars.
  void PushScrollbarOpacitiesFromActiveToPending();

  // Pushes state for image animations and checkerboarded images from the
  // pending to active tree. This is called during activation when a pending
  // tree exists, and during the commit if we are committing directly to the
  // active tree.
  void ActivateStateForImages();

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  void AllocateLocalSurfaceId();

  const LayerTreeSettings settings_;

  // This is set to true only if:
  //  . The compositor is running single-threaded (i.e. there is no separate
  //    compositor/impl thread).
  //  . There is no scheduler (which means layer-update, composite, etc. steps
  //    happen explicitly via. synchronous calls to appropriate functions).
  // This is usually turned on only in some tests (e.g. web-tests).
  const bool is_synchronous_single_threaded_;

  viz::ClientResourceProvider resource_provider_;

  std::unordered_map<UIResourceId, UIResourceData> ui_resource_map_;
  // UIResources are held here once requested to be deleted until they are
  // released from the display compositor, then the backing can be deleted.
  std::unordered_map<UIResourceId, UIResourceData> deleted_ui_resources_;
  // Resources that were evicted by EvictAllUIResources. Resources are removed
  // from this when they are touched by a create or destroy from the UI resource
  // request queue. The resource IDs held in here do not have any backing
  // associated with them anymore, as that is freed at the time of eviction.
  std::set<UIResourceId> evicted_ui_resources_;

  // These are valid when has_valid_layer_tree_frame_sink_ is true.
  //
  // A pointer used for communicating with and submitting output to the display
  // compositor.
  LayerTreeFrameSink* layer_tree_frame_sink_ = nullptr;
  // The maximum size (either width or height) that any texture can be. Also
  // holds a reasonable value for software compositing bitmaps.
  int max_texture_size_ = 0;

  // The following scoped variables must not outlive the
  // |layer_tree_frame_sink_|.
  // These should be transfered to viz::ContextCacheController's
  // ClientBecameNotVisible() before the output surface is destroyed.
  std::unique_ptr<viz::ContextCacheController::ScopedVisibility>
      compositor_context_visibility_;
  std::unique_ptr<viz::ContextCacheController::ScopedVisibility>
      worker_context_visibility_;

  bool can_use_msaa_ = false;
  bool supports_disable_msaa_ = false;

  bool need_update_gpu_rasterization_status_ = false;
  bool use_gpu_rasterization_ = false;
  bool use_oop_rasterization_ = false;
  GpuRasterizationStatus gpu_rasterization_status_ =
      GpuRasterizationStatus::OFF_DEVICE;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<ImageDecodeCache> image_decode_cache_;

  GlobalStateThatImpactsTilePriority global_tile_state_;

  // Tree currently being drawn.
  std::unique_ptr<LayerTreeImpl> active_tree_;

  // In impl-side painting mode, tree with possibly incomplete rasterized
  // content. May be promoted to active by ActivateSyncTree().
  std::unique_ptr<LayerTreeImpl> pending_tree_;

  // In impl-side painting mode, inert tree with layers that can be recycled
  // by the next sync from the main thread.
  std::unique_ptr<LayerTreeImpl> recycle_tree_;

  InputHandlerClient* input_handler_client_ = nullptr;

  // This is used to tell the scheduler there are active scroll handlers on the
  // page so we should prioritize latency during a scroll to try to keep
  // scroll-linked effects up to data.
  // TODO(bokan): This is quite old and scheduling has become much more
  // sophisticated since so it's not clear how much value it's still providing.
  bool scroll_affects_scroll_handler_ = false;

  ElementId scroll_element_id_mouse_currently_over_;
  ElementId scroll_element_id_mouse_currently_captured_;

  // Tracks, for debugging purposes, the amount of scroll received (not
  // necessarily applied) in this compositor frame. This will be reset each
  // time a CompositorFrame is generated.
  gfx::Vector2dF scroll_accumulated_this_frame_;

  // Tracks the last scroll update/begin state received. Used to infer the most
  // recent scroll type and direction.
  base::Optional<ScrollState> last_scroll_begin_state_;
  base::Optional<ScrollState> last_scroll_update_state_;

  std::vector<std::unique_ptr<SwapPromise>>
      swap_promises_for_main_thread_scroll_update_;

  // An object to implement the ScrollElasticityHelper interface and
  // hold all state related to elasticity. May be NULL if never requested.
  std::unique_ptr<ScrollElasticityHelper> scroll_elasticity_helper_;

  bool tile_priorities_dirty_ = false;

  LayerTreeDebugState debug_state_;
  bool visible_ = false;
  ManagedMemoryPolicy cached_managed_memory_policy_;

  TileManager tile_manager_;

  gfx::Vector2dF accumulated_root_overscroll_;

  // Unconsumed scroll delta sent to the main thread for firing overscroll DOM
  // events. Resets after each commit.
  gfx::Vector2dF overscroll_delta_for_main_thread_;

  // True iff some of the delta has been consumed for the current scroll
  // sequence on the specific axis.
  bool did_scroll_x_for_scroll_gesture_;
  bool did_scroll_y_for_scroll_gesture_;

  // This value is used to allow the compositor to throttle re-rastering during
  // pinch gestures, when the page scale factor may be changing frequently. It
  // is set in one of two ways:
  // i) In a layer tree serving the root of the frame/compositor tree, it is
  // directly set during processing of GesturePinch events on the impl thread
  // (only the root layer tree has access to these).
  // ii) In a layer tree serving a sub-frame in the frame/compositor tree, it
  // is set from the main thread during the commit process, using information
  // sent from the root layer tree via IPC messaging.
  bool pinch_gesture_active_ = false;
  bool external_pinch_gesture_active_ = false;
  bool pinch_gesture_end_should_clear_scrolling_node_ = false;

  std::unique_ptr<BrowserControlsOffsetManager>
      browser_controls_offset_manager_;

  std::unique_ptr<PageScaleAnimation> page_scale_animation_;

  DroppedFrameCounter dropped_frame_counter_;
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

  std::unique_ptr<MutatorHost> mutator_host_;
  std::unique_ptr<MutatorEvents> mutator_events_;
  std::set<VideoFrameController*> video_frame_controllers_;

  // Map from scroll element ID to scrollbar animation controller.
  // There is one animation controller per pair of overlay scrollbars.
  std::unordered_map<ElementId,
                     std::unique_ptr<ScrollbarAnimationController>,
                     ElementIdHash>
      scrollbar_animation_controllers_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;
  MicroBenchmarkControllerImpl micro_benchmark_controller_;
  std::unique_ptr<SynchronousTaskGraphRunner>
      single_thread_synchronous_task_graph_runner_;

  // Optional callback to notify of new tree activations.
  base::RepeatingClosure tree_activation_callback_;

  TaskGraphRunner* task_graph_runner_;
  int id_;

  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  bool requires_high_res_to_draw_ = false;
  bool is_likely_to_require_a_draw_ = false;

  // TODO(danakj): Delete the LayerTreeFrameSink and all resources when
  // it's lost instead of having this bool.
  bool has_valid_layer_tree_frame_sink_ = false;

  // If it is enabled in the LayerTreeSettings, we can check damage in
  // WillBeginImplFrame and abort early if there is no damage. We only check
  // damage in WillBeginImplFrame if a recent frame had no damage. We keep
  // track of this with |consecutive_frame_with_damage_count_|.
  int consecutive_frame_with_damage_count_;

  std::unique_ptr<Viewport> viewport_;

  std::unique_ptr<PendingTreeRasterDurationHistogramTimer>
      pending_tree_raster_duration_timer_;

  // If a scroll snap is being animated, then the value of this will be the
  // element id(s) of the target(s). Otherwise, the ids will be invalid.
  // At the end of a scroll animation, the target should be set as the scroll
  // node's snap target.
  TargetSnapAreaElementIds scroll_animating_snap_target_ids_;

  // A set of elements that scroll-snapped to a new target since the last
  // begin main frame. The snap target ids of these elements will be sent to
  // the main thread in the next begin main frame.
  base::flat_set<ElementId> updated_snapped_elements_;

  // These completion states to be transfered to the main thread when we
  // begin main frame. The pair represents a request id and the completion (ie
  // success) state.
  std::vector<std::pair<int, bool>> completed_image_decode_requests_;

  // These are used to transfer usage of different types of scrolling to the
  // main thread.
  bool has_scrolled_by_wheel_ = false;
  bool has_scrolled_by_touch_ = false;
  bool has_scrolled_by_precisiontouchpad_ = false;
  bool has_pinch_zoomed_ = false;

  ImplThreadPhase impl_thread_phase_ = ImplThreadPhase::IDLE;

  ImageAnimationController image_animation_controller_;

  std::unique_ptr<UkmManager> ukm_manager_;

  // Provides RenderFrameMetadata to the Browser process upon the submission of
  // each CompositorFrame.
  std::unique_ptr<RenderFrameMetadataObserver> render_frame_metadata_observer_;

  viz::FrameTokenGenerator next_frame_token_;

  viz::LocalSurfaceIdAllocation last_draw_local_surface_id_allocation_;
  base::flat_set<viz::SurfaceRange> last_draw_referenced_surfaces_;
  base::Optional<RenderFrameMetadata> last_draw_render_frame_metadata_;
  viz::ChildLocalSurfaceIdAllocator child_local_surface_id_allocator_;

  // Indicates the direction of the last vertical scroll of the root layer.
  // Until the first vertical scroll occurs, this value is |kNull|. Note that
  // once this value is updated, it will never return to |kNull|.
  viz::VerticalScrollDirection last_vertical_scroll_direction_ =
      viz::VerticalScrollDirection::kNull;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  PresentationTimeCallbackBuffer presentation_time_callbacks_;

  const PaintImage::GeneratorClientId paint_image_generator_client_id_;

  // Manages composited scrollbar hit testing.
  std::unique_ptr<ScrollbarController> scrollbar_controller_;

  FrameSequenceTrackerCollection frame_trackers_;

  // Set to true when a scroll gesture being handled on the compositor has
  // ended. i.e. When a GSE has arrived and any ongoing scroll animation has
  // ended.
  bool scroll_gesture_did_end_;

  // Set in ScrollBegin and outlives the currently scrolling node so it can be
  // used to send the scrollend and overscroll DOM events from the main thread
  // when scrolling occurs on the compositor thread. This value is cleared at
  // the first commit after a GSE.
  ElementId last_latched_scroller_;

  // The source device type that started the scroll gesture. Only set between a
  // ScrollBegin and ScrollEnd.
  base::Optional<ui::ScrollInputType> latched_scroll_type_;

  // Scroll animation can finish either before or after GSE arrival.
  // deferred_scroll_end_ is set when the GSE has arrvied before scroll
  // animation completion. ScrollEnd will get called once the animation is
  // over.
  bool deferred_scroll_end_ = false;

  // TODO(bokan): Mac doesn't yet have smooth scrolling for wheel; however, to
  // allow consistency in tests we use this bit to override that decision.
  // https://crbug.com/574283.
  bool force_smooth_wheel_scrolling_for_testing_ = false;

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

  // Provides support for PaintWorklets which depend on input properties that
  // are being animated by the compositor (aka 'animated' PaintWorklets).
  // Responsible for storing animated custom property values and for
  // invalidating PaintWorklets as the property values change.
  AnimatedPaintWorkletTracker paint_worklet_tracker_;

  // Helper for de-jelly logic.
  DeJellyState de_jelly_state_;

  EventsMetricsManager events_metrics_manager_;

  std::unique_ptr<LCDTextMetricsReporter> lcd_text_metrics_reporter_;

  FrameRateEstimator frame_rate_estimator_;
  bool has_observed_first_scroll_delay_ = false;

  // Must be the last member to ensure this is destroyed first in the
  // destruction order and invalidates all weak pointers.
  base::WeakPtrFactory<LayerTreeHostImpl> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_H_
