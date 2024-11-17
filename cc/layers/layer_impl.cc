// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/benchmarks/micro_benchmark_impl.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/scroll_state.h"
#include "cc/layers/layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

namespace {

template <typename T>
std::unique_ptr<T> ClonePtr(const std::unique_ptr<T>& value) {
  return value ? std::make_unique<T>(*value) : nullptr;
}

const char* LayerTypeAsString(mojom::LayerType type) {
  switch (type) {
    case mojom::LayerType::kLayer:
      return "cc::LayerImpl";
    case mojom::LayerType::kSolidColor:
      return "cc::SolidColorLayerImpl";
    case mojom::LayerType::kTexture:
      return "cc::TextureLayerImpl";
    case mojom::LayerType::kSurface:
      return "cc::SurfaceLayerImpl";
    case mojom::LayerType::kPicture:
      return "cc::PictureLayerImpl";
    case mojom::LayerType::kTileDisplay:
      return "cc::TileDisplayLayerImpl";
    case mojom::LayerType::kMirror:
      return "cc::MirrorLayerImpl";
    case mojom::LayerType::kHeadsUpDisplay:
      return "cc::HeadsUpDisplayLayerImpl";
    case mojom::LayerType::kUIResource:
      return "cc::UIResourceLayerImpl";
    case mojom::LayerType::kNinePatch:
      return "cc::NinePatchLayerImpl";
    case mojom::LayerType::kSolidColorScrollbar:
      return "cc::SolidColorScrollbarLayerImpl";
    case mojom::LayerType::kPaintedScrollbar:
      return "cc::PaintedScrollbarLayerImpl";
    case mojom::LayerType::kNinePatchThumbScrollbar:
      return "cc::NinePatchThumbScrollbarLayerImpl";
    case mojom::LayerType::kVideo:
      return "cc::VideoLayerImpl";
    case mojom::LayerType::kViewTransitionContent:
      return "cc::ViewTransitionContentLayerImpl";
  }
}

}  // namespace

LayerImpl::RareProperties::RareProperties() = default;
LayerImpl::RareProperties::RareProperties(const RareProperties&) = default;
LayerImpl::RareProperties::~RareProperties() = default;

LayerImpl::LayerImpl(LayerTreeImpl* tree_impl,
                     int id,
                     bool will_always_push_properties)
    : layer_id_(id),
      layer_tree_impl_(tree_impl),
      will_always_push_properties_(will_always_push_properties),
      transform_tree_index_(kInvalidPropertyNodeId),
      effect_tree_index_(kInvalidPropertyNodeId),
      clip_tree_index_(kInvalidPropertyNodeId),
      scroll_tree_index_(kInvalidPropertyNodeId),
      current_draw_mode_(DRAW_MODE_NONE) {
  DCHECK_GT(layer_id_, 0);

  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);

  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  layer_tree_impl_->UnregisterLayer(this);
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);
}

mojom::LayerType LayerImpl::GetLayerType() const {
  return mojom::LayerType::kLayer;
}

ElementListType LayerImpl::GetElementTypeForAnimation() const {
  return IsActive() ? ElementListType::ACTIVE : ElementListType::PENDING;
}

void LayerImpl::UpdateDebugInfo(LayerDebugInfo* debug_info) {
  // nullptr means we have stopped collecting debug info.
  if (!debug_info) {
    debug_info_.reset();
    return;
  }
  auto new_invalidations = std::move(debug_info->invalidations);
  if (!debug_info_) {
    debug_info_ = std::make_unique<LayerDebugInfo>(*debug_info);
    debug_info_->invalidations = std::move(new_invalidations);
    return;
  }
  // Accumulate invalidations until we draw the layer.
  auto existing_invalidations = std::move(debug_info_->invalidations);
  *debug_info_ = *debug_info;
  debug_info_->invalidations.insert(debug_info_->invalidations.begin(),
                                    existing_invalidations.begin(),
                                    existing_invalidations.end());
}

void LayerImpl::SetTransformTreeIndex(int index) {
  transform_tree_index_ = index;
}

void LayerImpl::SetClipTreeIndex(int index) {
  clip_tree_index_ = index;
}

void LayerImpl::SetEffectTreeIndex(int index) {
  effect_tree_index_ = index;
}

int LayerImpl::render_target_effect_tree_index() const {
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);

  return GetEffectTree().GetRenderSurface(effect_tree_index_)
             ? effect_node->id
             : effect_node->target_id;
}

void LayerImpl::SetScrollTreeIndex(int index) {
  scroll_tree_index_ = index;
}

void LayerImpl::PopulateSharedQuadState(viz::SharedQuadState* state,
                                        bool contents_opaque) const {
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);
  std::optional<gfx::Rect> clip_rect;
  if (draw_properties_.is_clipped) {
    clip_rect = draw_properties_.clip_rect;
  }
  state->SetAll(draw_properties_.target_space_transform, gfx::Rect(bounds()),
                draw_properties_.visible_layer_rect,
                draw_properties_.mask_filter_info, clip_rect, contents_opaque,
                draw_properties_.opacity,
                effect_node->HasRenderSurface() ? SkBlendMode::kSrcOver
                                                : effect_node->blend_mode,
                GetSortingContextId(), static_cast<uint32_t>(id()),
                draw_properties_.is_fast_rounded_corner);
}

void LayerImpl::PopulateScaledSharedQuadState(viz::SharedQuadState* state,
                                              float layer_to_content_scale,
                                              bool contents_opaque) const {
  gfx::Size scaled_bounds =
      gfx::ScaleToCeiledSize(bounds(), layer_to_content_scale);
  gfx::Rect scaled_visible_layer_rect =
      gfx::ScaleToEnclosingRect(visible_layer_rect(), layer_to_content_scale);
  scaled_visible_layer_rect.Intersect(gfx::Rect(scaled_bounds));

  PopulateScaledSharedQuadStateWithContentRects(
      state, layer_to_content_scale, gfx::Rect(scaled_bounds),
      scaled_visible_layer_rect, contents_opaque);
}

void LayerImpl::PopulateScaledSharedQuadStateWithContentRects(
    viz::SharedQuadState* state,
    float layer_to_content_scale,
    const gfx::Rect& content_rect,
    const gfx::Rect& visible_content_rect,
    bool contents_opaque) const {
  gfx::Transform scaled_draw_transform =
      GetScaledDrawTransform(layer_to_content_scale);

  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);
  std::optional<gfx::Rect> clip_rect;
  if (draw_properties().is_clipped) {
    clip_rect = draw_properties().clip_rect;
  }
  state->SetAll(scaled_draw_transform, content_rect, visible_content_rect,
                draw_properties().mask_filter_info, clip_rect, contents_opaque,
                draw_properties().opacity,
                effect_node->HasRenderSurface() ? SkBlendMode::kSrcOver
                                                : effect_node->blend_mode,
                GetSortingContextId(), static_cast<uint32_t>(id()),
                draw_properties().is_fast_rounded_corner);
}

bool LayerImpl::WillDraw(DrawMode draw_mode,
                         viz::ClientResourceProvider* resource_provider) {
  if (visible_layer_rect().IsEmpty() ||
      draw_properties().occlusion_in_content_space.IsOccluded(
          visible_layer_rect())) {
    return false;
  }

  // Resourceless mode does not support non-default blend mode. If we draw,
  // the result will be just like kSrcOver which is not too bad for blend modes
  // other than kDstIn. For kDstIn mode, we should ignore the source because
  // otherwise we would draw a bad black mask over the destination.
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    const auto* effect_node = GetEffectTree().Node(effect_tree_index());
    if (effect_node && effect_node->blend_mode == SkBlendMode::kDstIn)
      return false;
  }

  current_draw_mode_ = draw_mode;
  return true;
}

void LayerImpl::DidDraw(viz::ClientResourceProvider* resource_provider) {
  current_draw_mode_ = DRAW_MODE_NONE;
}

bool LayerImpl::ShowDebugBorders(DebugBorderType type) const {
  return layer_tree_impl()->debug_state().show_debug_borders.test(type);
}

void LayerImpl::GetDebugBorderProperties(SkColor4f* color, float* width) const {
  float device_scale_factor =
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1;

  if (draws_content_) {
    *color = DebugColors::ContentLayerBorderColor();
    *width = DebugColors::ContentLayerBorderWidth(device_scale_factor);
    return;
  }

  *color = DebugColors::ContainerLayerBorderColor();
  *width = DebugColors::ContainerLayerBorderWidth(device_scale_factor);
}

void LayerImpl::AppendDebugBorderQuad(
    viz::CompositorRenderPass* render_pass,
    const gfx::Rect& quad_rect,
    const viz::SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor4f color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass, quad_rect, shared_quad_state,
                        append_quads_data, color, width);
}

void LayerImpl::AppendDebugBorderQuad(
    viz::CompositorRenderPass* render_pass,
    const gfx::Rect& quad_rect,
    const viz::SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data,
    SkColor4f color,
    float width) const {
  if (!ShowDebugBorders(DebugBorderType::LAYER))
    return;

  // This is the debug border quad layer size. The mojo serialization will fail
  // if the area overflows, so just drop this debug border quad in that case to
  // avoid crashes.
  if (!quad_rect.size().GetCheckedArea().IsValid())
    return;

  gfx::Rect visible_quad_rect(quad_rect);
  auto* debug_border_quad =
      render_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                            color, width);
  if (contents_opaque()) {
    // When opaque, draw a second inner border that is thicker than the outer
    // border, but more transparent.
    static const float kFillOpacity = 0.3f;
    SkColor4f fill_color = color;
    fill_color.fA *= kFillOpacity;
    float fill_width = width * 3;
    gfx::Rect fill_rect = quad_rect;
    fill_rect.Inset(fill_width / 2.f);
    if (fill_rect.IsEmpty())
      return;
    gfx::Rect visible_fill_rect =
        gfx::IntersectRects(visible_quad_rect, fill_rect);
    auto* fill_quad =
        render_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
    fill_quad->SetNew(shared_quad_state, fill_rect, visible_fill_rect,
                      fill_color, fill_width);
  }
}

void LayerImpl::GetContentsResourceId(viz::ResourceId* resource_id,
                                      gfx::Size* resource_size,
                                      gfx::SizeF* resource_uv_size) const {
  NOTREACHED();
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index());
  DCHECK(scroll_node);
  return scroll_tree.ScrollBy(*scroll_node, scroll, layer_tree_impl());
}

void LayerImpl::SetTouchActionRegion(TouchActionRegion region) {
  // Avoid recalculating the cached |all_touch_action_regions_| value.
  if (touch_action_region_ == region)
    return;
  touch_action_region_ = std::move(region);
  all_touch_action_regions_ = nullptr;
}

const Region& LayerImpl::GetAllTouchActionRegions() const {
  if (!all_touch_action_regions_) {
    all_touch_action_regions_ =
        std::make_unique<Region>(touch_action_region_.GetAllRegions());
  } else {
    // Ensure the cached value of |all_touch_action_regions_| is up to date.
    DCHECK_EQ(touch_action_region_.GetAllRegions(), *all_touch_action_regions_);
  }
  return *all_touch_action_regions_;
}

void LayerImpl::SetCaptureBounds(viz::RegionCaptureBounds bounds) {
  if (rare_properties_ || !bounds.IsEmpty())
    EnsureRareProperties().capture_bounds = std::move(bounds);
}

std::unique_ptr<LayerImpl> LayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return LayerImpl::Create(tree_impl, layer_id_);
}

bool LayerImpl::IsSnappedToPixelGridInTarget() {
  return false;
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  DCHECK(layer->IsActive());

  // The element id should be set first because other setters may
  // depend on it. Referencing element id on a layer is
  // deprecated. http://crbug.com/709137
  layer->SetElementId(element_id_);

  layer->has_transform_node_ = has_transform_node_;
  layer->offset_to_transform_parent_ = offset_to_transform_parent_;
  layer->contents_opaque_ = contents_opaque_;
  layer->contents_opaque_for_text_ = contents_opaque_for_text_;
  layer->may_contain_video_ = may_contain_video_;
  layer->should_check_backface_visibility_ = should_check_backface_visibility_;
  layer->draws_content_ = draws_content_;
  layer->hit_test_opaqueness_ = hit_test_opaqueness_;
  layer->touch_action_region_ = touch_action_region_;
  layer->all_touch_action_regions_ = ClonePtr(all_touch_action_regions_);
  layer->background_color_ = background_color_;
  layer->safe_opaque_background_color_ = safe_opaque_background_color_;
  layer->transform_tree_index_ = transform_tree_index_;
  layer->effect_tree_index_ = effect_tree_index_;
  layer->clip_tree_index_ = clip_tree_index_;
  layer->scroll_tree_index_ = scroll_tree_index_;

  if (layer_property_changed_not_from_property_trees_ ||
      layer_property_changed_from_property_trees_)
    layer->layer_tree_impl()->set_needs_update_draw_properties();
  if (layer_property_changed_not_from_property_trees_)
    layer->layer_property_changed_not_from_property_trees_ = true;
  if (layer_property_changed_from_property_trees_)
    layer->layer_property_changed_from_property_trees_ = true;

  layer->SetBounds(bounds_);

  layer->UnionUpdateRect(update_rect_);

  layer->UpdateDebugInfo(debug_info_.get());

  if (rare_properties_) {
    layer->rare_properties_ =
        std::make_unique<RareProperties>(*rare_properties_);
  } else {
    layer->rare_properties_.reset();
  }

  // Reset any state that should be cleared for the next update.
  ResetChangeTracking();

  if (layer_tree_impl()->settings().UseLayerContextForDisplay()) {
    // Ensure updates also propagate to the display tree on its next update.
    layer->SetNeedsPushProperties();
  }
}

bool LayerImpl::IsAffectedByPageScale() const {
  TransformTree& transform_tree = GetTransformTree();
  return transform_tree.Node(transform_tree_index())
      ->in_subtree_of_page_scale_layer;
}

bool LayerImpl::LayerPropertyChanged() const {
  return layer_property_changed_not_from_property_trees_ ||
         LayerPropertyChangedFromPropertyTrees();
}

bool LayerImpl::LayerPropertyChangedFromPropertyTrees() const {
  if (layer_property_changed_from_property_trees_ ||
      GetPropertyTrees()->full_tree_damaged())
    return true;
  if (transform_tree_index() == kInvalidPropertyNodeId)
    return false;
  TransformNode* transform_node =
      GetTransformTree().Node(transform_tree_index());
  if (transform_node && transform_node->transform_changed)
    return true;
  if (effect_tree_index() == kInvalidPropertyNodeId)
    return false;
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  if (effect_node && effect_node->effect_changed)
    return true;
  return false;
}

bool LayerImpl::LayerPropertyChangedNotFromPropertyTrees() const {
  return layer_property_changed_not_from_property_trees_;
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_not_from_property_trees_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::NoteLayerPropertyChangedFromPropertyTrees() {
  layer_property_changed_from_property_trees_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::ValidateQuadResourcesInternal(viz::DrawQuad* quad) const {
#if DCHECK_IS_ON()
  const viz::ClientResourceProvider* resource_provider =
      layer_tree_impl_->resource_provider();
  for (viz::ResourceId resource_id : quad->resources)
    resource_provider->ValidateResource(resource_id);
#endif
}

gfx::Transform LayerImpl::GetScaledDrawTransform(
    float layer_to_content_scale) const {
  gfx::Transform scaled_draw_transform =
      draw_properties_.target_space_transform;
  scaled_draw_transform.Scale(SK_Scalar1 / layer_to_content_scale,
                              SK_Scalar1 / layer_to_content_scale);
  return scaled_draw_transform;
}

void LayerImpl::ResetChangeTracking() {
  layer_property_changed_not_from_property_trees_ = false;
  layer_property_changed_from_property_trees_ = false;
  needs_push_properties_ = false;

  update_rect_.SetRect(0, 0, 0, 0);
  if (debug_info_)
    debug_info_->invalidations.clear();
}

bool LayerImpl::IsActive() const {
  return layer_tree_impl_->IsActiveTree();
}

gfx::Size LayerImpl::bounds() const {
  if (!is_inner_viewport_scroll_layer_)
    return bounds_;

  auto viewport_bounds_delta = gfx::ToCeiledVector2d(
      GetPropertyTrees()->inner_viewport_scroll_bounds_delta());
  return gfx::Size(bounds_.width() + viewport_bounds_delta.x(),
                   bounds_.height() + viewport_bounds_delta.y());
}

void LayerImpl::SetBounds(const gfx::Size& bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;
  NoteLayerPropertyChanged();
}

bool LayerImpl::IsScrollbarLayer() const {
  return false;
}

bool LayerImpl::IsScrollerOrScrollbar() const {
  DCHECK(!layer_tree_impl()->settings().enable_hit_test_opaqueness);
  return IsScrollbarLayer() ||
         GetScrollTree().FindNodeFromElementId(element_id());
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetHitTestOpaqueness(HitTestOpaqueness opaqueness) {
  if (hit_test_opaqueness_ == opaqueness) {
    return;
  }

  hit_test_opaqueness_ = opaqueness;
  NoteLayerPropertyChanged();
}

bool LayerImpl::HitTestable() const {
  EffectTree& effect_tree = GetEffectTree();
  // TODO(sunxd): remove or refactor SetHideLayerAndSubtree, or move this logic
  // to subclasses of Layer. See https://crbug.com/595843 and
  // https://crbug.com/931865.
  // The bit |subtree_hidden| can only be true for ui::Layers. Other layers are
  // not supposed to set this bit.
  if (const EffectNode* node = effect_tree.Node(effect_tree_index())) {
    if (node->subtree_hidden) {
      return false;
    }
  }
  return hit_test_opaqueness_ != HitTestOpaqueness::kTransparent;
}

bool LayerImpl::OpaqueToHitTest() const {
  return HitTestable() && hit_test_opaqueness_ == HitTestOpaqueness::kOpaque &&
         !GetEffectTree()
              .Node(effect_tree_index())
              ->node_or_ancestor_has_fast_rounded_corner;
}

void LayerImpl::SetBackgroundColor(SkColor4f background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetSafeOpaqueBackgroundColor(SkColor4f background_color) {
  safe_opaque_background_color_ = background_color;
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  contents_opaque_ = opaque;
  contents_opaque_for_text_ = opaque;
}

void LayerImpl::SetContentsOpaqueForText(bool opaque) {
  DCHECK(!contents_opaque_ || opaque);
  contents_opaque_for_text_ = opaque;
}

float LayerImpl::Opacity() const {
  if (const EffectNode* node = GetEffectTree().Node(effect_tree_index()))
    return node->opacity;
  else
    return 1.f;
}

void LayerImpl::SetElementId(ElementId element_id) {
  if (element_id == element_id_)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "LayerImpl::SetElementId",
               "element", element_id.ToString());
  element_id_ = element_id;
}

void LayerImpl::UnionUpdateRect(const gfx::Rect& update_rect) {
  update_rect_.Union(update_rect);
}

gfx::Rect LayerImpl::GetDamageRect() const {
  return gfx::Rect();
}

DamageReasonSet LayerImpl::GetDamageReasons() const {
  DamageReasonSet reasons;
  if (LayerPropertyChanged() || !update_rect_.IsEmpty() ||
      !GetDamageRect().IsEmpty()) {
    reasons.Put(DamageReason::kUntracked);
  }
  return reasons;
}

void LayerImpl::SetCurrentScrollOffset(const gfx::PointF& scroll_offset) {
  DCHECK(IsActive());
  if (GetScrollTree().SetScrollOffset(element_id(), scroll_offset)) {
    layer_tree_impl()->DidUpdateScrollOffset(
        element_id(), /*pushed_from_main_or_pending_tree=*/false);
  }
}

SimpleEnclosedRegion LayerImpl::VisibleOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());
  return SimpleEnclosedRegion();
}

void LayerImpl::DidBeginTracing() {}

void LayerImpl::ReleaseResources() {}

void LayerImpl::OnPurgeMemory() {
  ReleaseResources();
}

void LayerImpl::ReleaseTileResources() {}

void LayerImpl::RecreateTileResources() {}

void LayerImpl::SetNeedsPushProperties() {
  // For the pending tree, there's no need to mark this layer to push properties
  // when |will_always_push_properties_| is true.
  if (will_always_push_properties_ && layer_tree_impl()->IsPendingTree()) {
    return;
  }

  // We never push properties from the active tree unless using a LayerContext.
  if (layer_tree_impl()->IsActiveTree() &&
      !layer_tree_impl()->settings().UseLayerContextForDisplay()) {
    return;
  }

  if (!needs_push_properties_) {
    needs_push_properties_ = true;
    layer_tree_impl()->AddLayerShouldPushProperties(this);
  }
}

void LayerImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
}

void LayerImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  // The output is consumed at least by
  // 1. DevTools for showing layer tree information for frame snapshots in
  //    performance timeline (third_party/devtools-frontend/src/front_end/
  //    models/timeline_model/TracingLayerTree.ts),
  // 2. trace_viewer
  //    (third_party/catapult/tracing/tracing/extras/chrome/cc/layer_impl.html)
  //    Note that trace_viewer uses "namingStyle" style instead of
  //    "naming_style". The difference is intentional and the names are
  //    converted automatically, but we need to keep this in mind when we
  //    search trace_viewer code for the usage of the names here.
  // When making changes here, we need to make sure we won't break these
  // consumers.
  viz::TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), state, "cc::LayerImpl",
      LayerTypeAsString(GetLayerType()), this);
  state->SetInteger("layer_id", id());
  MathUtil::AddToTracedValue("bounds", bounds_, state);

  state->SetDouble("opacity", Opacity());

  // For backward-compatibility of DevTools front-end.
  MathUtil::AddToTracedValue("position", gfx::PointF(), state);

  state->SetInteger("transform_tree_index", transform_tree_index());
  state->SetInteger("clip_tree_index", clip_tree_index());
  state->SetInteger("effect_tree_index", effect_tree_index());
  state->SetInteger("scroll_tree_index", scroll_tree_index());

  state->SetInteger("sorting_context_id", GetSortingContextId());

  state->SetInteger("draws_content", draws_content());
  state->SetInteger("gpu_memory_usage",
                    base::saturated_cast<int>(GPUMemoryUsageInBytes()));

  if (element_id_)
    element_id_.AddToTracedValue(state);

  if (!ScreenSpaceTransform().IsIdentity())
    MathUtil::AddToTracedValue("screen_space_transform", ScreenSpaceTransform(),
                               state);

  bool clipped;
  gfx::QuadF layer_quad =
      MathUtil::MapQuad(ScreenSpaceTransform(),
                        gfx::QuadF(gfx::RectF(gfx::Rect(bounds()))), &clipped);
  MathUtil::AddToTracedValue("layer_quad", layer_quad, state);
  if (!GetAllTouchActionRegions().IsEmpty()) {
    state->BeginArray("all_touch_action_regions");
    GetAllTouchActionRegions().AsValueInto(state);
    state->EndArray();
  }

  state->BeginArray("wheel_event_handler_region");
  wheel_event_handler_region().AsValueInto(state);
  state->EndArray();

  // TODO(crbug.com/358408565): At least DevTools reads from trace using this
  // name.
  state->BeginArray("non_fast_scrollable_region");
  main_thread_scroll_hit_test_region().AsValueInto(state);
  state->EndArray();

  state->SetBoolean("hit_testable", HitTestable());
  state->SetBoolean("opaque_to_hit_test", OpaqueToHitTest());
  state->SetBoolean("contents_opaque", contents_opaque());

  if (debug_info_) {
    state->SetString("layer_name", debug_info_->name);
    if (debug_info_->owner_node_id)
      state->SetInteger("owner_node", debug_info_->owner_node_id);

    if (debug_info_->compositing_reasons.size()) {
      state->BeginArray("compositing_reasons");
      for (const char* reason : debug_info_->compositing_reasons)
        state->AppendString(reason);
      state->EndArray();
    }

    if (debug_info_->compositing_reason_ids.size()) {
      state->BeginArray("compositing_reason_ids");
      for (const char* reason_id : debug_info_->compositing_reason_ids)
        state->AppendString(reason_id);
      state->EndArray();
    }

    if (debug_info_->invalidations.size()) {
      state->BeginArray("annotated_invalidation_rects");
      for (auto& invalidation : debug_info_->invalidations) {
        state->BeginDictionary();
        MathUtil::AddToTracedValue("geometry_rect", invalidation.rect, state);
        state->SetString("reason", invalidation.reason);
        state->SetString("client", invalidation.client);
        state->EndDictionary();
      }
      state->EndArray();
    }
  }
}

std::string LayerImpl::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

size_t LayerImpl::GPUMemoryUsageInBytes() const { return 0; }

void LayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

gfx::Transform LayerImpl::DrawTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!contributes_to_drawn_render_surface()) {
      return draw_property_utils::DrawTransform(this, GetTransformTree(),
                                                GetEffectTree());
  }

  return draw_properties().target_space_transform;
}

gfx::Transform LayerImpl::ScreenSpaceTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!contributes_to_drawn_render_surface()) {
    return draw_property_utils::ScreenSpaceTransform(this, GetTransformTree());
  }

  return draw_properties().screen_space_transform;
}

int LayerImpl::GetSortingContextId() const {
  return GetTransformTree().Node(transform_tree_index())->sorting_context_id;
}

Region LayerImpl::GetInvalidationRegionForDebugging() {
  return Region(update_rect_);
}

gfx::Rect LayerImpl::GetEnclosingVisibleRectInTargetSpace() const {
  return GetScaledEnclosingVisibleRectInTargetSpace(1.0f);
}

gfx::Rect LayerImpl::GetScaledEnclosingVisibleRectInTargetSpace(
    float scale) const {
  // TODO(oshima): Define an utility function to scale layer and conslidate with
  // the logic in ComputeDrawPropertiesOfVisibleLayers() in
  // draw_property_util.cc.
  DCHECK_GT(scale, 0.0);

  bool only_draws_visible_content = GetPropertyTrees()
                                        ->effect_tree()
                                        .Node(effect_tree_index())
                                        ->only_draws_visible_content;
  gfx::Rect drawable_bounds = visible_layer_rect();
  if (!only_draws_visible_content) {
    drawable_bounds = gfx::Rect(bounds());
  }
  gfx::Transform scaled_draw_transform = GetScaledDrawTransform(scale);
  gfx::Rect scaled_bounds = ScaleToEnclosingRect(drawable_bounds, scale);

  return MathUtil::MapEnclosingClippedRect(scaled_draw_transform,
                                           scaled_bounds);
}

RenderSurfaceImpl* LayerImpl::render_target() {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

const RenderSurfaceImpl* LayerImpl::render_target() const {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

gfx::Vector2dF LayerImpl::GetIdealContentsScale() const {
  const auto& transform = ScreenSpaceTransform();
  std::optional<gfx::Vector2dF> transform_scales =
      gfx::TryComputeTransform2dScaleComponents(transform);
  if (transform_scales) {
    // TODO(crbug.com/40176440): Remove this scale cap.
    float scale_cap = GetPreferredRasterScale(*transform_scales);
    transform_scales->SetToMin(gfx::Vector2dF(scale_cap, scale_cap));
    return *transform_scales;
  }

  // TryComputeTransform2dScaleComponents couldn't compute a scale because of
  // perspective components in the transform.

  float page_scale = IsAffectedByPageScale()
                         ? layer_tree_impl()->current_page_scale_factor()
                         : 1.f;
  float device_scale = layer_tree_impl()->device_scale_factor();

  float default_scale = page_scale * device_scale;

  // TODO(crbug.com/40176440): This function should return a 2D scale.
  float scale = gfx::ComputeApproximateMaxScale(transform);

  const int kMaxTilesToCoverLayerDimension = 5;
  // Cap the scale in a way that it should be covered by at most
  // |kMaxTilesToCoverLayerDimension|^2 default tile sizes. If this is left
  // uncapped, then we can fairly easily use too much memory (or too many
  // tiles). See crbug.com/752382 for an example of such a page. Note that
  // because this is an approximation anyway, it's fine to use a smaller scale
  // that desired. On top of this, the layer has a perspective transform so
  // technically it could all be within the viewport, so it's important for us
  // to have a reasonable scale here. The scale we use would also be at least
  // |default_scale|, as checked below.
  float scale_cap = std::min(
      (layer_tree_impl()->settings().default_tile_size.width() - 2) *
          kMaxTilesToCoverLayerDimension / static_cast<float>(bounds().width()),
      (layer_tree_impl()->settings().default_tile_size.height() - 2) *
          kMaxTilesToCoverLayerDimension /
          static_cast<float>(bounds().height()));
  scale = std::min(scale, scale_cap);

  // Since we're approximating the scale anyway, round it to the nearest
  // integer to prevent jitter when animating the transform.
  scale = std::round(scale);

  // Don't let the scale fall below the default scale.
  scale = std::max(scale, default_scale);
  return gfx::Vector2dF(scale, scale);
}

float LayerImpl::GetIdealContentsScaleKey() const {
  return GetPreferredRasterScale(GetIdealContentsScale());
}

float LayerImpl::GetPreferredRasterScale(
    gfx::Vector2dF raster_space_scale_factor) {
  constexpr float kMaxScaleRatio = 5.f;
  float lower_scale =
      std::min(raster_space_scale_factor.x(), raster_space_scale_factor.y());
  float higher_scale =
      std::max(raster_space_scale_factor.x(), raster_space_scale_factor.y());
  return std::min(kMaxScaleRatio * lower_scale, higher_scale);
}

PropertyTrees* LayerImpl::GetPropertyTrees() const {
  return layer_tree_impl_->property_trees();
}

ClipTree& LayerImpl::GetClipTree() const {
  return GetPropertyTrees()->clip_tree_mutable();
}

EffectTree& LayerImpl::GetEffectTree() const {
  return GetPropertyTrees()->effect_tree_mutable();
}

ScrollTree& LayerImpl::GetScrollTree() const {
  return GetPropertyTrees()->scroll_tree_mutable();
}

TransformTree& LayerImpl::GetTransformTree() const {
  return GetPropertyTrees()->transform_tree_mutable();
}

void LayerImpl::EnsureValidPropertyTreeIndices() const {
  DCHECK(GetTransformTree().Node(transform_tree_index()));
  DCHECK(GetEffectTree().Node(effect_tree_index()));
  DCHECK(GetClipTree().Node(clip_tree_index()));
  DCHECK(GetScrollTree().Node(scroll_tree_index()));
}

bool LayerImpl::is_surface_layer() const {
  return false;
}

static float TranslationFromActiveTreeLayerScreenSpaceTransform(
    LayerImpl* pending_tree_layer) {
  LayerTreeImpl* layer_tree_impl = pending_tree_layer->layer_tree_impl();
  if (layer_tree_impl) {
    LayerImpl* active_tree_layer =
        layer_tree_impl->FindActiveTreeLayerById(pending_tree_layer->id());
    if (active_tree_layer) {
      gfx::Transform active_tree_screen_space_transform =
          active_tree_layer->draw_properties().screen_space_transform;
      if (active_tree_screen_space_transform.IsIdentity())
        return 0.f;
      if (active_tree_screen_space_transform.ApproximatelyEqual(
              pending_tree_layer->draw_properties().screen_space_transform))
        return 0.f;
      return (active_tree_layer->draw_properties()
                  .screen_space_transform.To2dTranslation() -
              pending_tree_layer->draw_properties()
                  .screen_space_transform.To2dTranslation())
          .Length();
    }
  }
  return 0.f;
}

// A layer jitters if its screen space transform is same on two successive
// commits, but has changed in between the commits. CalculateLayerJitter
// computes the jitter for the layer.
int LayerImpl::CalculateJitter() {
  float jitter = 0.f;
  performance_properties().translation_from_last_frame = 0.f;
  performance_properties().last_commit_screen_space_transform =
      draw_properties().screen_space_transform;

  if (!visible_layer_rect().IsEmpty()) {
    if (draw_properties().screen_space_transform.ApproximatelyEqual(
            performance_properties().last_commit_screen_space_transform)) {
      float translation_from_last_commit =
          TranslationFromActiveTreeLayerScreenSpaceTransform(this);
      if (translation_from_last_commit > 0.f) {
        performance_properties().num_fixed_point_hits++;
        performance_properties().translation_from_last_frame =
            translation_from_last_commit;
        if (performance_properties().num_fixed_point_hits >
            LayerTreeImpl::kFixedPointHitsThreshold) {
          // Jitter = Translation from fixed point * sqrt(Area of the layer).
          // The square root of the area is used instead of the area to match
          // the dimensions of both terms on the rhs.
          jitter += translation_from_last_commit *
                    sqrt(visible_layer_rect().size().GetArea());
        }
      } else {
        performance_properties().num_fixed_point_hits = 0;
      }
    }
  }
  return jitter;
}

std::string LayerImpl::DebugName() const {
  return debug_info_ ? debug_info_->name : "";
}

gfx::ContentColorUsage LayerImpl::GetContentColorUsage() const {
  return gfx::ContentColorUsage::kSRGB;
}

viz::ViewTransitionElementResourceId LayerImpl::ViewTransitionResourceId()
    const {
  return viz::ViewTransitionElementResourceId();
}

}  // namespace cc
