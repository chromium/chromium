// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_TREES_LAYER_TREE_IMPL_H_
#define CC_TREES_LAYER_TREE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "cc/base/synced_property.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/metrics/event_metrics.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/browser_controls_params.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/swap_promise.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace viz {
class ClientResourceProvider;
class RasterContextProvider;
}

namespace cc {

enum class ActivelyScrollingType;
class DebugRectHistory;
class ViewTransitionRequest;
class DroppedFrameCounter;
class GlobalStateThatImpactsTilePriority;
class HeadsUpDisplayLayerImpl;
class ImageDecodeCache;
class LayerTreeDebugState;
class LayerTreeImpl;
class LayerTreeFrameSink;
class LayerTreeSettings;
class MemoryHistory;
class PictureLayerImpl;
class TaskRunnerProvider;
class TileManager;
class UIResourceRequest;
class VideoFrameControllerClient;
struct PendingPageScaleAnimation;

using UIResourceRequestQueue = std::vector<UIResourceRequest>;
using SyncedScale = SyncedProperty<ScaleGroup>;
using SyncedBrowserControls = SyncedProperty<AdditionGroup<float>>;
using SyncedElasticOverscroll = SyncedProperty<AdditionGroup<gfx::Vector2dF>>;

class LayerTreeLifecycle {
 public:
  enum LifecycleState {
    kNotSyncing,

    // The following states are the steps performed when syncing properties to
    // this tree (see: LayerTreeHost::FinishCommitOnImplThread or
    // LayerTreeHostImpl::ActivateSyncTree).
    kBeginningSync,
    kSyncedPropertyTrees,
    kSyncedLayerProperties,
    kLastSyncState = kSyncedLayerProperties,

    // TODO(pdr): Add states to cover more than just the synchronization steps.
  };

  void AdvanceTo(LifecycleState);

  bool AllowsPropertyTreeAccess() const {
    return state_ == kNotSyncing || state_ >= kSyncedPropertyTrees;
  }
  bool AllowsLayerPropertyAccess() const {
    return state_ == kNotSyncing || state_ >= kSyncedLayerProperties;
  }

 private:
  LifecycleState state_ = kNotSyncing;
};

class CC_EXPORT LayerTreeImpl {
 public:
  // This is the number of times a fixed point has to be hit continuously by a
  // layer to consider it as jittering.
  enum : int { kFixedPointHitsThreshold = 3 };
  LayerTreeImpl(
      LayerTreeHostImpl& host_impl,
      scoped_refptr<SyncedScale> page_scale_factor,
      scoped_refptr<SyncedBrowserControls> top_controls_shown_ratio,
      scoped_refptr<SyncedBrowserControls> bottom_controls_shown_ratio,
      scoped_refptr<SyncedElasticOverscroll> elastic_overscroll);
  LayerTreeImpl(const LayerTreeImpl&) = delete;
  virtual ~LayerTreeImpl();

  LayerTreeImpl& operator=(const LayerTreeImpl&) = delete;

  void Shutdown();
  void ReleaseResources();
  void OnPurgeMemory();
  void ReleaseTileResources();
  void RecreateTileResources();
  void SetVisible(bool visible);

  // Methods called by the layer tree that pass-through or access LTHI.
  // ---------------------------------------------------------------------------
  LayerTreeFrameSink* layer_tree_frame_sink();
  int max_texture_size() const;
  const LayerTreeSettings& settings() const;
  const LayerTreeDebugState& debug_state() const;
  viz::RasterContextProvider* context_provider() const;
  viz::ClientResourceProvider* resource_provider() const;
  TileManager* tile_manager() const;
  ImageDecodeCache* image_decode_cache() const;
  ImageAnimationController* image_animation_controller() const;
  DroppedFrameCounter* dropped_frame_counter() const;
  MemoryHistory* memory_history() const;
  DebugRectHistory* debug_rect_history() const;
  const GlobalStateThatImpactsTilePriority& global_tile_state() const {
    return host_impl_->global_tile_state();
  }
  bool IsActiveTree() const;
  bool IsPendingTree() const;
  bool IsRecycleTree() const;
  bool IsSyncTree() const;
  bool HasPendingTree() const;
  LayerImpl* FindActiveTreeLayerById(int id);
  LayerImpl* FindPendingTreeLayerById(int id);
  // TODO(bokan): PinchGestureActive is a layering violation, it's not related
  // to what LayerTreeImpl does.
  bool PinchGestureActive() const;
  const viz::BeginFrameArgs& CurrentBeginFrameArgs() const;
  base::TimeDelta CurrentBeginFrameInterval() const;
  const gfx::Rect ViewportRectForTilePriority() const;
  std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationController(ElementId scroll_element_id,
                                     float initial_opacity);
  void DidAnimateScrollOffset();
  bool use_gpu_rasterization() const;
  bool create_low_res_tiling() const;
  bool RequiresHighResToDraw() const;
  bool SmoothnessTakesPriority() const;
  VideoFrameControllerClient* GetVideoFrameControllerClient() const;
  MutatorHost* mutator_host() const { return host_impl_->mutator_host(); }
  void UpdateImageDecodingHints(
      base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
          decoding_mode_map);
  int GetMSAASampleCountForRaster(const DisplayItemList& display_list) const;
  TargetColorParams GetTargetColorParams(
      gfx::ContentColorUsage content_color_usage) const;
  bool IsReadyToActivate() const;
  void RequestImplSideInvalidationForRerasterTiling();

  // Tree specific methods exposed to layer-impl tree.
  // ---------------------------------------------------------------------------
  void SetNeedsRedraw();

  // Tracing methods.
  // ---------------------------------------------------------------------------
  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  void AsValueInto(base::trace_event::TracedValue* dict) const;

  // Other public methods
  // ---------------------------------------------------------------------------
  LayerImpl* root_layer() {
    return layer_list_.empty() ? nullptr : layer_list_[0].get();
  }
  const RenderSurfaceImpl* RootRenderSurface() const;
  bool LayerListIsEmpty() const;
  void SetRootLayerForTesting(std::unique_ptr<LayerImpl>);
  void OnCanDrawStateChangedForTree();
  bool IsRootLayer(const LayerImpl* layer) const;

  OwnedLayerImplList DetachLayers();
  OwnedLayerImplList DetachLayersKeepingRootLayerForTesting();

  void SetPropertyTrees(const PropertyTrees& property_trees,
                        PropertyTreesChangeState& change_state,
                        bool preserve_change_tracking);

  void SetPropertyTrees(PropertyTrees& property_trees,
                        bool preserve_change_tracking = false);

  PropertyTrees* property_trees() {
    // TODO(pdr): We should enable this DCHECK because it will catch uses of
    // stale property trees, but it currently fails too many existing tests.
    // DCHECK(lifecycle().AllowsPropertyTreeAccess());
    return &property_trees_;
  }
  const PropertyTrees* property_trees() const { return &property_trees_; }

  void PullPropertiesFrom(CommitState& commit_state,
                          const ThreadUnsafeCommitState& unsafe_state);
  void PullPropertyTreesFrom(CommitState& commit_state,
                             const ThreadUnsafeCommitState& unsafe_state);
  void PullLayerTreePropertiesFrom(CommitState& commit_state);
  void PushPropertyTreesTo(LayerTreeImpl* tree_impl);
  void PushPropertiesTo(LayerTreeImpl* tree_impl);
  void PushSurfaceRangesTo(LayerTreeImpl* tree_impl);

  void MoveChangeTrackingToLayers();

  void ForceRecalculateRasterScales();

  // Adapts an iterator of std::unique_ptr<LayerImpl> to an iterator of
  // LayerImpl*.
  template <typename Iterator>
  class IteratorAdapter {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = LayerImpl*;
    using difference_type = std::ptrdiff_t;
    using pointer = LayerImpl**;
    using reference = LayerImpl*&;

    constexpr IteratorAdapter() = default;
    explicit IteratorAdapter(Iterator it) : it_(it) {}
    bool operator==(IteratorAdapter o) const { return it_ == o.it_; }
    bool operator!=(IteratorAdapter o) const { return !(*this == o); }
    LayerImpl* operator*() const { return it_->get(); }
    LayerImpl* operator->() const { return it_->get(); }
    IteratorAdapter& operator++() {
      ++it_;
      return *this;
    }
    IteratorAdapter operator++(int) {
      IteratorAdapter other(*this);
      ++*this;
      return other;
    }

   private:
    Iterator it_;
  };
  using const_iterator = IteratorAdapter<OwnedLayerImplList::const_iterator>;
  using const_reverse_iterator =
      IteratorAdapter<OwnedLayerImplList::const_reverse_iterator>;
  const_iterator begin() const { return const_iterator(layer_list_.cbegin()); }
  const_iterator end() const { return const_iterator(layer_list_.cend()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(layer_list_.crbegin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(layer_list_.crend());
  }

  void SetTransformMutated(ElementId element_id,
                           const gfx::Transform& transform);
  void SetOpacityMutated(ElementId element_id, float opacity);
  void SetFilterMutated(ElementId element_id, const FilterOperations& filters);
  void SetBackdropFilterMutated(ElementId element_id,
                                const FilterOperations& backdrop_filters);

  const std::unordered_map<ElementId, float, ElementIdHash>&
  element_id_to_opacity_animations_for_testing() const {
    return element_id_to_opacity_animations_;
  }
  const std::unordered_map<ElementId, gfx::Transform, ElementIdHash>&
  element_id_to_transform_animations_for_testing() const {
    return element_id_to_transform_animations_;
  }
  const std::unordered_map<ElementId, FilterOperations, ElementIdHash>&
  element_id_to_filter_animations_for_testing() const {
    return element_id_to_filter_animations_;
  }
  const std::unordered_map<ElementId, FilterOperations, ElementIdHash>&
  element_id_to_backdrop_filter_animations_for_testing() const {
    return element_id_to_backdrop_filter_animations_;
  }

  int source_frame_number() const { return source_frame_number_; }
  void set_source_frame_number(int frame_number) {
    source_frame_number_ = frame_number;
  }

  BeginMainFrameTraceId trace_id() const { return trace_id_; }
  void set_trace_id(BeginMainFrameTraceId val) { trace_id_ = val; }

  bool is_first_frame_after_commit() const {
    return source_frame_number_ != is_first_frame_after_commit_tracker_;
  }

  void set_is_first_frame_after_commit(bool is_first_frame_after_commit) {
    is_first_frame_after_commit_tracker_ =
        is_first_frame_after_commit ? -1 : source_frame_number_;
  }

  const HeadsUpDisplayLayerImpl* hud_layer() const { return hud_layer_; }
  HeadsUpDisplayLayerImpl* hud_layer() { return hud_layer_; }
  void set_hud_layer(HeadsUpDisplayLayerImpl* layer_impl) {
    hud_layer_ = layer_impl;
  }

  gfx::PointF TotalScrollOffset() const;
  gfx::PointF TotalMaxScrollOffset() const;

  void AddPresentationCallbacks(
      std::vector<PresentationTimeCallbackBuffer::Callback> callbacks);
  std::vector<PresentationTimeCallbackBuffer::Callback>
  TakePresentationCallbacks();

  void AddSuccessfulPresentationCallbacks(
      std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
          callbacks);
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
  TakeSuccessfulPresentationCallbacks();

  // The following viewport related property nodes will only ever be set on the
  // main-frame's renderer (i.e. OOPIF and UI compositors will not have these
  // set.
  void SetViewportPropertyIds(const ViewportPropertyIds& ids);

  const TransformNode* OverscrollElasticityTransformNode() const;
  TransformNode* OverscrollElasticityTransformNode() {
    return const_cast<TransformNode*>(
        const_cast<const LayerTreeImpl*>(this)
            ->OverscrollElasticityTransformNode());
  }
  const TransformNode* PageScaleTransformNode() const;
  TransformNode* PageScaleTransformNode() {
    return const_cast<TransformNode*>(
        const_cast<const LayerTreeImpl*>(this)->PageScaleTransformNode());
  }
  const ScrollNode* InnerViewportScrollNode() const;
  ScrollNode* InnerViewportScrollNode() {
    return const_cast<ScrollNode*>(
        const_cast<const LayerTreeImpl*>(this)->InnerViewportScrollNode());
  }
  const ClipNode* OuterViewportClipNode() const;
  ClipNode* OuterViewportClipNode() {
    return const_cast<ClipNode*>(
        const_cast<const LayerTreeImpl*>(this)->OuterViewportClipNode());
  }
  const ScrollNode* OuterViewportScrollNode() const;
  ScrollNode* OuterViewportScrollNode() {
    return const_cast<ScrollNode*>(
        const_cast<const LayerTreeImpl*>(this)->OuterViewportScrollNode());
  }

  ViewportPropertyIds ViewportPropertyIdsForTesting() const {
    return viewport_property_ids_;
  }
  LayerImpl* InnerViewportScrollLayerForTesting() const;
  LayerImpl* OuterViewportScrollLayerForTesting() const;

  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;
  int LastScrolledScrollNodeIndex() const;
  void SetCurrentlyScrollingNode(const ScrollNode* node);
  void ClearCurrentlyScrollingNode();

  void ApplySentScrollAndScaleDeltasFromAbortedCommit(
      bool next_bmf,
      bool main_frame_applied_deltas);

  SkColor4f background_color() const { return background_color_; }
  void set_background_color(SkColor4f color) { background_color_ = color; }

  gfx::OverlayTransform display_transform_hint() const {
    return display_transform_hint_;
  }
  void set_display_transform_hint(gfx::OverlayTransform hint) {
    display_transform_hint_ = hint;
  }

  void UpdatePropertyTreeAnimationFromMainThread();

  void SetPageScaleOnActiveTree(float active_page_scale);
  void PushPageScaleFromMainThread(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float current_page_scale_factor() const {
    return page_scale_factor()->Current(IsActiveTree());
  }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  float page_scale_delta() const { return page_scale_factor()->Delta(); }

  SyncedScale* page_scale_factor();
  const SyncedScale* page_scale_factor() const;

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void set_painted_device_scale_factor(float painted_device_scale_factor) {
    painted_device_scale_factor_ = painted_device_scale_factor;
  }
  float painted_device_scale_factor() const {
    return painted_device_scale_factor_;
  }

  void SetLocalSurfaceIdFromParent(
      const viz::LocalSurfaceId& local_surface_id_from_parent);
  const viz::LocalSurfaceId& local_surface_id_from_parent() const {
    return local_surface_id_from_parent_;
  }

  void RequestNewLocalSurfaceId();
  bool TakeNewLocalSurfaceIdRequest();
  bool new_local_surface_id_request_for_testing() const {
    return new_local_surface_id_request_;
  }

  void SetScreenshotDestinationToken(base::UnguessableToken destination_token);
  base::UnguessableToken TakeScreenshotDestinationToken();

  void set_primary_main_frame_item_sequence_number(
      int64_t item_sequence_number) {
    primary_main_frame_item_sequence_number_ = item_sequence_number;
  }
  int64_t primary_main_frame_item_sequence_number() const {
    return primary_main_frame_item_sequence_number_;
  }

  void SetDeviceViewportRect(const gfx::Rect& device_viewport_rect);

  // TODO(fsamuel): The reason this is not a trivial accessor is because it
  // may return an external viewport specified in LayerTreeHostImpl. In the
  // future, all properties should flow through the pending and active layer
  // trees and we shouldn't need to reach out to LayerTreeHostImpl.
  gfx::Rect GetDeviceViewport() const;

  // This accessor is the same as above, except it only ever returns the
  // internal (i.e. not external) device viewport.
  gfx::Rect internal_device_viewport() { return device_viewport_rect_; }

  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces);

  // OOPIFs need to know the page scale factor used in the main frame, but it
  // is distributed differently (via VisualPropertiesSync), and used only to
  // set raster-scale (page_scale_factor has geometry implications that are
  // inappropriate for OOPIFs).
  void SetExternalPageScaleFactor(float external_page_scale_factor);
  float external_page_scale_factor() const {
    return external_page_scale_factor_;
  }
  // A function to provide page scale information for scaling scroll deltas. In
  // top-level frames we store this value in page_scale_factor_, but for
  // cross-process subframes it's stored in external_page_scale_factor_, so
  // that it only affects raster scale. These cases are mutually exclusive,
  // except for a page hosted in a GuestView, so only one of the values should
  // ever vary from 1.f.
  float page_scale_factor_for_scroll() const {
    DCHECK(external_page_scale_factor_ == 1.f ||
           current_page_scale_factor() == 1.f ||
           settings().is_for_scalable_page);
    return external_page_scale_factor_ * current_page_scale_factor();
  }
  const gfx::DisplayColorSpaces& display_color_spaces() const {
    return display_color_spaces_;
  }

  const ViewportPropertyIds& viewport_property_ids() const {
    return viewport_property_ids_;
  }

  SyncedElasticOverscroll* elastic_overscroll() {
    return elastic_overscroll_.get();
  }
  const SyncedElasticOverscroll* elastic_overscroll() const {
    return elastic_overscroll_.get();
  }

  SyncedBrowserControls* top_controls_shown_ratio() {
    return top_controls_shown_ratio_.get();
  }
  const SyncedBrowserControls* top_controls_shown_ratio() const {
    return top_controls_shown_ratio_.get();
  }
  SyncedBrowserControls* bottom_controls_shown_ratio() {
    return bottom_controls_shown_ratio_.get();
  }
  const SyncedBrowserControls* bottom_controls_shown_ratio() const {
    return bottom_controls_shown_ratio_.get();
  }
  gfx::Vector2dF current_elastic_overscroll() const {
    return elastic_overscroll()->Current(IsActiveTree());
  }

  void SetElementIdsForTesting();

  // Updates draw properties and render surface layer list, as well as tile
  // priorities. Returns false if it was unable to update.  Updating lcd
  // text may cause invalidations, so should only be done after a commit.
  bool UpdateDrawProperties(
      bool update_tiles,
      bool update_image_animation_controller,
      LayerImplList* output_update_layer_list_for_testing = nullptr);

  // Update picture layers' tiles. Return true if any layer's tile priority
  // is updated.
  bool UpdateTiles();

  void set_needs_update_draw_properties() {
    needs_update_draw_properties_ = true;
  }
  bool needs_update_draw_properties() const {
    return needs_update_draw_properties_;
  }

  bool is_in_resourceless_software_draw_mode() {
    return (host_impl_->GetDrawMode() == DRAW_MODE_RESOURCELESS_SOFTWARE);
  }

  void set_needs_full_tree_sync(bool needs) { needs_full_tree_sync_ = needs; }
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  bool needs_surface_ranges_sync() const { return needs_surface_ranges_sync_; }
  void set_needs_surface_ranges_sync(bool needs_surface_ranges_sync) {
    needs_surface_ranges_sync_ = needs_surface_ranges_sync;
  }

  bool always_push_properties_on_picture_layers() const {
    return always_push_properties_on_picture_layers_;
  }

  void ForceRedrawNextActivation() { next_activation_forces_redraw_ = true; }

  void set_ui_resource_request_queue(UIResourceRequestQueue queue);

  const RenderSurfaceList& GetRenderSurfaceList() const;
  const Region& UnoccludedScreenSpaceRegion() const;

  // These return the size of the root scrollable area and the size of
  // the user-visible scrolling viewport, in CSS layout coordinates.
  gfx::SizeF ScrollableSize() const;
  gfx::SizeF ScrollableViewportSize() const;

  gfx::Rect RootScrollLayerDeviceViewportBounds() const;

  LayerImpl* LayerById(int id) const;
  LayerImpl* LayerByElementId(ElementId element_id) const;

  bool IsElementInPropertyTree(ElementId element_id) const;

  void SetSurfaceRanges(base::flat_set<viz::SurfaceRange> surface_ranges);
  const base::flat_set<viz::SurfaceRange>& SurfaceRanges() const;
  void ClearSurfaceRanges();

  void AddLayerShouldPushProperties(LayerImpl* layer);
  void ClearLayersThatShouldPushProperties();
  const base::flat_set<LayerImpl*>& LayersThatShouldPushProperties() const {
    return layers_that_should_push_properties_;
  }

  // These should be called by LayerImpl's ctor/dtor.
  void RegisterLayer(LayerImpl* layer);
  void UnregisterLayer(LayerImpl* layer);

  // Append a layer to the list.
  void AddLayer(std::unique_ptr<LayerImpl> layer);

  size_t NumLayers();

  void DidBecomeActive();

  // Used for accessing the task runner and debug assertions.
  TaskRunnerProvider* task_runner_provider() const;

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  //
  // A swap promise queued by QueueSwapPromise travels with the layer
  // information currently associated with the tree. For example, when
  // a pending tree is activated, the swap promise is passed to the
  // active tree along with the layer information. Similarly, when a
  // new activation overwrites layer information on the active tree,
  // queued swap promises are broken.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Queue a swap promise, pinned to this tree. Pinned swap promises
  // may only be queued on the active tree.
  //
  // An active tree pinned swap promise will see only DidSwap() or
  // DidNotSwap(SWAP_FAILS). No DidActivate() will be seen because
  // that has already happened prior to queueing of the swap promise.
  //
  // Pinned active tree swap promises will not be broken prematurely
  // on the active tree if a new tree is activated.
  void QueuePinnedSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Takes ownership of |new_swap_promises|. Existing swap promises in
  // |swap_promise_list_| are cancelled (SWAP_FAILS).
  void PassSwapPromises(
      std::vector<std::unique_ptr<SwapPromise>> new_swap_promises);
  void AppendSwapPromises(
      std::vector<std::unique_ptr<SwapPromise>> new_swap_promises);
  void FinishSwapPromises(viz::CompositorFrameMetadata* metadata);
  void ClearSwapPromises();
  void BreakSwapPromises(SwapPromise::DidNotSwapReason reason);

  void DidModifyTilePriorities(bool pending_update_tiles = false);

  viz::ResourceId ResourceIdForUIResource(UIResourceId uid) const;
  void ProcessUIResourceRequestQueue();

  bool IsUIResourceOpaque(UIResourceId uid) const;

  void RegisterPictureLayerImpl(PictureLayerImpl* layer);
  void UnregisterPictureLayerImpl(PictureLayerImpl* layer);
  const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
  picture_layers() const {
    return picture_layers_;
  }

  void NotifyLayerHasPaintWorkletsChanged(PictureLayerImpl* layer,
                                          bool has_worklets);
  const base::flat_set<raw_ptr<PictureLayerImpl, CtnExperimental>>&
  picture_layers_with_paint_worklets() const {
    return picture_layers_with_paint_worklets_;
  }

  void RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer);
  void UnregisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer);
  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const;
  void RequestShowScrollbars(ElementId scroll_element_id);

  LayerImpl* FindLayerThatIsHitByPoint(const gfx::PointF& screen_space_point);

  LayerImpl* FindLayerThatIsHitByPointInTouchHandlerRegion(
      const gfx::PointF& screen_space_point);

  LayerImpl* FindLayerThatIsHitByPointInWheelEventHandlerRegion(
      const gfx::PointF& screen_space_point);

  // Returns all layers up to the first scroller or scrollbar layer (when
  // enable_hit_test_opaqueness is false) or a layer opaque to hit test (when
  // enable_hit_test_opaqueness is true), inclusive. The returned vector is
  // sorted in order of top most come first. The back of the vector will be the
  // scrollable layer or the first layer opaque to hit test, if one was hit.
  std::vector<const LayerImpl*> FindLayersUpToFirstScrollableOrOpaqueToHitTest(
      const gfx::PointF& screen_space_point);
  bool PointHitsMainThreadScrollHitTestRegion(
      const gfx::PointF& scree_space_point,
      const LayerImpl& layer) const;
  ElementId PointHitsNonCompositedScroll(const gfx::PointF& screen_space_point,
                                         const LayerImpl& layer) const;

  // Returns the ElementId representing a frame's document at the given point.
  // In cases where cc doesn't have enough information to perform accurate
  // attribution (e.g. in the presence of a complex clip), kInvalidElementId is
  // returned.
  ElementId FindFrameElementIdAtPoint(const gfx::PointF& screen_space_point);

  void RegisterSelection(const LayerSelection& selection);

  bool HandleVisibilityChanged() const { return handle_visibility_changed_; }
  void ResetHandleVisibilityChanged();

  // Compute the current selection handle location and visbility with respect to
  // the viewport.
  void GetViewportSelection(viz::Selection<gfx::SelectionBound>* selection);

  bool browser_controls_shrink_blink_size() const {
    return browser_controls_params_.browser_controls_shrink_blink_size;
  }
  bool SetCurrentBrowserControlsShownRatio(float top_ratio, float bottom_ratio);
  float CurrentTopControlsShownRatio() const {
    return top_controls_shown_ratio_->Current(IsActiveTree());
  }
  float CurrentBottomControlsShownRatio() const {
    return bottom_controls_shown_ratio_->Current(IsActiveTree());
  }
  void SetBrowserControlsParams(const BrowserControlsParams& params);
  float top_controls_height() const {
    return browser_controls_params_.top_controls_height;
  }
  float top_controls_min_height() const {
    return browser_controls_params_.top_controls_min_height;
  }
  void PushBrowserControlsFromMainThread(float top_controls_shown_ratio,
                                         float bottom_controls_shown_ratio);
  float bottom_controls_height() const {
    return browser_controls_params_.bottom_controls_height;
  }
  float bottom_controls_min_height() const {
    return browser_controls_params_.bottom_controls_min_height;
  }
  bool only_expand_top_controls_at_page_top() const {
    return browser_controls_params_.only_expand_top_controls_at_page_top;
  }

  void set_overscroll_behavior(const OverscrollBehavior& behavior);
  OverscrollBehavior overscroll_behavior() const {
    return overscroll_behavior_;
  }

  void SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation> pending_animation);
  std::unique_ptr<PendingPageScaleAnimation> TakePendingPageScaleAnimation();

  void AppendEventsMetricsFromMainThread(EventMetrics::List events_metrics);
  EventMetrics::List TakeEventsMetrics();

  // Requests that we force send RenderFrameMetadata with the next frame.
  void RequestForceSendMetadata() { force_send_metadata_request_ = true; }
  bool TakeForceSendMetadataRequest();

  void DidUpdateScrollOffset(ElementId id,
                             bool pushed_from_main_or_pending_tree);

  // See LayerTreeHost.
  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }
  void set_have_scroll_event_handlers(bool have_event_handlers) {
    have_scroll_event_handlers_ = have_event_handlers;
  }

  // See LayerTreeHost.
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }
  void set_event_listener_properties(EventListenerClass event_class,
                                     EventListenerProperties event_properties) {
    event_listener_properties_[static_cast<size_t>(event_class)] =
        event_properties;
  }

  void ResetAllChangeTracking();

  void HandleTickmarksVisibilityChange();
  void HandleScrollbarShowRequests();

  void InvalidateRegionForImages(
      const PaintImageIdFlatSet& images_to_invalidate);

  void InvalidateRasterInducingScrolls(
      const base::flat_set<ElementId>& scrolls_to_invalidate);

  void UpdateViewportContainerSizes();

  LayerTreeLifecycle& lifecycle() { return lifecycle_; }

  std::string LayerListAsJson() const;

  AnimatedPaintWorkletTracker& paint_worklet_tracker() {
    return host_impl_->paint_worklet_tracker();
  }

  const gfx::Transform& DrawTransform() const {
    return host_impl_->DrawTransform();
  }

  bool IsInSynchronousComposite() const {
    return host_impl_->IsInSynchronousComposite();
  }

  ActivelyScrollingType GetActivelyScrollingType() const {
    return host_impl_->GetActivelyScrollingType();
  }

  // These functions are used for plumbing DelegatedInkMetadata from blink
  // through the compositor and into viz via a compositor frame. They should
  // only be called after the JS API |updateInkTrailStartPoint| has been
  // called, which populates the metadata with provided information.
  void set_delegated_ink_metadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
    delegated_ink_metadata_ = std::move(metadata);
  }
  const gfx::DelegatedInkMetadata* delegated_ink_metadata() const {
    return delegated_ink_metadata_.get();
  }

  void clear_delegated_ink_metadata() { delegated_ink_metadata_.reset(); }

  size_t events_metrics_from_main_thread_count_for_testing() const {
    return events_metrics_from_main_thread_.size();
  }

  bool device_viewport_rect_changed() const {
    return device_viewport_rect_changed_;
  }

  bool viewport_mobile_optimized() const {
    return host_impl_->viewport_mobile_optimized();
  }

  // Add a view transition request from the embedder.
  void AddViewTransitionRequest(std::unique_ptr<ViewTransitionRequest> request);

  // Returns all of the view transition requests stored so far, and empties
  // the internal list.
  std::vector<std::unique_ptr<ViewTransitionRequest>>
  TakeViewTransitionRequests();

  // Returns true if there are pending ViewTransition requests that need a draw.
  bool HasViewTransitionRequests() const;

  // Returns true if there is a pending ViewTransition save request to cache
  // output of the current frame.
  bool HasViewTransitionSaveRequest() const;

  void UpdateAllScrollbarGeometriesForTesting() {
    UpdateAllScrollbarGeometries();
  }

 protected:
  float ClampPageScaleFactorToLimits(float page_scale_factor) const;
  void PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                    float min_page_scale_factor,
                                    float max_page_scale_factor);
  bool SetPageScaleFactorLimits(float min_page_scale_factor,
                                float max_page_scale_factor);
  void DidUpdatePageScale();
  void PushBrowserControls(const float* top_controls_shown_ratio,
                           const float* bottom_controls_shown_ratio);
  bool ClampTopControlsShownRatio();
  bool ClampBottomControlsShownRatio();

 private:
  friend class LayerTreeHost;

  void UpdatePageScaleNode();

  ElementListType GetElementTypeForAnimation() const;
  void UpdateTransformAnimation(ElementId element_id, int transform_node_index);
  template <typename Functor>
  LayerImpl* FindLayerThatIsHitByPointInEventHandlerRegion(
      const gfx::PointF& screen_space_point,
      const Functor& func);

  // Update the geometries of all scrollbars (e.g., thumb size and position).
  void UpdateAllScrollbarGeometries();
  void UpdateViewportScrollbarGeometries();
  void UpdateScrollbarGeometries(const ScrollNode& scroll_node);

  raw_ptr<LayerTreeHostImpl> host_impl_;
  int source_frame_number_;
  BeginMainFrameTraceId trace_id_{0};
  int is_first_frame_after_commit_tracker_;
  raw_ptr<HeadsUpDisplayLayerImpl, DanglingUntriaged> hud_layer_;
  PropertyTrees property_trees_;
  SkColor4f background_color_;

  int last_scrolled_scroll_node_index_;

  ViewportPropertyIds viewport_property_ids_;

  LayerSelection selection_;

  scoped_refptr<SyncedScale> page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  float external_page_scale_factor_;

  float device_scale_factor_;
  float painted_device_scale_factor_;
  gfx::DisplayColorSpaces display_color_spaces_;

  viz::LocalSurfaceId local_surface_id_from_parent_;

  bool new_local_surface_id_request_ : 1 = false;

  bool needs_update_draw_properties_ : 1 = true;

  bool needs_update_tiles_ : 1 = false;

  // In impl-side painting mode, this is true when the pending tree may contain
  // structural differences relative to the active tree. When using a
  // LayerContext, this is also true on the active tree when it may contain
  // structural differences relative to the display tree.
  bool needs_full_tree_sync_ : 1 = true;

  bool needs_surface_ranges_sync_ : 1 = false;

  bool next_activation_forces_redraw_ : 1 = false;

  bool handle_visibility_changed_ : 1 = false;

  bool have_scroll_event_handlers_ : 1 = false;

  // Contains the physical rect of the device viewport, to be used in
  // determining what needs to be drawn.
  bool device_viewport_rect_changed_ : 1 = false;

  // Whether we have a request to force-send RenderFrameMetadata with the next
  // frame.
  bool force_send_metadata_request_ : 1 = false;

  bool always_push_properties_on_picture_layers_ : 1 = false;

  gfx::Rect device_viewport_rect_;

  scoped_refptr<SyncedElasticOverscroll> elastic_overscroll_;

  // TODO(wangxianzhu): Combine layers_ and layer_list_ when we remove
  // support of mask layers.

  OwnedLayerImplList layer_list_;
  // Maps from layer id to layer.
  LayerImplMap layer_id_map_;

  // Set of layers that need to push properties.
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
  RAW_PTR_EXCLUSION base::flat_set<LayerImpl*>
      layers_that_should_push_properties_;

  std::unordered_map<ElementId, float, ElementIdHash>
      element_id_to_opacity_animations_;
  std::unordered_map<ElementId, gfx::Transform, ElementIdHash>
      element_id_to_transform_animations_;
  std::unordered_map<ElementId, FilterOperations, ElementIdHash>
      element_id_to_filter_animations_;
  std::unordered_map<ElementId, FilterOperations, ElementIdHash>
      element_id_to_backdrop_filter_animations_;

  struct ScrollbarLayerIds {
    int horizontal = Layer::INVALID_ID;
    int vertical = Layer::INVALID_ID;
  };
  // Each scroller can have up to two scrollbar layers (vertical and/or
  // horizontal). This mapping is maintained as part of scrollbar registration.
  base::flat_map<ElementId, ScrollbarLayerIds>
      element_id_to_scrollbar_layer_ids_;

  // Tracks a pending requests to show overlay scrollbars. It's set by the
  // scrollbar layer and consumed by PushPropertiesTo() and
  // HandleScrollbarShowRequests().
  base::flat_set<ElementId> show_scrollbar_requests_;

  std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>> picture_layers_;

  // After commit (or impl-side invalidation), the LayerTreeHostImpl must walk
  // all PictureLayerImpls that have PaintWorklets to ensure they are painted.
  // To avoid unnecessary walking, we track that set here.
  base::flat_set<raw_ptr<PictureLayerImpl, CtnExperimental>>
      picture_layers_with_paint_worklets_;

  base::flat_set<viz::SurfaceRange> surface_layer_ranges_;

  // List of render surfaces for the most recently prepared frame.
  //
  // RAW_PTR_EXCLUSION: visible in stack samples when Renderer BRP is enabled.
  RAW_PTR_EXCLUSION RenderSurfaceList render_surface_list_;
  // After drawing the |render_surface_list_| the areas in this region
  // would not be fully covered by opaque content.
  Region unoccluded_screen_space_region_;

  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list_;
  std::vector<std::unique_ptr<SwapPromise>> pinned_swap_promise_list_;

  UIResourceRequestQueue ui_resource_request_queue_;

  EventListenerProperties event_listener_properties_
      [static_cast<size_t>(EventListenerClass::kLast) + 1];

  BrowserControlsParams browser_controls_params_;

  OverscrollBehavior overscroll_behavior_;

  // The amount that the browser controls are shown from 0 (hidden) to 1 (fully
  // shown).
  scoped_refptr<SyncedBrowserControls> top_controls_shown_ratio_;
  scoped_refptr<SyncedBrowserControls> bottom_controls_shown_ratio_;

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  // Tracks the lifecycle which is used for enforcing dependencies between
  // lifecycle states. See: |LayerTreeLifecycle|.
  LayerTreeLifecycle lifecycle_;

  // Display transform hint to tag frames generated from this tree.
  gfx::OverlayTransform display_transform_hint_ = gfx::OVERLAY_TRANSFORM_NONE;

  std::vector<PresentationTimeCallbackBuffer::Callback> presentation_callbacks_;
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
      successful_presentation_callbacks_;

  // Event metrics that are reported back from the main thread.
  EventMetrics::List events_metrics_from_main_thread_;

  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata_;

  // Document transition requests to be transferred to Viz.
  std::vector<std::unique_ptr<ViewTransitionRequest>> view_transition_requests_;

  // See `CommitState::screenshot_destination_token`.
  base::UnguessableToken screenshot_destination_;

  // See `CommitState::primary_main_frame_item_sequence_number`.
  int64_t primary_main_frame_item_sequence_number_ =
      RenderFrameMetadata::kInvalidItemSequenceNumber;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_IMPL_H_
