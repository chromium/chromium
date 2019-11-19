// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_H_
#define CC_LAYERS_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/base/synced_property.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/input_handler.h"
#include "cc/layers/draw_mode.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/performance_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/touch_action_region.h"
#include "cc/paint/element_id.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/target_property.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
class DictionaryValue;
}

namespace viz {
class ClientResourceProvider;
class RenderPass;
}

namespace cc {

class AppendQuadsData;
struct LayerDebugInfo;
class LayerTreeImpl;
class MicroBenchmarkImpl;
class PrioritizedTile;
class ScrollbarLayerImplBase;
class SimpleEnclosedRegion;
class Tile;

enum ViewportLayerType {
  NOT_VIEWPORT_LAYER,
  INNER_VIEWPORT_CONTAINER,
  OUTER_VIEWPORT_CONTAINER,
  INNER_VIEWPORT_SCROLL,
  OUTER_VIEWPORT_SCROLL,
  LAST_VIEWPORT_LAYER_TYPE = OUTER_VIEWPORT_SCROLL,
};

class CC_EXPORT LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new LayerImpl(tree_impl, id));
  }

  LayerImpl(const LayerImpl&) = delete;
  virtual ~LayerImpl();

  LayerImpl& operator=(const LayerImpl&) = delete;

  int id() const { return layer_id_; }

  // Whether this layer is on the active tree, return false if it's on the
  // pending tree.
  bool IsActive() const;

  void SetHasTransformNode(bool val) { has_transform_node_ = val; }
  bool has_transform_node() const { return has_transform_node_; }

  void set_property_tree_sequence_number(int sequence_number) {}

  void SetTransformTreeIndex(int index);
  int transform_tree_index() const { return transform_tree_index_; }

  void SetClipTreeIndex(int index);
  int clip_tree_index() const { return clip_tree_index_; }

  void SetEffectTreeIndex(int index);
  int effect_tree_index() const { return effect_tree_index_; }
  int render_target_effect_tree_index() const;

  void SetScrollTreeIndex(int index);
  int scroll_tree_index() const { return scroll_tree_index_; }

  void SetOffsetToTransformParent(const gfx::Vector2dF& offset) {
    offset_to_transform_parent_ = offset;
  }
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  bool is_clipped() const { return draw_properties_.is_clipped; }

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  void PopulateSharedQuadState(viz::SharedQuadState* state,
                               bool contents_opaque) const;

  // If using these two, you need to override GetEnclosingRectInTargetSpace() to
  // use GetScaledEnclosingRectInTargetSpace(). To do otherwise may result in
  // inconsistent values, and drawing/clipping problems.
  void PopulateScaledSharedQuadState(viz::SharedQuadState* state,
                                     float layer_to_content_scale,
                                     bool contents_opaque) const;
  void PopulateScaledSharedQuadStateWithContentRects(
      viz::SharedQuadState* state,
      float layer_to_content_scale,
      const gfx::Rect& content_rect,
      const gfx::Rect& content_visible_rect,
      bool contents_opaque) const;

  // WillDraw must be called before AppendQuads. If WillDraw returns false,
  // AppendQuads and DidDraw will not be called. If WillDraw returns true,
  // DidDraw is guaranteed to be called before another WillDraw or before
  // the layer is destroyed.
  virtual bool WillDraw(DrawMode draw_mode,
                        viz::ClientResourceProvider* resource_provider);
  virtual void AppendQuads(viz::RenderPass* render_pass,
                           AppendQuadsData* append_quads_data) {}
  virtual void DidDraw(viz::ClientResourceProvider* resource_provider);

  // Verify that the resource ids in the quad are valid.
  void ValidateQuadResources(viz::DrawQuad* quad) const {
#if DCHECK_IS_ON()
    ValidateQuadResourcesInternal(quad);
#endif
  }

  virtual void GetContentsResourceId(viz::ResourceId* resource_id,
                                     gfx::Size* resource_size,
                                     gfx::SizeF* resource_uv_size) const;

  virtual void NotifyTileStateChanged(const Tile* tile) {}

  virtual ScrollbarLayerImplBase* ToScrollbarLayer();

  // Returns true if this layer has content to draw.
  void SetDrawsContent(bool draws_content);
  bool DrawsContent() const { return draws_content_; }

  // Make the layer hit testable.
  void SetHitTestable(bool should_hit_test);
  bool HitTestable() const;

  void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  float Opacity() const;

  // Stable identifier for clients. See comment in cc/trees/element_id.h.
  void SetElementId(ElementId element_id);
  ElementId element_id() const { return element_id_; }

  void SetMirrorCount(int mirror_count);
  int mirror_count() const { return mirror_count_; }

  bool IsAffectedByPageScale() const;

  bool Is3dSorted() const { return GetSortingContextId() != 0; }

  void SetUseParentBackfaceVisibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility) {
    should_check_backface_visibility_ = should_check_backface_visibility;
  }
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  bool ShowDebugBorders(DebugBorderType type) const;

  // The render surface which this layer draws into. This can be either owned by
  // the same layer or an ancestor of this layer.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  DrawProperties& draw_properties() { return draw_properties_; }
  const DrawProperties& draw_properties() const { return draw_properties_; }

  gfx::Transform DrawTransform() const;
  gfx::Transform ScreenSpaceTransform() const;
  PerformanceProperties<LayerImpl>& performance_properties() {
    return performance_properties_;
  }

  bool CanUseLCDText() const;

  // Setter for draw_properties_.
  void set_visible_layer_rect(const gfx::Rect& visible_rect) {
    draw_properties_.visible_layer_rect = visible_rect;
  }
  void set_clip_rect(const gfx::Rect& clip_rect) {
    draw_properties_.clip_rect = clip_rect;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  float draw_opacity() const { return draw_properties_.opacity; }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_layer_rect() const {
    return draw_properties_.visible_layer_rect;
  }

  // The client should be responsible for setting bounds, content bounds and
  // contents scale to appropriate values. LayerImpl doesn't calculate any of
  // them from the other values.

  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const;

  void set_is_inner_viewport_scroll_layer() {
    is_inner_viewport_scroll_layer_ = true;
  }

  void SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset);
  gfx::ScrollOffset CurrentScrollOffset() const;

  gfx::ScrollOffset MaxScrollOffset() const;
  gfx::ScrollOffset ClampScrollOffsetToLimits(gfx::ScrollOffset offset) const;
  gfx::Vector2dF ClampScrollToMaxScrollOffset();

  // Returns the delta of the scroll that was outside of the bounds of the
  // initial scroll
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& scroll);

  // Marks this layer as being scrollable and needing an associated scroll node.
  // The scroll node's bounds and container_bounds will be kept in sync with
  // this layer.
  void SetScrollable(const gfx::Size& bounds);
  gfx::Size scroll_container_bounds() const { return scroll_container_bounds_; }
  bool scrollable() const { return scrollable_; }

  void SetNonFastScrollableRegion(const Region& region) {
    non_fast_scrollable_region_ = region;
  }
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchActionRegion(TouchActionRegion);
  const TouchActionRegion& touch_action_region() const {
    return touch_action_region_;
  }
  const Region& GetAllTouchActionRegions() const;
  bool has_touch_action_regions() const {
    return !touch_action_region_.IsEmpty();
  }

  // Set or get the region that contains wheel event handler.
  // The |wheel_event_handler_region| specify the area where wheel event handler
  // could block impl scrolling.
  void SetWheelEventHandlerRegion(const Region& wheel_event_handler_region) {
    wheel_event_handler_region_ = wheel_event_handler_region;
  }
  const Region& wheel_event_handler_region() const {
    return wheel_event_handler_region_;
  }

  // The main thread may commit multiple times before the impl thread actually
  // draws, so we need to accumulate (i.e. union) any update changes that have
  // occurred on the main thread until we draw.
  // Note this rect is in layer space (not content space).
  void UnionUpdateRect(const gfx::Rect& update_rect);
  const gfx::Rect& update_rect() const { return update_rect_; }

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space. By default returns empty rect, but can be overridden by subclasses
  // as appropriate.
  virtual gfx::Rect GetDamageRect() const;

  virtual std::unique_ptr<base::DictionaryValue> LayerAsJson() const;

  // This includes |layer_property_changed_not_from_property_trees_| and
  // property_trees changes.
  bool LayerPropertyChanged() const;
  bool LayerPropertyChangedFromPropertyTrees() const;
  // Only checks |layer_property_changed_not_from_property_trees_|. Used in
  // damage_tracker to determine if there is a contributing content damage not
  // from property_trees changes in animaiton.
  bool LayerPropertyChangedNotFromPropertyTrees() const;

  virtual void ResetChangeTracking();

  virtual SimpleEnclosedRegion VisibleOpaqueRegion() const;

  virtual void DidBecomeActive() {}

  virtual void DidBeginTracing();

  // Release resources held by this layer. Called when the output surface
  // that rendered this layer was lost.
  virtual void ReleaseResources();

  // Releases resources in response to memory pressure. The default
  // implementation just calls ReleaseResources() and subclasses will override
  // if that's not appropriate.
  virtual void OnPurgeMemory();

  // Release tile resources held by this layer. Called when a rendering mode
  // switch has occurred and tiles are no longer valid.
  virtual void ReleaseTileResources();

  // Recreate tile resources held by this layer after they were released by a
  // ReleaseTileResources call.
  virtual void RecreateTileResources();

  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);
  virtual void PushPropertiesTo(LayerImpl* layer);

  // Internal to property tree construction (which only happens in tests on a
  // LayerImpl tree. See Layer::IsSnappedToPixelGridInTarget() for explanation,
  // as this mirrors that method.
  virtual bool IsSnappedToPixelGridInTarget();

  virtual void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;
  std::string ToString() const;

  virtual size_t GPUMemoryUsageInBytes() const;

  // Mark a layer on pending tree that needs to push its properties to the
  // active tree. These properties should not be changed during pending tree
  // lifetime, and only changed by being pushed from the main thread. There are
  // two cases where this function needs to be called: when main thread layer
  // has properties that need to be pushed, or when a new LayerImpl is created
  // on pending tree when syncing layers from main thread.
  void SetNeedsPushProperties();

  virtual void RunMicroBenchmark(MicroBenchmarkImpl* benchmark);

  void UpdateDebugInfo(LayerDebugInfo* debug_info);

  void set_contributes_to_drawn_render_surface(bool is_member) {
    contributes_to_drawn_render_surface_ = is_member;
  }

  bool contributes_to_drawn_render_surface() const {
    return contributes_to_drawn_render_surface_;
  }

  bool is_scrollbar() const { return is_scrollbar_; }

  void set_is_scrollbar(bool is_scrollbar) { is_scrollbar_ = is_scrollbar; }

  void set_may_contain_video(bool yes) { may_contain_video_ = yes; }
  bool may_contain_video() const { return may_contain_video_; }

  // Layers that share a sorting context id will be sorted together in 3d
  // space.  0 is a special value that means this layer will not be sorted and
  // will be drawn in paint order.
  int GetSortingContextId() const;

  // Get the correct invalidation region instead of conservative Rect
  // for layers that provide it.
  virtual Region GetInvalidationRegionForDebugging();

  // If you override this, and are making use of
  // PopulateScaledSharedQuadState(), make sure you call
  // GetScaledEnclosingRectInTargetSpace(). See comment for
  // PopulateScaledSharedQuadState().
  virtual gfx::Rect GetEnclosingRectInTargetSpace() const;

  // Returns the bounds of this layer in target space when scaled by |scale|.
  // This function scales in the same way as
  // PopulateScaledSharedQuadStateQuadState(). See
  // PopulateScaledSharedQuadStateQuadState() for more details.
  gfx::Rect GetScaledEnclosingRectInTargetSpace(float scale) const;

  float GetIdealContentsScale() const;

  void NoteLayerPropertyChanged();
  void NoteLayerPropertyChangedFromPropertyTrees();

  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return has_will_change_transform_hint_;
  }

  ElementListType GetElementTypeForAnimation() const;

  void set_needs_show_scrollbars(bool yes) { needs_show_scrollbars_ = yes; }
  bool needs_show_scrollbars() { return needs_show_scrollbars_; }

  void set_raster_even_if_not_drawn(bool yes) {
    raster_even_if_not_drawn_ = yes;
  }
  bool raster_even_if_not_drawn() const { return raster_even_if_not_drawn_; }

  void EnsureValidPropertyTreeIndices() const;

  // TODO(sunxd): Remove this function and replace it with visitor pattern.
  virtual bool is_surface_layer() const;

  int CalculateJitter();

 protected:
  // When |will_always_push_properties| is true, the layer will not itself set
  // its SetNeedsPushProperties() state, as it expects to be always pushed to
  // the active tree regardless.
  LayerImpl(LayerTreeImpl* layer_impl,
            int id,
            bool will_always_push_properties = false);

  // Get the color and size of the layer's debug border.
  virtual void GetDebugBorderProperties(SkColor* color, float* width) const;

  void AppendDebugBorderQuad(viz::RenderPass* render_pass,
                             const gfx::Rect& quad_rect,
                             const viz::SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data) const;
  void AppendDebugBorderQuad(viz::RenderPass* render_pass,
                             const gfx::Rect& quad_rect,
                             const viz::SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data,
                             SkColor color,
                             float width) const;

 private:
  void ValidateQuadResourcesInternal(viz::DrawQuad* quad) const;

  virtual const char* LayerTypeAsString() const;

  const int layer_id_;
  LayerTreeImpl* const layer_tree_impl_;
  const bool will_always_push_properties_ : 1;

  // Properties synchronized from the associated Layer.
  gfx::Size bounds_;

  gfx::Vector2dF offset_to_transform_parent_;

  // Size of the scroll container that this layer scrolls in.
  gfx::Size scroll_container_bounds_;

  // Indicates that this layer will have a scroll property node and that this
  // layer's bounds correspond to the scroll node's bounds (both |bounds| and
  // |scroll_container_bounds|).
  bool scrollable_ : 1;

  // Tracks if drawing-related properties have changed since last redraw.
  // TODO(wutao): We want to distinquish the sources of change so that we can
  // reuse the cache of render pass. For example, we can reuse the cache when
  // transform and opacity changing on a surface during animation. Currently
  // |layer_property_changed_from_property_trees_| does not mean the layer is
  // damaged from animation. We need better mechanism to explicitly capture
  // damage from animations. http://crbug.com/755828.
  bool layer_property_changed_not_from_property_trees_ : 1;
  bool layer_property_changed_from_property_trees_ : 1;
  bool may_contain_video_ : 1;

  bool masks_to_bounds_ : 1;
  bool contents_opaque_ : 1;
  bool use_parent_backface_visibility_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool draws_content_ : 1;
  bool contributes_to_drawn_render_surface_ : 1;

  // Tracks if this layer should participate in hit testing.
  bool hit_testable_ : 1;

  bool is_inner_viewport_scroll_layer_ : 1;

  Region non_fast_scrollable_region_;
  TouchActionRegion touch_action_region_;
  Region wheel_event_handler_region_;
  SkColor background_color_;
  SkColor safe_opaque_background_color_;

  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;

 protected:
  friend class TreeSynchronizer;

  DrawMode current_draw_mode_;
  EffectTree& GetEffectTree() const;

 private:
  PropertyTrees* GetPropertyTrees() const;
  ClipTree& GetClipTree() const;
  ScrollTree& GetScrollTree() const;
  TransformTree& GetTransformTree() const;

  ElementId element_id_;
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // This is in the layer's space.
  gfx::Rect update_rect_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties draw_properties_;
  PerformanceProperties<LayerImpl> performance_properties_;

  std::unique_ptr<LayerDebugInfo> debug_info_;

  // Cache of all regions represented by any touch action from
  // |touch_action_region_|.
  mutable std::unique_ptr<Region> all_touch_action_regions_;

  bool has_will_change_transform_hint_ : 1;
  bool needs_push_properties_ : 1;
  bool is_scrollbar_ : 1;
  bool scrollbars_hidden_ : 1;

  // The needs_show_scrollbars_ bit tracks a pending request from Blink to show
  // the overlay scrollbars. It's set on the scroll layer (not the scrollbar
  // layers) and consumed by LayerTreeImpl::PushPropertiesTo during activation.
  bool needs_show_scrollbars_ : 1;

  // This is set for layers that have a property because of which they are not
  // drawn (singular transforms), but they can become visible soon (the property
  // is being animated). For this reason, while these layers are not drawn, they
  // are still rasterized.
  bool raster_even_if_not_drawn_ : 1;

  bool has_transform_node_ : 1;

  // Number of layers mirroring this layer. If greater than zero, forces a
  // render pass for the layer so it can be embedded by the mirroring layer.
  int mirror_count_;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
