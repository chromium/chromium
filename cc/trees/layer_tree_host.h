// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/paint/node_id.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/target_property.h"
#include "cc/trees/viewport_layers.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
struct PresentationFeedback;
}

namespace cc {

class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class LayerTreeHostSingleThreadClient;
class LayerTreeMutator;
class PaintWorkletLayerPainter;
class MutatorEvents;
class MutatorHost;
struct PendingPageScaleAnimation;
class RenderFrameMetadataObserver;
class RenderingStatsInstrumentation;
struct OverscrollBehavior;
class TaskGraphRunner;
class UIResourceManager;
class UkmRecorderFactory;
struct RenderingStats;
struct ScrollAndScaleSet;

// Returned from LayerTreeHost::DeferMainFrameUpdate. Automatically un-defers on
// destruction.
class CC_EXPORT ScopedDeferMainFrameUpdate {
 public:
  explicit ScopedDeferMainFrameUpdate(LayerTreeHost* host);
  ~ScopedDeferMainFrameUpdate();

 private:
  base::WeakPtr<LayerTreeHost> host_;
};

class CC_EXPORT LayerTreeHost : public MutatorHostClient {
 public:
  struct CC_EXPORT InitParams {
    InitParams();
    ~InitParams();

    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&);

    LayerTreeHostClient* client = nullptr;
    LayerTreeHostSchedulingClient* scheduling_client = nullptr;
    TaskGraphRunner* task_graph_runner = nullptr;
    LayerTreeSettings const* settings = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
    MutatorHost* mutator_host = nullptr;

    // The image worker task runner is used to schedule image decodes. The
    // compositor thread may make sync calls to this thread, analogous to the
    // raster worker threads.
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner;

    std::unique_ptr<UkmRecorderFactory> ukm_recorder_factory;
  };

  // Constructs a LayerTreeHost with a compositor thread where scrolling and
  // animation take place. This is used for the web compositor in the renderer
  // process to move work off the main thread which javascript can dominate.
  static std::unique_ptr<LayerTreeHost> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams params);

  // Constructs a LayerTreeHost without a separate compositor thread, but which
  // behaves and looks the same as a threaded compositor externally, with the
  // exception of the additional client interface. This is used in other cases
  // where the main thread creating this instance can be expected to not become
  // blocked, so moving work to another thread and the overhead it adds are not
  // required.
  static std::unique_ptr<LayerTreeHost> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams params);

  LayerTreeHost(const LayerTreeHost&) = delete;
  virtual ~LayerTreeHost();

  LayerTreeHost& operator=(const LayerTreeHost&) = delete;

  // Returns the process global unique identifier for this LayerTreeHost.
  int GetId() const;

  // The current source frame number. This is incremented for each main frame
  // update(commit) pushed to the compositor thread. The initial frame number
  // is 0, and it is incremented once commit is completed (which is before the
  // compositor-thread-side submits its frame for the commit).
  int SourceFrameNumber() const;

  // Returns the UIResourceManager used to create UIResources for
  // UIResourceLayers pushed to the LayerTree.
  UIResourceManager* GetUIResourceManager() const;

  // Returns the TaskRunnerProvider used to access the main and compositor
  // thread task runners.
  TaskRunnerProvider* GetTaskRunnerProvider() const;

  // Returns the settings used by this host. These settings are constants given
  // at startup.
  const LayerTreeSettings& GetSettings() const;

  // Sets the LayerTreeMutator interface used to directly mutate the compositor
  // state on the compositor thread. (Compositor-Worker)
  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  // Sets the LayerTreePainter interface used to dispatch the JS paint callback
  // to a worklet thread.
  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter);

  // Attachs a SwapPromise to the Layer tree, that passes through the
  // LayerTreeHost and LayerTreeHostImpl with the next commit and frame
  // submission, which can be used to observe that progress. This also
  // causes a main frame to be requested.
  // See swap_promise.h for how to use SwapPromise.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Returns the SwapPromiseManager, used to insert SwapPromises dynamically
  // when a main frame is requested.
  SwapPromiseManager* GetSwapPromiseManager();

  // Visibility and LayerTreeFrameSink -------------------------------

  // Sets or gets if the LayerTreeHost is visible. When not visible it will:
  // - Not request a new LayerTreeFrameSink from the client.
  // - Stop submitting frames to the display compositor.
  // - Stop producing main frames and committing them.
  // The LayerTreeHost is not visible when first created, so this must be called
  // to make it visible before it will attempt to start producing output.
  void SetVisible(bool visible);
  bool IsVisible() const;

  // Called in response to a LayerTreeFrameSink request made to the client
  // using LayerTreeHostClient::RequestNewLayerTreeFrameSink. The client will
  // be informed of the LayerTreeFrameSink initialization status using
  // DidInitializaLayerTreeFrameSink or DidFailToInitializeLayerTreeFrameSink.
  // The request is completed when the host successfully initializes an
  // LayerTreeFrameSink.
  void SetLayerTreeFrameSink(
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);

  // Forces the host to immediately release all references to the
  // LayerTreeFrameSink, if any. Can be safely called any time, but the
  // compositor should not be visible.
  std::unique_ptr<LayerTreeFrameSink> ReleaseLayerTreeFrameSink();

  // Frame Scheduling (main and compositor frames) requests -------

  // Requests a main frame update even if no content has changed. This is used,
  // for instance in the case of RequestAnimationFrame from blink to ensure the
  // main frame update is run on the next tick without pre-emptively forcing a
  // full commit synchronization or layer updates.
  void SetNeedsAnimate();

  // Requests a main frame update and also ensure that the host pulls layer
  // updates from the client, even if no content might have changed, without
  // forcing a full commit synchronization.
  virtual void SetNeedsUpdateLayers();

  // Requests that the next main frame update performs a full commit
  // synchronization.
  virtual void SetNeedsCommit();

  // Returns true after SetNeedsAnimate(), SetNeedsUpdateLayers() or
  // SetNeedsCommit(), until it is satisfied.
  bool RequestedMainFramePendingForTesting();

  // Requests that the next frame re-chooses crisp raster scales for all layers.
  void SetNeedsRecalculateRasterScales();

  // Returns true if a main frame with commit synchronization has been
  // requested.
  bool CommitRequested() const;

  // Prevents the compositor from requesting main frame updates from the client
  // until the ScopedDeferMainFrameUpdate object is destroyed, or
  // StopDeferringCommits is called.
  std::unique_ptr<ScopedDeferMainFrameUpdate> DeferMainFrameUpdate();

  // Notification that the proxy started or stopped deferring main frame updates
  void OnDeferMainFrameUpdatesChanged(bool);

  // Prevents the proxy from committing the layer tree to the compositor,
  // while still allowing main frame lifecycle updates. |timeout|
  // is the interval after which commits will restart if nothing stops
  // deferring sooner. If multiple calls are made to StartDeferringCommits
  // while deferal is active, the first timeout continues to apply.
  void StartDeferringCommits(base::TimeDelta timeout);

  // Stop deferring commits immediately.
  void StopDeferringCommits(PaintHoldingCommitTrigger);

  // Notification that the proxy started or stopped deferring commits.
  void OnDeferCommitsChanged(bool);

  // Returns whether there are any outstanding ScopedDeferMainFrameUpdate,
  // though commits may be deferred also when the local_surface_id_from_parent()
  // is not valid.
  bool defer_main_frame_update() const {
    return defer_main_frame_update_count_;
  }

  // Synchronously performs a main frame update and layer updates. Used only in
  // single threaded mode when the compositor's internal scheduling is disabled.
  void LayoutAndUpdateLayers();

  // Synchronously performs a complete main frame update, commit and compositor
  // frame. Used only in single threaded mode when the compositor's internal
  // scheduling is disabled.
  void Composite(base::TimeTicks frame_begin_time, bool raster);

  // Requests a redraw (compositor frame) for the given rect.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);

  // Requests a main frame (including layer updates) and ensures that this main
  // frame results in a redraw for the complete viewport when producing the
  // CompositorFrame.
  void SetNeedsCommitWithForcedRedraw();

  // Input Handling ---------------------------------------------

  // Sets the state of the browser controls. (Used for URL bar animations).
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate);

  // Returns a reference to the InputHandler used to respond to input events on
  // the compositor thread.
  const base::WeakPtr<InputHandler>& GetInputHandler() const;

  // Debugging and benchmarks ---------------------------------
  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& GetDebugState() const;

  // Returns the id of the benchmark on success, 0 otherwise.
  int ScheduleMicroBenchmark(const std::string& benchmark_name,
                             std::unique_ptr<base::Value> value,
                             MicroBenchmark::DoneCallback callback);

  // Returns true if the message was successfully delivered and handled.
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);

  // When the main thread informs the compositor thread that it is ready to
  // commit, generally it would remain blocked until the main thread state is
  // copied to the pending tree. Calling this would ensure that the main thread
  // remains blocked until the pending tree is activated.
  void SetNextCommitWaitsForActivation();

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen (it's entirely possible some frames may be dropped between
  // the time this is called and the callback is run).
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  void RequestPresentationTimeForNextFrame(PresentationTimeCallback callback);

  // Layer tree accessors and modifiers ------------------------

  // Sets or gets the root of the Layer tree. Children of the root Layer are
  // attached to it and will be added/removed along with the root Layer. The
  // LayerTreeHost retains ownership of a reference to the root Layer.
  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }

  // Sets the root layer which is not managed by blink, and we will initialize
  // its paint properties using PropertyTreeBuilder. For ui::Compositor, because
  // for now we always use PropertyTreeBulder, this function is equivalent to
  // SetRootLayer().
  // TODO(crbug.com/925855): This is temporary. Eventually we should let the
  // caller inform blink about the layer and remove the function.
  void SetNonBlinkManagedRootLayer(scoped_refptr<Layer> root_layer);

  struct ViewportPropertyIds {
    int overscroll_elasticity_transform = TransformTree::kInvalidNodeId;
    int page_scale_transform = TransformTree::kInvalidNodeId;
    int inner_scroll = ScrollTree::kInvalidNodeId;
    int outer_clip = ClipTree::kInvalidNodeId;
    int outer_scroll = ScrollTree::kInvalidNodeId;
  };

  // Sets the collection of viewport property ids, defined to allow viewport
  // pinch-zoom etc. on the compositor thread.
  void RegisterViewportPropertyIds(const ViewportPropertyIds&);

  LayerTreeHost::ViewportPropertyIds ViewportPropertyIdsForTesting() const {
    return viewport_property_ids_;
  }
  Layer* InnerViewportScrollLayerForTesting() const;
  Layer* OuterViewportScrollLayerForTesting() const;

  // Sets or gets the position of touch handles for a text selection. These are
  // submitted to the display compositor along with the Layer tree's contents
  // allowing it to present the selection handles. This is done because the
  // handles are a UI widget above, and not clipped to, the viewport of this
  // LayerTreeHost.
  void RegisterSelection(const LayerSelection& selection);
  const LayerSelection& selection() const { return selection_; }

  // Sets or gets if the client has any scroll event handlers registered. This
  // allows the threaded compositor to prioritize main frames even when
  // servicing a touch scroll on the compositor thread, in order to give the
  // event handler a chance to be part of each frame.
  void SetHaveScrollEventHandlers(bool have_event_handlers);
  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }

  // Set or get what event handlers exist on the layer tree in order to inform
  // the compositor thread if it is able to handle an input event, or it needs
  // to pass it to the main thread to be handled. The class is the type of input
  // event, and for each class there is a properties defining if the compositor
  // thread can handle the event.
  void SetEventListenerProperties(EventListenerClass event_class,
                                  EventListenerProperties event_properties);
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }

  void SetViewportRectAndScale(const gfx::Rect& device_viewport_rect,
                               float device_scale_factor,
                               const viz::LocalSurfaceIdAllocation&
                                   local_surface_id_allocation_from_parent);

  void SetViewportVisibleRect(const gfx::Rect& visible_rect);

  gfx::Rect viewport_visible_rect() const { return viewport_visible_rect_; }

  gfx::Rect device_viewport_rect() const { return device_viewport_rect_; }

  void SetBrowserControlsHeight(float top_height,
                                float bottom_height,
                                bool shrink);
  void SetBrowserControlsShownRatio(float top_ratio, float bottom_ratio);

  void SetOverscrollBehavior(const OverscrollBehavior& overscroll_behavior);
  const OverscrollBehavior& overscroll_behavior() const {
    return overscroll_behavior_;
  }

  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return page_scale_factor_; }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);
  bool HasPendingPageScaleAnimation() const;

  float device_scale_factor() const { return device_scale_factor_; }

  void SetRecordingScaleFactor(float recording_scale_factor);

  float painted_device_scale_factor() const {
    return painted_device_scale_factor_;
  }

  // Clears image caches and resets the scheduling history for the content
  // produced by this host so far.
  void ClearCachesOnNextCommit();

  // If this LayerTreeHost needs a valid viz::LocalSurfaceId then commits will
  // be deferred until a valid viz::LocalSurfaceId is provided.
  void SetLocalSurfaceIdAllocationFromParent(
      const viz::LocalSurfaceIdAllocation&
          local_surface_id_allocation_from_parent);

  const viz::LocalSurfaceIdAllocation& local_surface_id_allocation_from_parent()
      const {
    return local_surface_id_allocation_from_parent_;
  }

  // Requests the allocation of a new LocalSurfaceId on the compositor thread.
  void RequestNewLocalSurfaceId();

  // Returns the current state of the new LocalSurfaceId request and resets
  // the state.
  bool TakeNewLocalSurfaceIdRequest();
  bool new_local_surface_id_request_for_testing() const {
    return new_local_surface_id_request_;
  }

  void SetRasterColorSpace(const gfx::ColorSpace& raster_color_space);
  const gfx::ColorSpace& raster_color_space() const {
    return raster_color_space_;
  }

  // This layer tree may be embedded in a hierarchy that has page scale
  // factor controlled at the top level. We represent that scale here as
  // 'external_page_scale_factor', a value that affects raster scale in the
  // same way that page_scale_factor does, but doesn't affect any geometry
  // calculations.
  void SetExternalPageScaleFactor(float page_scale_factor,
                                  bool is_external_pinch_gesture_active);
  bool is_external_pinch_gesture_active_for_testing() {
    return is_external_pinch_gesture_active_;
  }

  // Requests that we force send RenderFrameMetadata with the next frame.
  void RequestForceSendMetadata() { force_send_metadata_request_ = true; }

  // Returns the state of |force_send_metadata_request_| and resets the
  // variable to false.
  bool TakeForceSendMetadataRequest();

  // Used externally by blink for setting the PropertyTrees when
  // UseLayerLists() is true, which also implies that Slimming Paint
  // v2 is enabled.
  PropertyTrees* property_trees() { return &property_trees_; }
  const PropertyTrees* property_trees() const { return &property_trees_; }

  void SetPropertyTreesForTesting(const PropertyTrees* property_trees);

  void SetNeedsDisplayOnAllLayers();

  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
  Layer* LayerById(int id) const;

  bool PaintContent(const LayerList& update_layer_list);
  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  void SetHasCopyRequest(bool has_copy_request);
  bool has_copy_request() const { return has_copy_request_; }

  void AddSurfaceRange(const viz::SurfaceRange& surface_range);
  void RemoveSurfaceRange(const viz::SurfaceRange& surface_range);
  base::flat_set<viz::SurfaceRange> SurfaceRanges() const;

  // Marks or unmarks a layer are needing PushPropertiesTo in the next commit.
  // These are internal methods, called from the Layer itself when changing a
  // property or completing a PushPropertiesTo.
  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  void ClearLayersThatShouldPushProperties();
  // The current set of all Layers attached to the LayerTreeHost's tree that
  // have been marked as needing PushPropertiesTo in the next commit.
  const base::flat_set<Layer*>& LayersThatShouldPushProperties() {
    return layers_that_should_push_properties_;
  }

  void SetPageScaleFromImplSide(float page_scale);
  void SetElasticOverscrollFromImplSide(gfx::Vector2dF elastic_overscroll);
  gfx::Vector2dF elastic_overscroll() const { return elastic_overscroll_; }

  // Ensures a HUD layer exists if it is needed, and updates the HUD bounds and
  // position. If a HUD layer exists but is no longer needed, it is destroyed.
  void UpdateHudLayer(bool show_hud_info);
  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }
  bool is_hud_layer(const Layer*) const;

  virtual void SetNeedsFullTreeSync();
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  bool needs_surface_ranges_sync() const { return needs_surface_ranges_sync_; }
  void set_needs_surface_ranges_sync(bool needs_surface_ranges_sync) {
    needs_surface_ranges_sync_ = needs_surface_ranges_sync;
  }

  void SetPropertyTreesNeedRebuild();

  void PushPropertyTreesTo(LayerTreeImpl* tree_impl);
  void PushLayerTreePropertiesTo(LayerTreeImpl* tree_impl);
  void PushSurfaceRangesTo(LayerTreeImpl* tree_impl);
  void PushLayerTreeHostPropertiesTo(LayerTreeHostImpl* host_impl);
  void MoveChangeTrackingToLayers(LayerTreeImpl* tree_impl);

  MutatorHost* mutator_host() const { return mutator_host_; }

  // Returns the layer with the given |element_id|. In layer-list mode, only
  // scrollable layers are registered in this map.
  Layer* LayerByElementId(ElementId element_id) const;
  void RegisterElement(ElementId element_id,
                       ElementListType list_type,
                       Layer* layer);
  void UnregisterElement(ElementId element_id, ElementListType list_type);

  void UpdateActiveElements();

  void SetElementIdsForTesting();

  void BuildPropertyTreesForTesting();

  // Layer iterators.
  LayerListIterator begin() const;
  LayerListIterator end() const;
  LayerListReverseIterator rbegin();
  LayerListReverseIterator rend();

  // LayerTreeHost interface to Proxy.
  void WillBeginMainFrame();
  void DidBeginMainFrame();
  void BeginMainFrame(const viz::BeginFrameArgs& args);
  void BeginMainFrameNotExpectedSoon();
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time);
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void RequestMainFrameUpdate(bool report_cc_metrics);
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  void RequestNewLayerTreeFrameSink();
  void DidInitializeLayerTreeFrameSink();
  void DidFailToInitializeLayerTreeFrameSink();
  virtual std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseLayerTreeFrameSink();
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidReceiveCompositorFrameAck() {
    client_->DidReceiveCompositorFrameAck();
  }
  bool UpdateLayers();
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const gfx::PresentationFeedback& feedback);
  // Called when the compositor completed page scale animation.
  void DidCompletePageScaleAnimation();
  void ApplyScrollAndScale(ScrollAndScaleSet* info);
  void RecordStartOfFrameMetrics();
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time);

  LayerTreeHostClient* client() { return client_; }
  LayerTreeHostSchedulingClient* scheduling_client() {
    return scheduling_client_;
  }

  bool gpu_rasterization_histogram_recorded() const {
    return gpu_rasterization_histogram_recorded_;
  }

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events);

  Proxy* proxy() const { return proxy_.get(); }

  bool IsSingleThreaded() const;
  bool IsThreaded() const;

  // Indicates whether this host is configured to use layer lists
  // rather than layer trees. This also implies that property trees
  // are always already built and so cc doesn't have to build them.
  bool IsUsingLayerLists() const;

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
      PaintWorkletInput::PropertyValue custom_property_value) override {}

  void ScrollOffsetAnimationFinished() override {}
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override {}

  void QueueImageDecode(const PaintImage& image,
                        base::OnceCallback<void(bool)> callback);
  void ImageDecodesFinished(const std::vector<std::pair<int, bool>>& results);

  void RequestBeginMainFrameNotExpected(bool new_state);

  float recording_scale_factor() const { return recording_scale_factor_; }

  void SetSourceURL(ukm::SourceId source_id, const GURL& url);

  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer);

  std::string LayersAsString() const;

  // Captures the on-screen text content, if success, fills the associated
  // NodeId in |content| and return true, otherwise return false.
  bool CaptureContent(std::vector<NodeId>* content);

  std::unique_ptr<BeginMainFrameMetrics> begin_main_frame_metrics() {
    return std::move(begin_main_frame_metrics_);
  }

 protected:
  LayerTreeHost(InitParams params, CompositorMode mode);

  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing);
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);
  void SetUIResourceManagerForTesting(
      std::unique_ptr<UIResourceManager> ui_resource_manager);

  // task_graph_runner() returns a valid value only until the LayerTreeHostImpl
  // is created in CreateLayerTreeHostImpl().
  TaskGraphRunner* task_graph_runner() const { return task_graph_runner_; }

  void OnCommitForSwapPromises();

  void RecordGpuRasterizationHistogram(const LayerTreeHostImpl* host_impl);

  MicroBenchmarkController micro_benchmark_controller_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;

  scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner_;
  std::unique_ptr<UkmRecorderFactory> ukm_recorder_factory_;

 private:
  friend class LayerTreeHostSerializationTest;
  friend class ScopedDeferMainFrameUpdate;

  // This is the number of consecutive frames in which we want the content to be
  // free of slow-paths before toggling the flag.
  enum { kNumFramesToConsiderBeforeRemovingSlowPathFlag = 60 };

  void ApplyViewportChanges(const ScrollAndScaleSet& info);
  void RecordManipulationTypeCounts(const ScrollAndScaleSet& scroll_info);
  void SendOverscrollAndScrollEndEventsFromImplSide(
      const ScrollAndScaleSet& info);
  void ApplyPageScaleDeltaFromImplSide(float page_scale_delta);
  void InitializeProxy(std::unique_ptr<Proxy> proxy);

  bool DoUpdateLayers();

  void UpdateDeferMainFrameUpdateInternal();

  const CompositorMode compositor_mode_;

  std::unique_ptr<UIResourceManager> ui_resource_manager_;

  LayerTreeHostClient* client_;
  LayerTreeHostSchedulingClient* scheduling_client_;
  std::unique_ptr<Proxy> proxy_;
  std::unique_ptr<TaskRunnerProvider> task_runner_provider_;

  int source_frame_number_ = 0U;
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  SwapPromiseManager swap_promise_manager_;

  // |current_layer_tree_frame_sink_| can't be updated until we've successfully
  // initialized a new LayerTreeFrameSink. |new_layer_tree_frame_sink_|
  // contains the new LayerTreeFrameSink that is currently being initialized.
  // If initialization is successful then |new_layer_tree_frame_sink_| replaces
  // |current_layer_tree_frame_sink_|.
  std::unique_ptr<LayerTreeFrameSink> new_layer_tree_frame_sink_;
  std::unique_ptr<LayerTreeFrameSink> current_layer_tree_frame_sink_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  bool visible_ = false;

  bool gpu_rasterization_histogram_recorded_ = false;

  // If set, then page scale animation has completed, but the client hasn't been
  // notified about it yet.
  bool did_complete_scale_animation_ = false;

  int id_;
  bool next_commit_forces_redraw_ = false;
  bool next_commit_forces_recalculate_raster_scales_ = false;
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/654672
  bool inside_main_frame_ = false;

  TaskGraphRunner* task_graph_runner_;

  uint32_t num_consecutive_frames_without_slow_paths_ = 0;

  scoped_refptr<Layer> root_layer_;

  ViewportPropertyIds viewport_property_ids_;

  float top_controls_height_ = 0.f;
  float top_controls_shown_ratio_ = 0.f;
  bool browser_controls_shrink_blink_size_ = false;
  OverscrollBehavior overscroll_behavior_;

  float bottom_controls_height_ = 0.f;
  float bottom_controls_shown_ratio_ = 0.f;

  float device_scale_factor_ = 1.f;
  float painted_device_scale_factor_ = 1.f;
  float recording_scale_factor_ = 1.f;
  float page_scale_factor_ = 1.f;
  float min_page_scale_factor_ = 1.f;
  float max_page_scale_factor_ = 1.f;
  float external_page_scale_factor_ = 1.f;
  bool is_external_pinch_gesture_active_ = false;
  // Used to track the out-bound state for ApplyViewportChanges.
  bool is_pinch_gesture_active_from_impl_ = false;

  int raster_color_space_id_ = -1;
  gfx::ColorSpace raster_color_space_;

  bool clear_caches_on_next_commit_ = false;
  viz::LocalSurfaceIdAllocation local_surface_id_allocation_from_parent_;
  // Used to detect surface invariant violations.
  bool has_pushed_local_surface_id_from_parent_ = false;
  bool new_local_surface_id_request_ = false;
  uint32_t defer_main_frame_update_count_ = 0;

  SkColor background_color_ = SK_ColorWHITE;

  LayerSelection selection_;

  gfx::Rect device_viewport_rect_;

  gfx::Rect viewport_visible_rect_;

  bool have_scroll_event_handlers_ = false;
  EventListenerProperties event_listener_properties_
      [static_cast<size_t>(EventListenerClass::kLast) + 1];

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  // Whether we have a pending request to force send RenderFrameMetadata with
  // the next frame.
  bool force_send_metadata_request_ = false;

  PropertyTrees property_trees_;

  bool needs_full_tree_sync_ = true;

  bool needs_surface_ranges_sync_ = false;

  gfx::Vector2dF elastic_overscroll_;

  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  // The number of SurfaceLayers that have (fallback,primary) set to
  // viz::SurfaceRange.
  base::flat_map<viz::SurfaceRange, int> surface_ranges_;

  // Set of layers that need to push properties.
  base::flat_set<Layer*> layers_that_should_push_properties_;

  // Layer id to Layer map.
  std::unordered_map<int, Layer*> layer_id_map_;

  // In layer-list mode, this map is only used for scrollable layers.
  std::unordered_map<ElementId, Layer*, ElementIdHash> element_layers_map_;

  bool in_paint_layer_contents_ = false;

  // This is true if atleast one layer in the layer tree has a copy request. We
  // use this bool to decide whether we need to compute subtree has copy request
  // for every layer during property tree building.
  bool has_copy_request_ = false;

  // When settings_.use_layer_lists is true, paint properties are generated by
  // blink and we don't use PropertyTreeBuilder, except that the root layer
  // is set by SetNonBlinkManagedRootLayer().
  // TODO(crbug.com/925855): Remove this field when removing
  // SetNonBlinkManagedRootLayer().
  bool force_use_property_tree_builder_ = false;

  MutatorHost* mutator_host_;

  std::vector<std::pair<PaintImage, base::OnceCallback<void(bool)>>>
      queued_image_decodes_;
  std::unordered_map<int, base::OnceCallback<void(bool)>>
      pending_image_decodes_;

  // Presentation time callbacks requested for the next frame are initially
  // added here.
  std::vector<PresentationTimeCallback> pending_presentation_time_callbacks_;

  // Latency information for work done in ProxyMain::BeginMainFrame. The
  // unique_ptr is allocated in RequestMainFrameUpdate, and passed to Blink's
  // LocalFrameView that fills in the fields. This object adds the timing for
  // UpdateLayers. CC reads the data during commit, and clears the unique_ptr.
  std::unique_ptr<BeginMainFrameMetrics> begin_main_frame_metrics_;

  // Used to vend weak pointers to LayerTreeHost to ScopedDeferMainFrameUpdate
  // objects.
  base::WeakPtrFactory<LayerTreeHost> defer_main_frame_update_weak_ptr_factory_{
      this};
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
