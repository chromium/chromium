// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_H_
#define CC_TREES_PROPERTY_TREE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/synced_property.h"
#include "cc/cc_export.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/scroll_offset_map.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_ids.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/sticky_position_constraint.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {
class CopyOutputRequest;
}

namespace cc {

class LayerTreeImpl;
class RenderSurfaceImpl;
struct RenderSurfacePropertyChangedFlags;
struct CompositorCommitData;
struct ViewportPropertyIds;

using SyncedScrollOffset =
    SyncedProperty<AdditionGroup<gfx::PointF, gfx::Vector2dF>>;

class PropertyTrees;

template <typename T>
class CC_EXPORT PropertyTree {
  friend class PropertyTrees;

 public:
  using NodeType = T;

  PropertyTree(const PropertyTree& other) = delete;
  ~PropertyTree();
  PropertyTree<T>& operator=(const PropertyTree<T>&);

#if DCHECK_IS_ON()
  bool operator==(const PropertyTree<T>& other) const;
#endif

  int Insert(const T& tree_node, int parent_id);

  // Removes the last `n` nodes from the tree.
  void RemoveNodes(size_t n);

  T* Node(int i) {
    CHECK_LT(i, static_cast<int>(nodes_.size()));
    return i > kInvalidPropertyNodeId ? &nodes_[i] : nullptr;
  }
  const T* Node(int i) const {
    CHECK_LT(i, static_cast<int>(nodes_.size()));
    return i > kInvalidPropertyNodeId ? &nodes_[i] : nullptr;
  }

  T* parent(const T* t) { return Node(t->parent_id); }
  const T* parent(const T* t) const { return Node(t->parent_id); }

  T* back() { return size() ? &nodes_.back() : nullptr; }
  const T* back() const { return size() ? &nodes_.back() : nullptr; }

  void SetElementIdForNodeId(int node_id, ElementId element_id) {
    element_id_to_node_index_[element_id] = node_id;
  }
  T* FindNodeFromElementId(ElementId id) {
    auto iterator = element_id_to_node_index_.find(id);
    if (iterator == element_id_to_node_index_.end())
      return nullptr;
    return Node(iterator->second);
  }
  const T* FindNodeFromElementId(ElementId id) const {
    auto iterator = element_id_to_node_index_.find(id);
    if (iterator == element_id_to_node_index_.end())
      return nullptr;
    return Node(iterator->second);
  }

  void clear();
  size_t size() const { return nodes_.size(); }

  void set_needs_update(bool needs_update) {
    needs_update_ = needs_update;
  }
  bool needs_update() const { return needs_update_; }

  std::vector<T>& nodes() { return nodes_; }
  const std::vector<T>& nodes() const { return nodes_; }

  int next_available_id() const { return static_cast<int>(size()); }

  PropertyTrees* property_trees() const { return property_trees_; }

  void AsValueInto(base::trace_event::TracedValue* value) const;

  const base::flat_map<ElementId, int>& element_id_to_node_index() const {
    return element_id_to_node_index_;
  }

 protected:
  explicit PropertyTree(PropertyTrees* property_trees);
  std::vector<T> nodes_;

 private:
  void SetPropertyTrees(PropertyTrees* property_trees) {
    property_trees_ = property_trees;
  }

  bool needs_update_;
  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks.
  RAW_PTR_EXCLUSION PropertyTrees* property_trees_;
  // This map allow mapping directly from a compositor element id to the
  // respective property node. This will eventually allow simplifying logic in
  // various places that today has to map from element id to layer id, and then
  // from layer id to the respective property node. Completing that work is
  // pending the launch of BlinkGenPropertyTrees and reworking UI compositor
  // logic to produce cc property trees and these maps.
  base::flat_map<ElementId, int> element_id_to_node_index_;
};

struct AnchorPositionScrollData;
struct StickyPositionNodeData;

class CC_EXPORT TransformTree final : public PropertyTree<TransformNode> {
 public:
  explicit TransformTree(PropertyTrees* property_trees = nullptr);

  // These C++ special member functions cannot be implicit inline because
  // they are exported by CC_EXPORT. They will be instantiated in every
  // compilation units that included this header, and compilation can fail
  // because TransformCachedNodeData may be incomplete.
  TransformTree(const TransformTree&) = delete;
  ~TransformTree();
  TransformTree& operator=(const TransformTree&);

#if DCHECK_IS_ON()
  bool operator==(const TransformTree& other) const;
#endif

  int Insert(const TransformNode& tree_node, int parent_id);
  void RemoveNodes(size_t n);

  void clear();

  bool OnTransformAnimated(ElementId element_id,
                           const gfx::Transform& transform);
  void ResetChangeTracking();

  // Updates the parent, target, and screen space transforms and snapping for
  // all nodes.
  void UpdateAllTransforms(const ViewportPropertyIds& viewport_property_ids);
  // UpdateAllTransforms() may update the transform tree in multiple passes.
  // This struct stores data collected and used across the passes.
  struct UpdateTransformsData {
    UpdateTransformsData();
    ~UpdateTransformsData();
    // A transform node may depend on a later transform node (e.g. an anchor
    // position offset node references later adjustment container nodes). When
    // the former is updated, the latter id will be stored in this set assuming
    // the depended data is stale. When the latter node is updated, its id will
    // be removed from this set if it hasn't changed anything affecting the
    // depending node. If this set is not empty after a pass,
    // UpdateAllTransforms() will run another pass.
    base::flat_set<int> stale_forward_dependencies;
  };

  // Updates transforms for a node.
  void UpdateTransforms(
      int id,
      const ViewportPropertyIds* viewport_property_ids = nullptr,
      UpdateTransformsData* update_data = nullptr);
  void UpdateTransformChanged(TransformNode* node, TransformNode* parent_node);
  void UpdateNodeAndAncestorsAreAnimatedOrInvertible(
      TransformNode* node,
      TransformNode* parent_node);
  void UpdateNodeOrAncestorsWillChangeTransform(TransformNode* node,
                                                TransformNode* parent_node);

  void set_needs_update(bool needs_update);

  // We store the page scale factor on the transform tree so that it can be
  // easily be retrieved and updated in UpdatePageScale.
  void set_page_scale_factor(float page_scale_factor) {
    page_scale_factor_ = page_scale_factor;
  }
  float page_scale_factor() const { return page_scale_factor_; }

  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }
  float device_scale_factor() const { return device_scale_factor_; }

  void SetRootScaleAndTransform(float device_scale_factor,
                                const gfx::Transform& device_transform);
  void set_device_transform_scale_factor(float device_transform_scale_factor) {
    device_transform_scale_factor_ = device_transform_scale_factor;
  }
  float device_transform_scale_factor() const {
    return device_transform_scale_factor_;
  }

  void UpdateOuterViewportContainerBoundsDelta();

  void AddNodeAffectedByOuterViewportBoundsDelta(int node_id);

  bool HasNodesAffectedByOuterViewportBoundsDelta() const;

  const std::vector<int>& nodes_affected_by_outer_viewport_bounds_delta()
      const {
    return nodes_affected_by_outer_viewport_bounds_delta_;
  }
  void set_nodes_affected_by_outer_viewport_bounds_delta(
      std::vector<int> nodes) {
    nodes_affected_by_outer_viewport_bounds_delta_ = std::move(nodes);
  }

  const std::vector<StickyPositionNodeData>& sticky_position_data() const {
    return sticky_position_data_;
  }
  std::vector<StickyPositionNodeData>& sticky_position_data() {
    return sticky_position_data_;
  }

  const std::vector<AnchorPositionScrollData>& anchor_position_scroll_data()
      const {
    return anchor_position_scroll_data_;
  }
  std::vector<AnchorPositionScrollData>& anchor_position_scroll_data() {
    return anchor_position_scroll_data_;
  }

  const gfx::Transform& FromScreen(int node_id) const;
  void SetFromScreen(int node_id, const gfx::Transform& transform);

  const gfx::Transform& ToScreen(int node_id) const;
  void SetToScreen(int node_id, const gfx::Transform& transform);

  int TargetId(int node_id) const;
  void SetTargetId(int node_id, int target_id);

  int ContentTargetId(int node_id) const;
  void SetContentTargetId(int node_id, int content_target_id);

  const std::vector<TransformCachedNodeData>& cached_data() const {
    return cached_data_;
  }

  void UndoOverscroll(const TransformNode* node,
                      gfx::Vector2dF& position_adjustment,
                      const ViewportPropertyIds* viewport_property_ids);

  const StickyPositionNodeData* GetStickyPositionData(int node_id) const {
    return const_cast<TransformTree*>(this)->MutableStickyPositionData(node_id);
  }
  StickyPositionNodeData& EnsureStickyPositionData(int node_id);

  const AnchorPositionScrollData* GetAnchorPositionScrollData(
      int node_id) const;
  AnchorPositionScrollData& EnsureAnchorPositionScrollData(int node_id);

  // Computes the combined transform between |source_id| and |dest_id|. These
  // two nodes must be on the same ancestor chain.
  void CombineTransformsBetween(int source_id,
                                int dest_id,
                                gfx::Transform* transform) const;

  // Computes the combined inverse transform between |source_id| and |dest_id|
  // and returns false if the inverse of a singular transform was used. These
  // two nodes must be on the same ancestor chain.
  bool CombineInversesBetween(int source_id,
                              int dest_id,
                              gfx::Transform* transform) const;

 private:
  // Returns true iff the node at |desc_id| is a descendant of the node at
  // |anc_id|.
  bool IsDescendant(int desc_id, int anc_id) const;

  StickyPositionNodeData* MutableStickyPositionData(int node_id);
  gfx::Vector2dF StickyPositionOffset(TransformNode* node);
  gfx::Vector2dF AnchorPositionOffset(TransformNode* node,
                                      int max_updated_node_id,
                                      UpdateTransformsData* update_data);
  void UpdateLocalTransform(TransformNode* node,
                            const ViewportPropertyIds* viewport_property_ids,
                            UpdateTransformsData* update_data);
  void UpdateScreenSpaceTransform(TransformNode* node,
                                  TransformNode* parent_node);
  void UpdateAnimationProperties(TransformNode* node,
                                 TransformNode* parent_node);
  void UndoSnapping(TransformNode* node);
  void UpdateSnapping(TransformNode* node);
  void UpdateNodeAndAncestorsHaveIntegerTranslations(
      TransformNode* node,
      TransformNode* parent_node);

  // When to_screen transform has perspective, the transform node's sublayer
  // scale is calculated using page scale factor, device scale factor and the
  // scale factor of device transform. So we need to store them explicitly.
  float page_scale_factor_;
  float device_scale_factor_;
  float device_transform_scale_factor_;
  std::vector<int> nodes_affected_by_outer_viewport_bounds_delta_;
  std::vector<TransformCachedNodeData> cached_data_;
  std::vector<StickyPositionNodeData> sticky_position_data_;
  std::vector<AnchorPositionScrollData> anchor_position_scroll_data_;
};

struct CC_EXPORT AnchorPositionScrollData {
  AnchorPositionScrollData();
  ~AnchorPositionScrollData();
  AnchorPositionScrollData(const AnchorPositionScrollData&);

  bool operator==(const AnchorPositionScrollData&) const;

  // See blink::AnchorPositionScrollData for the definition of adjustment
  // containers.
  std::vector<ElementId> adjustment_container_ids;
  gfx::Vector2d accumulated_scroll_origin;
  bool needs_scroll_adjustment_in_x = false;
  bool needs_scroll_adjustment_in_y = false;
};

struct CC_EXPORT StickyPositionNodeData {
  int scroll_ancestor;
  StickyPositionConstraint constraints;

  // In order to properly compute the sticky offset, we need to know if we have
  // any sticky ancestors both between ourselves and our containing block and
  // between our containing block and the viewport. These ancestors are then
  // used to correct the constraining rect locations.
  int nearest_node_shifting_sticky_box;
  int nearest_node_shifting_containing_block;

  // For performance we cache our accumulated sticky offset to allow descendant
  // sticky elements to offset their constraint rects. Because we can either
  // affect the sticky box constraint rect or the containing block constraint
  // rect, we need to accumulate both.
  gfx::Vector2dF total_sticky_box_sticky_offset;
  gfx::Vector2dF total_containing_block_sticky_offset;

  StickyPositionNodeData()
      : scroll_ancestor(kInvalidPropertyNodeId),
        nearest_node_shifting_sticky_box(kInvalidPropertyNodeId),
        nearest_node_shifting_containing_block(kInvalidPropertyNodeId) {}

  bool operator==(const StickyPositionNodeData&) const;
};

class CC_EXPORT ClipTree final : public PropertyTree<ClipNode> {
 public:
  explicit ClipTree(PropertyTrees* property_trees = nullptr);
#if DCHECK_IS_ON()
  bool operator==(const ClipTree& other) const;
#endif

  void SetViewportClip(gfx::RectF viewport_rect);
  gfx::RectF ViewportClip() const;
};

class CC_EXPORT EffectTree final : public PropertyTree<EffectNode> {
 public:
  explicit EffectTree(PropertyTrees* property_trees = nullptr);
  ~EffectTree();

  EffectTree& operator=(const EffectTree& from);

#if DCHECK_IS_ON()
  bool operator==(const EffectTree& other) const;
#endif

  int Insert(const EffectNode& tree_node, int parent_id);
  void RemoveNodes(size_t n);

  void clear();

  float EffectiveOpacity(const EffectNode* node) const;

  void UpdateSurfaceContentsScale(EffectNode* node);

  bool OnOpacityAnimated(ElementId id, float opacity);
  bool OnFilterAnimated(ElementId id, const FilterOperations& filters);
  bool OnBackdropFilterAnimated(ElementId id,
                                const FilterOperations& backdrop_filters);

  void UpdateEffects(int id);

  void UpdateEffectChanged(EffectNode* node, EffectNode* parent_node);

  void UpdateHasFilters(EffectNode* node, EffectNode* parent_node);
  void UpdateHasFastRoundedCorner(EffectNode* node, EffectNode* parent_node);

  typedef std::unordered_multimap<int, std::unique_ptr<viz::CopyOutputRequest>>
      CopyRequestMap;

  void AddCopyRequest(int node_id,
                      std::unique_ptr<viz::CopyOutputRequest> request);
  void PullCopyRequestsFrom(CopyRequestMap& new_copy_requests);
  void PushCopyRequestsTo(EffectTree* other_tree);
  void TakeCopyRequestsAndTransformToSurface(
      int node_id,
      std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests);
  bool HasCopyRequests() const;
  void ClearCopyRequests();
  void GetRenderSurfaceChangedFlags(
      std::vector<RenderSurfacePropertyChangedFlags>& flags) const;
  void ApplyRenderSurfaceChangedFlags(
      const std::vector<RenderSurfacePropertyChangedFlags>& flags);

  // Given the ids of two effect nodes that have render surfaces, returns the
  // id of the lowest common ancestor effect node that also has a render
  // surface.
  int LowestCommonAncestorWithRenderSurface(int id_1, int id_2) const;

  RenderSurfaceImpl* GetRenderSurface(int id) {
    return render_surfaces_[static_cast<size_t>(id)].get();
  }

  const RenderSurfaceImpl* GetRenderSurface(int id) const {
    return render_surfaces_[static_cast<size_t>(id)].get();
  }

  bool ContributesToDrawnSurface(int id) const;

  void ResetChangeTracking();

  void TakeRenderSurfaces(
      std::vector<std::unique_ptr<RenderSurfaceImpl>>* render_surfaces);

  // Returns true if render surfaces changed (that is, if any render surfaces
  // were created or destroyed).
  bool CreateOrReuseRenderSurfaces(
      std::vector<std::unique_ptr<RenderSurfaceImpl>>* old_render_surfaces,
      LayerTreeImpl* layer_tree_impl);

  // This function checks if the layer's hit test region is a rectangle so that
  // we may be able to use |visible_layer_rect| for viz hit test. It returns
  // true when the following three conditions are met:
  // 1) All clips preserve 2d axis.
  // 2) There are no mask layers.
  bool ClippedHitTestRegionIsRectangle(int effect_node_id) const;

  // This function checks if the associated layer can use its layer bounds to
  // correctly hit test. It returns true if the layer bounds cannot be trusted.
  bool HitTestMayBeAffectedByMask(int effect_node_id) const;

  CopyRequestMap TakeCopyRequests();

 private:
  void UpdateOpacities(EffectNode* node, EffectNode* parent_node);
  void UpdateSubtreeHidden(EffectNode* node, EffectNode* parent_node);
  void UpdateIsDrawn(EffectNode* node, EffectNode* parent_node);
  void UpdateBackfaceVisibility(EffectNode* node, EffectNode* parent_node);
  void UpdateHasMaskingChild(EffectNode* node, EffectNode* parent_node);
  void UpdateOnlyDrawsVisibleContent(EffectNode* node, EffectNode* parent_node);
  void UpdateClosestAncestorSharedElement(EffectNode* node,
                                          EffectNode* parent_node);

  // Stores copy requests, keyed by node id.
  std::unordered_multimap<int, std::unique_ptr<viz::CopyOutputRequest>>
      copy_requests_;

  // Indexed by node id.
  std::vector<std::unique_ptr<RenderSurfaceImpl>> render_surfaces_;
};

// These callbacks are called in the main thread to notify changes of scroll
// information in the compositor thread during commit.
class ScrollCallbacks {
 public:
  // Called after the composited scroll offset changed.
  virtual void DidCompositorScroll(
      ElementId scroll_element_id,
      const gfx::PointF&,
      const std::optional<TargetSnapAreaElementIds>&) = 0;
  // Called after the hidden status of composited scrollbars changed. Note that
  // |scroll_element_id| is the element id of the scroll not of the scrollbars.
  virtual void DidChangeScrollbarsHidden(ElementId scroll_element_id,
                                         bool hidden) = 0;

 protected:
  virtual ~ScrollCallbacks() {}
};

class CC_EXPORT ScrollTree final : public PropertyTree<ScrollNode> {
 public:
  explicit ScrollTree(PropertyTrees* property_trees = nullptr);
  ~ScrollTree();

  ScrollTree& operator=(const ScrollTree& from);

#if DCHECK_IS_ON()
  bool operator==(const ScrollTree& other) const;
#endif

  void clear();

  gfx::PointF MaxScrollOffset(int scroll_node_id) const;
  void OnScrollOffsetAnimated(ElementId id,
                              int scroll_tree_index,
                              const gfx::PointF& scroll_offset,
                              LayerTreeImpl* layer_tree_impl);
  gfx::Size container_bounds(int scroll_node_id) const;
  gfx::SizeF scroll_bounds(int scroll_node_id) const;
  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;
#if DCHECK_IS_ON()
  int CurrentlyScrollingNodeId() const;
#endif
  void set_currently_scrolling_node(int scroll_node_id);
  int currently_scrolling_node() const { return currently_scrolling_node_id_; }
  gfx::Transform ScreenSpaceTransform(int scroll_node_id) const;

  gfx::Vector2dF ClampScrollToMaxScrollOffset(const ScrollNode& node,
                                              LayerTreeImpl*);

  // Returns the current scroll offset. On the main thread this would return the
  // value for the LayerTree while on the impl thread this is the current value
  // on the active tree.
  const gfx::PointF current_scroll_offset(ElementId id) const;

  // Returns the scroll offset taking into account any adjustments that may be
  // included due to pixel snapping.
  //
  // Note: Using this method may causes the associated transform node for this
  // scroll node to update its transforms.
  //
  // TODO(crbug.com/41238797): Updating single transform node only works for
  // simple cases but we really should update the whole transform tree otherwise
  // we are ignoring any parent transform node that needs updating and thus our
  // snap amount can be incorrect.
  gfx::PointF GetScrollOffsetForScrollTimeline(const ScrollNode&) const;

  // Collects deltas for scroll changes on the impl thread that need to be
  // reported to the main thread during the main frame. As such, should only be
  // called on the impl thread side PropertyTrees.
  void CollectScrollDeltas(
      CompositorCommitData* commit_data,
      ElementId inner_viewport_scroll_element_id,
      bool use_fractional_deltas,
      const base::flat_map<ElementId, TargetSnapAreaElementIds>&
          snapped_elements,
      const MutatorHost* main_thread_mutator_host);

  // Applies deltas sent in the previous main frame onto the impl thread state.
  // Should only be called on the impl thread side PropertyTrees.
  void ApplySentScrollDeltasFromAbortedCommit(bool next_bmf,
                                              bool main_frame_applied_deltas);

  // Pushes scroll updates from the ScrollTree on the main thread onto the
  // impl thread associated state.
  void PushScrollUpdatesFromMainThread(const PropertyTrees& main_property_trees,
                                       LayerTreeImpl* sync_tree,
                                       bool use_fractional_deltas);

  // Pushes scroll updates from the ScrollTree on the pending tree onto the
  // active tree associated state.
  void PushScrollUpdatesFromPendingTree(PropertyTrees* pending_property_trees,
                                        LayerTreeImpl* active_tree);

  void SetBaseScrollOffset(ElementId id, const gfx::PointF& scroll_offset);
  // Returns true if the scroll offset is changed.
  bool SetScrollOffset(ElementId id, const gfx::PointF& scroll_offset);
  void SetScrollOffsetClobberActiveValue(ElementId id) {
    if (auto* synced_offset = GetSyncedScrollOffset(id))
      synced_offset->set_clobber_active_value();
  }

  void SetScrollingContentsCullRect(ElementId id, const gfx::Rect& cull_rect);
  const gfx::Rect* ScrollingContentsCullRect(ElementId id) const;

  SyncedScrollOffset* GetOrCreateSyncedScrollOffsetForTesting(ElementId id);
  bool UpdateScrollOffsetBaseForTesting(ElementId id,
                                        const gfx::PointF& offset);
  bool SetScrollOffsetDeltaForTesting(ElementId id,
                                      const gfx::Vector2dF& delta);
  const gfx::PointF GetScrollOffsetBaseForTesting(ElementId id) const;
  const gfx::Vector2dF GetScrollOffsetDeltaForTesting(ElementId id) const;
  void CollectScrollDeltasForTesting(bool use_fractional_deltas = false);

  gfx::Vector2dF ScrollBy(const ScrollNode& scroll_node,
                          const gfx::Vector2dF& scroll,
                          LayerTreeImpl* layer_tree_impl);
  gfx::PointF ClampScrollOffsetToLimits(gfx::PointF offset,
                                        const ScrollNode& scroll_node) const;

  SyncedScrollOffset* GetSyncedScrollOffset(ElementId id);
  const SyncedScrollOffset* GetSyncedScrollOffset(ElementId id) const;

#if DCHECK_IS_ON()
  void CopyCompleteTreeState(const ScrollTree& other);
#endif

  void SetScrollCallbacks(base::WeakPtr<ScrollCallbacks> callbacks);

  void NotifyDidCompositorScroll(
      ElementId scroll_element_id,
      const gfx::PointF& scroll_offset,
      const std::optional<TargetSnapAreaElementIds>& snap_target_ids);
  void NotifyDidChangeScrollbarsHidden(ElementId scroll_element_id,
                                       bool hidden) const;

  // These functions determines how the rendered result of a compositor-
  // initiated scroll should be realized by updating the scroll offset in
  // the associated transform node.
  // All of them return false if `node.transform_id` is invalid which means
  // Blink didn't paint the transform node because the scrolling contents
  // were far from the viewport and we don't need to realize the scrolls.
  bool CanRealizeScrollsOnActiveTree(const ScrollNode& node) const;
  bool CanRealizeScrollsOnPendingTree(const ScrollNode& node) const;
  bool ShouldRealizeScrollsOnMain(const ScrollNode& node) const;

  // Reports reasons for blocking scroll updates on main-thread repaint.
  // Returns bitfield of values from MainThreadScrollingReason.
  uint32_t GetMainThreadRepaintReasons(const ScrollNode& node) const;

 private:
  // ScrollTree doesn't use the needs_update flag.
  using PropertyTree::needs_update;
  using PropertyTree::set_needs_update;

  using SyncedScrollOffsetMap =
      base::flat_map<ElementId, scoped_refptr<SyncedScrollOffset>>;

  int currently_scrolling_node_id_ = kInvalidPropertyNodeId;

  // On the main thread we store the scroll offsets directly since the main
  // thread only needs to keep track of the current main thread state. The impl
  // thread stores a map of SyncedProperty instances in order to track
  // additional state necessary to synchronize scroll changes between the main
  // and impl threads.
  ScrollOffsetMap scroll_offset_map_;
  SyncedScrollOffsetMap synced_scroll_offset_map_;

  // Maps from scroll element id to scrolling contents cull rect.
  base::flat_map<ElementId, gfx::Rect> scrolling_contents_cull_rects_;

  base::WeakPtr<ScrollCallbacks> callbacks_;

  gfx::Vector2dF PullDeltaForMainThread(SyncedScrollOffset* scroll_offset,
                                        bool use_fractional_deltas,
                                        bool next_bmf);
};

constexpr int kInvalidUpdateNumber = -1;

struct AnimationScaleData {
  // Variable used to invalidate cached maximum scale data when transform tree
  // updates.
  int update_number = kInvalidUpdateNumber;

  // The maximum scale that this node's |to_screen| transform will have during
  // current animations of this node and its ancestors, or the current scale of
  // this node's |to_screen| transform if there are no animations.
  float maximum_to_screen_scale = kInvalidScale;

  // Whether |maximum_to_screen_scale| is affected by any animation of this
  // node or its ancestors. A scale animation having maximum scale of 1 is
  // treated as not affecting |maximum_to_screen_scale|.
  bool affected_by_animation_scale = false;

  // Whether |maximum_to_screen_scale| is affected by any non-calculatable
  // scale.
  bool affected_by_invalid_scale = false;
};

struct DrawTransforms {
  // We compute invertibility of a draw transforms lazily.
  // Might_be_invertible is true if we have not computed the inverse of either
  // to_target or from_target, or to_target / from_target is invertible.
  bool might_be_invertible;
  // From_valid is true if the from_target is already computed directly or
  // computed by inverting an invertible to_target.
  bool from_valid;
  // To_valid is true if to_target stores a valid result, similar to from_valid.
  bool to_valid;
  gfx::Transform from_target;
  gfx::Transform to_target;

  DrawTransforms(gfx::Transform from, gfx::Transform to)
      : might_be_invertible(true),
        from_valid(false),
        to_valid(false),
        from_target(from),
        to_target(to) {}
  bool operator==(const DrawTransforms& other) const {
    return from_valid == other.from_valid && to_valid == other.to_valid &&
           from_target == other.from_target && to_target == other.to_target;
  }
};

struct DrawTransformData {
  int update_number = kInvalidUpdateNumber;
  int target_id = kInvalidPropertyNodeId;

  // TODO(sunxd): Move screen space transforms here if it can improve
  // performance.
  DrawTransforms transforms{gfx::Transform(), gfx::Transform()};
};

struct PropertyTreesCachedData {
  int transform_tree_update_number;
  std::vector<AnimationScaleData> animation_scales;
  mutable std::vector<std::vector<DrawTransformData>> draw_transforms;

  PropertyTreesCachedData();
  ~PropertyTreesCachedData();
};

struct CC_EXPORT PropertyTreesChangeState {
  PropertyTreesChangeState();
  ~PropertyTreesChangeState();
  PropertyTreesChangeState(PropertyTreesChangeState&&);
  PropertyTreesChangeState& operator=(PropertyTreesChangeState&&);
  bool changed = false;
  bool needs_rebuild = false;
  bool full_tree_damaged = false;
  EffectTree::CopyRequestMap effect_tree_copy_requests;
  std::vector<int> changed_effect_nodes;
  std::vector<int> changed_transform_nodes;
  std::vector<RenderSurfacePropertyChangedFlags> surface_property_changed_flags;
};

class CC_EXPORT PropertyTrees final {
 public:
  explicit PropertyTrees(const ProtectedSequenceSynchronizer& synchronizer);
  PropertyTrees(const PropertyTrees& other) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;
  void* operator new[](size_t) = delete;
  void* operator new[](size_t, void*) = delete;
  ~PropertyTrees();

  PropertyTrees& operator=(const PropertyTrees& from);

#if DCHECK_IS_ON()
  bool operator==(const PropertyTrees& other) const;
#endif

  const ProtectedSequenceSynchronizer& synchronizer() const {
    return *synchronizer_;
  }

  const ClipTree& clip_tree() const { return clip_tree_; }
  ClipTree& clip_tree_mutable() { return clip_tree_; }
  const EffectTree& effect_tree() const { return effect_tree_; }
  EffectTree& effect_tree_mutable() { return effect_tree_; }
  const ScrollTree& scroll_tree() const { return scroll_tree_; }
  ScrollTree& scroll_tree_mutable() { return scroll_tree_; }
  const TransformTree& transform_tree() const { return transform_tree_; }
  TransformTree& transform_tree_mutable() { return transform_tree_; }

  void set_needs_rebuild(bool value) {
    needs_rebuild_.Write(synchronizer()) = value;
  }
  bool needs_rebuild() const { return needs_rebuild_.Read(synchronizer()); }

  void set_changed(bool value) { changed_.Write(synchronizer()) = value; }
  bool changed() const { return changed_.Read(synchronizer()); }

  void set_full_tree_damaged(bool value) {
    full_tree_damaged_.Write(synchronizer()) = value;
  }
  bool full_tree_damaged() const {
    return full_tree_damaged_.Read(synchronizer());
  }

  void set_is_main_thread(bool value) {
    is_main_thread_.Write(synchronizer()) = value;
  }
  bool is_main_thread() const { return is_main_thread_.Read(synchronizer()); }

  void set_is_active(bool value) { is_active_.Write(synchronizer()) = value; }
  bool is_active() const { return is_active_.Read(synchronizer()); }

  void set_sequence_number(int n) {
    sequence_number_.Write(synchronizer()) = n;
  }
  void increment_sequence_number() { sequence_number_.Write(synchronizer())++; }
  int sequence_number() const { return sequence_number_.Read(synchronizer()); }

  void clear();

  // Applies an animation state change for a particular element in
  // this property tree. Returns whether a draw property update is
  // needed.
  bool ElementIsAnimatingChanged(const PropertyToElementIdMap& element_id_map,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state,
                                 bool check_node_existence);
  void MaximumAnimationScaleChanged(ElementId element_id, float maximum_scale);
  void SetInnerViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void SetOuterViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void UpdateChangeTracking();
  void GetChangedNodes(std::vector<int>& effect_nodes,
                       std::vector<int>& transform_nodes) const;
  void ApplyChangedNodes(const std::vector<int>& effect_nodes,
                         const std::vector<int>& transform_nodes);
  // Note that GetChangeState mutates the state of effect_tree_.
  void GetChangeState(PropertyTreesChangeState& change_state);
  void ResetAllChangeTracking();

  gfx::Vector2dF inner_viewport_container_bounds_delta() const {
    return inner_viewport_container_bounds_delta_.Read(synchronizer());
  }
  gfx::Vector2dF inner_viewport_scroll_bounds_delta() const {
    // Inner viewport scroll bounds are always the same as outer viewport
    // container bounds.
    return outer_viewport_container_bounds_delta();
  }
  gfx::Vector2dF outer_viewport_container_bounds_delta() const {
    return outer_viewport_container_bounds_delta_.Read(synchronizer());
  }

  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;
  void AsValueInto(base::trace_event::TracedValue* value) const;
  std::string ToString() const;

  bool AnimationScaleCacheIsInvalid(int transform_id) const;
  float MaximumAnimationToScreenScale(int transform_id);
  bool AnimationAffectedByInvalidScale(int transform_id);

  void SetMaximumAnimationToScreenScaleForTesting(
      int transform_id,
      float maximum_scale,
      bool affected_by_invalid_scale);

  bool GetToTarget(int transform_id,
                   int effect_id,
                   gfx::Transform* to_target) const;
  bool GetFromTarget(int transform_id,
                     int effect_id,
                     gfx::Transform* from_target) const;

  void ResetCachedData();
  void UpdateTransformTreeUpdateNumber();
  gfx::Transform ToScreenSpaceTransformWithoutSurfaceContentsScale(
      int transform_id,
      int effect_id) const;

  ClipRectData* FetchClipRectFromCache(int clip_id, int target_id);

  bool HasElement(ElementId element_id) const;

 private:
  const raw_ref<const ProtectedSequenceSynchronizer> synchronizer_;

  TransformTree transform_tree_;
  EffectTree effect_tree_;
  ClipTree clip_tree_;
  ScrollTree scroll_tree_;

  ProtectedSequenceReadable<bool> needs_rebuild_;
  // Change tracking done on property trees needs to be preserved across commits
  // (when they are not rebuild). We cache a global bool which stores whether
  // we did any change tracking so that we can skip copying the change status
  // between property trees when this bool is false.
  ProtectedSequenceReadable<bool> changed_;
  // We cache a global bool for full tree damages to avoid walking the entire
  // tree.
  // TODO(jaydasika): Changes to transform and effects that damage the entire
  // tree should be tracked by this bool. Currently, they are tracked by the
  // individual nodes.
  ProtectedSequenceReadable<bool> full_tree_damaged_;
  ProtectedSequenceReadable<bool> is_main_thread_;
  ProtectedSequenceReadable<bool> is_active_;

  ProtectedSequenceReadable<int> sequence_number_;

  ProtectedSequenceReadable<gfx::Vector2dF>
      inner_viewport_container_bounds_delta_;
  ProtectedSequenceReadable<gfx::Vector2dF>
      outer_viewport_container_bounds_delta_;

  const AnimationScaleData& GetAnimationScaleData(int transform_id);

  // GetDrawTransforms may change the value of cached_data_.
  DrawTransforms& GetDrawTransforms(int transform_id, int effect_id) const;
  DrawTransformData& FetchDrawTransformsDataFromCache(int transform_id,
                                                      int effect_id) const;

  // This can be mutable and not wrapped in ProtectedSequence* because it isn't
  // copied by operator=().
  mutable PropertyTreesCachedData cached_data_;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_H_
