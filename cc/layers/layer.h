// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/touch_action_region.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_record.h"
#include "cc/trees/element_id.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace viz {
class CopyOutputRequest;
}

namespace cc {

class LayerClient;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostCommon;
class LayerTreeImpl;
class PictureLayer;

// Base class for composited layers. Special layer types are derived from
// this class. Each layer is an independent unit in the compositor, be that
// for transforming or for content. If a layer has content it can be
// transformed efficiently without requiring the content to be recreated.
// Layers form a tree, with each layer having 0 or more children, and a single
// parent (or none at the root). Layers within the tree, other than the root
// layer, are kept alive by that tree relationship, with refpointer ownership
// from parents to children.
class CC_EXPORT Layer : public base::RefCounted<Layer> {
 public:
  // An invalid layer id, as all layer ids are positive.
  enum LayerIdLabels {
    INVALID_ID = -1,
  };

  // A layer can be attached to another layer as a mask for it. These
  // describe how the mask would be generated as a texture in that case.
  enum LayerMaskType {
    NOT_MASK = 0,
    MULTI_TEXTURE_MASK,
    SINGLE_TEXTURE_MASK,
  };

  // Factory to create a new Layer, with a unique id.
  static scoped_refptr<Layer> Create();

  // Sets an optional client on this layer, that will be called when relevant
  // events happen. The client is a WeakPtr so it can be destroyed without
  // unsetting itself as the client.
  void SetLayerClient(base::WeakPtr<LayerClient> client);
  LayerClient* GetLayerClientForTesting() const { return inputs_.client.get(); }

  // A unique and stable id for the Layer. Ids are always positive.
  int id() const { return inputs_.layer_id; }

  // Returns a pointer to the highest ancestor of this layer, or itself.
  Layer* RootLayer();
  // Returns a pointer to the direct ancestor of this layer if it exists,
  // or null.
  Layer* parent() { return parent_; }
  const Layer* parent() const { return parent_; }
  // Appends |child| to the list of children of this layer, and maintains
  // ownership of a reference to that |child|.
  void AddChild(scoped_refptr<Layer> child);
  // Inserts |child| into the list of children of this layer, before position
  // |index| (0 based) and maintains ownership of a reference to that |child|.
  void InsertChild(scoped_refptr<Layer> child, size_t index);
  // Removes an existing child |reference| from this layer's list of children,
  // and inserts |new_layer| it its place in the list. This layer maintains
  // ownership of a reference to the |new_layer|. The |new_layer| may be null,
  // in which case |reference| is simply removed from the list of children,
  // which ends this layers ownership of the child.
  void ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer);
  // Removes this layer from the list of children in its parent, removing the
  // parent's ownership of this layer.
  void RemoveFromParent();
  // Removes all children from this layer's list of children, removing ownership
  // of those children.
  void RemoveAllChildren();
  // Sets the children while minimizing changes to layers that are already
  // children of this layer.
  void SetChildLayerList(LayerList children);
  // Returns true if |ancestor| is this layer's parent or higher ancestor.
  bool HasAncestor(const Layer* ancestor) const;

  // The list of children of this layer.
  const LayerList& children() const { return inputs_.children; }

  // Gets the LayerTreeHost that this layer is attached to, or null if not.
  // A layer is attached to a LayerTreeHost if it or an ancestor layer is set as
  // the root layer of a LayerTreeHost (while noting only a layer without a
  // parent may be set as the root layer).
  LayerTreeHost* layer_tree_host() const { return layer_tree_host_; }

  // This requests the layer and its subtree be rendered and given to the
  // callback. If the copy is unable to be produced (the layer is destroyed
  // first), then the callback is called with a nullptr/empty result. If the
  // request's source property is set, any prior uncommitted requests having the
  // same source will be aborted.
  void RequestCopyOfOutput(std::unique_ptr<viz::CopyOutputRequest> request);
  // True if a copy request has been inserted on this layer and a commit has not
  // occured yet.
  bool HasCopyRequest() const { return !inputs_.copy_requests.empty(); }

  // Set and get the background color for the layer. This color is not used by
  // basic Layers, but subclasses may make use of it.
  virtual void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return inputs_.background_color; }

  // Internal to property tree generation. Sets an opaque background color for
  // the layer, to be used in place of the background_color() if the layer says
  // contents_opaque() is true.
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // Returns a background color with opaque-ness equal to the value of
  // contents_opaque().
  // If the layer says contents_opaque() is true, this returns the value set by
  // SetSafeOpaqueBackgroundColor() which should be an opaque color. Otherwise,
  // it returns something non-opaque. It prefers to return the
  // background_color(), but if the background_color() is opaque (and this layer
  // claims to not be), then SK_ColorTRANSPARENT is returned.
  SkColor SafeOpaqueBackgroundColor() const;

  // Set and get the position of this layer, relative to its parent. This is
  // specified in layer space, which excludes device scale and page scale
  // factors, and ignoring transforms for this layer or ancestor layers. The
  // root layer's position is not used as it always appears at the origin of
  // the viewport.
  void SetPosition(const gfx::PointF& position);
  const gfx::PointF& position() const { return inputs_.position; }

  // Set and get the layers bounds. This is specified in layer space, which
  // excludes device scale and page scale factors, and ignoring transforms for
  // this layer or ancestor layers.
  //
  // The root layer in the tree has bounds in viewport space, which includes
  // the device scale factor.
  void SetBounds(const gfx::Size& bounds);
  const gfx::Size& bounds() const { return inputs_.bounds; }

  // Set and get the behaviour to be applied for compositor-thread scrolling of
  // this layer beyond the beginning or end of the layer's content.
  // TODO(bokan): With blink-gen-property-trees this is stored on the
  // ScrollNode and can be removed here.
  void SetOverscrollBehavior(const OverscrollBehavior& behavior);
  OverscrollBehavior overscroll_behavior() const {
    return inputs_.overscroll_behavior;
  }

  // Set and get the snapping behaviour for compositor-thread scrolling of
  // this layer. The default value of null means there is no snapping for the
  // layer.
  // TODO(crbug.com/836884) With blink-gen-property-trees this is stored on the
  // ScrollNode and can be removed here.
  void SetSnapContainerData(base::Optional<SnapContainerData> data);
  const base::Optional<SnapContainerData>& snap_container_data() const {
    return inputs_.snap_container_data;
  }

  // Set or get that this layer clips its subtree to within its bounds. Content
  // of children will be intersected with the bounds of this layer when true.
  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return inputs_.masks_to_bounds; }

  // Set or get a layer that is not an ancestor of this layer, but which should
  // be clipped to this layer's bounds if SetMasksToBounds() is set to true.
  // The parent layer does *not* retain ownership of a reference on this layer.
  void SetClipParent(Layer* ancestor);
  Layer* clip_parent() { return inputs_.clip_parent; }

  // The set of layers which are not in this layers subtree but which should be
  // clipped to only appear within this layer's bounds.
  std::set<Layer*>* clip_children() { return clip_children_.get(); }
  const std::set<Layer*>* clip_children() const { return clip_children_.get(); }

  // Set or get a layer that will mask the contents of this layer. The alpha
  // channel of the mask layer's content is used as an alpha mask of this
  // layer's content. IOW the mask's alpha is multiplied by this layer's alpha
  // for each matching pixel.
  void SetMaskLayer(PictureLayer* mask_layer);
  PictureLayer* mask_layer() { return inputs_.mask_layer.get(); }
  const PictureLayer* mask_layer() const { return inputs_.mask_layer.get(); }

  // Marks the |dirty_rect| as being changed, which will cause a commit and
  // the compositor to submit a new frame with a damage rect that includes the
  // layer's dirty area. This rect is in layer space, the same as bounds().
  virtual void SetNeedsDisplayRect(const gfx::Rect& dirty_rect);
  // Marks the entire layer's bounds as being changed, which will cause a commit
  // and the compositor to submit a new frame with a damage rect that includes
  // the entire layer. Note that if the layer resizes afterward, but before
  // commit, the dirty rect would not cover the layer, however then the layer
  // bounds change would implicitly damage the full layer.
  void SetNeedsDisplay() { SetNeedsDisplayRect(gfx::Rect(bounds())); }
  // Returns the union of previous calls to SetNeedsDisplayRect() and
  // SetNeedsDisplay() that have not been committed to the compositor thread.
  const gfx::Rect& update_rect() const { return inputs_.update_rect; }

  // Set or get the opacity which should be applied to the contents of the layer
  // and its subtree (together as a single composited entity) when blending them
  // into their target. Note that this does not speak to the contents of this
  // layer, which may be opaque or not (see contents_opaque()). Note that the
  // opacity is cumulative since it applies to the layer's subtree.
  virtual void SetOpacity(float opacity);
  float opacity() const { return inputs_.opacity; }
  // Gets the true opacity that will be used for blending the contents of this
  // layer and its subtree into its target during composite. This value is the
  // same as the user-specified opacity() unless the layer should not be visible
  // at all for other reasons, in which case the opacity here becomes 0.
  float EffectiveOpacity() const;

  // Set or get the blend mode to be applied when blending the contents of the
  // layer and its subtree (together as a single composited entity) when
  // blending them into their target.
  void SetBlendMode(SkBlendMode blend_mode);
  SkBlendMode blend_mode() const { return inputs_.blend_mode; }

  // A layer is root for an isolated group when it and all its descendants are
  // drawn over a black and fully transparent background, creating an isolated
  // group. It should be used along with SetBlendMode(), in order to restrict
  // layers within the group to blend with layers outside this group.
  void SetIsRootForIsolatedGroup(bool root);
  bool is_root_for_isolated_group() const {
    return inputs_.is_root_for_isolated_group;
  }

  // Set or get the list of filter effects to be applied to the contents of the
  // layer and its subtree (together as a single composited entity) when
  // drawing them into their target.
  void SetFilters(const FilterOperations& filters);
  const FilterOperations& filters() const { return inputs_.filters; }

  // Set or get the origin to be used when applying the filters given to
  // SetFilters(). By default the origin is at the origin of this layer, but
  // may be moved positively or negatively relative to that. The origin effects
  // any filters which do not apply uniformly to the entire layer and its
  // subtree.
  void SetFiltersOrigin(const gfx::PointF& origin);
  gfx::PointF filters_origin() const { return inputs_.filters_origin; }

  // Set or get the list of filters that should be applied to the content this
  // layer and its subtree will be drawn into. The effect is clipped to only
  // apply directly behind this layer and its subtree.
  void SetBackdropFilters(const FilterOperations& filters);
  const FilterOperations& backdrop_filters() const {
    return inputs_.backdrop_filters;
  }

  // Set or get an optimization hint that the contents of this layer are fully
  // opaque or not. If true, every pixel of content inside the layer's bounds
  // must be opaque or visual errors can occur. This applies only to this layer
  // and not to children, and does not imply the layer should be composited
  // opaquely, as effects may be applied such as opacity() or filters().
  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return inputs_.contents_opaque; }

  // Set or get whether this layer should be a hit test target even if not
  // visible. Normally if DrawsContent() is false, making the layer not
  // contribute to the final composited output, the layer will not be eligable
  // for hit testing since it is invisible. Set this to true to allow the layer
  // to be hit tested regardless.
  void SetHitTestableWithoutDrawsContent(bool should_hit_test);
  bool hit_testable_without_draws_content() const {
    return inputs_.hit_testable_without_draws_content;
  }

  // Set or gets if this layer is a container for fixed position layers in its
  // subtree. Such layers will be positioned and transformed relative to this
  // layer instead of their direct parent.
  //
  // A layer that is a container for fixed position layers cannot be both
  // scrollable and have a non-identity transform.
  void SetIsContainerForFixedPositionLayers(bool container);
  bool IsContainerForFixedPositionLayers() const;

  // Set or get constraints applied to the layer's position, where it may be
  // in a fixed position relative to the nearest ancestor that returns true for
  // IsContainerForFixedPositionLayers(). This may also specify which edges
  // of the layer are fixed to the same edges of the container ancestor. When
  // fixed position, this layer's transform will be appended to the container
  // ancestor's transform instead of to this layer's direct parent's.
  void SetPositionConstraint(const LayerPositionConstraint& constraint);
  const LayerPositionConstraint& position_constraint() const {
    return inputs_.position_constraint;
  }

  // Set or get constraints applied to the layer's position, where it may act
  // like a normal layer until, during scroll, its position triggers it to
  // become fixed position relative to its scroller. See CSS position: sticky
  // for more details.
  void SetStickyPositionConstraint(
      const LayerStickyPositionConstraint& constraint);
  const LayerStickyPositionConstraint& sticky_position_constraint() const {
    return inputs_.sticky_position_constraint;
  }

  // On some platforms (Android renderer) the viewport may resize during scroll
  // on the compositor thread. During this resize and until the main thread
  // matches, position fixed layers may need to have their position adjusted on
  // the compositor thread to keep them fixed in place.  If
  // IsContainerForFixedPositionLayers() is true for this layer, these set and
  // get whether fixed position descendants of this layer should have this
  // adjustment to their position applied during such a viewport resize.
  void SetIsResizedByBrowserControls(bool resized);
  bool IsResizedByBrowserControls() const;

  // Set or get the transform to be used when compositing this layer into its
  // target. The transform is inherited by this layers children.
  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return inputs_.transform; }

  // Gets the transform, including transform origin and position, of this layer
  // and its ancestors, device scale and page scale factors, into the device
  // viewport.
  gfx::Transform ScreenSpaceTransform() const;

  // Set or get the origin to be used when applying the transform. The value is
  // a position in layer space, relative to the top left corner of this layer.
  // For instance, if set to the center of the layer, with a transform to rotate
  // 180deg around the X axis, it would flip the layer vertically around the
  // center of the layer, leaving it occupying the same space. Whereas set to
  // the top left of the layer, the rotation wouldoccur around the top of the
  // layer, moving it vertically while flipping it.
  void SetTransformOrigin(const gfx::Point3F&);
  const gfx::Point3F& transform_origin() const {
    return inputs_.transform_origin;
  }

  // Set or get a scroll parent layer. It is not an ancestor of this layer, but
  // this layer will be moved by the scroll parent's scroll offset.
  void SetScrollParent(Layer* parent);
  Layer* scroll_parent() { return inputs_.scroll_parent; }

  // Set or get the scroll offset of the layer. The content of the layer, and
  // position of its subtree, as well as other layers for which this layer is
  // their scroll parent, and their subtrees) is moved up by the amount of
  // offset specified here.
  void SetScrollOffset(const gfx::ScrollOffset& scroll_offset);
  // Accessor named to match LayerImpl for templated code.
  const gfx::ScrollOffset& CurrentScrollOffset() const {
    return inputs_.scroll_offset;
  }

  // Called internally during commit to update the layer with state from the
  // compositor thread. Not to be called externally by users of this class.
  void SetScrollOffsetFromImplSide(const gfx::ScrollOffset& scroll_offset);

  // Marks this layer as being scrollable and needing an associated scroll node,
  // and specifies the total size of the content to be scrolled (ie the max
  // scroll offsets. The size should be a union of the layer and its subtree, as
  // well as any layers for whom this layer is their scroll parent, and their
  // subtrees, when they are transformed into this layer's space. Thus
  // transforms of children affect the size of the |scroll_container_bounds|.
  // Once scrollable, a Layer cannot become un-scrollable.
  void SetScrollable(const gfx::Size& scroll_container_bounds);
  bool scrollable() const { return inputs_.scrollable; }
  const gfx::Size& scroll_container_bounds() const {
    return inputs_.scroll_container_bounds;
  }

  void SetIsScrollbar(bool is_scrollbar);
  bool is_scrollbar() const { return inputs_.is_scrollbar; }

  // Set or get if this layer is able to be scrolled along each axis. These are
  // independant of the scrollable state, or size of the scrollable area
  // specified in SetScrollable(), as these may be enabled or disabled
  // dynamically, while SetScrollable() defines what would be possible if these
  // are enabled.
  // When disabled, overscroll elasticity will not be used if the scroll offset
  // ends up past the maximum range. And when enabled, with overlay scrollbars,
  // the scrollbars will be shown when the scroll offset changes if these are
  // set to true.
  void SetUserScrollable(bool horizontal, bool vertical);
  bool user_scrollable_horizontal() const {
    return inputs_.user_scrollable_horizontal;
  }
  bool user_scrollable_vertical() const {
    return inputs_.user_scrollable_vertical;
  }

  // Set or get if this layer is able to be scrolled on the compositor thread.
  // This only applies for layers that are marked as scrollable, not for layers
  // that are moved by a scroll parent. When any reason is present, the layer
  // will not be scrolled on the compositor thread. The reasons are a set of
  // bitflags from MainThreadScrollingReason, used to track the reason for
  // debugging and reporting.
  // AddMainThreadScrollingReasons() is used to add flags to the current set,
  // and ClearMainThreadScrollingReasons() removes flags from the current set.
  void AddMainThreadScrollingReasons(uint32_t main_thread_scrolling_reasons);
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear);
  uint32_t main_thread_scrolling_reasons() const {
    return inputs_.main_thread_scrolling_reasons;
  }

  // Set or get an area of this layer within which initiating a scroll can not
  // be done from the compositor thread. Within this area, if the user attempts
  // to start a scroll, the events must be sent to the main thread and processed
  // there.
  void SetNonFastScrollableRegion(const Region& non_fast_scrollable_region);
  const Region& non_fast_scrollable_region() const {
    return inputs_.non_fast_scrollable_region;
  }

  // Set or get the set of touch actions allowed across each point of this
  // layer. The |touch_action_region| can specify, for any number of areas,
  // which touch actions are allowed in each area. The result is the
  // intersection of overlapping areas. These allowed actions control if
  // a touch event can initiate a scroll or zoom on the compositor thread.
  void SetTouchActionRegion(TouchActionRegion touch_action_region);
  const TouchActionRegion& touch_action_region() const {
    return inputs_.touch_action_region;
  }

  // Sets a RepeatingCallback that is run during a main frame, before layers are
  // asked to prepare content with Update(), if the scroll offset for the layer
  // was changed by the InputHandlerClient, on the compositor thread (or on the
  // main thread in single-thread mode). It may be set to a null callback, in
  // which case nothing is called.
  void set_did_scroll_callback(
      base::RepeatingCallback<void(const gfx::ScrollOffset&, const ElementId&)>
          callback) {
    inputs_.did_scroll_callback = std::move(callback);
  }

  // Set or get if the layer and its subtree should be cached as a texture in
  // the display compositor. This is used as an optimization when it is known
  // that the layer will be animated without changing its content, or any of its
  // subtree.
  //
  // Note that this also disables occlusion culling, as the entire texture will
  // be drawn so that it is not left with incomplete areas. This should only be
  // used when paying the cost of creating an intermediate texture is worth it,
  // even when the layer's subtree may be occluded, or not visible in the final
  // output.
  void SetCacheRenderSurface(bool cache_render_surface);
  bool cache_render_surface() const { return cache_render_surface_; }

  // Set or get if the layer and its subtree will be drawn through an
  // intermediate texture, called a RenderSurface. This mimics the need
  // for a RenderSurface that is caused by compositing effects such as masks
  // without needing to set up such effects.
  void SetForceRenderSurfaceForTesting(bool force_render_surface);
  bool force_render_surface_for_testing() const {
    return force_render_surface_for_testing_;
  }

  // Set or get if this layer should continue to be visible when rotated such
  // that its back face is facing toward the camera. If false, the layer will
  // disappear when its back face is visible, but if true, the mirror image of
  // its front face will be shown. For instance, with a 180deg rotation around
  // the middle of the layer on the Y axis, if this is false then nothing is
  // visible. But if true, the layer is seen with its contents flipped along the
  // Y axis. Being single-sided applies transitively to the subtree of this
  // layer. If it is hidden because of its back face being visible, then its
  // subtree will be too (even if a subtree layer's front face would have been
  // visible).
  //
  // Note that should_check_backface_visibility() is the final computed value
  // for back face visibility, which is only for internal use.
  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return inputs_.double_sided; }

  // Set or get if SetDoubleSided() for this layer should be ignored and
  // inherited directly from this layer's parent instead. Used to attach this
  // layer's backface visibility to the value of its parent.
  //
  // Note that should_check_backface_visibility() is the final computed value
  // for back face visibility, which is only for internal use.
  void SetUseParentBackfaceVisibility(bool use);
  bool use_parent_backface_visibility() const {
    return inputs_.use_parent_backface_visibility;
  }

  // Set or get if the subtree of this layer is composited in 3d-space, or if
  // the layers are flattened into the plane of this layer. This supports the
  // transform-style CSS property.
  void SetShouldFlattenTransform(bool flatten);
  bool should_flatten_transform() const {
    return inputs_.should_flatten_transform;
  }

  // Set or get a 3d sorting context for this layer, where adjacent layers (in a
  // pre-order traversal) with the same id are sorted as a group and may occlude
  // each other based on their z-position, including intersecting each other and
  // each occluding the other layer partially. Layers in different sorting
  // contexts will be composited and occlude in tree order (children occlude
  // ancestors and earlier siblings in the children list). If the |id| is 0,
  // then the layer is not part of any sorting context, and is always composited
  // in tree order.
  void Set3dSortingContextId(int id);
  int sorting_context_id() const { return inputs_.sorting_context_id; }

  // When true the layer may contribute to the compositor's output. When false,
  // it does not. This property does not apply to children of the layer, they
  // may contribute while this layer does not. The layer itself will determine
  // if it has content to contribute, but when false, this prevents it from
  // doing so.
  void SetIsDrawable(bool is_drawable);
  // Is true if the layer will contribute content to the compositor's output.
  // Will be false if SetIsDrawable(false) is called. But will also be false if
  // the layer itself has no content to contribute, even though the layer was
  // given SetIsDrawable(true).
  bool DrawsContent() const;

  // Returns the number of layers in this layers subtree (excluding itself) for
  // which DrawsContent() is true.
  int NumDescendantsThatDrawContent() const;

  // Set or get if this layer and its subtree should be part of the compositor's
  // output to the screen. When set to true, the layer's subtree does not appear
  // to the user, but still remains part of the tree with all its normal drawing
  // properties. This can be used to execute a CopyOutputRequest on this layer
  // or another in its subtree, since the layers are still able to be drawn by
  // the compositor, while not being composed into the result shown to the user.
  void SetHideLayerAndSubtree(bool hide);
  bool hide_layer_and_subtree() const { return inputs_.hide_layer_and_subtree; }

  // The index of this layer's node in the various property trees. These are
  // only valid after a main frame, when Update() is called on the layer, and
  // remain valid and in in the same state until the next main frame, or until
  // the layer is removed from its LayerTreeHost. Otherwise kInvalidNodeId is
  // returned.
  int transform_tree_index() const;
  int clip_tree_index() const;
  int effect_tree_index() const;
  int scroll_tree_index() const;

  // While all layers have an index into the transform tree, this value
  // indicates whether the transform tree node was created for this layer.
  void SetHasTransformNode(bool val) { has_transform_node_ = val; }
  bool has_transform_node() { return has_transform_node_; }

  // Sets that the content shown in this layer may be a video. This may be used
  // by the system compositor to distinguish between animations updating the
  // screen and video, which the user would be watching. This allows
  // optimizations like turning off the display when video is not playing,
  // without interfering with video playback.
  void SetMayContainVideo(bool yes);

  // Stable identifier for clients. See comment in cc/trees/element_id.h.
  void SetElementId(ElementId id);
  ElementId element_id() const { return inputs_.element_id; }

  // Sets or gets a hint that the transform on this layer (including its
  // position) may be changed often in the future. The layer may change its
  // strategy for generating content as a result. PictureLayers will not attempt
  // to raster crisply as the transform changes, allowing the client to trade
  // off crisp content at each scale for a smoother visual and cheaper
  // animation.
  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return inputs_.has_will_change_transform_hint;
  }

  // Sets or gets if trilinear filtering should be used to scaling the contents
  // of this layer and its subtree. When set the layer and its subtree will be
  // composited together as a single unit, mip maps will be generated of the
  // subtree together, and trilinear filtering applied when supported, if
  // scaling during composite of the content from this layer and its subtree
  // into the target.
  void SetTrilinearFiltering(bool trilinear_filtering);
  bool trilinear_filtering() const { return inputs_.trilinear_filtering; }

  // Called on the scroll layer to trigger showing the overlay scrollbars.
  void ShowScrollbars() { needs_show_scrollbars_ = true; }

  // For tracing. Gets a recorded rasterization of this layer's contents that
  // can be displayed inside representations of this layer. May return null, in
  // which case the layer won't be shown with any content in the tracing
  // display.
  virtual sk_sp<SkPicture> GetPicture() const;

  // For tracing. Calls out to the LayerClient to get tracing data that will
  // be attached to this layer's tracing outputs under the 'debug_info' key.
  void UpdateDebugInfo();

  // For telemetry testing. Runs a given test behaviour implemented in
  // |benchmark| for this layer. The base class does nothing as benchmarks
  // only exist for subclass layer types. For each subclass that the
  // MicroBenchmark supports, the class should override this method and run the
  // |benchmark| against this layer.
  virtual void RunMicroBenchmark(MicroBenchmark* benchmark);

  // Internal method to create the compositor thread type for this Layer.
  // Subclasses should override this method if they want to return their own
  // subclass of LayerImpl instead.
  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);

  // Internal method to copy all state from this Layer to the compositor thread.
  // Should be overridden by any subclass that has additional state, to copy
  // that state as well. The |layer| passed in will be of the type created by
  // CreateLayerImpl(), so can be safely down-casted if the subclass uses a
  // different type for the compositor thread.
  virtual void PushPropertiesTo(LayerImpl* layer);

  // Internal method to be overridden by Layer subclasses that need to do work
  // during a main frame. The method should compute any state that will need to
  // propogated to the compositor thread for the next commit, and return true
  // if there is anything new to commit. If all layers return false, the commit
  // may be aborted.
  virtual bool Update();
  // Internal method to be overriden by Layer subclasses that override Update()
  // and require rasterization. After Update() is called, this is immediately
  // called, and should return whether the layer will require rasterization of
  // paths that will be difficult/slow to raster. Only layers that do
  // rasterization via TileManager need to override this, other layers that have
  // content generated in other ways may leave it as the default.
  virtual bool HasSlowPaths() const;
  // Internal method to be overriden by Layer subclasses that override Update()
  // and require rasterization. After Update() is called, this is immediately
  // called, and should return whether the layer will require rasterization of a
  // drawing operation that must not be anti-aliased. In this case using MSAA to
  // antialias the entire layer's content would produce an incorrect result.
  // This result is considered sticky, once a layer returns true, so false
  // positives should be avoided. Only layers that do rasterization via
  // TileManager need to override this, other layers that have content generated
  // in other ways may leave it as the default.
  virtual bool HasNonAAPaint() const;

  // Internal to property tree construction. This allows a layer to request that
  // its transform should be snapped such that the layer aligns with the pixel
  // grid in its rendering target. This ensures that the layer is not fuzzy
  // (unless it is being scaled). Layers may override this to return true, by
  // default layers are not snapped.
  virtual bool IsSnappedToPixelGridInTarget();

  // Internal method that is called when a Layer is attached to a LayerTreeHost.
  // This would happen when
  // a) the Layer is added to an existing Layer tree that is attached to a
  // LayerTreeHost.
  // b) the Layer is made the root layer of a LayerTreeHost.
  // c) the Layer is part of a Layer tree, and an ancestor is attached to a
  // LayerTreeHost via a) or b).
  // The |host| is the new LayerTreeHost which the Layer is now attached to.
  // Subclasses may override this if they have data or resources which are
  // specific to a LayerTreeHost that should be updated or reset. After this
  // returns the Layer will hold a pointer to the new LayerTreeHost.
  virtual void SetLayerTreeHost(LayerTreeHost* host);

  // Internal method to mark this layer as needing to push its state to the
  // compositor thread during the next commit. The PushPropertiesTo() method
  // will be called for this layer during the next commit only if this method
  // was called before it.
  void SetNeedsPushProperties();

  // Internal method to call the LayerClient, if there is one, to inform it when
  // overlay scrollbars have been completely hidden (due to lack of scrolling by
  // the user).
  void SetScrollbarsHiddenFromImplSide(bool hidden);

  // Internal to property tree construction. A generation number for the
  // property trees, to verify the layer's indices are pointers into the trees
  // currently held by the LayerTreeHost. The number is updated when property
  // trees are built from the Layer tree.
  void set_property_tree_sequence_number(int sequence_number) {
    property_tree_sequence_number_ = sequence_number;
  }
  int property_tree_sequence_number() const {
    return property_tree_sequence_number_;
  }

  // Internal to property tree construction. Sets the index for this Layer's
  // node in each property tree.
  void SetTransformTreeIndex(int index);
  void SetClipTreeIndex(int index);
  void SetEffectTreeIndex(int index);
  void SetScrollTreeIndex(int index);

  // Internal to property tree construction. Set or get the position of this
  // layer relative to the origin after transforming according to this layer's
  // index into the transform tree. This translation is appended to the
  // transform that comes from the transform tree for this layer.
  void SetOffsetToTransformParent(gfx::Vector2dF offset);
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  // Internal to property tree construction. Indicates that a property changed
  // on this layer that may affect the position or content of all layers in this
  // layer's subtree, including itself. This causes the layer's subtree to be
  // considered damaged and re-displayed to the user.
  void SetSubtreePropertyChanged();
  bool subtree_property_changed() const { return subtree_property_changed_; }

  // Internal to property tree construction. Returns ElementListType::ACTIVE
  // as main thread layers do not have a pending/active tree split, and
  // animations should run normally on the main thread layer tree.
  ElementListType GetElementTypeForAnimation() const;

  // Internal to property tree construction. Whether this layer may animate its
  // opacity on the compositor thread. Layer subclasses may override this to
  // report true. If true, assumptions about opacity can not be made on the main
  // thread.
  virtual bool OpacityCanAnimateOnImplThread() const;

  // Internal to property tree construction. Set to true if this layer or any
  // layer below it in the tree has a CopyOutputRequest pending commit.
  void SetSubtreeHasCopyRequest(bool subtree_has_copy_request);
  // Internal to property tree construction. Returns true if this layer or any
  // layer below it in the tree has a CopyOutputRequest pending commit.
  bool SubtreeHasCopyRequest() const;
  // Internal to property tree construction. Removes all CopyOutputRequests from
  // this layer, moving them into |requests|.
  void TakeCopyRequests(
      std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests);

  // Internal to property tree construction. Set if the layer should not be
  // shown when its back face is visible to the user. This is a derived value
  // from SetDoubleSided() and SetUseParentBackfaceVisibility().
  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility);
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  // Internal to property tree construction. The value here derives from
  // should_flatten_transform() along with other state, and is for internal use
  // in order to flatten the layer's ScreenSpaceTransform() in cases where the
  // property tree did not handle it.
  void SetShouldFlattenScreenSpaceTransformFromPropertyTree(bool should);
  bool should_flatten_screen_space_transform_from_property_tree() const {
    return should_flatten_screen_space_transform_from_property_tree_;
  }

  void set_is_rounded_corner_mask(bool rounded) {
    is_rounded_corner_mask_ = rounded;
  }

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;

  Layer();
  virtual ~Layer();

  // These SetNeeds functions are in order of severity of update:
  //
  // Called when a property has been modified in a way that the layer knows
  // immediately that a commit is required.  This implies SetNeedsPushProperties
  // to push that property.
  void SetNeedsCommit();

  // Called when there's been a change in layer structure.  Implies
  // SetNeedsCommit and property tree rebuld, but not SetNeedsPushProperties
  // (the full tree is synced over).
  void SetNeedsFullTreeSync();

  // Called when the next commit should wait until the pending tree is activated
  // before finishing the commit and unblocking the main thread. Used to ensure
  // unused resources on the impl thread are returned before commit completes.
  void SetNextCommitWaitsForActivation();

  // Will recalculate whether the layer draws content and set draws_content_
  // appropriately.
  void UpdateDrawsContent(bool has_drawable_content);
  // May be overridden by subclasses if they have optional content, to return
  // false if there is no content to be displayed. If they do have content, then
  // they should return the value from this base class method.
  virtual bool HasDrawableContent() const;

  // Called when the layer's number of drawable descendants changes.
  void AddDrawableDescendants(int num);

  // For debugging. Returns false if the LayerTreeHost this layer is attached to
  // is in the process of updating layers for a BeginMainFrame. Layer properties
  // should be changed by the client before the BeginMainFrame, and should not
  // be changed while the frame is being generated for commit.
  bool IsPropertyChangeAllowed() const;

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implicitly after the update completes.
  bool ignore_set_needs_commit_;

 private:
  friend class base::RefCounted<Layer>;
  friend class LayerTreeHostCommon;
  friend class LayerTreeHost;

  // Interactions with attached animations.
  void OnFilterAnimated(const FilterOperations& filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);

  void AddClipChild(Layer* child);
  void RemoveClipChild(Layer* child);

  void SetParent(Layer* layer);
  bool DescendantIsFixedToContainerLayer() const;

  // This should only be called from RemoveFromParent().
  void RemoveChildOrDependent(Layer* child);

  // If this layer has a clip parent, it removes |this| from its list of clip
  // children.
  void RemoveFromClipTree();

  // When we detach or attach layer to new LayerTreeHost, all property trees'
  // indices becomes invalid.
  void InvalidatePropertyTreesIndices();

  // This is set whenever a property changed on layer that affects whether this
  // layer should own a property tree node or not.
  void SetPropertyTreesNeedRebuild();

  // Fast-path for |SetScrollOffset| and |SetScrollOffsetFromImplSide| to
  // directly update scroll offset values in the property tree without needing a
  // full property tree update. If property trees do not exist yet, ensures
  // they are marked as needing to be rebuilt.
  void UpdateScrollOffset(const gfx::ScrollOffset&);

  // Encapsulates all data, callbacks or interfaces received from the embedder.
  // TODO(khushalsagar): This is only valid when PropertyTrees are built
  // internally in cc. Update this for the SPv2 path where blink generates
  // PropertyTrees.
  struct Inputs {
    explicit Inputs(int layer_id);
    ~Inputs();

    int layer_id;

    LayerList children;

    gfx::Rect update_rect;

    gfx::Size bounds;
    bool masks_to_bounds;

    scoped_refptr<PictureLayer> mask_layer;

    float opacity;
    SkBlendMode blend_mode;

    bool is_root_for_isolated_group : 1;

    // Hit testing depends on draws_content (see: |LayerImpl::should_hit_test|)
    // and this bit can be set to cause the LayerImpl to be hit testable without
    // draws_content.
    bool hit_testable_without_draws_content : 1;

    bool contents_opaque : 1;

    gfx::PointF position;
    gfx::Transform transform;
    gfx::Point3F transform_origin;

    bool is_drawable : 1;

    bool double_sided : 1;
    bool should_flatten_transform : 1;

    // Layers that share a sorting context id will be sorted together in 3d
    // space.  0 is a special value that means this layer will not be sorted
    // and will be drawn in paint order.
    int sorting_context_id;

    bool use_parent_backface_visibility : 1;

    SkColor background_color;

    FilterOperations filters;
    FilterOperations backdrop_filters;
    gfx::PointF filters_origin;

    gfx::ScrollOffset scroll_offset;

    // Size of the scroll container that this layer scrolls in.
    gfx::Size scroll_container_bounds;

    // Indicates that this layer will need a scroll property node and that this
    // layer's bounds correspond to the scroll node's bounds (both |bounds| and
    // |scroll_container_bounds|).
    bool scrollable : 1;

    // Indicates that this layer is a scrollbar.
    bool is_scrollbar : 1;

    bool user_scrollable_horizontal : 1;
    bool user_scrollable_vertical : 1;

    uint32_t main_thread_scrolling_reasons;
    Region non_fast_scrollable_region;

    TouchActionRegion touch_action_region;

    // When set, position: fixed children of this layer will be affected by URL
    // bar movement. bottom-fixed element will be pushed down as the URL bar
    // hides (and the viewport expands) so that the element stays fixed to the
    // viewport bottom. This will always be set on the outer viewport scroll
    // layer. In the case of a non-default rootScroller, all iframes in the
    // rootScroller ancestor chain will also have it set on their scroll
    // layers.
    bool is_resized_by_browser_controls : 1;
    bool is_container_for_fixed_position_layers : 1;
    LayerPositionConstraint position_constraint;

    LayerStickyPositionConstraint sticky_position_constraint;

    ElementId element_id;

    Layer* scroll_parent;
    Layer* clip_parent;

    bool has_will_change_transform_hint : 1;

    bool trilinear_filtering : 1;

    bool hide_layer_and_subtree : 1;

    // The following elements can not and are not serialized.
    base::WeakPtr<LayerClient> client;
    std::unique_ptr<base::trace_event::TracedValue> debug_info;

    base::RepeatingCallback<void(const gfx::ScrollOffset&, const ElementId&)>
        did_scroll_callback;
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests;

    OverscrollBehavior overscroll_behavior;

    base::Optional<SnapContainerData> snap_container_data;
  };

  Layer* parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  LayerTreeHost* layer_tree_host_;

  Inputs inputs_;

  int num_descendants_that_draw_content_;
  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;
  int property_tree_sequence_number_;
  gfx::Vector2dF offset_to_transform_parent_;
  bool should_flatten_screen_space_transform_from_property_tree_ : 1;
  bool draws_content_ : 1;
  bool should_check_backface_visibility_ : 1;
  // Force use of and cache render surface.
  bool cache_render_surface_ : 1;
  bool force_render_surface_for_testing_ : 1;
  bool subtree_property_changed_ : 1;
  bool may_contain_video_ : 1;
  bool needs_show_scrollbars_ : 1;
  bool has_transform_node_ : 1;
  bool is_rounded_corner_mask_ : 1;
  // This value is valid only when LayerTreeHost::has_copy_request() is true
  bool subtree_has_copy_request_ : 1;
  SkColor safe_opaque_background_color_;

  std::unique_ptr<std::set<Layer*>> clip_children_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
