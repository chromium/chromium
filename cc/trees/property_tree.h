// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_H_
#define CC_TREES_PROPERTY_TREE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/synced_property.h"
#include "cc/cc_export.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/sticky_position_constraint.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

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
struct ClipNode;
struct EffectNode;
struct CompositorCommitData;
struct ScrollNode;
struct TransformNode;
struct TransformCachedNodeData;

typedef SyncedProperty<AdditionGroup<gfx::ScrollOffset>> SyncedScrollOffset;

class PropertyTrees;

template <typename T>
class CC_EXPORT PropertyTree {
 public:
  PropertyTree();
  PropertyTree(const PropertyTree& other) = delete;

  // These C++ special member functions cannot be implicit inline because
  // they are exported by CC_EXPORT. They will be instantiated in every
  // compilation units that included this header, and compilation can fail
  // because T may be incomplete.
  virtual ~PropertyTree();
  PropertyTree<T>& operator=(const PropertyTree<T>&);

  // Property tree node starts from index 0. See equivalent constants in
  // property_tree_manager.cc for comments.
  static const int kInvalidNodeId = -1;
  static const int kRootNodeId = 0;
  static const int kSecondaryRootNodeId = 1;

#if DCHECK_IS_ON()
  bool operator==(const PropertyTree<T>& other) const;
#endif

  int Insert(const T& tree_node, int parent_id);

  T* Node(int i) {
    DCHECK(i < static_cast<int>(nodes_.size()));
    return i > kInvalidNodeId ? &nodes_[i] : nullptr;
  }
  const T* Node(int i) const {
    DCHECK(i < static_cast<int>(nodes_.size()));
    return i > kInvalidNodeId ? &nodes_[i] : nullptr;
  }

  T* parent(const T* t) { return Node(t->parent_id); }
  const T* parent(const T* t) const { return Node(t->parent_id); }

  T* back() { return size() ? &nodes_.back() : nullptr; }
  const T* back() const { return size() ? &nodes_.back() : nullptr; }

  void clear();
  size_t size() const { return nodes_.size(); }

  virtual void set_needs_update(bool needs_update) {
    needs_update_ = needs_update;
  }
  bool needs_update() const { return needs_update_; }

  std::vector<T>& nodes() { return nodes_; }
  const std::vector<T>& nodes() const { return nodes_; }

  int next_available_id() const { return static_cast<int>(size()); }

  void SetPropertyTrees(PropertyTrees* property_trees) {
    property_trees_ = property_trees;
  }
  PropertyTrees* property_trees() const { return property_trees_; }

  void AsValueInto(base::trace_event::TracedValue* value) const;

 protected:
  std::vector<T> nodes_;
  bool needs_update_;
  PropertyTrees* property_trees_;
};

struct StickyPositionNodeData;

class CC_EXPORT TransformTree final : public PropertyTree<TransformNode> {
 public:
  TransformTree();

  // These C++ special member functions cannot be implicit inline because
  // they are exported by CC_EXPORT. They will be instantiated in every
  // compilation units that included this header, and compilation can fail
  // because TransformCachedNodeData may be incomplete.
  TransformTree(const TransformTree&) = delete;
  ~TransformTree() final;
  TransformTree& operator=(const TransformTree&);

#if DCHECK_IS_ON()
  bool operator==(const TransformTree& other) const;
#endif

  static const int kContentsRootNodeId = 1;

  int Insert(const TransformNode& tree_node, int parent_id);

  void clear();

  TransformNode* FindNodeFromElementId(ElementId id);
  bool OnTransformAnimated(ElementId element_id,
                           const gfx::Transform& transform);
  void ResetChangeTracking();
  // Updates the parent, target, and screen space transforms and snapping.
  void UpdateTransforms(int id);
  void UpdateTransformChanged(TransformNode* node, TransformNode* parent_node);
  void UpdateNodeAndAncestorsAreAnimatedOrInvertible(
      TransformNode* node,
      TransformNode* parent_node);
  void UpdateNodeOrAncestorsWillChangeTransform(TransformNode* node,
                                                TransformNode* parent_node);

  void set_needs_update(bool needs_update) final;

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

  const StickyPositionNodeData* GetStickyPositionData(int node_id) const {
    return const_cast<TransformTree*>(this)->MutableStickyPositionData(node_id);
  }
  StickyPositionNodeData& EnsureStickyPositionData(int node_id);

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
  void UpdateLocalTransform(TransformNode* node);
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
};

struct StickyPositionNodeData {
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
      : scroll_ancestor(TransformTree::kInvalidNodeId),
        nearest_node_shifting_sticky_box(TransformTree::kInvalidNodeId),
        nearest_node_shifting_containing_block(TransformTree::kInvalidNodeId) {}
};

class CC_EXPORT ClipTree final : public PropertyTree<ClipNode> {
 public:
#if DCHECK_IS_ON()
  bool operator==(const ClipTree& other) const;
#endif

  static const int kViewportNodeId = 1;

  void SetViewportClip(gfx::RectF viewport_rect);
  gfx::RectF ViewportClip() const;
};

class CC_EXPORT EffectTree final : public PropertyTree<EffectNode> {
 public:
  EffectTree();
  ~EffectTree() final;

  EffectTree& operator=(const EffectTree& from);

#if DCHECK_IS_ON()
  bool operator==(const EffectTree& other) const;
#endif

  static const int kContentsRootNodeId = 1;

  int Insert(const EffectNode& tree_node, int parent_id);

  void clear();

  float EffectiveOpacity(const EffectNode* node) const;

  void UpdateSurfaceContentsScale(EffectNode* node);

  EffectNode* FindNodeFromElementId(ElementId id);
  bool OnOpacityAnimated(ElementId id, float opacity);
  bool OnFilterAnimated(ElementId id, const FilterOperations& filters);
  bool OnBackdropFilterAnimated(ElementId id,
                                const FilterOperations& backdrop_filters);

  void UpdateEffects(int id);

  void UpdateEffectChanged(EffectNode* node, EffectNode* parent_node);

  void UpdateHasFilters(EffectNode* node, EffectNode* parent_node);

  void AddCopyRequest(int node_id,
                      std::unique_ptr<viz::CopyOutputRequest> request);
  void PushCopyRequestsTo(EffectTree* other_tree);
  void TakeCopyRequestsAndTransformToSurface(
      int node_id,
      std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests);
  bool HasCopyRequests() const;
  void ClearCopyRequests();

  // Given the ids of two effect nodes that have render surfaces, returns the
  // id of the lowest common ancestor effect node that also has a render
  // surface.
  int LowestCommonAncestorWithRenderSurface(int id_1, int id_2) const;

  RenderSurfaceImpl* GetRenderSurface(int id) {
    return render_surfaces_[id].get();
  }

  const RenderSurfaceImpl* GetRenderSurface(int id) const {
    return render_surfaces_[id].get();
  }

  bool ContributesToDrawnSurface(int id);

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

 private:
  void UpdateOpacities(EffectNode* node, EffectNode* parent_node);
  void UpdateSubtreeHidden(EffectNode* node, EffectNode* parent_node);
  void UpdateIsDrawn(EffectNode* node, EffectNode* parent_node);
  void UpdateBackfaceVisibility(EffectNode* node, EffectNode* parent_node);
  void UpdateHasMaskingChild(EffectNode* node, EffectNode* parent_node);
  void UpdateOnlyDrawsVisibleContent(EffectNode* node, EffectNode* parent_node);

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
  virtual void DidScroll(ElementId scroll_element_id,
                         const gfx::ScrollOffset&,
                         const base::Optional<TargetSnapAreaElementIds>&) = 0;
  // Called after the hidden status of composited scrollbars changed. Note that
  // |scroll_element_id| is the element id of the scroll not of the scrollbars.
  virtual void DidChangeScrollbarsHidden(ElementId scroll_element_id,
                                         bool hidden) = 0;

 protected:
  virtual ~ScrollCallbacks() {}
};

class CC_EXPORT ScrollTree final : public PropertyTree<ScrollNode> {
 public:
  ScrollTree();
  ~ScrollTree() final;

  ScrollTree& operator=(const ScrollTree& from);

#if DCHECK_IS_ON()
  bool operator==(const ScrollTree& other) const;
#endif

  void clear();

  gfx::ScrollOffset MaxScrollOffset(int scroll_node_id) const;
  void OnScrollOffsetAnimated(ElementId id,
                              int scroll_tree_index,
                              const gfx::ScrollOffset& scroll_offset,
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
  const gfx::ScrollOffset current_scroll_offset(ElementId id) const;

  // Returns the scroll offset taking into account any adjustments that may be
  // included due to pixel snapping.
  //
  // Note: Using this method may causes the associated transform node for this
  // scroll node to update its transforms.
  //
  // TODO(crbug.com/585458): Updating single transform node only works for
  // simple cases but we really should update the whole transform tree otherwise
  // we are ignoring any parent transform node that needs updating and thus our
  // snap amount can be incorrect.
  const gfx::ScrollOffset GetPixelSnappedScrollOffset(int scroll_node_id) const;

  // Collects deltas for scroll changes on the impl thread that need to be
  // reported to the main thread during the main frame. As such, should only be
  // called on the impl thread side PropertyTrees.
  void CollectScrollDeltas(CompositorCommitData* commit_data,
                           ElementId inner_viewport_scroll_element_id,
                           bool use_fractional_deltas,
                           const base::flat_set<ElementId>& snapped_elements);

  // Applies deltas sent in the previous main frame onto the impl thread state.
  // Should only be called on the impl thread side PropertyTrees.
  void ApplySentScrollDeltasFromAbortedCommit();

  // Pushes scroll updates from the ScrollTree on the main thread onto the
  // impl thread associated state.
  void PushScrollUpdatesFromMainThread(PropertyTrees* main_property_trees,
                                       LayerTreeImpl* sync_tree);

  // Pushes scroll updates from the ScrollTree on the pending tree onto the
  // active tree associated state.
  void PushScrollUpdatesFromPendingTree(PropertyTrees* pending_property_trees,
                                        LayerTreeImpl* active_tree);

  void SetBaseScrollOffset(ElementId id,
                           const gfx::ScrollOffset& scroll_offset);
  // Returns true if the scroll offset is changed.
  bool SetScrollOffset(ElementId id, const gfx::ScrollOffset& scroll_offset);
  void SetScrollOffsetClobberActiveValue(ElementId id) {
    GetOrCreateSyncedScrollOffset(id)->set_clobber_active_value();
  }
  bool UpdateScrollOffsetBaseForTesting(ElementId id,
                                        const gfx::ScrollOffset& offset);
  bool SetScrollOffsetDeltaForTesting(ElementId id,
                                      const gfx::Vector2dF& delta);
  const gfx::ScrollOffset GetScrollOffsetBaseForTesting(ElementId id) const;
  const gfx::ScrollOffset GetScrollOffsetDeltaForTesting(ElementId id) const;
  void CollectScrollDeltasForTesting();

  gfx::Vector2dF ScrollBy(const ScrollNode& scroll_node,
                          const gfx::Vector2dF& scroll,
                          LayerTreeImpl* layer_tree_impl);
  gfx::ScrollOffset ClampScrollOffsetToLimits(
      gfx::ScrollOffset offset,
      const ScrollNode& scroll_node) const;

  const SyncedScrollOffset* GetSyncedScrollOffset(ElementId id) const;

#if DCHECK_IS_ON()
  void CopyCompleteTreeState(const ScrollTree& other);
#endif

  ScrollNode* FindNodeFromElementId(ElementId id);
  const ScrollNode* FindNodeFromElementId(ElementId id) const;

  void SetScrollCallbacks(base::WeakPtr<ScrollCallbacks> callbacks);

  void NotifyDidScroll(
      ElementId scroll_element_id,
      const gfx::ScrollOffset& scroll_offset,
      const base::Optional<TargetSnapAreaElementIds>& snap_target_ids);
  void NotifyDidChangeScrollbarsHidden(ElementId scroll_element_id,
                                       bool hidden);

  // A composited scroll node is a scroll node that has an associated composited
  // layer, otherwise the scroll node corresponds to a scroller that requires
  // repainting.
  bool IsComposited(const ScrollNode& node) const;

 private:
  // ScrollTree doesn't use the needs_update flag.
  using PropertyTree::needs_update;
  using PropertyTree::set_needs_update;

  using ScrollOffsetMap = base::flat_map<ElementId, gfx::ScrollOffset>;
  using SyncedScrollOffsetMap =
      base::flat_map<ElementId, scoped_refptr<SyncedScrollOffset>>;

  int currently_scrolling_node_id_;

  // On the main thread we store the scroll offsets directly since the main
  // thread only needs to keep track of the current main thread state. The impl
  // thread stores a map of SyncedProperty instances in order to track
  // additional state necessary to synchronize scroll changes between the main
  // and impl threads.
  ScrollOffsetMap scroll_offset_map_;
  SyncedScrollOffsetMap synced_scroll_offset_map_;

  base::WeakPtr<ScrollCallbacks> callbacks_;

  SyncedScrollOffset* GetOrCreateSyncedScrollOffset(ElementId id);
  gfx::ScrollOffset PullDeltaForMainThread(SyncedScrollOffset* scroll_offset,
                                           bool use_fractional_deltas);
};

struct AnimationScaleData {
  // Variable used to invalidate cached animation scale data when transform tree
  // updates.
  int update_number;

  // The maximum scale that this node's |to_target| transform will have during
  // current animations, considering only scales at keyframes not incuding the
  // starting keyframe of each animation.
  float combined_maximum_animation_target_scale;

  // The maximum scale that this node's |to_target| transform will have during
  // current animations, considering only the starting scale of each animation.
  float combined_starting_animation_scale;

  bool to_screen_has_scale_animation;

  AnimationScaleData() {
    update_number = -1;
    combined_maximum_animation_target_scale = 0.f;
    combined_starting_animation_scale = 0.f;
    to_screen_has_scale_animation = false;
  }
};

struct CombinedAnimationScale {
  float maximum_animation_scale;
  float starting_animation_scale;

  CombinedAnimationScale(float maximum, float starting)
      : maximum_animation_scale(maximum), starting_animation_scale(starting) {}
  bool operator==(const CombinedAnimationScale& other) const {
    return maximum_animation_scale == other.maximum_animation_scale &&
           starting_animation_scale == other.starting_animation_scale;
  }
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
  int update_number;
  int target_id;

  DrawTransforms transforms;

  // TODO(sunxd): Move screen space transforms here if it can improve
  // performance.
  DrawTransformData()
      : update_number(-1),
        target_id(EffectTree::kInvalidNodeId),
        transforms(gfx::Transform(), gfx::Transform()) {}
};

struct ConditionalClip {
  bool is_clipped;
  gfx::RectF clip_rect;
};

struct ClipRectData {
  int target_id;
  ConditionalClip clip;

  ClipRectData() : target_id(-1) {}
};

struct PropertyTreesCachedData {
  int transform_tree_update_number;
  std::vector<AnimationScaleData> animation_scales;
  mutable std::vector<std::vector<DrawTransformData>> draw_transforms;

  PropertyTreesCachedData();
  ~PropertyTreesCachedData();
};

class CC_EXPORT PropertyTrees final {
 public:
  PropertyTrees();
  PropertyTrees(const PropertyTrees& other) = delete;
  ~PropertyTrees();

  PropertyTrees& operator=(const PropertyTrees& from);

#if DCHECK_IS_ON()
  bool operator==(const PropertyTrees& other) const;
#endif

  // These maps allow mapping directly from a compositor element id to the
  // respective property node. This will eventually allow simplifying logic in
  // various places that today has to map from element id to layer id, and then
  // from layer id to the respective property node. Completing that work is
  // pending the launch of BlinkGenPropertyTrees and reworking UI compositor
  // logic to produce cc property trees and these maps.
  base::flat_map<ElementId, int> element_id_to_effect_node_index;
  base::flat_map<ElementId, int> element_id_to_scroll_node_index;
  base::flat_map<ElementId, int> element_id_to_transform_node_index;

  TransformTree transform_tree;
  EffectTree effect_tree;
  ClipTree clip_tree;
  ScrollTree scroll_tree;
  bool needs_rebuild;
  // Change tracking done on property trees needs to be preserved across commits
  // (when they are not rebuild). We cache a global bool which stores whether
  // we did any change tracking so that we can skip copying the change status
  // between property trees when this bool is false.
  bool changed;
  // We cache a global bool for full tree damages to avoid walking the entire
  // tree.
  // TODO(jaydasika): Changes to transform and effects that damage the entire
  // tree should be tracked by this bool. Currently, they are tracked by the
  // individual nodes.
  bool full_tree_damaged;
  int sequence_number;
  bool is_main_thread;
  bool is_active;

  void clear();

  // Applies an animation state change for a particular element in
  // this property tree. Returns whether a draw property update is
  // needed.
  bool ElementIsAnimatingChanged(const PropertyToElementIdMap& element_id_map,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state,
                                 bool check_node_existence);
  void AnimationScalesChanged(ElementId element_id,
                              float maximum_scale,
                              float starting_scale);
  void SetInnerViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void SetOuterViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void UpdateChangeTracking();
  void PushChangeTrackingTo(PropertyTrees* tree);
  void ResetAllChangeTracking();

  gfx::Vector2dF inner_viewport_container_bounds_delta() const {
    return inner_viewport_container_bounds_delta_;
  }
  gfx::Vector2dF inner_viewport_scroll_bounds_delta() const {
    // Inner viewport scroll bounds are always the same as outer viewport
    // container bounds.
    return outer_viewport_container_bounds_delta_;
  }
  gfx::Vector2dF outer_viewport_container_bounds_delta() const {
    return outer_viewport_container_bounds_delta_;
  }

  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;
  void AsValueInto(base::trace_event::TracedValue* value) const;
  std::string ToString() const;

  CombinedAnimationScale GetAnimationScales(int transform_node_id,
                                            LayerTreeImpl* layer_tree_impl);
  void SetAnimationScalesForTesting(int transform_id,
                                    float maximum_animation_scale,
                                    float starting_animation_scale);

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
  gfx::Vector2dF inner_viewport_container_bounds_delta_;
  gfx::Vector2dF outer_viewport_container_bounds_delta_;

  // GetDrawTransforms may change the value of cached_data_.
  DrawTransforms& GetDrawTransforms(int transform_id, int effect_id) const;
  DrawTransformData& FetchDrawTransformsDataFromCache(int transform_id,
                                                      int effect_id) const;

  PropertyTreesCachedData cached_data_;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_H_
