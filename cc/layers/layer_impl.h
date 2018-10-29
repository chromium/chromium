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
#include "base/macros.h"
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
#include "cc/layers/layer_impl_test_properties.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/performance_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/touch_action_region.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/element_id.h"
#include "cc/trees/target_property.h"
#include "components/viz/common/quads/shared_quad_state.h"
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
class DictionaryValue;
}

namespace viz {
class ClientResourceProvider;
class RenderPass;
}

namespace cc {

class AppendQuadsData;
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

  virtual ~LayerImpl();

  int id() const { return layer_id_; }

  // Whether this layer is on the active tree, return false if it's on the
  // pending tree.
  bool IsActive() const;

  void SetHasTransformNode(bool val) { has_transform_node_ = val; }
  bool has_transform_node() { return has_transform_node_; }

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

  void SetShouldFlattenScreenSpaceTransformFromPropertyTree(
      bool should_flatten) {
    should_flatten_screen_space_transform_from_property_tree_ = should_flatten;
  }
  bool should_flatten_screen_space_transform_from_property_tree() const {
    return should_flatten_screen_space_transform_from_property_tree_;
  }

  bool is_clipped() const { return draw_properties_.is_clipped; }

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  void PopulateSharedQuadState(viz::SharedQuadState* state,
                               bool contents_opaque) const;
  void PopulateScaledSharedQuadState(viz::SharedQuadState* state,
                                     float layer_to_content_scale_x,
                                     float layer_to_content_scale_y,
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

  // Make the layer hit test (see: |should_hit_test|) even if !draws_content_.
  void SetHitTestableWithoutDrawsContent(bool should_hit_test);
  bool hit_testable_without_draws_content() const {
    return hit_testable_without_draws_content_;
  }

  // True if either the layer draws content or has been marked as hit testable
  // without draws_content.
  bool should_hit_test() const {
    return draws_content_ || hit_testable_without_draws_content_;
  }

  LayerImplTestProperties* test_properties() {
    if (!test_properties_)
      test_properties_.reset(new LayerImplTestProperties(this));
    return test_properties_.get();
  }

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

  void SetPosition(const gfx::PointF& position);
  gfx::PointF position() const { return position_; }

  bool IsAffectedByPageScale() const;

  bool Is3dSorted() const { return GetSortingContextId() != 0; }

  void SetUseParentBackfaceVisibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  bool IsResizedByBrowserControls() const;
  void SetIsResizedByBrowserControls(bool resized);

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
  // Like bounds() but doesn't snap to int. Lossy on giant pages (e.g. millions
  // of pixels) due to use of single precision float.
  gfx::SizeF BoundsForScrolling() const;

  // Viewport bounds delta are only used for viewport layers and account for
  // changes in the viewport layers from browser controls and page scale
  // factors. These deltas are only set on the active tree.
  // TODO(bokan): These methods should be unneeded now that LTHI sets these
  // directly on the property trees.
  void SetViewportBoundsDelta(const gfx::Vector2dF& bounds_delta);
  gfx::Vector2dF ViewportBoundsDelta() const;

  void SetViewportLayerType(ViewportLayerType type) {
    // Once set as a viewport layer type, the viewport type should not change.
    DCHECK(viewport_layer_type() == NOT_VIEWPORT_LAYER ||
           viewport_layer_type() == type);
    viewport_layer_type_ = type;
  }
  ViewportLayerType viewport_layer_type() const {
    return static_cast<ViewportLayerType>(viewport_layer_type_);
  }
  bool is_viewport_layer_type() const {
    return viewport_layer_type() != NOT_VIEWPORT_LAYER;
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

  void set_main_thread_scrolling_reasons(
      uint32_t main_thread_scrolling_reasons) {
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
  }
  uint32_t main_thread_scrolling_reasons() const {
    return main_thread_scrolling_reasons_;
  }

  void SetNonFastScrollableRegion(const Region& region) {
    non_fast_scrollable_region_ = region;
  }
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchActionRegion(TouchActionRegion touch_action_region) {
    touch_action_region_ = std::move(touch_action_region);
  }
  const TouchActionRegion& touch_action_region() const {
    return touch_action_region_;
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

  // Note this rect is in layer space (not content space).
  void SetUpdateRect(const gfx::Rect& update_rect);
  const gfx::Rect& update_rect() const { return update_rect_; }

  void AddDamageRect(const gfx::Rect& damage_rect);
  const gfx::Rect& damage_rect() const { return damage_rect_; }

  virtual std::unique_ptr<base::DictionaryValue> LayerAsJson();
  // TODO(pdr): This should be removed because there is no longer a tree
  // of layers, only a list.
  std::unique_ptr<base::DictionaryValue> LayerTreeAsJson();

  // This includes |layer_property_changed_not_from_property_trees_| and
  // property_trees changes.
  bool LayerPropertyChanged() const;
  bool LayerPropertyChangedFromPropertyTrees() const;
  // Only checks |layer_property_changed_not_from_property_trees_|. Used in
  // damage_tracker to determine if there is a contributing content damage not
  // from property_trees changes in animaiton.
  bool LayerPropertyChangedNotFromPropertyTrees() const;

  void ResetChangeTracking();

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

  virtual size_t GPUMemoryUsageInBytes() const;

  // Mark a layer on pending tree that needs to push its properties to the
  // active tree. These properties should not be changed during pending tree
  // lifetime, and only changed by being pushed from the main thread. There are
  // two cases where this function needs to be called: when main thread layer
  // has properties that need to be pushed, or when a new LayerImpl is created
  // on pending tree when syncing layers from main thread.
  void SetNeedsPushProperties();

  virtual void RunMicroBenchmark(MicroBenchmarkImpl* benchmark);

  void SetDebugInfo(std::unique_ptr<base::trace_event::TracedValue> debug_info);

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

  virtual gfx::Rect GetEnclosingRectInTargetSpace() const;

  void UpdatePropertyTreeForAnimationIfNeeded(ElementId element_id);

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

  void set_is_rounded_corner_mask(bool rounded) {
    is_rounded_corner_mask_ = rounded;
  }
  bool is_rounded_corner_mask() const { return is_rounded_corner_mask_; }

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

  gfx::Rect GetScaledEnclosingRectInTargetSpace(float scale) const;

 private:
  void ValidateQuadResourcesInternal(viz::DrawQuad* quad) const;

  virtual const char* LayerTypeAsString() const;

  const int layer_id_;
  LayerTreeImpl* const layer_tree_impl_;
  const bool will_always_push_properties_ : 1;

  std::unique_ptr<LayerImplTestProperties> test_properties_;

  // Properties synchronized from the associated Layer.
  gfx::Size bounds_;

  gfx::Vector2dF offset_to_transform_parent_;
  uint32_t main_thread_scrolling_reasons_;

  // Size of the scroll container that this layer scrolls in.
  gfx::Size scroll_container_bounds_;

  // Indicates that this layer will have a scroll property node and that this
  // layer's bounds correspond to the scroll node's bounds (both |bounds| and
  // |scroll_container_bounds|).
  bool scrollable_ : 1;

  bool should_flatten_screen_space_transform_from_property_tree_ : 1;

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

  // Hit testing depends on draws_content (see: |LayerImpl::should_hit_test|)
  // and this bit can be set to cause the layer to be hit testable without
  // draws_content.
  bool hit_testable_without_draws_content_ : 1;
  bool is_resized_by_browser_controls_ : 1;

  // TODO(bokan): This can likely be removed after blink-gen-property-trees
  // is shipped. https://crbug.com/836884.
  static_assert(LAST_VIEWPORT_LAYER_TYPE < (1u << 3),
                "enough bits for ViewportLayerType (viewport_layer_type_)");
  uint8_t viewport_layer_type_ : 3;  // ViewportLayerType

  Region non_fast_scrollable_region_;
  TouchActionRegion touch_action_region_;
  Region wheel_event_handler_region_;
  SkColor background_color_;
  SkColor safe_opaque_background_color_;

  gfx::PointF position_;

  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;

 protected:
  friend class TreeSynchronizer;

  DrawMode current_draw_mode_;

 private:
  PropertyTrees* GetPropertyTrees() const;
  ClipTree& GetClipTree() const;
  EffectTree& GetEffectTree() const;
  ScrollTree& GetScrollTree() const;
  TransformTree& GetTransformTree() const;

  ElementId element_id_;
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // This is in the layer's space.
  gfx::Rect update_rect_;

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties draw_properties_;
  PerformanceProperties<LayerImpl> performance_properties_;

  std::unique_ptr<base::trace_event::TracedValue> owned_debug_info_;
  base::trace_event::TracedValue* debug_info_;

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
  bool is_rounded_corner_mask_ : 1;

  DISALLOW_COPY_AND_ASSIGN(LayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
