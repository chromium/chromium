// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
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
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
LayerImpl::LayerImpl(LayerTreeImpl* tree_impl,
                     int id,
                     bool will_always_push_properties)
    : layer_id_(id),
      layer_tree_impl_(tree_impl),
      will_always_push_properties_(will_always_push_properties),
      scrollable_(false),
      layer_property_changed_not_from_property_trees_(false),
      layer_property_changed_from_property_trees_(false),
      may_contain_video_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      use_parent_backface_visibility_(false),
      should_check_backface_visibility_(false),
      draws_content_(false),
      contributes_to_drawn_render_surface_(false),
      hit_testable_(false),
      is_inner_viewport_scroll_layer_(false),
      background_color_(0),
      safe_opaque_background_color_(0),
      transform_tree_index_(TransformTree::kInvalidNodeId),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      clip_tree_index_(ClipTree::kInvalidNodeId),
      scroll_tree_index_(ScrollTree::kInvalidNodeId),
      current_draw_mode_(DRAW_MODE_NONE),
      has_will_change_transform_hint_(false),
      needs_push_properties_(false),
      is_scrollbar_(false),
      scrollbars_hidden_(false),
      needs_show_scrollbars_(false),
      raster_even_if_not_drawn_(false),
      has_transform_node_(false),
      mirror_count_(0) {
  DCHECK_GT(layer_id_, 0);

  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  layer_tree_impl_->AddToElementLayerList(element_id_, this);

  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  layer_tree_impl_->UnregisterLayer(this);
  layer_tree_impl_->RemoveFromElementLayerList(element_id_);
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);
}

void LayerImpl::SetHasWillChangeTransformHint(bool has_will_change) {
  has_will_change_transform_hint_ = has_will_change;
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
  state->SetAll(draw_properties_.target_space_transform, gfx::Rect(bounds()),
                draw_properties_.visible_layer_rect,
                draw_properties_.rounded_corner_bounds,
                draw_properties_.clip_rect, draw_properties_.is_clipped,
                contents_opaque, draw_properties_.opacity,
                effect_node->HasRenderSurface() ? SkBlendMode::kSrcOver
                                                : effect_node->blend_mode,
                GetSortingContextId());
  state->is_fast_rounded_corner = draw_properties_.is_fast_rounded_corner;
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
      draw_properties_.target_space_transform;
  scaled_draw_transform.Scale(SK_MScalar1 / layer_to_content_scale,
                              SK_MScalar1 / layer_to_content_scale);

  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);
  state->SetAll(scaled_draw_transform, content_rect, visible_content_rect,
                draw_properties().rounded_corner_bounds,
                draw_properties().clip_rect, draw_properties().is_clipped,
                contents_opaque, draw_properties().opacity,
                effect_node->HasRenderSurface() ? SkBlendMode::kSrcOver
                                                : effect_node->blend_mode,
                GetSortingContextId());
  state->is_fast_rounded_corner = draw_properties().is_fast_rounded_corner;
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

void LayerImpl::GetDebugBorderProperties(SkColor* color, float* width) const {
  float device_scale_factor =
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1;

  if (draws_content_) {
    *color = DebugColors::ContentLayerBorderColor();
    *width = DebugColors::ContentLayerBorderWidth(device_scale_factor);
    return;
  }

  if (masks_to_bounds_) {
    *color = DebugColors::MaskingLayerBorderColor();
    *width = DebugColors::MaskingLayerBorderWidth(device_scale_factor);
    return;
  }

  *color = DebugColors::ContainerLayerBorderColor();
  *width = DebugColors::ContainerLayerBorderWidth(device_scale_factor);
}

void LayerImpl::AppendDebugBorderQuad(
    viz::RenderPass* render_pass,
    const gfx::Rect& quad_rect,
    const viz::SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass, quad_rect, shared_quad_state,
                        append_quads_data, color, width);
}

void LayerImpl::AppendDebugBorderQuad(
    viz::RenderPass* render_pass,
    const gfx::Rect& quad_rect,
    const viz::SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data,
    SkColor color,
    float width) const {
  if (!ShowDebugBorders(DebugBorderType::LAYER))
    return;

  gfx::Rect visible_quad_rect(quad_rect);
  auto* debug_border_quad =
      render_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(
      shared_quad_state, quad_rect, visible_quad_rect, color, width);
  if (contents_opaque()) {
    // When opaque, draw a second inner border that is thicker than the outer
    // border, but more transparent.
    static const float kFillOpacity = 0.3f;
    SkColor fill_color = SkColorSetA(
        color, static_cast<uint8_t>(SkColorGetA(color) * kFillOpacity));
    float fill_width = width * 3;
    gfx::Rect fill_rect = quad_rect;
    fill_rect.Inset(fill_width / 2.f, fill_width / 2.f);
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
  *resource_id = 0;
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index());
  return scroll_tree.ScrollBy(scroll_node, scroll, layer_tree_impl());
}

void LayerImpl::SetScrollable(const gfx::Size& bounds) {
  if (scrollable_ && scroll_container_bounds_ == bounds)
    return;

  bool was_scrollable = scrollable_;
  scrollable_ = true;
  scroll_container_bounds_ = bounds;

  // Scrollbar positions depend on the bounds.
  layer_tree_impl()->SetScrollbarGeometriesNeedUpdate();

  if (!was_scrollable)
    layer_tree_impl()->AddScrollableLayer(this);

  if (layer_tree_impl()->settings().scrollbar_animator ==
      LayerTreeSettings::AURA_OVERLAY) {
    set_needs_show_scrollbars(true);
  }

  NoteLayerPropertyChanged();
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

std::unique_ptr<LayerImpl> LayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
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
  layer->masks_to_bounds_ = masks_to_bounds_;
  layer->contents_opaque_ = contents_opaque_;
  layer->may_contain_video_ = may_contain_video_;
  layer->use_parent_backface_visibility_ = use_parent_backface_visibility_;
  layer->should_check_backface_visibility_ = should_check_backface_visibility_;
  layer->draws_content_ = draws_content_;
  layer->hit_testable_ = hit_testable_;
  layer->non_fast_scrollable_region_ = non_fast_scrollable_region_;
  layer->touch_action_region_ = touch_action_region_;
  layer->all_touch_action_regions_ =
      all_touch_action_regions_
          ? std::make_unique<Region>(*all_touch_action_regions_)
          : nullptr;
  layer->wheel_event_handler_region_ = wheel_event_handler_region_;
  layer->background_color_ = background_color_;
  layer->safe_opaque_background_color_ = safe_opaque_background_color_;
  layer->transform_tree_index_ = transform_tree_index_;
  layer->effect_tree_index_ = effect_tree_index_;
  layer->clip_tree_index_ = clip_tree_index_;
  layer->scroll_tree_index_ = scroll_tree_index_;
  layer->has_will_change_transform_hint_ = has_will_change_transform_hint_;
  layer->mirror_count_ = mirror_count_;
  layer->scrollbars_hidden_ = scrollbars_hidden_;
  if (needs_show_scrollbars_)
    layer->needs_show_scrollbars_ = needs_show_scrollbars_;

  if (layer_property_changed_not_from_property_trees_ ||
      layer_property_changed_from_property_trees_)
    layer->layer_tree_impl()->set_needs_update_draw_properties();
  if (layer_property_changed_not_from_property_trees_)
    layer->layer_property_changed_not_from_property_trees_ = true;
  if (layer_property_changed_from_property_trees_)
    layer->layer_property_changed_from_property_trees_ = true;

  layer->SetBounds(bounds_);
  if (scrollable_)
    layer->SetScrollable(scroll_container_bounds_);

  layer->set_is_scrollbar(is_scrollbar_);

  layer->UnionUpdateRect(update_rect_);

  layer->UpdateDebugInfo(debug_info_.get());

  // Reset any state that should be cleared for the next update.
  needs_show_scrollbars_ = false;
  ResetChangeTracking();
}

bool LayerImpl::IsAffectedByPageScale() const {
  TransformTree& transform_tree = GetTransformTree();
  return transform_tree.Node(transform_tree_index())
      ->in_subtree_of_page_scale_layer;
}

std::unique_ptr<base::DictionaryValue> LayerImpl::LayerAsJson() const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->SetInteger("LayerId", id());
  if (element_id())
    result->SetString("ElementId", element_id().ToString());
  result->SetString("LayerType", LayerTypeAsString());

  auto list = std::make_unique<base::ListValue>();
  list->AppendInteger(bounds().width());
  list->AppendInteger(bounds().height());
  result->Set("Bounds", std::move(list));

  list = std::make_unique<base::ListValue>();
  list->AppendInteger(offset_to_transform_parent().x());
  list->AppendInteger(offset_to_transform_parent().y());
  result->Set("OffsetToTransformParent", std::move(list));

  result->SetBoolean("DrawsContent", draws_content_);
  result->SetBoolean("HitTestable", hit_testable_);
  result->SetBoolean("Is3dSorted", Is3dSorted());
  result->SetDouble("Opacity", Opacity());
  result->SetBoolean("ContentsOpaque", contents_opaque_);

  result->SetInteger("transform_tree_index", transform_tree_index());
  result->SetInteger("clip_tree_index", clip_tree_index());
  result->SetInteger("effect_tree_index", effect_tree_index());
  result->SetInteger("scroll_tree_index", scroll_tree_index());

  if (scrollable())
    result->SetBoolean("Scrollable", true);

  if (!GetAllTouchActionRegions().IsEmpty()) {
    std::unique_ptr<base::Value> region = GetAllTouchActionRegions().AsValue();
    result->Set("TouchRegion", std::move(region));
  }

  if (!wheel_event_handler_region_.IsEmpty()) {
    std::unique_ptr<base::Value> region = wheel_event_handler_region_.AsValue();
    result->Set("WheelRegion", std::move(region));
  }

  if (!non_fast_scrollable_region_.IsEmpty()) {
    std::unique_ptr<base::Value> region = non_fast_scrollable_region_.AsValue();
    result->Set("NonFastScrollableRegion", std::move(region));
  }

  return result;
}

bool LayerImpl::LayerPropertyChanged() const {
  return layer_property_changed_not_from_property_trees_ ||
         LayerPropertyChangedFromPropertyTrees();
}

bool LayerImpl::LayerPropertyChangedFromPropertyTrees() const {
  if (layer_property_changed_from_property_trees_ ||
      GetPropertyTrees()->full_tree_damaged)
    return true;
  if (transform_tree_index() == TransformTree::kInvalidNodeId)
    return false;
  TransformNode* transform_node =
      GetTransformTree().Node(transform_tree_index());
  if (transform_node && transform_node->transform_changed)
    return true;
  if (effect_tree_index() == EffectTree::kInvalidNodeId)
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

const char* LayerImpl::LayerTypeAsString() const {
  return "cc::LayerImpl";
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

  // Scrollbar positions depend on the scrolling layer bounds.
  if (scrollable())
    layer_tree_impl()->SetScrollbarGeometriesNeedUpdate();

  NoteLayerPropertyChanged();
}

ScrollbarLayerImplBase* LayerImpl::ToScrollbarLayer() {
  return nullptr;
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetHitTestable(bool should_hit_test) {
  if (hit_testable_ == should_hit_test)
    return;

  hit_testable_ = should_hit_test;
  NoteLayerPropertyChanged();
}

bool LayerImpl::HitTestable() const {
  EffectTree& effect_tree = GetEffectTree();
  bool should_hit_test = hit_testable_;
  // TODO(sunxd): remove or refactor SetHideLayerAndSubtree, or move this logic
  // to subclasses of Layer. See https://crbug.com/595843 and
  // https://crbug.com/931865.
  // The bit |subtree_hidden| can only be true for ui::Layers. Other layers are
  // not supposed to set this bit.
  if (effect_tree.Node(effect_tree_index())) {
    should_hit_test &= !effect_tree.Node(effect_tree_index())->subtree_hidden;
  }
  return should_hit_test;
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetSafeOpaqueBackgroundColor(SkColor background_color) {
  safe_opaque_background_color_ = background_color;
}

SkColor LayerImpl::SafeOpaqueBackgroundColor() const {
  if (contents_opaque()) {
    // TODO(936906): We should uncomment this DCHECK, since the
    // |safe_opaque_background_color_| could be transparent if it is never set
    // (the default is 0). But to do that, one test needs to be fixed.
    // DCHECK_EQ(SkColorGetA(safe_opaque_background_color_), SK_AlphaOPAQUE);
    return safe_opaque_background_color_;
  }
  SkColor color = background_color();
  if (SkColorGetA(color) == 255)
    color = SK_ColorTRANSPARENT;
  return color;
}

void LayerImpl::SetMasksToBounds(bool masks_to_bounds) {
  masks_to_bounds_ = masks_to_bounds;
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  contents_opaque_ = opaque;
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
               "element", element_id.AsValue().release());

  layer_tree_impl_->RemoveFromElementLayerList(element_id_);
  element_id_ = element_id;
  layer_tree_impl_->AddToElementLayerList(element_id_, this);
}

void LayerImpl::SetMirrorCount(int mirror_count) {
  mirror_count_ = mirror_count;
}

void LayerImpl::UnionUpdateRect(const gfx::Rect& update_rect) {
  update_rect_.Union(update_rect);
}

gfx::Rect LayerImpl::GetDamageRect() const {
  return gfx::Rect();
}

void LayerImpl::SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsActive());
  if (GetScrollTree().SetScrollOffset(element_id(), scroll_offset))
    layer_tree_impl()->DidUpdateScrollOffset(element_id());
}

gfx::ScrollOffset LayerImpl::CurrentScrollOffset() const {
  return GetScrollTree().current_scroll_offset(element_id());
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

gfx::ScrollOffset LayerImpl::MaxScrollOffset() const {
  return GetScrollTree().MaxScrollOffset(scroll_tree_index());
}

gfx::ScrollOffset LayerImpl::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset) const {
  offset.SetToMin(MaxScrollOffset());
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

gfx::Vector2dF LayerImpl::ClampScrollToMaxScrollOffset() {
  gfx::ScrollOffset old_offset = CurrentScrollOffset();
  gfx::ScrollOffset clamped_offset = ClampScrollOffsetToLimits(old_offset);
  gfx::Vector2dF delta = clamped_offset.DeltaFrom(old_offset);
  if (!delta.IsZero())
    ScrollBy(delta);
  return delta;
}

void LayerImpl::SetNeedsPushProperties() {
  // There's no need to push layer properties on the active tree, or when
  // |will_always_push_properties_| is true.
  if (will_always_push_properties_ || layer_tree_impl()->IsActiveTree())
    return;
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
  //    performance timeline (third_party/devtools_frontend/src/front_end/
  //    timeline_model/TracingLayerTree.js),
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
      LayerTypeAsString(), this);
  state->SetInteger("layer_id", id());
  MathUtil::AddToTracedValue("bounds", bounds_, state);

  state->SetDouble("opacity", Opacity());

  // For backward-compatibility of DevTools front-end.
  MathUtil::AddToTracedValue("position", gfx::PointF(), state);

  state->SetInteger("transform_tree_index", transform_tree_index());
  state->SetInteger("clip_tree_index", clip_tree_index());
  state->SetInteger("effect_tree_index", effect_tree_index());
  state->SetInteger("scroll_tree_index", scroll_tree_index());

  state->SetInteger("draws_content", DrawsContent());
  state->SetInteger("gpu_memory_usage",
                    base::saturated_cast<int>(GPUMemoryUsageInBytes()));

  if (element_id_)
    element_id_.AddToTracedValue(state);

  MathUtil::AddToTracedValue("scroll_offset", CurrentScrollOffset(), state);

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
  if (!wheel_event_handler_region_.IsEmpty()) {
    state->BeginArray("wheel_event_handler_region");
    wheel_event_handler_region_.AsValueInto(state);
    state->EndArray();
  }
  if (!non_fast_scrollable_region_.IsEmpty()) {
    state->BeginArray("non_fast_scrollable_region");
    non_fast_scrollable_region_.AsValueInto(state);
    state->EndArray();
  }

  state->SetBoolean("can_use_lcd_text", CanUseLCDText());
  state->SetBoolean("contents_opaque", contents_opaque());

  state->SetBoolean("has_will_change_transform_hint",
                    has_will_change_transform_hint());

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
  std::string str;
  base::JSONWriter::WriteWithOptions(
      *LayerAsJson(),
      base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION |
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
      &str);
  return str;
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

bool LayerImpl::CanUseLCDText() const {
  if (layer_tree_impl()->settings().layers_always_allowed_lcd_text)
    return true;
  if (!layer_tree_impl()->settings().can_use_lcd_text)
    return false;
  if (!contents_opaque())
    return false;

  if (GetEffectTree().Node(effect_tree_index())->screen_space_opacity != 1.f)
    return false;
  if (!GetTransformTree()
           .Node(transform_tree_index())
           ->node_and_ancestors_have_only_integer_translation)
    return false;
  if (static_cast<int>(offset_to_transform_parent().x()) !=
      offset_to_transform_parent().x())
    return false;
  if (static_cast<int>(offset_to_transform_parent().y()) !=
      offset_to_transform_parent().y())
    return false;

  if (has_will_change_transform_hint())
    return false;
  return true;
}

int LayerImpl::GetSortingContextId() const {
  return GetTransformTree().Node(transform_tree_index())->sorting_context_id;
}

Region LayerImpl::GetInvalidationRegionForDebugging() {
  return Region(update_rect_);
}

gfx::Rect LayerImpl::GetEnclosingRectInTargetSpace() const {
  return MathUtil::MapEnclosingClippedRect(DrawTransform(),
                                           gfx::Rect(bounds()));
}

gfx::Rect LayerImpl::GetScaledEnclosingRectInTargetSpace(float scale) const {
  gfx::Transform scaled_draw_transform = DrawTransform();
  scaled_draw_transform.Scale(SK_MScalar1 / scale, SK_MScalar1 / scale);
  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(bounds(), scale);
  return MathUtil::MapEnclosingClippedRect(scaled_draw_transform,
                                           gfx::Rect(scaled_bounds));
}

RenderSurfaceImpl* LayerImpl::render_target() {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

const RenderSurfaceImpl* LayerImpl::render_target() const {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

float LayerImpl::GetIdealContentsScale() const {
  float page_scale = IsAffectedByPageScale()
                         ? layer_tree_impl()->current_page_scale_factor()
                         : 1.f;
  float device_scale = layer_tree_impl()->device_scale_factor();

  float default_scale = page_scale * device_scale;

  const auto& transform = ScreenSpaceTransform();
  if (transform.HasPerspective()) {
    float scale = MathUtil::ComputeApproximateMaxScale(transform);

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
            kMaxTilesToCoverLayerDimension /
            static_cast<float>(bounds().width()),
        (layer_tree_impl()->settings().default_tile_size.height() - 2) *
            kMaxTilesToCoverLayerDimension /
            static_cast<float>(bounds().height()));
    scale = std::min(scale, scale_cap);

    // Since we're approximating the scale anyway, round it to the nearest
    // integer to prevent jitter when animating the transform.
    scale = std::round(scale);

    // Don't let the scale fall below the default scale.
    return std::max(scale, default_scale);
  }

  gfx::Vector2dF transform_scales =
      MathUtil::ComputeTransform2dScaleComponents(transform, default_scale);

  constexpr float kMaxScaleRatio = 5.f;
  float lower_scale = std::min(transform_scales.x(), transform_scales.y());
  float higher_scale = std::max(transform_scales.x(), transform_scales.y());
  return std::min(kMaxScaleRatio * lower_scale, higher_scale);
}

PropertyTrees* LayerImpl::GetPropertyTrees() const {
  return layer_tree_impl_->property_trees();
}

ClipTree& LayerImpl::GetClipTree() const {
  return GetPropertyTrees()->clip_tree;
}

EffectTree& LayerImpl::GetEffectTree() const {
  return GetPropertyTrees()->effect_tree;
}

ScrollTree& LayerImpl::GetScrollTree() const {
  return GetPropertyTrees()->scroll_tree;
}

TransformTree& LayerImpl::GetTransformTree() const {
  return GetPropertyTrees()->transform_tree;
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

}  // namespace cc
