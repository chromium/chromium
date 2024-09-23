// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/base/protected_sequence_synchronizer.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/cc_export.h"
#include "cc/input/hit_test_opaqueness.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/scroll_hit_test_rect.h"
#include "cc/layers/touch_action_region.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/node_id.h"
#include "cc/paint/paint_record.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/target_property.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace viz {
class CopyOutputRequest;
}

namespace cc {

class LayerImpl;
class LayerTreeHost;
class LayerTreeHostCommon;
class LayerTreeImpl;
class PictureLayer;

struct CommitState;
struct ThreadUnsafeCommitState;

// For tracing and debugging. The info will be attached to this layer's tracing
// output.
struct CC_EXPORT LayerDebugInfo {
  LayerDebugInfo();
  LayerDebugInfo(const LayerDebugInfo&);
  ~LayerDebugInfo();

  std::string name;
  NodeId owner_node_id = kInvalidNodeId;
  int paint_count = 0;
  std::vector<const char*> compositing_reasons;
  std::vector<const char*> compositing_reason_ids;
  struct Invalidation {
    gfx::Rect rect;
    const char* reason;
    std::string client;
  };
  std::vector<Invalidation> invalidations;
};

// Base class for composited layers. Special layer types are derived from
// this class. Each layer is an independent unit in the compositor, be that
// for transforming or for content. If a layer has content it can be
// transformed efficiently without requiring the content to be recreated.
// Layers form a tree, with each layer having 0 or more children, and a single
// parent (or none at the root). Layers within the tree, other than the root
// layer, are kept alive by that tree relationship, with refpointer ownership
// from parents to children.
class CC_EXPORT Layer : public base::RefCounted<Layer>,
                        public ProtectedSequenceSynchronizer {
 public:
  // An invalid layer id, as all layer ids are positive.
  enum LayerIdLabels {
    INVALID_ID = -1,
  };

  // Factory to create a new Layer, with a unique id.
  static scoped_refptr<Layer> Create();

  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

  // A unique and stable id for the Layer. Ids are always positive.
  int id() const { return layer_id_; }

  // Returns a pointer to the highest ancestor of this layer, or itself.
  Layer* RootLayer();
  // Returns a pointer to the direct ancestor of this layer if it exists,
  // or null.
  Layer* mutable_parent() { return parent_.Write(*this); }
  const Layer* parent() const { return parent_.Read(*this); }
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
  const LayerList& children() const { return inputs_.Read(*this).children; }

  // These methods provide information from layer_tree_host_ in a way that is
  // safe to query from either the main or impl thread.
  bool IsAttached() const { return layer_tree_host_; }
  bool IsMainThread() const;
  bool IsUsingLayerLists() const;

  // Gets the LayerTreeHost that this layer is attached to, or null if not.
  // A layer is attached to a LayerTreeHost if it or an ancestor layer is set as
  // the root layer of a LayerTreeHost (while noting only a layer without a
  // parent may be set as the root layer).
  LayerTreeHost* layer_tree_host() {
    DCHECK(!IsAttached() || IsMainThread());
    return layer_tree_host_.get();
  }
  const LayerTreeHost* layer_tree_host() const {
    DCHECK(!IsAttached() || IsMainThread());
    return layer_tree_host_.get();
  }

  // This requests the layer and its subtree be rendered and given to the
  // callback. If the copy is unable to be produced (the layer is destroyed
  // first), then the callback is called with a nullptr/empty result. If the
  // request's source property is set, any prior uncommitted requests having the
  // same source will be aborted.
  void RequestCopyOfOutput(std::unique_ptr<viz::CopyOutputRequest> request);
  // True if a copy request has been inserted on this layer and a commit has not
  // occurred yet.
  bool HasCopyRequest() const {
    return layer_tree_inputs() && !layer_tree_inputs()->copy_requests.empty();
  }

  // Set and get the background color for the layer. This color is used to
  // calculate the safe opaque background color. Subclasses may also use the
  // color for other purposes.
  virtual void SetBackgroundColor(SkColor4f background_color);
  SkColor4f background_color() const {
    return inputs_.Read(*this).background_color;
  }

  // For layer tree mode only. In layer list mode, client doesn't need to set
  // it. Sets an opaque background color for the layer, to be used in place of
  // the background_color() if the layer says contents_opaque() is true.
  void SetSafeOpaqueBackgroundColor(SkColor4f background_color);

  // Returns a background color with opaqueness equal to the value of
  // contents_opaque().
  // If the layer says contents_opaque() is true, in layer tree mode, this
  // returns the value set by SetSafeOpaqueBackgroundColor() which should be an
  // opaque color, and in layer list mode, returns background_color() which
  // should be opaque (otherwise SetBackgroundColor() should have set
  // contents_opaque to false).
  // Otherwise, it returns something non-opaque. It prefers to return the
  // background_color(), but if the background_color() is opaque (and this layer
  // claims to not be), then SkColors::kTransparent is returned to avoid
  // intrusive checkerboard where the layer is not covered by the
  // background_color().
  SkColor4f SafeOpaqueBackgroundColor() const;

  // For layer tree mode only.
  // Set and get the position of this layer, relative to its parent. This is
  // specified in layer space, which excludes device scale and page scale
  // factors, and ignoring transforms for this layer or ancestor layers. The
  // root layer's position is not used as it always appears at the origin of
  // the viewport.
  void SetPosition(const gfx::PointF& position);
  const gfx::PointF position() const {
    return layer_tree_inputs() ? layer_tree_inputs()->position : gfx::PointF();
  }

  // Reorder the entirety of the children() vector to follow new_children_order.
  // All elements inside new_children_order must be inside children(), and vice
  // versa. Will empty the |new_children_order| LayerList passed into this
  // method.
  void ReorderChildren(LayerList* new_children_order);

  // Set and get the layers bounds. This is specified in layer space, which
  // excludes device scale and page scale factors, and ignoring transforms for
  // this layer or ancestor layers.
  //
  // The root layer in the tree has bounds in viewport space, which includes
  // the device scale factor.
  void SetBounds(const gfx::Size& bounds);
  const gfx::Size& bounds() const { return inputs_.Read(*this).bounds; }

  // For layer tree mode only.
  // Set or get that this layer clips its subtree to within its bounds. Content
  // of children will be intersected with the bounds of this layer when true.
  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const {
    return layer_tree_inputs() && layer_tree_inputs()->masks_to_bounds;
  }

  // For layer tree mode only.
  // Set or get the clip rect for this layer. |clip_rect| is relative to |this|
  // layer. If you are trying to clip the subtree to the bounds of this layer,
  // SetMasksToBounds() would be a better alternative.
  void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect clip_rect() const {
    return layer_tree_inputs() ? layer_tree_inputs()->clip_rect : gfx::Rect();
  }

  // Returns the bounds which is clipped by the clip rect.
  gfx::RectF EffectiveClipRect() const;

  // For layer tree mode only.
  // Set or get a layer that will mask the contents of this layer. The alpha
  // channel of the mask layer's content is used as an alpha mask of this
  // layer's content. IOW the mask's alpha is multiplied by this layer's alpha
  // for each matching pixel.
  void SetMaskLayer(scoped_refptr<PictureLayer> mask_layer);
  const PictureLayer* mask_layer() const {
    return layer_tree_inputs() ? layer_tree_inputs()->mask_layer.get()
                               : nullptr;
  }
  PictureLayer* mask_layer() {
    return layer_tree_inputs() ? layer_tree_inputs()->mask_layer.get()
                               : nullptr;
  }

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
  const gfx::Rect& update_rect() const { return update_rect_.Read(*this); }

  // If this returns true, then `SetNeedsDisplay` will be called in response to
  // the HDR headroom of the display that the content is rendering to changing.
  virtual bool RequiresSetNeedsDisplayOnHdrHeadroomChange() const;

  void ResetUpdateRectForTesting() { update_rect_.Write(*this) = gfx::Rect(); }

  // For layer tree mode only.
  // Set or get the rounded corner radii which is applied to the layer and its
  // subtree (as if they are together as a single composited entity) when
  // blitting into their target. Setting this makes the layer masked to bounds.
  // If the layer has a clip of its own, the rounded corner will be applied
  // along the layer's clip rect corners. TODO(sashamcintosh): Apply rounded
  // corner when the layer has a transform that is not 2d axis aligned.
  // Currently the rounded corner is ignored in this case.
  void SetRoundedCorner(const gfx::RoundedCornersF& corner_radii);
  const gfx::RoundedCornersF& corner_radii() const {
    return layer_tree_inputs() ? layer_tree_inputs()->corner_radii
                               : kNoRoundedCornersF;
  }
  // Returns true if any of the corner has a non-zero radius set.
  bool HasRoundedCorner() const { return !corner_radii().IsEmpty(); }

  // For layer tree mode only.
  // Set or get the flag that disables the requirement of a render surface for
  // this layer due to it having rounded corners. This improves performance at
  // the cost of maybe having some blending artifacts. Not having a render
  // surface is not guaranteed however.
  void SetIsFastRoundedCorner(bool enable);
  bool is_fast_rounded_corner() const {
    return layer_tree_inputs() && layer_tree_inputs()->is_fast_rounded_corner;
  }

  // For layer tree mode only.
  // Set or get the gradient mask which is applied to the layer and its
  // subtree (as if they are together as a single composited entity) when
  // blitting into their target. Setting applies a linear gradient to the layer
  // bounds and optionally the rounded corner defined by SetRoundedCorner.
  // TODO(sashamcintosh): Apply gradient mask when the layer has a transform
  // that is not 2d axis aligned. Currently the gradient mask is ignored in this
  // case.
  void SetGradientMask(const gfx::LinearGradient& gradient_mask);
  const gfx::LinearGradient& gradient_mask() const {
    return layer_tree_inputs() ? layer_tree_inputs()->gradient_mask
                               : gfx::LinearGradient::GetEmpty();
  }
  bool HasGradientMask() const { return !gradient_mask().IsEmpty(); }

  bool HasMaskFilter() const { return HasRoundedCorner() || HasGradientMask(); }

  // For layer tree mode only.
  // Set or get the opacity which should be applied to the contents of the layer
  // and its subtree (together as a single composited entity) when blending them
  // into their target. Note that this does not speak to the contents of this
  // layer, which may be opaque or not (see contents_opaque()). Note that the
  // opacity is cumulative since it applies to the layer's subtree.
  virtual void SetOpacity(float opacity);
  float opacity() const {
    return layer_tree_inputs() ? layer_tree_inputs()->opacity : 1.0f;
  }
  // Gets the true opacity that will be used for blending the contents of this
  // layer and its subtree into its target during composite. This value is the
  // same as the user-specified opacity() unless the layer should not be visible
  // at all for other reasons, in which case the opacity here becomes 0.
  float EffectiveOpacity() const;

  // For layer tree mode only.
  // Set or get the blend mode to be applied when blending the contents of the
  // layer and its subtree (together as a single composited entity) when
  // blending them into their target.
  void SetBlendMode(SkBlendMode blend_mode);
  SkBlendMode blend_mode() const {
    return layer_tree_inputs() ? layer_tree_inputs()->blend_mode
                               : SkBlendMode::kSrcOver;
  }

  // For layer tree mode only.
  // Set or get the list of filter effects to be applied to the contents of the
  // layer and its subtree (together as a single composited entity) when
  // drawing them into their target.
  void SetFilters(const FilterOperations& filters);
  FilterOperations filters() const {
    return layer_tree_inputs() ? layer_tree_inputs()->filters
                               : FilterOperations();
  }

  // For layer tree mode only.
  // Set or get the list of filters that should be applied to the content this
  // layer and its subtree will be drawn into. The effect is clipped by
  // backdrop_filter_bounds.
  void SetBackdropFilters(const FilterOperations& filters);
  FilterOperations backdrop_filters() const {
    return layer_tree_inputs() ? layer_tree_inputs()->backdrop_filters
                               : FilterOperations();
  }

  // For layer tree mode only.
  void SetBackdropFilterBounds(const gfx::RRectF& backdrop_filter_bounds);
  void ClearBackdropFilterBounds();
  std::optional<gfx::RRectF> backdrop_filter_bounds() const {
    return layer_tree_inputs() ? layer_tree_inputs()->backdrop_filter_bounds
                               : std::nullopt;
  }

  // For layer tree mode only.
  void SetBackdropFilterQuality(const float quality);
  float backdrop_filter_quality() const {
    return layer_tree_inputs() ? layer_tree_inputs()->backdrop_filter_quality
                               : 1.0f;
  }

  // Set or get an optimization hint that the contents of this layer are fully
  // opaque or not. If true, every pixel of content inside the layer's bounds
  // must be opaque or visual errors can occur. This applies only to this layer
  // and not to children, and does not imply the layer should be composited
  // opaquely, as effects may be applied such as opacity() or filters().
  // Note that this also calls SetContentsOpaqueForText(opaque) internally.
  // To override a different contents_opaque_for_text, the client should call
  // SetContentsOpaqueForText() after SetContentsOpaque().
  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return inputs_.Read(*this).contents_opaque; }

  // Whether the contents area containing text is known to be opaque.
  // For example, blink will SetContentsOpaque(false) but
  // SetContentsOpaqueForText(true) for the following case:
  //   <div style="overflow: hidden; border-radius: 10px; background: white">
  //     TEXT
  //   </div>
  // See also the note for SetContentsOpaque().
  void SetContentsOpaqueForText(bool opaque);
  bool contents_opaque_for_text() const {
    return inputs_.Read(*this).contents_opaque_for_text;
  }

  void SetHitTestOpaqueness(HitTestOpaqueness opaqueness);
  // For callers that don't know the HitTestOpaqueness::kOpaque concept.
  void SetHitTestable(bool hit_testable);
  HitTestOpaqueness hit_test_opaqueness() const {
    return inputs_.Read(*this).hit_test_opaqueness;
  }

  // For layer tree mode only.
  // Set or get the transform to be used when compositing this layer into its
  // target. The transform is inherited by this layers children.
  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const {
    return layer_tree_inputs() ? layer_tree_inputs()->transform
                               : kIdentityTransform;
  }

  // Gets the transform, including transform origin and position, of this layer
  // and its ancestors, device scale and page scale factors, into the device
  // viewport.
  gfx::Transform ScreenSpaceTransform() const;

  // For layer tree mode only.
  // Set or get the origin to be used when applying the transform. The value is
  // a position in layer space, relative to the top left corner of this layer.
  // For instance, if set to the center of the layer, with a transform to rotate
  // 180deg around the X axis, it would flip the layer vertically around the
  // center of the layer, leaving it occupying the same space. Whereas set to
  // the top left of the layer, the rotation wouldoccur around the top of the
  // layer, moving it vertically while flipping it.
  void SetTransformOrigin(const gfx::Point3F&);
  gfx::Point3F transform_origin() const {
    return layer_tree_inputs() ? layer_tree_inputs()->transform_origin
                               : gfx::Point3F();
  }

  // For layer tree mode only.
  // Set or get the scroll offset of the layer. The content of the layer, and
  // position of its subtree, as well as other layers for which this layer is
  // their scroll parent, and their subtrees) is moved up by the amount of
  // offset specified here.
  void SetScrollOffset(const gfx::PointF& scroll_offset);
  gfx::PointF scroll_offset() const {
    return layer_tree_inputs() ? layer_tree_inputs()->scroll_offset
                               : gfx::PointF();
  }

  // For layer tree mode only.
  // Called internally during commit to update the layer with state from the
  // compositor thread. Not to be called externally by users of this class.
  void SetScrollOffsetFromImplSide(const gfx::PointF& scroll_offset);

  // For layer tree mode only.
  // Marks this layer as being scrollable and needing an associated scroll node,
  // and specifies the size of the container in which the scrolling contents are
  // visible. (Use SetBounds to set the size of the content to be scrolled.)
  // Once scrollable, a Layer cannot become un-scrollable.
  void SetScrollable(const gfx::Size& scroll_container_bounds);
  bool scrollable() const {
    return layer_tree_inputs() && layer_tree_inputs()->scrollable;
  }
  gfx::Size scroll_container_bounds() const {
    return layer_tree_inputs() ? layer_tree_inputs()->scroll_container_bounds
                               : gfx::Size();
  }

  virtual bool IsScrollbarLayerForTesting() const;

  // For layer list mode only.
  // Set or get an area of this layer within which a scroll hit-test can not be
  // done from the compositor thread. Within this area, if the user attempts to
  // start a scroll, the events must be sent to the main thread and processed
  // there.
  void SetMainThreadScrollHitTestRegion(
      const Region& main_thread_scroll_hit_test_region);
  const Region& main_thread_scroll_hit_test_region() const {
    if (const auto& rare_inputs = inputs_.Read(*this).rare_inputs)
      return rare_inputs->main_thread_scroll_hit_test_region;
    return Region::Empty();
  }

  // For layer list mode only.
  // A scroll in any of the rects but not in non_fast_scrollable_region can
  // start on the compositor thread. The scroll node is determined by checking
  // non_composited_scroll_hit_test_rects in reversed order.
  void SetNonCompositedScrollHitTestRects(std::vector<ScrollHitTestRect> rects);
  const std::vector<ScrollHitTestRect>* non_composited_scroll_hit_test_rects()
      const {
    if (const auto& rare_inputs = inputs_.Read(*this).rare_inputs) {
      return &rare_inputs->non_composited_scroll_hit_test_rects;
    }
    return nullptr;
  }

  // Set or get the set of touch actions allowed across each point of this
  // layer. The |touch_action_region| can specify, for any number of areas,
  // which touch actions are allowed in each area. The result is the
  // intersection of overlapping areas. These allowed actions control if
  // a touch event can initiate a scroll or zoom on the compositor thread.
  void SetTouchActionRegion(TouchActionRegion touch_action_region);
  const TouchActionRegion& touch_action_region() const {
    return inputs_.Read(*this).touch_action_region;
  }

  // Set or get the region that should be used for capture.
  void SetCaptureBounds(viz::RegionCaptureBounds bounds);
  const viz::RegionCaptureBounds& capture_bounds() const {
    if (const auto& rare_inputs = inputs_.Read(*this).rare_inputs)
      return rare_inputs->capture_bounds;
    return viz::RegionCaptureBounds::Empty();
  }

  // Set or get the set of blocking wheel rects of this layer. The
  // |wheel_event_region| is the set of rects for which there is a non-passive
  // wheel event listener that paints into this layer. Mouse wheel messages
  // that intersect these rects must execute their relevant JS handler before we
  // can start scrolling.
  void SetWheelEventRegion(Region wheel_event_region);
  const Region& wheel_event_region() const {
    if (const auto& rare_inputs = inputs_.Read(*this).rare_inputs)
      return rare_inputs->wheel_event_region;
    return Region::Empty();
  }

  // For layer tree mode only.
  // In layer list mode, use ScrollTree::SetScrollCallbacks() instead.
  // Sets a RepeatingCallback that is run during a main frame, before layers are
  // asked to prepare content with Update(), if the scroll offset for the layer
  // was changed by the InputHandlerClient, on the compositor thread (or on the
  // main thread in single-thread mode). It may be set to a null callback, in
  // which case nothing is called. This is for layer tree mode only. Should use
  // ScrollTree::SetScrollCallbacks() in layer list mode.
  void SetDidScrollCallback(
      base::RepeatingCallback<void(const gfx::PointF&, const ElementId&)>);

  // For layer tree mode only.
  // Sets the given |subtree_id| on this layer, so that the layer subtree rooted
  // at this layer can be uniquely identified by a FrameSinkVideoCapturer.
  // The existence of a valid SubtreeCaptureId on this layer will force it to be
  // drawn into a separate CompositorRenderPass.
  // Setting a non-valid (i.e. default-constructed SubtreeCaptureId) will clear
  // this property.
  // It is not allowed to change this ID from a valid ID to another valid ID,
  // since a client might already using the existing valid ID to make this layer
  // subtree identifiable by a capturer.
  //
  // Note that this is useful when it's desired to video record a layer subtree
  // of a non-root layer using a FrameSinkVideoCapturer, since non-root layers
  // are usually not drawn into their own CompositorRenderPass.
  void SetSubtreeCaptureId(viz::SubtreeCaptureId subtree_id);
  viz::SubtreeCaptureId subtree_capture_id() const {
    if (layer_tree_inputs())
      return layer_tree_inputs()->subtree_capture_id;
    return viz::SubtreeCaptureId();
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
  void SetCacheRenderSurface(bool value) {
    DCHECK(IsPropertyChangeAllowed());
    SetBitFlag(value, kCacheRenderSurfaceFlagMask, /*invalidate=*/true);
  }
  bool cache_render_surface() const {
    return GetBitFlag(kCacheRenderSurfaceFlagMask);
  }

  // If the layer induces a render surface, this returns the cause for the
  // render surface. If the layer does not induce a render surface, this returns
  // kNone.
  RenderSurfaceReason GetRenderSurfaceReason() const;

  // Set or get if the layer and its subtree will be drawn through an
  // intermediate texture, called a RenderSurface. This mimics the need
  // for a RenderSurface that is caused by compositing effects such as masks
  // without needing to set up such effects.
  void SetForceRenderSurfaceForTesting(bool value) {
    DCHECK(IsPropertyChangeAllowed());
    SetBitFlag(value, kForceRenderSurfaceForTestingFlagMask,
               /*invalidate=*/true);
  }
  bool force_render_surface_for_testing() const {
    return GetBitFlag(kForceRenderSurfaceForTestingFlagMask);
  }

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
  bool draws_content() const { return GetBitFlag(kDrawsContentFlagMask); }

  // Returns the number of layers in this layers subtree (excluding itself) for
  // which DrawsContent() is true.
  int NumDescendantsThatDrawContent() const;

  // For layer tree mode only.
  // Set or get if this layer and its subtree should be part of the compositor's
  // output to the screen. When set to true, the layer's subtree does not appear
  // to the user, but still remains part of the tree with all its normal drawing
  // properties. This can be used to execute a CopyOutputRequest on this layer
  // or another in its subtree, since the layers are still able to be drawn by
  // the compositor, while not being composed into the result shown to the user.
  void SetHideLayerAndSubtree(bool hide);
  bool hide_layer_and_subtree() const {
    return layer_tree_inputs() && layer_tree_inputs()->hide_layer_and_subtree;
  }

  // The index of this layer's node in the various property trees. These are
  // only valid after a main frame, when Update() is called on the layer, and
  // remain valid and in in the same state until the next main frame, or until
  // the layer is removed from its LayerTreeHost. Otherwise kInvalidNodeId is
  // returned.
  int transform_tree_index() const;
  int clip_tree_index() const;
  int effect_tree_index() const;
  int scroll_tree_index() const;

  bool transform_tree_index_is_valid(const PropertyTrees&) const;
  bool clip_tree_index_is_valid(const PropertyTrees&) const;
  bool effect_tree_index_is_valid(const PropertyTrees&) const;
  bool scroll_tree_index_is_valid(const PropertyTrees&) const;

  // While all layers have an index into the transform tree, this value
  // indicates whether the transform tree node was created for this layer.
  void SetHasTransformNode(bool value) {
    SetBitFlag(value, kHasTransformNodeFlagMask);
  }
  bool has_transform_node() const {
    return GetBitFlag(kHasTransformNodeFlagMask);
  }

  // This value indicates whether a clip node was created for |this| layer.
  void SetHasClipNode(bool val) { SetBitFlag(val, kHasClipNodeFlagMask); }
  bool has_clip_node() const { return GetBitFlag(kHasClipNodeFlagMask); }

  // Sets that the content shown in this layer may be a video. This may be used
  // by the system compositor to distinguish between animations updating the
  // screen and video, which the user would be watching. This allows
  // optimizations like turning off the display when video is not playing,
  // without interfering with video playback.
  void SetMayContainVideo(bool value) {
    SetBitFlag(value, kMayContainVideoFlagMask, /*invalidate=*/false,
               /*needs_push=*/true);
  }
  bool may_contain_video() const {
    return GetBitFlag(kMayContainVideoFlagMask);
  }

  // Stable identifier for clients. See comment in cc/paint/element_id.h.
  void SetElementId(ElementId id);
  ElementId element_id() const { return inputs_.Read(*this).element_id; }

  // For layer tree mode only.
  // Sets or gets if trilinear filtering should be used to scaling the contents
  // of this layer and its subtree. When set the layer and its subtree will be
  // composited together as a single unit, mip maps will be generated of the
  // subtree together, and trilinear filtering applied when supported, if
  // scaling during composite of the content from this layer and its subtree
  // into the target.
  void SetTrilinearFiltering(bool trilinear_filtering);
  bool trilinear_filtering() const {
    return layer_tree_inputs() && layer_tree_inputs()->trilinear_filtering;
  }

  // For layer tree mode only.
  // Increments/decrements/gets number of layers mirroring this layer.
  void IncrementMirrorCount();
  void DecrementMirrorCount();
  int mirror_count() const {
    return layer_tree_inputs() ? layer_tree_inputs()->mirror_count : 0;
  }

  // Captures text content within the given |rect| and returns the associated
  // NodeInfo in |content|.
  virtual void CaptureContent(const gfx::Rect& rect,
                              std::vector<NodeInfo>* content) const;

  // For tracing. Gets a recorded rasterization of this layer's contents that
  // can be displayed inside representations of this layer. May return null, in
  // which case the layer won't be shown with any content in the tracing
  // display.
  virtual sk_sp<const SkPicture> GetPicture() const;

  virtual bool IsSolidColorLayerForTesting() const;

  const LayerDebugInfo* debug_info() const { return debug_info_.Read(*this); }
  LayerDebugInfo& EnsureDebugInfo();
  void ClearDebugInfo();

  // For telemetry testing. Runs a given test behaviour implemented in
  // |benchmark| for this layer. The base class does nothing as benchmarks
  // only exist for subclass layer types. For each subclass that the
  // MicroBenchmark supports, the class should override this method and run the
  // |benchmark| against this layer.
  virtual void RunMicroBenchmark(MicroBenchmark* benchmark);

  // Internal method to create the compositor thread type for this Layer.
  // Subclasses should override this method if they want to return their own
  // subclass of LayerImpl instead.
  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const;

  // Internal method to copy all state from this Layer to the compositor thread.
  // Should be overridden by any subclass that has additional state, to copy
  // that state as well. The |layer| passed in will be of the type created by
  // CreateLayerImpl(), so can be safely down-casted if the subclass uses a
  // different type for the compositor thread.
  virtual void PushPropertiesTo(LayerImpl* layer,
                                const CommitState& commit_state,
                                const ThreadUnsafeCommitState& unsafe_state);

  // Internal method to be overridden by Layer subclasses that need to do work
  // during a main frame. The method should compute any state that will need to
  // propagated to the compositor thread for the next commit, and return true
  // if there is anything new to commit. If all layers return false, the commit
  // may be aborted.
  virtual bool Update();

  // Internal to property tree construction. This allows a layer to request that
  // its transform should be snapped such that the layer aligns with the pixel
  // grid in its rendering target. This ensures that the layer is not fuzzy
  // (unless it is being scaled). Layers may override this to return true, by
  // default layers are not snapped.
  virtual bool IsSnappedToPixelGridInTarget() const;

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

  // Internal to property tree construction. A generation number for the
  // property trees, to verify the layer's indices are pointers into the trees
  // currently held by the LayerTreeHost. The number is updated when property
  // trees are built from the Layer tree.
  void set_property_tree_sequence_number(int sequence_number) {
    property_tree_sequence_number_.Write(*this) = sequence_number;
  }
  int property_tree_sequence_number() const {
    return property_tree_sequence_number_.Read(*this);
  }

  // Internal to property tree construction. Sets the index for this Layer's
  // node in each property tree.
  void SetTransformTreeIndex(int index);
  void SetClipTreeIndex(int index);
  void SetEffectTreeIndex(int index);
  void SetScrollTreeIndex(int index);

  // The position of this layer after transforming by the layer's transform
  // node. When property trees are built by cc (when IsUsingLayerLists is false)
  // this is set by property_tree_builder.cc.
  void SetOffsetToTransformParent(gfx::Vector2dF offset);
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_.Read(*this);
  }

  // Internal to property tree construction. Indicates that a property changed
  // on this layer that may affect the position or content of all layers in this
  // layer's subtree, including itself. This causes the layer's subtree to be
  // considered damaged and re-displayed to the user.
  void SetSubtreePropertyChanged();
  void ClearSubtreePropertyChangedForTesting() {
    subtree_property_changed_.Write(*this) = false;
  }
  bool subtree_property_changed() const {
    return subtree_property_changed_.Read(*this);
  }

  // Internal to property tree construction. Returns ElementListType::ACTIVE
  // as main thread layers do not have a pending/active tree split, and
  // animations should run normally on the main thread layer tree.
  ElementListType GetElementTypeForAnimation() const;

  // Internal to property tree construction. Whether this layer may animate its
  // opacity on the compositor thread. Layer subclasses may override this to
  // report true. If true, assumptions about opacity can not be made on the main
  // thread.
  virtual bool OpacityCanAnimateOnImplThread() const;

  // For layer tree mode only.
  // Internal to property tree construction. Set to true if this layer or any
  // layer below it in the tree has a CopyOutputRequest pending commit.
  // This flag is valid only when LayerTreeHost::has_copy_request() is true
  void SetSubtreeHasCopyRequest(bool value) {
    SetBitFlag(value, kSubtreeHasCopyRequestFlagMask);
  }
  bool subtree_has_copy_request() const {
    return GetBitFlag(kSubtreeHasCopyRequestFlagMask);
  }
  // Internal to property tree construction. Removes all CopyOutputRequests from
  // this layer, moving them into |requests|.
  void TakeCopyRequests(
      std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests);

  // Internal to property tree construction. Set if the layer should not be
  // shown when its back face is visible to the user. This is a derived value
  // from SetDoubleSided().
  void SetShouldCheckBackfaceVisibility(bool value) {
    SetBitFlag(value, kShouldCheckBackfaceVisibilityFlagMask,
               /*invalidate=*/false, /*needs_push=*/true);
  }
  bool should_check_backface_visibility() const {
    return GetBitFlag(kShouldCheckBackfaceVisibilityFlagMask);
  }

  // For debugging, containing information about the associated DOM, etc.
  std::string DebugName() const;

  std::string ToString() const;

  // Called when a property has been modified in a way that the layer knows
  // immediately that a commit is required.  This implies SetNeedsPushProperties
  // to push that property.
  // This is public, so that it can be called directly when needed, for example
  // in PropertyTreeManager when handling scroll offsets.
  void SetNeedsCommit();

  void SetDebugName(const std::string& name);

  // If the content of this layer is provided by a cached or live render
  // surface, returns the ID of that resource.
  virtual viz::ViewTransitionElementResourceId ViewTransitionResourceId() const;

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;

  Layer();
  ~Layer() override;

  // These SetNeeds functions are in order of severity of update:

  // See SetNeedsCommit() above - it belongs here in the order of severity.

  // Called when there's been a change in layer structure.  Implies
  // SetNeedsCommit and property tree rebuld, but not SetNeedsPushProperties
  // (the full tree is synced over).
  void SetNeedsFullTreeSync();

  // May be overridden by subclasses if they have optional content, to return
  // false if there is no content to be displayed. If they do have content, then
  // they should return the value from this base class method.
  virtual bool HasDrawableContent() const;

  // Updates draws_content() according to the current HasDrawableContent().
  // This should be called when HasDrawableContent() changes.
  void UpdateDrawsContent();

  // Called when the layer's number of drawable descendants changes.
  void AddDrawableDescendants(int num);

  // For debugging. Returns false if the LayerTreeHost this layer is attached to
  // is in the process of updating layers or performing commit for a
  // BeginMainFrame. Layer properties should be changed by the client before the
  // BeginMainFrame, and should not be changed while the frame is being
  // generated or committed.
  bool IsPropertyChangeAllowed() const;

  void IncreasePaintCount() {
    if (debug_info_.Read(*this))
      ++debug_info_.Write(*this)->paint_count;
  }

  base::AutoReset<bool> IgnoreSetNeedsCommitForTest() {
    return base::AutoReset<bool>(
        &ignore_set_needs_commit_for_test_.Write(*this), true);
  }

 private:
  friend class base::RefCounted<Layer>;
  friend class LayerTreeHostCommon;
  friend class LayerTreeHost;

  // For layer tree mode only.
  struct LayerTreeInputs;
  LayerTreeInputs& EnsureLayerTreeInputs();
#if DCHECK_IS_ON()
  const LayerTreeInputs* layer_tree_inputs() const;
#else
  const LayerTreeInputs* layer_tree_inputs() const {
    return layer_tree_inputs_.Read(*this);
  }
#endif

  // Interactions with attached animations.
  void OnFilterAnimated(const FilterOperations& filters);
  void OnBackdropFilterAnimated(const FilterOperations& backdrop_filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);

  void AddClipChild(Layer* child);
  void RemoveClipChild(Layer* child);

  // For functions that do or (as SetParent) might remove a child layer,
  // passing kForReadd causes the removal to *not* call SetLayerTreeHost.
  // This variation assumes that the caller will re-add the layer (probably to
  // the same layer tree host) and then call SetLayerTreeHost.
  enum class RemovalReason {
    kNormal,
    kForReadd,
  };

  void SetParent(Layer* layer, RemovalReason reason);

  // This should only be called from RemoveFromParent().
  void RemoveChild(Layer* child, RemovalReason reason);

  // Variant (for internal use) of RemoveFromParent (which is a widely-used
  // public API) as though it were passed RemovalReason::kForReadd.
  void RemoveFromParentForReadd();

  bool GetBitFlag(uint8_t mask) const;

  // invalidate: if true and the flag's value changes, the host is marked as
  //     needing a property tree update and commit.
  // needs_push: if true and the flag's value changes, the layer is marked as
  //     needing to push its properties to its corresponding LayerImpl, but
  //     without marking the host as needing a property update or commit.
  // return value: 'true' if the flag's value changes.
  bool SetBitFlag(bool new_value,
                  uint8_t mask,
                  bool invalidate = false,
                  bool needs_push = false);

  // When we detach or attach layer to new LayerTreeHost, all property trees'
  // indices becomes invalid.
  void InvalidatePropertyTreesIndices();

  // This is set whenever a property changed on layer that affects whether this
  // layer should own a property tree node or not.
  void SetPropertyTreesNeedRebuild();

  // For layer tree mode only.
  // Fast-path for |SetScrollOffset| and |SetScrollOffsetFromImplSide| to
  // directly update scroll offset values in the property tree without needing a
  // full property tree update. If property trees do not exist yet, ensures
  // they are marked as needing to be rebuilt.
  void UpdatePropertyTreeScrollOffset();

  void SetMirrorCount(int mirror_count);

  int transform_tree_index(const PropertyTrees&) const;
  int clip_tree_index(const PropertyTrees&) const;
  int effect_tree_index(const PropertyTrees&) const;
  int scroll_tree_index(const PropertyTrees&) const;

  // Contains a set of input properties that are infrequently set on layers,
  // generally speaking in <10% of use cases. When adding new values to this
  // struct, consider the memory implications versus simply adding to Inputs.
  struct RareInputs {
    RareInputs();
    ~RareInputs();

    viz::RegionCaptureBounds capture_bounds;
    Region main_thread_scroll_hit_test_region;
    std::vector<ScrollHitTestRect> non_composited_scroll_hit_test_rects;
    Region wheel_event_region;
  };

  RareInputs& EnsureRareInputs() {
    auto& rare_inputs = inputs_.Write(*this).rare_inputs;
    if (!rare_inputs)
      rare_inputs = std::make_unique<RareInputs>();
    return *rare_inputs;
  }

  // Encapsulates all data, callbacks or interfaces received from the embedder.
  struct Inputs {
    Inputs();
    ~Inputs();

    // In layer list mode, only the root layer can have children.
    // TODO(wangxianzhu): Move this field into LayerTreeHost when we remove
    // layer tree mode.
    LayerList children;

    gfx::Size bounds;

    HitTestOpaqueness hit_test_opaqueness = HitTestOpaqueness::kTransparent;

    bool contents_opaque : 1 = false;
    bool contents_opaque_for_text : 1 = false;
    bool is_drawable : 1 = false;
    bool double_sided : 1 = true;

    SkColor4f background_color = SkColors::kTransparent;
    TouchActionRegion touch_action_region;

    ElementId element_id;

    std::unique_ptr<RareInputs> rare_inputs;
  };

  // These inputs are used in layer tree mode (ui compositor) only. Most of them
  // are inputs of PropertyTreeBuilder for this layer. A few of them are for
  // ui-compositor-specific features (i.e. mirror and copy request) which will
  // be still used after the ui compositor switch to layer tree mode, but for
  // now they work in layer tree mode only.
  struct LayerTreeInputs {
    LayerTreeInputs();
    ~LayerTreeInputs();

    gfx::Rect clip_rect;

    // If not null, points to one of child layers which is set as mask layer
    // by SetMaskLayer().
    raw_ptr<PictureLayer> mask_layer = nullptr;

    float opacity = 1.0f;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    bool masks_to_bounds : 1 = false;

    // If set, disables this layer's rounded corner from triggering a render
    // surface on itself if possible.
    bool is_fast_rounded_corner : 1 = false;

    bool trilinear_filtering : 1 = false;

    bool hide_layer_and_subtree : 1 = false;

    // Indicates that this layer will need a scroll property node and that this
    // layer's bounds correspond to the scroll node's bounds (both |bounds| and
    // |scroll_container_bounds|).
    bool scrollable : 1 = false;

    gfx::PointF position;
    gfx::Transform transform;
    gfx::Point3F transform_origin;

    // A unique ID that identifies the layer subtree rooted at this layer, so
    // that it can be independently captured by the FrameSinkVideoCapturer. If
    // this ID is set (i.e. valid), it would force this subtree into a render
    // surface that darws in a render pass.
    viz::SubtreeCaptureId subtree_capture_id;

    SkColor4f safe_opaque_background_color = SkColors::kTransparent;

    FilterOperations filters;
    FilterOperations backdrop_filters;
    std::optional<gfx::RRectF> backdrop_filter_bounds;
    float backdrop_filter_quality = 1.0f;

    int mirror_count = 0;

    gfx::PointF scroll_offset;
    // Size of the scroll container that this layer scrolls in.
    gfx::Size scroll_container_bounds;

    // Corner clip radius for the 4 corners of the layer in the following order:
    //     top left, top right, bottom right, bottom left
    gfx::RoundedCornersF corner_radii;

    // Linear gradient mask applied to the layer's clip bounds and optionally
    // the rounded corner given by |corner_radii|.
    gfx::LinearGradient gradient_mask;

    base::RepeatingCallback<void(const gfx::PointF&, const ElementId&)>
        did_scroll_callback;
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests;
  };

  // Set either one or both components of the mask filter info which is then
  // applied to the layer and its
  // subtree (as if they are together as a single composited entity) when
  // blitting into their target.
  void UpdateMaskFilterInfo(const gfx::RoundedCornersF* corner_radii,
                            const gfx::LinearGradient* gradient_mask);

  ProtectedSequenceReadable<raw_ptr<Layer>> parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  //
  // Note about const-ness: layer_tree_host_ cannot be
  // ProtectedSequence(Readable|Writable), because that would create a circular
  // reference in WaitForProtectedSequenceCompletion(). However, it's definitely
  // *not* OK to modify layer_tree_host_ while in a protected sequence. To make
  // it hard to do the wrong thing, layer_tree_host_ is const, and
  // SetLayerTreeHost() uses a custom protected sequence check, and then uses
  // const_cast to do the assignment.
  const raw_ptr<LayerTreeHost> layer_tree_host_;

  ProtectedSequenceReadable<Inputs> inputs_;
  ProtectedSequenceReadable<std::unique_ptr<LayerTreeInputs>>
      layer_tree_inputs_;

  ProtectedSequenceWritable<gfx::Rect> update_rect_;

  const int layer_id_;

  ProtectedSequenceReadable<int> num_descendants_that_draw_content_;
  ProtectedSequenceReadable<int> transform_tree_index_;
  ProtectedSequenceReadable<int> effect_tree_index_;
  ProtectedSequenceReadable<int> clip_tree_index_;
  ProtectedSequenceReadable<int> scroll_tree_index_;
  ProtectedSequenceReadable<int> property_tree_sequence_number_;

  ProtectedSequenceReadable<gfx::Vector2dF> offset_to_transform_parent_;

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implicitly after the update completes. Not a bitfield
  // because it's used in base::AutoReset.
  ProtectedSequenceReadable<bool> ignore_set_needs_commit_for_test_;

  enum : uint8_t {
    kDrawsContentFlagMask = 1 << 0,
    kShouldCheckBackfaceVisibilityFlagMask = 1 << 1,
    kCacheRenderSurfaceFlagMask = 1 << 2,
    kForceRenderSurfaceForTestingFlagMask = 1 << 3,
    kMayContainVideoFlagMask = 1 << 4,
    kHasTransformNodeFlagMask = 1 << 5,
    kHasClipNodeFlagMask = 1 << 6,
    kSubtreeHasCopyRequestFlagMask = 1 << 7
  };
  ProtectedSequenceReadable<uint8_t> bitflags_;

  ProtectedSequenceWritable<bool> subtree_property_changed_;

#if DCHECK_IS_ON()
  class AllowRemoveForReadd {
   public:
    explicit AllowRemoveForReadd(Layer* layer) : layer_(layer) {
      // Assume these will never be nested.  If this DCHECK() fails due to
      // nesting, we could convert to using base::AutoReset.
      DCHECK(!layer_->allow_remove_for_readd_);
      layer_->allow_remove_for_readd_ = true;
    }
    ~AllowRemoveForReadd() {
      // Check that the layer has actually been re-added.
      DCHECK(layer_->parent());

      // Assume these will never be nested.  If this DCHECK() fails due to
      // nesting, we could convert to using base::AutoReset.
      DCHECK(layer_->allow_remove_for_readd_);
      layer_->allow_remove_for_readd_ = false;
    }

    AllowRemoveForReadd(const AllowRemoveForReadd&) = delete;
    AllowRemoveForReadd& operator=(const AllowRemoveForReadd&) = delete;

   private:
    raw_ptr<Layer> layer_;
  };

  bool allow_remove_for_readd_ = false;
#else
  class AllowRemoveForReadd {
   public:
    explicit AllowRemoveForReadd(Layer* layer) {}

    AllowRemoveForReadd(const AllowRemoveForReadd&) = delete;
    AllowRemoveForReadd& operator=(const AllowRemoveForReadd&) = delete;
  };
#endif

  ProtectedSequenceWritable<std::unique_ptr<LayerDebugInfo>> debug_info_;

  static constexpr gfx::Transform kIdentityTransform{};
  static constexpr gfx::RoundedCornersF kNoRoundedCornersF{};
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
