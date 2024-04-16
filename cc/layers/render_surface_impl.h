// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RENDER_SURFACE_IMPL_H_
#define CC_LAYERS_RENDER_SURFACE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/layers/draw_mode.h"
#include "cc/layers/layer_collections.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/property_tree.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {

class AppendQuadsData;
class DamageTracker;
class FilterOperations;
class Occlusion;
class LayerImpl;
class LayerTreeImpl;
class PictureLayerImpl;

struct RenderSurfacePropertyChangedFlags {
 public:
  RenderSurfacePropertyChangedFlags() = default;
  RenderSurfacePropertyChangedFlags(bool self_changed, bool ancestor_changed)
      : self_changed_(self_changed), ancestor_changed_(ancestor_changed) {}
  bool self_changed() const { return self_changed_; }
  bool ancestor_changed() const { return ancestor_changed_; }

 private:
  bool self_changed_ = false;
  bool ancestor_changed_ = false;
};

class CC_EXPORT RenderSurfaceImpl {
 public:
  RenderSurfaceImpl(LayerTreeImpl* layer_tree_impl, ElementId stable_id);
  RenderSurfaceImpl(const RenderSurfaceImpl&) = delete;
  virtual ~RenderSurfaceImpl();

  RenderSurfaceImpl& operator=(const RenderSurfaceImpl&) = delete;

  // Returns the RenderSurfaceImpl that this render surface contributes to. Root
  // render surface's render_target is itself.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  // Returns the rect that encloses the RenderSurfaceImpl including any
  // reflection.
  gfx::RectF DrawableContentRect() const;

  void SetDrawOpacity(float opacity) {
    draw_properties_.draw_opacity = opacity;
  }
  float draw_opacity() const { return draw_properties_.draw_opacity; }

  void SetMaskFilterInfo(const gfx::MaskFilterInfo& mask_filter_info,
                         bool is_fast_rounded_corner) {
    draw_properties_.mask_filter_info = mask_filter_info;
    draw_properties_.is_fast_rounded_corner = is_fast_rounded_corner;
  }
  const gfx::MaskFilterInfo& mask_filter_info() const {
    return draw_properties_.mask_filter_info;
  }
  bool is_fast_rounded_corner() const {
    return draw_properties_.is_fast_rounded_corner;
  }

  SkBlendMode BlendMode() const;

  void SetNearestOcclusionImmuneAncestor(const RenderSurfaceImpl* surface) {
    nearest_occlusion_immune_ancestor_ = surface;
  }
  const RenderSurfaceImpl* nearest_occlusion_immune_ancestor() const {
    return nearest_occlusion_immune_ancestor_;
  }

  SkColor4f GetDebugBorderColor() const;
  float GetDebugBorderWidth() const;

  void SetDrawTransform(const gfx::Transform& draw_transform) {
    draw_properties_.draw_transform = draw_transform;
  }
  const gfx::Transform& draw_transform() const {
    return draw_properties_.draw_transform;
  }

  void SetScreenSpaceTransform(const gfx::Transform& screen_space_transform) {
    draw_properties_.screen_space_transform = screen_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return draw_properties_.screen_space_transform;
  }

  void SetIsClipped(bool is_clipped) {
    draw_properties_.is_clipped = is_clipped;
  }
  bool is_clipped() const { return draw_properties_.is_clipped; }

  void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }

  // When false, the RenderSurface does not contribute to another target
  // RenderSurface that is being drawn for the current frame. It could still be
  // drawn to as a target, but its output will not be a part of any other
  // surface.
  bool contributes_to_drawn_surface() const {
    return contributes_to_drawn_surface_;
  }
  void set_contributes_to_drawn_surface(bool contributes_to_drawn_surface) {
    contributes_to_drawn_surface_ = contributes_to_drawn_surface;
  }

  // Called when any contributing layer's escapes OwningEffectNode's clip node,
  // or to clear the current `common_ancestor_clip_id_` before a full update.
  // After this is called for all clip-escaping layers,
  // `common_ancestor_clip_id_` is the lowest common ancestor of OwningEffect's
  // clip node and all contributing layers' clips. It will be used as the
  // render surface's clip. For now this is behind the
  // RenderSurfaceCommonAncestorClip feature.
  void set_common_ancestor_clip_id(int id) {
    DCHECK_NE(id, ClipTreeIndex());
    DCHECK(id < ClipTreeIndex() || id == kInvalidPropertyNodeId);
    common_ancestor_clip_id_ = id;
  }
  int common_ancestor_clip_id() const {
    return common_ancestor_clip_id_ == kInvalidPropertyNodeId
               ? ClipTreeIndex()
               : common_ancestor_clip_id_;
  }

  // TODO(wangxianzhu): Remove this when removing the
  // RenderSurfaceCommonAncestorClip feature.
  void set_has_contributing_layer_that_escapes_clip(
      bool contributing_layer_escapes_clip) {
    has_contributing_layer_that_escapes_clip_ = contributing_layer_escapes_clip;
  }
  bool has_contributing_layer_that_escapes_clip() const {
    return common_ancestor_clip_id_ != kInvalidPropertyNodeId ||
           // TODO(wangxianzhu): Remove this when removing the
           // RenderSurfaceCommonAncestorClip feature.
           has_contributing_layer_that_escapes_clip_;
  }

  void set_is_render_surface_list_member(bool is_render_surface_list_member) {
    is_render_surface_list_member_ = is_render_surface_list_member;
  }
  bool is_render_surface_list_member() const {
    return is_render_surface_list_member_;
  }

  void set_intersects_damage_under(bool intersects_damage_under) {
    intersects_damage_under_ = intersects_damage_under;
  }
  bool intersects_damage_under() const { return intersects_damage_under_; }

  void CalculateContentRectFromAccumulatedContentRect(int max_texture_size);
  void SetContentRectToViewport();
  void SetContentRectForTesting(const gfx::Rect& rect);
  gfx::Rect content_rect() const { return draw_properties_.content_rect; }

  void ClearAccumulatedContentRect();
  void AccumulateContentRectFromContributingLayer(
      LayerImpl* contributing_layer);
  void AccumulateContentRectFromContributingRenderSurface(
      RenderSurfaceImpl* contributing_surface);

  gfx::Rect accumulated_content_rect() const {
    return accumulated_content_rect_;
  }

  void increment_num_contributors() { num_contributors_++; }
  void decrement_num_contributors() {
    num_contributors_--;
    DCHECK_GE(num_contributors_, 0);
  }
  void reset_num_contributors() { num_contributors_ = 0; }
  int num_contributors() const { return num_contributors_; }

  const Occlusion& occlusion_in_content_space() const {
    return occlusion_in_content_space_;
  }
  void set_occlusion_in_content_space(const Occlusion& occlusion) {
    occlusion_in_content_space_ = occlusion;
  }

  ElementId id() const { return id_; }
  viz::CompositorRenderPassId render_pass_id() const {
    return viz::CompositorRenderPassId(id().GetInternalValue());
  }

  bool HasMaskingContributingSurface() const;

  const FilterOperations& Filters() const;
  const FilterOperations& BackdropFilters() const;
  std::optional<gfx::RRectF> BackdropFilterBounds() const;
  LayerImpl* BackdropMaskLayer() const;
  gfx::Transform SurfaceScale() const;

  bool TrilinearFiltering() const;

  bool HasCopyRequest() const;

  // The capture identifier for this render surface and its originating effect
  // node. If empty, this surface has not been selected as a subtree capture and
  // is either a root surface or will not be rendered separately.
  viz::SubtreeCaptureId SubtreeCaptureId() const;

  // The size of this surface that should be used for cropping capture. If
  // empty, the entire size of this surface should be used for capture.
  gfx::Size SubtreeSize() const;

  bool ShouldCacheRenderSurface() const;

  // Returns true if it's required to copy the output of this surface (i.e. when
  // it has copy requests, should be cached, or has a valid subtree capture ID),
  // and should be e.g. immune from occlusion, etc. Returns false otherwise.
  bool CopyOfOutputRequired() const;

  // These are to enable commit, where we need to snapshot these flags from the
  // main thread property trees, and then apply them to the sync tree.
  RenderSurfacePropertyChangedFlags GetPropertyChangeFlags() const;
  void ApplyPropertyChangeFlags(const RenderSurfacePropertyChangedFlags& flags);

  void ResetPropertyChangedFlags();
  bool SurfacePropertyChanged() const;
  bool AncestorPropertyChanged() const;
  void NoteAncestorPropertyChanged();
  bool HasDamageFromeContributingContent() const;

  DamageTracker* damage_tracker() const { return damage_tracker_.get(); }
  gfx::Rect GetDamageRect() const;

  std::unique_ptr<viz::CompositorRenderPass> CreateRenderPass();
  viz::ResourceId GetMaskResourceFromLayer(PictureLayerImpl* mask_layer,
                                           gfx::Size* mask_texture_size,
                                           gfx::RectF* mask_uv_rect) const;
  void AppendQuads(DrawMode draw_mode,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data);

  int TransformTreeIndex() const;
  int ClipTreeIndex() const;

  void set_effect_tree_index(int index) { effect_tree_index_ = index; }
  int EffectTreeIndex() const;

  const EffectNode* OwningEffectNode() const;
  EffectNode* OwningEffectNodeMutableForTest() const;

 private:
  void SetContentRect(const gfx::Rect& content_rect);
  gfx::Rect CalculateClippedAccumulatedContentRect();
  gfx::Rect CalculateExpandedClipForFilters(
      const gfx::Transform& target_to_surface);
  void TileMaskLayer(viz::CompositorRenderPass* render_pass,
                     viz::SharedQuadState* shared_quad_state,
                     const gfx::Rect& unoccluded_content_rect);

  // Returns true if this surface should be clipped. This is false if there
  // are copy requests, it should be cached, or is part of a view transition.
  bool ShouldClip() const;

  raw_ptr<LayerTreeImpl> layer_tree_impl_;
  ElementId id_;
  int effect_tree_index_;

  // Container for properties that render surfaces need to compute before they
  // can be drawn.
  struct DrawProperties {
    DrawProperties();
    ~DrawProperties();

    float draw_opacity = 1.0f;

    // Transforms from the surface's own space to the space of its target
    // surface.
    gfx::Transform draw_transform;
    // Transforms from the surface's own space to the viewport.
    gfx::Transform screen_space_transform;

    // This is in the surface's own space.
    gfx::Rect content_rect;

    // This is in the space of the surface's target surface.
    gfx::Rect clip_rect;

    // True if the surface needs to be clipped by clip_rect.
    bool is_clipped : 1 = false;

    // Contains a mask information applied to the layer. The coordinates is in
    // the target space of the render surface. The root render surface will
    // never have this set.
    gfx::MaskFilterInfo mask_filter_info;

    // This information is further passed to SharedQuadState when a
    // SharedQuadState and a quad for this layer that represents a render
    // surface is appended. Then, it's up to the SurfaceAggregator to decide
    // whether it can actually merge this render surface and avoid having
    // additional render pass.
    bool is_fast_rounded_corner : 1 = false;
  };

  DrawProperties draw_properties_;

  // Is used to calculate the content rect from property trees.
  gfx::Rect accumulated_content_rect_;
  int num_contributors_ = 0;

  // If this is not kInvalidPropertyNodeId, it means that some contributing
  // layer escaping the effect's clip node, and this is the the lowest common
  // ancestor of the effect's clip node and the clip nodes of all contributing
  // layers. Otherwise `ClipTreeIndex()` is already the common ancestor clip.
  int common_ancestor_clip_id_ = kInvalidPropertyNodeId;
  // Is used to decide if the surface is clipped.
  // TODO(wangxianzhu): Remove this when removing the
  // RenderSurfaceCommonAncestorClip feature.
  bool has_contributing_layer_that_escapes_clip_ : 1 = false;

  bool surface_property_changed_ : 1 = false;
  bool ancestor_property_changed_ : 1 = false;

  bool contributes_to_drawn_surface_ : 1 = false;
  bool is_render_surface_list_member_ : 1 = false;
  bool intersects_damage_under_ : 1 = true;

  Occlusion occlusion_in_content_space_;

  // The nearest ancestor target surface that will contain the contents of this
  // surface, and that ignores outside occlusion. This can point to itself.
  raw_ptr<const RenderSurfaceImpl, AcrossTasksDanglingUntriaged>
      nearest_occlusion_immune_ancestor_ = nullptr;

  std::unique_ptr<DamageTracker> damage_tracker_;
};

}  // namespace cc
#endif  // CC_LAYERS_RENDER_SURFACE_IMPL_H_
