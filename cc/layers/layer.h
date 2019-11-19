// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/touch_action_region.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_record.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace viz {
class CopyOutputRequest;
}

namespace cc {

class LayerImpl;
class LayerTreeHost;
class LayerTreeHostCommon;
class LayerTreeImpl;
class PictureLayer;

// For tracing and debugging. The info will be attached to this layer's tracing
// output.
struct LayerDebugInfo {
  LayerDebugInfo();
  LayerDebugInfo(const LayerDebugInfo&);
  ~LayerDebugInfo();

  std::string name;
  NodeId owner_node_id = kInvalidNodeId;
  int paint_count = 0;
  std::vector<const char*> compositing_reasons;
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
class CC_EXPORT Layer : public base::RefCounted<Layer> {
 public:
  // An invalid layer id, as all layer ids are positive.
  enum LayerIdLabels {
    INVALID_ID = -1,
  };

  // Factory to create a new Layer, with a unique id.
  static scoped_refptr<Layer> Create();

  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

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
  // For testing, return the actual stored value.
  SkColor ActualSafeOpaqueBackgroundColorForTesting() const {
    return safe_opaque_background_color_;
  }

  // Set and get the position of this layer, relative to its parent. This is
  // specified in layer space, which excludes device scale and page scale
  // factors, and ignoring transforms for this layer or ancestor layers. The
  // root layer's position is not used as it always appears at the origin of
  // the viewport. When property trees are built by cc (when IsUsingLayerLists
  // is false), position is used to update |offset_to_transform_parent|.
  void SetPosition(const gfx::PointF& position);
  const gfx::PointF& position() const { return inputs_.position; }

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
  const gfx::Size& bounds() const { return inputs_.bounds; }

  // Set or get that this layer clips its subtree to within its bounds. Content
  // of children will be intersected with the bounds of this layer when true.
  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return inputs_.masks_to_bounds; }

  // Set or get the clip rect for this layer. |clip_rect| is relative to |this|
  // layer. If you are trying to clip the subtree to the bounds of this layer,
  // SetMasksToBounds() would be a better alternative.
  void SetClipRect(const gfx::Rect& clip_rect);
  const gfx::Rect& clip_rect() const { return inputs_.clip_rect; }

  // Returns the bounds which is clipped by the clip rect.
  gfx::RectF EffectiveClipRect();

  // Set or get a layer that will mask the contents of this layer. The alpha
  // channel of the mask layer's content is used as an alpha mask of this
  // layer's content. IOW the mask's alpha is multiplied by this layer's alpha
  // for each matching pixel.
  // This is for layer tree mode only.
  void SetMaskLayer(scoped_refptr<PictureLayer> mask_layer);
  bool IsMaskedByChild() const { return !!inputs_.mask_layer; }

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

  void ResetUpdateRectForTesting() { inputs_.update_rect = gfx::Rect(); }

  // Set or get the rounded corner radii which is applied to the layer and its
  // subtree (as if they are together as a single composited entity) when
  // blitting into their target. Setting this makes the layer masked to bounds.
  // If the layer has a clip of its own, the rounded corner will be applied
  // along the layer's clip rect corners.
  void SetRoundedCorner(const gfx::RoundedCornersF& corner_radii);
  const gfx::RoundedCornersF& corner_radii() const {
    return inputs_.corner_radii;
  }

  // Returns true if any of the corner has a non-zero radius set.
  bool HasRoundedCorner() const { return !corner_radii().IsEmpty(); }

  // Set or get the flag that disables the requirement of a render surface for
  // this layer due to it having rounded corners. This improves performance at
  // the cost of maybe having some blending artifacts. Not having a render
  // surface is not guaranteed however.
  void SetIsFastRoundedCorner(bool enable);
  bool is_fast_rounded_corner() const { return inputs_.is_fast_rounded_corner; }

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
  // layer and its subtree will be drawn into. The effect is clipped by
  // backdrop_filter_bounds.
  void SetBackdropFilters(const FilterOperations& filters);
  const FilterOperations& backdrop_filters() const {
    return inputs_.backdrop_filters;
  }

  void SetBackdropFilterBounds(const gfx::RRectF& backdrop_filter_bounds);
  void ClearBackdropFilterBounds();
  const base::Optional<gfx::RRectF>& backdrop_filter_bounds() const {
    return inputs_.backdrop_filter_bounds;
  }

  void SetBackdropFilterQuality(const float quality);
  float backdrop_filter_quality() const {
    return inputs_.backdrop_filter_quality;
  }

  // Set or get an optimization hint that the contents of this layer are fully
  // opaque or not. If true, every pixel of content inside the layer's bounds
  // must be opaque or visual errors can occur. This applies only to this layer
  // and not to children, and does not imply the layer should be composited
  // opaquely, as effects may be applied such as opacity() or filters().
  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return inputs_.contents_opaque; }

  // Set or get whether this layer should be a hit test target
  void SetHitTestable(bool should_hit_test);
  virtual bool HitTestable() const;

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
  // independent of the scrollable state, or size of the scrollable area
  // specified in SetScrollable(), as these may be enabled or disabled
  // dynamically, while SetScrollable() defines what would be possible if these
  // are enabled.
  // When disabled, overscroll elasticity will not be used if the scroll offset
  // ends up past the maximum range. And when enabled, with overlay scrollbars,
  // the scrollbars will be shown when the scroll offset changes if these are
  // set to true.
  void SetUserScrollable(bool horizontal, bool vertical);
  bool GetUserScrollableHorizontal() const;
  bool GetUserScrollableVertical() const;

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
  // which case nothing is called. This is for layer tree mode only. Should use
  // ScrollTree::SetScrollCallbacks() in layer list mode.
  void SetDidScrollCallback(base::RepeatingCallback<
                            void(const gfx::ScrollOffset&, const ElementId&)>);

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

  // If the layer induces a render surface, this returns the cause for the
  // render surface. If the layer does not induce a render surface, this returns
  // kNone.
  RenderSurfaceReason GetRenderSurfaceReason() const;

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
  bool has_transform_node() const { return has_transform_node_; }

  // This value indicates whether a clip node was created for |this| layer.
  void SetHasClipNode(bool val) { has_clip_node_ = val; }

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

  // Increments/decrements/gets number of layers mirroring this layer.
  void IncrementMirrorCount();
  void DecrementMirrorCount();
  int mirror_count() const { return inputs_.mirror_count; }

  // Called on the scroll layer to trigger showing the overlay scrollbars.
  void ShowScrollbars() { needs_show_scrollbars_ = true; }

  // Captures text content within the given |rect| and returns the associated
  // NodeId in |content|.
  virtual void CaptureContent(const gfx::Rect& rect,
                              std::vector<NodeId>* content);

  // For tracing. Gets a recorded rasterization of this layer's contents that
  // can be displayed inside representations of this layer. May return null, in
  // which case the layer won't be shown with any content in the tracing
  // display.
  virtual sk_sp<SkPicture> GetPicture() const;

  const LayerDebugInfo* debug_info() const { return debug_info_.get(); }
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

  // The position of this layer after transforming by the layer's transform
  // node. When property trees are built by cc (when IsUsingLayerLists is false)
  // this is set by property_tree_builder.cc.
  void SetOffsetToTransformParent(gfx::Vector2dF offset);
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  // Internal to property tree construction. Indicates that a property changed
  // on this layer that may affect the position or content of all layers in this
  // layer's subtree, including itself. This causes the layer's subtree to be
  // considered damaged and re-displayed to the user.
  void SetSubtreePropertyChanged();
  void ClearSubtreePropertyChangedForTesting() {
    subtree_property_changed_ = false;
  }
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

  // For debugging, containing information about the associated DOM, etc.
  std::string DebugName() const;

  std::string ToString() const;

  // Called when a property has been modified in a way that the layer knows
  // immediately that a commit is required.  This implies SetNeedsPushProperties
  // to push that property.
  // This is public, so that it can be called directly when needed, for example
  // in PropertyTreeManager when handling scroll offsets.
  void SetNeedsCommit();

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;

  Layer();
  virtual ~Layer();

  // These SetNeeds functions are in order of severity of update:

  // See SetNeedsCommit() above - it belongs here in the order of severity.

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

  void IncreasePaintCount() {
    if (debug_info_)
      ++debug_info_->paint_count;
  }

  base::AutoReset<bool> IgnoreSetNeedsCommit() {
    return base::AutoReset<bool>(&ignore_set_needs_commit_, true);
  }

 private:
  friend class base::RefCounted<Layer>;
  friend class LayerTreeHostCommon;
  friend class LayerTreeHost;

  // Interactions with attached animations.
  void OnFilterAnimated(const FilterOperations& filters);
  void OnBackdropFilterAnimated(const FilterOperations& backdrop_filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);

  void AddClipChild(Layer* child);
  void RemoveClipChild(Layer* child);

  void SetParent(Layer* layer);

  // This should only be called from RemoveFromParent().
  void RemoveChild(Layer* child);

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

  void SetMirrorCount(int mirror_count);

  // Encapsulates all data, callbacks or interfaces received from the embedder.
  struct Inputs {
    explicit Inputs(int layer_id);
    ~Inputs();

    LayerList children;

    gfx::Rect update_rect;

    gfx::Size bounds;
    gfx::Rect clip_rect;

    // If not null, points to one of child layers which is set as mask layer
    // by SetMaskLayer().
    Layer* mask_layer;

    int layer_id;

    float opacity;
    SkBlendMode blend_mode;

    bool masks_to_bounds : 1;

    // Hit testing depends on this bit.
    bool hit_testable : 1;

    bool contents_opaque : 1;

    bool is_drawable : 1;

    bool double_sided : 1;

    bool use_parent_backface_visibility : 1;

    // If set, disables this layer's rounded corner from triggering a render
    // surface on itself if possible.
    bool is_fast_rounded_corner : 1;

    // Indicates that this layer will need a scroll property node and that this
    // layer's bounds correspond to the scroll node's bounds (both |bounds| and
    // |scroll_container_bounds|).
    bool scrollable : 1;

    // Indicates that this layer is a scrollbar.
    bool is_scrollbar : 1;

    bool user_scrollable_horizontal : 1;
    bool user_scrollable_vertical : 1;

    bool has_will_change_transform_hint : 1;

    bool trilinear_filtering : 1;

    bool hide_layer_and_subtree : 1;

    gfx::PointF position;
    gfx::Transform transform;
    gfx::Point3F transform_origin;

    SkColor background_color;

    FilterOperations filters;
    FilterOperations backdrop_filters;
    base::Optional<gfx::RRectF> backdrop_filter_bounds;
    gfx::PointF filters_origin;
    float backdrop_filter_quality;

    // Corner clip radius for the 4 corners of the layer in the following order:
    //     top left, top right, bottom right, bottom left
    gfx::RoundedCornersF corner_radii;

    gfx::ScrollOffset scroll_offset;

    // Size of the scroll container that this layer scrolls in.
    gfx::Size scroll_container_bounds;

    int mirror_count;

    Region non_fast_scrollable_region;

    TouchActionRegion touch_action_region;

    ElementId element_id;

    // These for for layer tree mode (ui compositor) only.
    base::RepeatingCallback<void(const gfx::ScrollOffset&, const ElementId&)>
        did_scroll_callback;
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests;
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

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implicitly after the update completes. Not a bitfield
  // because it's used in base::AutoReset.
  bool ignore_set_needs_commit_;

  bool draws_content_ : 1;
  bool should_check_backface_visibility_ : 1;
  // Force use of and cache render surface.
  bool cache_render_surface_ : 1;
  bool force_render_surface_for_testing_ : 1;
  bool subtree_property_changed_ : 1;
  bool may_contain_video_ : 1;
  bool needs_show_scrollbars_ : 1;
  bool has_transform_node_ : 1;
  bool has_clip_node_ : 1;
  // This value is valid only when LayerTreeHost::has_copy_request() is true
  bool subtree_has_copy_request_ : 1;

  SkColor safe_opaque_background_color_;

  std::unique_ptr<LayerDebugInfo> debug_info_;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
