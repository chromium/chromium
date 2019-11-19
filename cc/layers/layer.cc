// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

struct SameSizeAsLayer : public base::RefCounted<SameSizeAsLayer> {
 private:
  SameSizeAsLayer();
  virtual ~SameSizeAsLayer();

  void* pointers[2];
  struct {
    LayerList children;
    gfx::Rect update_rect;
    gfx::Size bounds;
    gfx::Rect clip_rect;
    scoped_refptr<PictureLayer> mask_layer;
    int layer_id;
    float opacity;
    SkBlendMode blend_mode;
    unsigned bitfields;
    gfx::PointF position;
    gfx::Transform transform;
    gfx::Point3F transform_origin;
    SkColor background_color;
    FilterOperations filters[2];
    base::Optional<gfx::RRectF> backdrop_filter_bounds;
    gfx::PointF filters_origin;
    float backdrop_filter_quality;
    gfx::RoundedCornersF corner_radii;
    gfx::ScrollOffset scroll_offset;
    gfx::Size scroll_container_bounds;
    int mirror_count;
    Region non_fast_scrollable_region;
    TouchActionRegion touch_action_region;
    ElementId element_id;
    base::RepeatingCallback<void()> did_scroll_callback;
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests;
  } inputs;
  int int_fields[6];
  gfx::Vector2dF offset;
  unsigned bitfields;
  SkColor safe_opaque_background_color;
  void* debug_info;
};

static_assert(sizeof(Layer) == sizeof(SameSizeAsLayer),
              "Layer should stay small");

base::AtomicSequenceNumber g_next_layer_id;

LayerDebugInfo::LayerDebugInfo() = default;
LayerDebugInfo::LayerDebugInfo(const LayerDebugInfo&) = default;
LayerDebugInfo::~LayerDebugInfo() = default;

Layer::Inputs::Inputs(int layer_id)
    : mask_layer(nullptr),
      layer_id(layer_id),
      opacity(1.f),
      blend_mode(SkBlendMode::kSrcOver),
      masks_to_bounds(false),
      hit_testable(false),
      contents_opaque(false),
      is_drawable(false),
      double_sided(true),
      use_parent_backface_visibility(false),
      is_fast_rounded_corner(false),
      scrollable(false),
      is_scrollbar(false),
      user_scrollable_horizontal(true),
      user_scrollable_vertical(true),
      has_will_change_transform_hint(false),
      trilinear_filtering(false),
      hide_layer_and_subtree(false),
      background_color(0),
      backdrop_filter_quality(1.0f),
      corner_radii({0, 0, 0, 0}),
      mirror_count(0) {}

Layer::Inputs::~Inputs() = default;

scoped_refptr<Layer> Layer::Create() {
  return base::WrapRefCounted(new Layer());
}

Layer::Layer()
    : parent_(nullptr),
      layer_tree_host_(nullptr),
      // Layer IDs start from 1.
      inputs_(g_next_layer_id.GetNext() + 1),
      num_descendants_that_draw_content_(0),
      transform_tree_index_(TransformTree::kInvalidNodeId),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      clip_tree_index_(ClipTree::kInvalidNodeId),
      scroll_tree_index_(ScrollTree::kInvalidNodeId),
      property_tree_sequence_number_(-1),
      ignore_set_needs_commit_(false),
      draws_content_(false),
      should_check_backface_visibility_(false),
      cache_render_surface_(false),
      force_render_surface_for_testing_(false),
      subtree_property_changed_(false),
      may_contain_video_(false),
      needs_show_scrollbars_(false),
      has_transform_node_(false),
      has_clip_node_(false),
      subtree_has_copy_request_(false),
      safe_opaque_background_color_(0) {}

Layer::~Layer() {
  // Our parent should be holding a reference to us so there should be no
  // way for us to be destroyed while we still have a parent.
  DCHECK(!parent());
  // Similarly we shouldn't have a layer tree host since it also keeps a
  // reference to us.
  DCHECK(!layer_tree_host());

  // Remove the parent reference from all children and dependents.
  RemoveAllChildren();
}

void Layer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host_ == host)
    return;

  bool property_tree_indices_invalid = false;
  if (layer_tree_host_) {
    bool should_register_element =
        inputs_.element_id &&
        (!layer_tree_host_->IsUsingLayerLists() || inputs_.scrollable);
    layer_tree_host_->property_trees()->needs_rebuild = true;
    layer_tree_host_->UnregisterLayer(this);
    if (should_register_element) {
      layer_tree_host_->UnregisterElement(inputs_.element_id,
                                          ElementListType::ACTIVE);
    }
    if (!layer_tree_host_->IsUsingLayerLists())
      property_tree_indices_invalid = true;
  }
  if (host) {
    bool should_register_element =
        inputs_.element_id &&
        (!host->IsUsingLayerLists() || inputs_.scrollable);
    host->property_trees()->needs_rebuild = true;
    host->RegisterLayer(this);
    if (should_register_element)
      host->RegisterElement(inputs_.element_id, ElementListType::ACTIVE, this);
    if (!host->IsUsingLayerLists())
      property_tree_indices_invalid = true;
  }

  layer_tree_host_ = host;

  if (property_tree_indices_invalid)
    InvalidatePropertyTreesIndices();

  // When changing hosts, the layer needs to commit its properties to the impl
  // side for the new host.
  SetNeedsPushProperties();

  for (size_t i = 0; i < inputs_.children.size(); ++i)
    inputs_.children[i]->SetLayerTreeHost(host);

  if (host && !host->IsUsingLayerLists() &&
      host->mutator_host()->IsElementAnimating(element_id())) {
    host->SetNeedsCommit();
  }
}

void Layer::SetNeedsCommit() {
  if (!layer_tree_host_)
    return;

  SetNeedsPushProperties();

  if (ignore_set_needs_commit_)
    return;

  layer_tree_host_->SetNeedsCommit();
}

void Layer::SetNeedsFullTreeSync() {
  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetNeedsFullTreeSync();
}

void Layer::SetNextCommitWaitsForActivation() {
  if (!layer_tree_host_)
    return;

  layer_tree_host_->SetNextCommitWaitsForActivation();
}

void Layer::SetNeedsPushProperties() {
  if (layer_tree_host_)
    layer_tree_host_->AddLayerShouldPushProperties(this);
}

bool Layer::IsPropertyChangeAllowed() const {
  if (!layer_tree_host_)
    return true;

  return !layer_tree_host_->in_paint_layer_contents();
}

void Layer::CaptureContent(const gfx::Rect& rect,
                           std::vector<NodeId>* content) {}

sk_sp<SkPicture> Layer::GetPicture() const {
  return nullptr;
}

void Layer::SetParent(Layer* layer) {
  DCHECK(!layer || !layer->HasAncestor(this));

  parent_ = layer;
  SetLayerTreeHost(parent_ ? parent_->layer_tree_host() : nullptr);

  SetPropertyTreesNeedRebuild();
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChild(child, inputs_.children.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t index) {
  DCHECK(IsPropertyChangeAllowed());
  child->RemoveFromParent();
  AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                         (child->DrawsContent() ? 1 : 0));
  child->SetParent(this);
  child->SetSubtreePropertyChanged();

  index = std::min(index, inputs_.children.size());
  if (inputs_.mask_layer && index && index == inputs_.children.size()) {
    // Ensure that the mask layer is always the last child.
    DCHECK_EQ(inputs_.mask_layer, inputs_.children.back().get());
    index--;
  }
  inputs_.children.insert(inputs_.children.begin() + index, child);
  SetNeedsFullTreeSync();
}

void Layer::RemoveFromParent() {
  DCHECK(IsPropertyChangeAllowed());
  if (parent_)
    parent_->RemoveChild(this);
}

void Layer::RemoveChild(Layer* child) {
  if (child == inputs_.mask_layer)
    inputs_.mask_layer = nullptr;

  for (auto iter = inputs_.children.begin(); iter != inputs_.children.end();
       ++iter) {
    if (iter->get() != child)
      continue;

    child->SetParent(nullptr);
    AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                           (child->DrawsContent() ? 1 : 0));
    inputs_.children.erase(iter);
    SetNeedsFullTreeSync();
    return;
  }
}

void Layer::ReorderChildren(LayerList* new_children_order) {
#if DCHECK_IS_ON()
  base::flat_set<Layer*> children_set;
  for (const auto& child : *new_children_order) {
    DCHECK_EQ(child->parent(), this);
    children_set.insert(child.get());
  }
  for (const auto& child : inputs_.children)
    DCHECK_GT(children_set.count(child.get()), 0u);
#endif
  inputs_.children = std::move(*new_children_order);

  // We do not need to call SetSubtreePropertyChanged for each child here
  // since SetSubtreePropertyChanged includes SetNeedsPushProperties, but this
  // change is not included in properties pushing.
  for (const auto& child : inputs_.children)
    child->subtree_property_changed_ = true;

  SetNeedsFullTreeSync();
}

void Layer::ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer) {
  DCHECK(reference);
  DCHECK_EQ(reference->parent(), this);
  DCHECK(IsPropertyChangeAllowed());

  if (reference == new_layer.get())
    return;

  // Find the index of |reference| in |children_|.
  auto reference_it =
      std::find_if(inputs_.children.begin(), inputs_.children.end(),
                   [reference](const scoped_refptr<Layer>& layer) {
                     return layer.get() == reference;
                   });
  DCHECK(reference_it != inputs_.children.end());
  size_t reference_index = reference_it - inputs_.children.begin();
  reference->RemoveFromParent();

  if (new_layer.get()) {
    new_layer->RemoveFromParent();
    InsertChild(new_layer, reference_index);
  }
}

void Layer::SetBounds(const gfx::Size& size) {
  DCHECK(IsPropertyChangeAllowed());
  if (bounds() == size)
    return;
  inputs_.bounds = size;

  if (!layer_tree_host_)
    return;

  // Rounded corner clipping, bounds clipping and mask clipping can result in
  // new areas of subtrees being exposed on a bounds change. Ensure the damaged
  // areas are updated.
  if (masks_to_bounds() || IsMaskedByChild() || HasRoundedCorner()) {
    SetSubtreePropertyChanged();
    SetPropertyTreesNeedRebuild();
  }

  if (scrollable() && !layer_tree_host_->IsUsingLayerLists()) {
    auto& scroll_tree = layer_tree_host_->property_trees()->scroll_tree;
    if (auto* scroll_node = scroll_tree.Node(scroll_tree_index_))
      scroll_node->bounds = inputs_.bounds;
    else
      SetPropertyTreesNeedRebuild();
  }

  SetNeedsCommit();
}

Layer* Layer::RootLayer() {
  Layer* layer = this;
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

void Layer::RemoveAllChildren() {
  DCHECK(IsPropertyChangeAllowed());
  while (inputs_.children.size()) {
    Layer* layer = inputs_.children[0].get();
    DCHECK_EQ(this, layer->parent());
    layer->RemoveFromParent();
  }
}

void Layer::SetChildLayerList(LayerList new_children) {
  DCHECK(layer_tree_host_->IsUsingLayerLists());

  // Early out without calling |LayerTreeHost::SetNeedsFullTreeSync| if no
  // layer has changed.
  if (children() == new_children)
    return;

  // Remove existing children that will not be in the new child list.
  {
    std::unordered_set<Layer*> children_to_remove;
    for (auto& existing_child : children())
      children_to_remove.insert(existing_child.get());
    for (auto& new_child : new_children)
      children_to_remove.erase(new_child.get());
    for (auto* child : children_to_remove) {
      child->SetParent(nullptr);
      AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                             (child->DrawsContent() ? 1 : 0));
    }
  }

  // Mark existing children as changed if their order changes.
  auto existing_child_it = children().begin();
  for (auto& child : new_children) {
    if (child->parent() == this) {
      // Search forward in the existing child list to find the new child.
      existing_child_it = std::find(existing_child_it, children().end(), child);
      if (existing_child_it == children().end())
        child->SetSubtreePropertyChanged();
    }
  }

  // Process new children and mark them as changed.
  // Because this changes the child's parent, it must be after code that uses
  // |child->parent()| such as the above loop.
  for (auto& child : new_children) {
    if (child->parent() != this) {
      child->RemoveFromParent();
      AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                             (child->DrawsContent() ? 1 : 0));
      child->SetParent(this);
      child->SetSubtreePropertyChanged();
    }
  }

  inputs_.children = std::move(new_children);

  layer_tree_host_->SetNeedsFullTreeSync();
}

bool Layer::HasAncestor(const Layer* ancestor) const {
  for (const Layer* layer = parent(); layer; layer = layer->parent()) {
    if (layer == ancestor)
      return true;
  }
  return false;
}

void Layer::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  DCHECK(IsPropertyChangeAllowed());
  if (request->has_source()) {
    const base::UnguessableToken& source = request->source();
    auto it = std::find_if(
        inputs_.copy_requests.begin(), inputs_.copy_requests.end(),
        [&source](const std::unique_ptr<viz::CopyOutputRequest>& x) {
          return x->has_source() && x->source() == source;
        });
    if (it != inputs_.copy_requests.end())
      inputs_.copy_requests.erase(it);
  }
  inputs_.copy_requests.push_back(std::move(request));
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
  if (layer_tree_host_)
    layer_tree_host_->SetHasCopyRequest(true);
}

void Layer::SetSubtreeHasCopyRequest(bool subtree_has_copy_request) {
  subtree_has_copy_request_ = subtree_has_copy_request;
}

bool Layer::SubtreeHasCopyRequest() const {
  DCHECK(layer_tree_host_);
  // When the copy request is pushed to effect tree, we reset layer tree host's
  // has_copy_request but do not clear subtree_has_copy_request on individual
  // layers.
  return layer_tree_host_->has_copy_request() && subtree_has_copy_request_;
}

void Layer::SetBackgroundColor(SkColor background_color) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.background_color == background_color)
    return;
  inputs_.background_color = background_color;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetSafeOpaqueBackgroundColor(SkColor background_color) {
  DCHECK(IsPropertyChangeAllowed());
  SkColor opaque_color = SkColorSetA(background_color, 255);
  if (safe_opaque_background_color_ == opaque_color)
    return;
  safe_opaque_background_color_ = opaque_color;
  SetNeedsPushProperties();
}

SkColor Layer::SafeOpaqueBackgroundColor() const {
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

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.masks_to_bounds == masks_to_bounds)
    return;
  inputs_.masks_to_bounds = masks_to_bounds;
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
  SetSubtreePropertyChanged();
}

void Layer::SetClipRect(const gfx::Rect& clip_rect) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.clip_rect == clip_rect)
    return;
  inputs_.clip_rect = clip_rect;

  // If the clip bounds have been cleared, the property trees needs a rebuild.
  const bool force_rebuild = clip_rect.IsEmpty() || !has_clip_node_;

  SetSubtreePropertyChanged();
  if (clip_tree_index() != ClipTree::kInvalidNodeId && !force_rebuild) {
    PropertyTrees* property_trees = layer_tree_host_->property_trees();
    gfx::RectF effective_clip_rect = EffectiveClipRect();
    if (ClipNode* node = property_trees->clip_tree.Node(clip_tree_index())) {
      node->clip = effective_clip_rect;
      node->clip += offset_to_transform_parent();
      property_trees->clip_tree.set_needs_update(true);
    }
    if (HasRoundedCorner() &&
        effect_tree_index() != EffectTree::kInvalidNodeId) {
      if (EffectNode* node =
              property_trees->effect_tree.Node(effect_tree_index())) {
        node->rounded_corner_bounds =
            gfx::RRectF(effective_clip_rect, corner_radii());
        node->effect_changed = true;
        property_trees->effect_tree.set_needs_update(true);
      }
    }
  } else {
    SetPropertyTreesNeedRebuild();
  }
  SetNeedsCommit();
}

gfx::RectF Layer::EffectiveClipRect() {
  // If this does not have a clip rect set, then the subtree is clipped by
  // the bounds.
  const gfx::RectF layer_bounds = gfx::RectF(gfx::SizeF(bounds()));
  if (clip_rect().IsEmpty())
    return layer_bounds;

  const gfx::RectF clip_rect_f(clip_rect());

  // Layer needs to clip to its bounds as well apply a clip rect. Intersect the
  // two to get the effective clip.
  if (masks_to_bounds() || IsMaskedByChild() ||
      filters().HasFilterThatMovesPixels())
    return gfx::IntersectRects(layer_bounds, clip_rect_f);

  // Clip rect is the only clip effecting the layer.
  return clip_rect_f;
}

void Layer::SetMaskLayer(scoped_refptr<PictureLayer> mask_layer) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK(!layer_tree_host_ || !layer_tree_host_->IsUsingLayerLists());
  if (inputs_.mask_layer == mask_layer)
    return;
  if (inputs_.mask_layer) {
    DCHECK_EQ(this, inputs_.mask_layer->parent());
    inputs_.mask_layer->RemoveFromParent();
  }
  // Clear mask_layer first and set it later because InsertChild() checks it to
  // ensure the mask layer is the last child.
  inputs_.mask_layer = nullptr;
  if (mask_layer) {
    // The mask layer should not have any children.
    DCHECK(mask_layer->children().empty());

    mask_layer->inputs_.position = gfx::PointF();
    mask_layer->SetIsDrawable(true);
    mask_layer->SetBlendMode(SkBlendMode::kDstIn);
    AddChild(mask_layer);
  }
  inputs_.mask_layer = mask_layer.get();
  SetSubtreePropertyChanged();
}

void Layer::SetFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.filters == filters)
    return;
  inputs_.filters = filters;
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetBackdropFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.backdrop_filters == filters)
    return;
  inputs_.backdrop_filters = filters;

  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetBackdropFilterBounds(const gfx::RRectF& backdrop_filter_bounds) {
  inputs_.backdrop_filter_bounds = backdrop_filter_bounds;
}

void Layer::ClearBackdropFilterBounds() {
  inputs_.backdrop_filter_bounds.reset();
}

void Layer::SetBackdropFilterQuality(const float quality) {
  inputs_.backdrop_filter_quality = quality;
}

void Layer::SetFiltersOrigin(const gfx::PointF& filters_origin) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.filters_origin == filters_origin)
    return;
  inputs_.filters_origin = filters_origin;
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetRoundedCorner(const gfx::RoundedCornersF& corner_radii) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.corner_radii == corner_radii)
    return;

  inputs_.corner_radii = corner_radii;
  SetSubtreePropertyChanged();
  SetNeedsCommit();
  PropertyTrees* property_trees =
      layer_tree_host_ ? layer_tree_host_->property_trees() : nullptr;
  EffectNode* node = nullptr;
  if (property_trees && effect_tree_index() != EffectTree::kInvalidNodeId &&
      (node = property_trees->effect_tree.Node(effect_tree_index()))) {
    node->rounded_corner_bounds =
        gfx::RRectF(EffectiveClipRect(), corner_radii);
    node->effect_changed = true;
    property_trees->effect_tree.set_needs_update(true);
  } else {
    SetPropertyTreesNeedRebuild();
  }
}

void Layer::SetIsFastRoundedCorner(bool enable) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.is_fast_rounded_corner == enable)
    return;
  inputs_.is_fast_rounded_corner = enable;

  // If this layer does not have a rounded corner, then modifying this flag is
  // going to have no effect.
  if (!HasRoundedCorner())
    return;

  SetSubtreePropertyChanged();
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
}

void Layer::SetOpacity(float opacity) {
  DCHECK(IsPropertyChangeAllowed());
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (inputs_.opacity == opacity)
    return;
  // We need to force a property tree rebuild when opacity changes from 1 to a
  // non-1 value or vice-versa as render surfaces can change.
  bool force_rebuild = opacity == 1.f || inputs_.opacity == 1.f;
  inputs_.opacity = opacity;
  SetSubtreePropertyChanged();

  if (layer_tree_host_ && !layer_tree_host_->IsUsingLayerLists()) {
    if (!force_rebuild) {
      PropertyTrees* property_trees = layer_tree_host_->property_trees();
      if (EffectNode* node =
              property_trees->effect_tree.Node(effect_tree_index())) {
        node->opacity = opacity;
        node->effect_changed = true;
        property_trees->effect_tree.set_needs_update(true);
      }
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

float Layer::EffectiveOpacity() const {
  return inputs_.hide_layer_and_subtree ? 0.f : inputs_.opacity;
}

bool Layer::OpacityCanAnimateOnImplThread() const {
  return false;
}

void Layer::SetBlendMode(SkBlendMode blend_mode) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.blend_mode == blend_mode)
    return;

  // Allowing only blend modes that are defined in the CSS Compositing standard,
  // plus destination-in which is used to implement masks.
  // http://dev.w3.org/fxtf/compositing-1/#blending
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
    case SkBlendMode::kDstIn:
    case SkBlendMode::kScreen:
    case SkBlendMode::kOverlay:
    case SkBlendMode::kDarken:
    case SkBlendMode::kLighten:
    case SkBlendMode::kColorDodge:
    case SkBlendMode::kColorBurn:
    case SkBlendMode::kHardLight:
    case SkBlendMode::kSoftLight:
    case SkBlendMode::kDifference:
    case SkBlendMode::kExclusion:
    case SkBlendMode::kMultiply:
    case SkBlendMode::kHue:
    case SkBlendMode::kSaturation:
    case SkBlendMode::kColor:
    case SkBlendMode::kLuminosity:
      // supported blend modes
      break;
    case SkBlendMode::kClear:
    case SkBlendMode::kSrc:
    case SkBlendMode::kDst:
    case SkBlendMode::kDstOver:
    case SkBlendMode::kSrcIn:
    case SkBlendMode::kSrcOut:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kSrcATop:
    case SkBlendMode::kDstATop:
    case SkBlendMode::kXor:
    case SkBlendMode::kPlus:
    case SkBlendMode::kModulate:
      // Porter Duff Compositing Operators are not yet supported
      // http://dev.w3.org/fxtf/compositing-1/#porterduffcompositingoperators
      NOTREACHED();
      return;
  }

  inputs_.blend_mode = blend_mode;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
}

void Layer::SetHitTestable(bool should_hit_test) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.hit_testable == should_hit_test)
    return;
  inputs_.hit_testable = should_hit_test;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

bool Layer::HitTestable() const {
  return inputs_.hit_testable;
}

void Layer::SetContentsOpaque(bool opaque) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.contents_opaque == opaque)
    return;
  inputs_.contents_opaque = opaque;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
}

void Layer::SetPosition(const gfx::PointF& position) {
  // The mask layer should always be at the same location as the masked layer
  // which is its parent, so its position should be always zero.
  if (parent() && parent()->inputs_.mask_layer == this) {
    DCHECK(!layer_tree_host_ || !layer_tree_host_->IsUsingLayerLists());
    DCHECK(inputs_.position.IsOrigin());
    return;
  }

  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.position == position)
    return;
  inputs_.position = position;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();

  if (!layer_tree_host_->IsUsingLayerLists()) {
    if (has_transform_node_) {
      TransformNode* transform_node =
          layer_tree_host_->property_trees()->transform_tree.Node(
              transform_tree_index_);
      // We should never set root layer's position to non-zero.
      DCHECK(parent());
      transform_node->post_translation =
          position.OffsetFromOrigin() + parent()->offset_to_transform_parent();
      transform_node->needs_local_transform_update = true;
      transform_node->transform_changed = true;
      layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

bool Are2dAxisAligned(const gfx::Transform& a, const gfx::Transform& b) {
  if (a.IsScaleOrTranslation() && b.IsScaleOrTranslation()) {
    return true;
  }

  gfx::Transform inverse(gfx::Transform::kSkipInitialization);
  if (b.GetInverse(&inverse)) {
    inverse *= a;
    return inverse.Preserves2dAxisAlignment();
  } else {
    // TODO(weiliangc): Should return false because b is not invertible.
    return a.Preserves2dAxisAlignment();
  }
}

void Layer::SetTransform(const gfx::Transform& transform) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.transform == transform)
    return;

  SetSubtreePropertyChanged();
  if (layer_tree_host_ && !layer_tree_host_->IsUsingLayerLists()) {
    if (has_transform_node_) {
      TransformNode* transform_node =
          layer_tree_host_->property_trees()->transform_tree.Node(
              transform_tree_index_);
      // We need to trigger a rebuild if we could have affected 2d axis
      // alignment. We'll check to see if transform and inputs_.transform are
      // axis align with respect to one another.
      DCHECK_EQ(transform_tree_index(), transform_node->id);
      bool preserves_2d_axis_alignment =
          Are2dAxisAligned(inputs_.transform, transform);
      transform_node->local = transform;
      transform_node->needs_local_transform_update = true;
      transform_node->transform_changed = true;
      layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
      if (!preserves_2d_axis_alignment)
        SetPropertyTreesNeedRebuild();
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  inputs_.transform = transform;
  SetNeedsCommit();
}

void Layer::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.transform_origin == transform_origin)
    return;
  inputs_.transform_origin = transform_origin;

  if (!layer_tree_host_)
    return;

  SetSubtreePropertyChanged();

  if (!layer_tree_host_->IsUsingLayerLists()) {
    if (has_transform_node_) {
      TransformNode* transform_node =
          layer_tree_host_->property_trees()->transform_tree.Node(
              transform_tree_index_);
      DCHECK_EQ(transform_tree_index(), transform_node->id);
      transform_node->origin = transform_origin;
      transform_node->needs_local_transform_update = true;
      transform_node->transform_changed = true;
      layer_tree_host_->property_trees()->transform_tree.set_needs_update(true);
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

void Layer::SetScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());

  if (inputs_.scroll_offset == scroll_offset)
    return;
  inputs_.scroll_offset = scroll_offset;

  if (!layer_tree_host_)
    return;

  UpdateScrollOffset(scroll_offset);

  SetNeedsCommit();
}

void Layer::SetScrollOffsetFromImplSide(
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());
  // This function only gets called during a BeginMainFrame, so there
  // is no need to call SetNeedsUpdate here.
  DCHECK(layer_tree_host_ && layer_tree_host_->CommitRequested());
  if (inputs_.scroll_offset == scroll_offset)
    return;
  inputs_.scroll_offset = scroll_offset;
  SetNeedsPushProperties();

  UpdateScrollOffset(scroll_offset);

  if (!inputs_.did_scroll_callback.is_null())
    inputs_.did_scroll_callback.Run(scroll_offset, element_id());

  // The callback could potentially change the layer structure:
  // "this" may have been destroyed during the process.
}

void Layer::UpdateScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(scrollable());

  // This function updates the property tree scroll offsets but in layer list
  // mode this should occur during the main -> cc property tree push.
  if (layer_tree_host_->IsUsingLayerLists())
    return;

  if (scroll_tree_index() == ScrollTree::kInvalidNodeId) {
    // Ensure the property trees just have not been built yet but are marked for
    // being built which will set the correct scroll offset values.
    DCHECK(layer_tree_host_->property_trees()->needs_rebuild);
    return;
  }

  // If a scroll node exists, it should have an associated transform node.
  DCHECK(transform_tree_index() != TransformTree::kInvalidNodeId);

  auto& property_trees = *layer_tree_host_->property_trees();
  property_trees.scroll_tree.SetScrollOffset(element_id(), scroll_offset);
  auto* transform_node =
      property_trees.transform_tree.Node(transform_tree_index());
  DCHECK_EQ(transform_tree_index(), transform_node->id);
  transform_node->scroll_offset = CurrentScrollOffset();
  transform_node->needs_local_transform_update = true;
  property_trees.transform_tree.set_needs_update(true);
}

void Layer::SetDidScrollCallback(
    base::RepeatingCallback<void(const gfx::ScrollOffset&, const ElementId&)>
        callback) {
  DCHECK(!layer_tree_host_ || !layer_tree_host_->IsUsingLayerLists());
  inputs_.did_scroll_callback = std::move(callback);
}

void Layer::SetScrollable(const gfx::Size& bounds) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.scrollable && inputs_.scroll_container_bounds == bounds)
    return;
  bool was_scrollable = inputs_.scrollable;
  inputs_.scrollable = true;
  inputs_.scroll_container_bounds = bounds;

  if (!layer_tree_host_)
    return;

  if (layer_tree_host_->IsUsingLayerLists() && !was_scrollable &&
      inputs_.element_id) {
    layer_tree_host_->RegisterElement(inputs_.element_id,
                                      ElementListType::ACTIVE, this);
  }

  if (!layer_tree_host_->IsUsingLayerLists()) {
    auto& scroll_tree = layer_tree_host_->property_trees()->scroll_tree;
    auto* scroll_node = scroll_tree.Node(scroll_tree_index_);
    if (was_scrollable && scroll_node)
      scroll_node->container_bounds = inputs_.scroll_container_bounds;
    else
      SetPropertyTreesNeedRebuild();
  }

  SetNeedsCommit();
}

void Layer::SetIsScrollbar(bool is_scrollbar) {
  if (inputs_.is_scrollbar == is_scrollbar)
    return;

  inputs_.is_scrollbar = is_scrollbar;
  SetNeedsCommit();
}

void Layer::SetUserScrollable(bool horizontal, bool vertical) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.user_scrollable_horizontal == horizontal &&
      inputs_.user_scrollable_vertical == vertical)
    return;
  inputs_.user_scrollable_horizontal = horizontal;
  inputs_.user_scrollable_vertical = vertical;
  if (!layer_tree_host_)
    return;

  if (scrollable() && !layer_tree_host_->IsUsingLayerLists()) {
    auto& scroll_tree = layer_tree_host_->property_trees()->scroll_tree;
    if (auto* scroll_node = scroll_tree.Node(scroll_tree_index_)) {
      scroll_node->user_scrollable_horizontal = horizontal;
      scroll_node->user_scrollable_vertical = vertical;
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

bool Layer::GetUserScrollableHorizontal() const {
  // When using layer lists, horizontal scrollability is stored in scroll nodes.
  if (layer_tree_host() && layer_tree_host()->IsUsingLayerLists()) {
    auto& scroll_tree = layer_tree_host()->property_trees()->scroll_tree;
    if (auto* scroll_node = scroll_tree.Node(scroll_tree_index_))
      return scroll_node->user_scrollable_horizontal;
    return false;
  }
  return inputs_.user_scrollable_horizontal;
}

bool Layer::GetUserScrollableVertical() const {
  // When using layer lists, vertical scrollability is stored in scroll nodes.
  if (layer_tree_host() && layer_tree_host()->IsUsingLayerLists()) {
    auto& scroll_tree = layer_tree_host()->property_trees()->scroll_tree;
    if (auto* scroll_node = scroll_tree.Node(scroll_tree_index_))
      return scroll_node->user_scrollable_vertical;
    return false;
  }
  return inputs_.user_scrollable_vertical;
}

void Layer::SetNonFastScrollableRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.non_fast_scrollable_region == region)
    return;
  inputs_.non_fast_scrollable_region = region;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetTouchActionRegion(TouchActionRegion touch_action_region) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.touch_action_region == touch_action_region)
    return;

  inputs_.touch_action_region = std::move(touch_action_region);
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetCacheRenderSurface(bool cache) {
  DCHECK(IsPropertyChangeAllowed());
  if (cache_render_surface_ == cache)
    return;
  cache_render_surface_ = cache;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

RenderSurfaceReason Layer::GetRenderSurfaceReason() const {
  if (!layer_tree_host_)
    return RenderSurfaceReason::kNone;
  PropertyTrees* property_trees = layer_tree_host_->property_trees();
  DCHECK(!property_trees->needs_rebuild);
  EffectNode* effect_node =
      property_trees->effect_tree.Node(this->effect_tree_index());

  // Effect node can also be the effect node of an ancestor layer.
  // Check if this effect node was created for this layer specifically.
  if (!effect_node ||
      (parent_ && this->effect_tree_index() == parent_->effect_tree_index())) {
    return RenderSurfaceReason::kNone;
  }
  return effect_node->render_surface_reason;
}

void Layer::SetForceRenderSurfaceForTesting(bool force) {
  DCHECK(IsPropertyChangeAllowed());
  if (force_render_surface_for_testing_ == force)
    return;
  force_render_surface_for_testing_ = force;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetDoubleSided(bool double_sided) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.double_sided == double_sided)
    return;
  inputs_.double_sided = double_sided;
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
  SetSubtreePropertyChanged();
}

void Layer::SetTransformTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (transform_tree_index_ == index)
    return;
  if (index == TransformTree::kInvalidNodeId)
    has_transform_node_ = false;
  transform_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::transform_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return TransformTree::kInvalidNodeId;
  }
  return transform_tree_index_;
}

void Layer::SetClipTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (clip_tree_index_ == index)
    return;
  clip_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::clip_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return ClipTree::kInvalidNodeId;
  }
  return clip_tree_index_;
}

void Layer::SetEffectTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (effect_tree_index_ == index)
    return;
  effect_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::effect_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return EffectTree::kInvalidNodeId;
  }
  return effect_tree_index_;
}

void Layer::SetScrollTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (scroll_tree_index_ == index)
    return;
  scroll_tree_index_ = index;
  SetNeedsPushProperties();
}

int Layer::scroll_tree_index() const {
  if (!layer_tree_host_ ||
      layer_tree_host_->property_trees()->sequence_number !=
          property_tree_sequence_number_) {
    return ScrollTree::kInvalidNodeId;
  }
  return scroll_tree_index_;
}

void Layer::SetOffsetToTransformParent(gfx::Vector2dF offset) {
  if (offset_to_transform_parent_ == offset)
    return;
  offset_to_transform_parent_ = offset;
  SetNeedsPushProperties();
  SetSubtreePropertyChanged();
}

void Layer::InvalidatePropertyTreesIndices() {
  SetTransformTreeIndex(TransformTree::kInvalidNodeId);
  SetClipTreeIndex(ClipTree::kInvalidNodeId);
  SetEffectTreeIndex(EffectTree::kInvalidNodeId);
  SetScrollTreeIndex(ScrollTree::kInvalidNodeId);
}

void Layer::SetPropertyTreesNeedRebuild() {
  if (layer_tree_host_)
    layer_tree_host_->property_trees()->needs_rebuild = true;
}

LayerDebugInfo& Layer::EnsureDebugInfo() {
  if (!debug_info_) {
    debug_info_ = std::make_unique<LayerDebugInfo>();
    // We just enabled debug info collection. Force PushPropertiesTo() to ensure
    // the first layer tree snapshot contains the debug info. Otherwise we will
    // push debug_info when we have other changes to push.
    SetNeedsPushProperties();
  }
  return *debug_info_;
}

void Layer::ClearDebugInfo() {
  if (!debug_info_)
    return;

  debug_info_.reset();
  SetNeedsPushProperties();
}

std::string Layer::DebugName() const {
  return debug_info_ ? debug_info_->name : "";
}

std::string Layer::ToString() const {
  return base::StringPrintf(
      "layer_id: %d\n"
      "  name: %s\n"
      "  Bounds: %s\n"
      "  ElementId: %s\n"
      "  OffsetToTransformParent: %s\n"
      "  Position: %s\n"
      "  scrollable: %d\n"
      "  clip_tree_index: %d\n"
      "  effect_tree_index: %d\n"
      "  scroll_tree_index: %d\n"
      "  transform_tree_index: %d\n",
      id(),
      DebugName().c_str(),
      bounds().ToString().c_str(), element_id().ToString().c_str(),
      offset_to_transform_parent().ToString().c_str(),
      position().ToString().c_str(), scrollable(), clip_tree_index(),
      effect_tree_index(), scroll_tree_index(), transform_tree_index());
}

void Layer::SetUseParentBackfaceVisibility(bool use) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.use_parent_backface_visibility == use)
    return;
  inputs_.use_parent_backface_visibility = use;
  SetNeedsPushProperties();
}

void Layer::SetShouldCheckBackfaceVisibility(
    bool should_check_backface_visibility) {
  if (should_check_backface_visibility_ == should_check_backface_visibility)
    return;
  should_check_backface_visibility_ = should_check_backface_visibility;
  SetNeedsPushProperties();
}

void Layer::SetIsDrawable(bool is_drawable) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.is_drawable == is_drawable)
    return;

  inputs_.is_drawable = is_drawable;
  UpdateDrawsContent(HasDrawableContent());
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.hide_layer_and_subtree == hide)
    return;

  inputs_.hide_layer_and_subtree = hide;
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
  SetSubtreePropertyChanged();
}

void Layer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  if (dirty_rect.IsEmpty())
    return;

  SetNeedsPushProperties();
  inputs_.update_rect.Union(dirty_rect);

  if (DrawsContent() && layer_tree_host_ && !ignore_set_needs_commit_)
    layer_tree_host_->SetNeedsUpdateLayers();
}

bool Layer::IsSnappedToPixelGridInTarget() {
  return false;
}

void Layer::PushPropertiesTo(LayerImpl* layer) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "Layer::PushPropertiesTo");
  DCHECK(layer_tree_host_);

  if (inputs_.mask_layer) {
    DCHECK_EQ(bounds(), inputs_.mask_layer->bounds());
    DCHECK(inputs_.mask_layer->position().IsOrigin());
  }

  // The element id should be set first because other setters may
  // depend on it. Referencing element id on a layer is
  // deprecated. http://crbug.com/709137
  layer->SetElementId(inputs_.element_id);
  layer->SetHasTransformNode(has_transform_node_);
  layer->SetBackgroundColor(inputs_.background_color);
  layer->SetSafeOpaqueBackgroundColor(safe_opaque_background_color_);
  layer->SetBounds(inputs_.bounds);
  layer->SetTransformTreeIndex(transform_tree_index());
  layer->SetEffectTreeIndex(effect_tree_index());
  layer->SetClipTreeIndex(clip_tree_index());
  layer->SetScrollTreeIndex(scroll_tree_index());
  layer->SetOffsetToTransformParent(offset_to_transform_parent_);
  layer->SetDrawsContent(DrawsContent());
  layer->SetHitTestable(HitTestable());
  // subtree_property_changed_ is propagated to all descendants while building
  // property trees. So, it is enough to check it only for the current layer.
  if (subtree_property_changed_)
    layer->NoteLayerPropertyChanged();
  layer->set_may_contain_video(may_contain_video_);
  layer->SetMasksToBounds(inputs_.masks_to_bounds);
  layer->SetNonFastScrollableRegion(inputs_.non_fast_scrollable_region);
  layer->SetTouchActionRegion(inputs_.touch_action_region);
  layer->SetMirrorCount(inputs_.mirror_count);
  // TODO(sunxd): Pass the correct region for wheel event handlers, see
  // https://crbug.com/841364.
  EventListenerProperties mouse_wheel_props =
      layer_tree_host()->event_listener_properties(
          EventListenerClass::kMouseWheel);
  if (mouse_wheel_props == EventListenerProperties::kBlocking ||
      mouse_wheel_props == EventListenerProperties::kBlockingAndPassive) {
    layer->SetWheelEventHandlerRegion(Region(gfx::Rect(bounds())));
  } else {
    layer->SetWheelEventHandlerRegion(Region());
  }
  layer->SetContentsOpaque(inputs_.contents_opaque);
  layer->SetUseParentBackfaceVisibility(inputs_.use_parent_backface_visibility);
  layer->SetShouldCheckBackfaceVisibility(should_check_backface_visibility_);

  if (scrollable())
    layer->SetScrollable(inputs_.scroll_container_bounds);

  layer->set_is_scrollbar(inputs_.is_scrollbar);

  // The property trees must be safe to access because they will be used below
  // to call |SetScrollOffsetClobberActiveValue|.
  DCHECK(layer->layer_tree_impl()->lifecycle().AllowsPropertyTreeAccess());

  // When a scroll offset animation is interrupted the new scroll position on
  // the pending tree will clobber any impl-side scrolling occuring on the
  // active tree. To do so, avoid scrolling the pending tree along with it
  // instead of trying to undo that scrolling later.
  if (layer_tree_host_->mutator_host()->ScrollOffsetAnimationWasInterrupted(
          element_id())) {
    PropertyTrees* trees = layer->layer_tree_impl()->property_trees();
    trees->scroll_tree.SetScrollOffsetClobberActiveValue(layer->element_id());
  }

  if (needs_show_scrollbars_)
    layer->set_needs_show_scrollbars(true);

  layer->UnionUpdateRect(inputs_.update_rect);
  layer->SetHasWillChangeTransformHint(has_will_change_transform_hint());
  layer->SetNeedsPushProperties();

  // debug_info_->invalidations, if exist, will be cleared in the function.
  layer->UpdateDebugInfo(debug_info_.get());

  // Reset any state that should be cleared for the next update.
  needs_show_scrollbars_ = false;
  subtree_property_changed_ = false;
  inputs_.update_rect = gfx::Rect();
}

void Layer::TakeCopyRequests(
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests) {
  for (std::unique_ptr<viz::CopyOutputRequest>& request :
       inputs_.copy_requests) {
    // Ensure the result callback is not invoked on the compositing thread.
    if (!request->has_result_task_runner()) {
      request->set_result_task_runner(
          layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner());
    }
    if (request->has_area()) {
      request->set_area(
          gfx::IntersectRects(request->area(), gfx::Rect(bounds())));
    }
    requests->push_back(std::move(request));
  }

  inputs_.copy_requests.clear();
}

std::unique_ptr<LayerImpl> Layer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, inputs_.layer_id);
}

bool Layer::DrawsContent() const {
  return draws_content_;
}

bool Layer::HasDrawableContent() const {
  return inputs_.is_drawable;
}

void Layer::UpdateDrawsContent(bool has_drawable_content) {
  bool draws_content = has_drawable_content;
  DCHECK(inputs_.is_drawable || !has_drawable_content);
  if (draws_content == draws_content_)
    return;

  if (parent())
    parent()->AddDrawableDescendants(draws_content ? 1 : -1);

  draws_content_ = draws_content;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

int Layer::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_;
}

bool Layer::Update() {
  DCHECK(layer_tree_host_);
  return false;
}

void Layer::SetSubtreePropertyChanged() {
  if (subtree_property_changed_)
    return;
  subtree_property_changed_ = true;
  SetNeedsPushProperties();
}

void Layer::SetMayContainVideo(bool yes) {
  if (may_contain_video_ == yes)
    return;
  may_contain_video_ = yes;
  SetNeedsPushProperties();
}

// On<Property>Animated is called due to an ongoing accelerated animation.
// Since this animation is also being run on the compositor thread, there
// is no need to request a commit to push this value over, so the value is
// set directly rather than by calling Set<Property>.
void Layer::OnFilterAnimated(const FilterOperations& filters) {
  inputs_.filters = filters;
}

void Layer::OnBackdropFilterAnimated(const FilterOperations& backdrop_filters) {
  inputs_.backdrop_filters = backdrop_filters;
}

void Layer::OnOpacityAnimated(float opacity) {
  inputs_.opacity = opacity;
}

void Layer::OnTransformAnimated(const gfx::Transform& transform) {
  inputs_.transform = transform;
}

void Layer::SetHasWillChangeTransformHint(bool has_will_change) {
  if (inputs_.has_will_change_transform_hint == has_will_change)
    return;
  inputs_.has_will_change_transform_hint = has_will_change;
  SetNeedsCommit();
}

void Layer::SetTrilinearFiltering(bool trilinear_filtering) {
  if (inputs_.trilinear_filtering == trilinear_filtering)
    return;
  inputs_.trilinear_filtering = trilinear_filtering;
  // When true, makes a RenderSurface which makes an effect node.
  SetPropertyTreesNeedRebuild();
  // Adding a RenderSurface may change how things in the subtree appear, since
  // it flattens transforms.
  SetSubtreePropertyChanged();
  SetNeedsCommit();
}

void Layer::IncrementMirrorCount() {
  SetMirrorCount(mirror_count() + 1);
}

void Layer::DecrementMirrorCount() {
  SetMirrorCount(mirror_count() - 1);
}

void Layer::SetMirrorCount(int mirror_count) {
  if (inputs_.mirror_count == mirror_count)
    return;

  DCHECK_LE(0, mirror_count);
  bool was_mirrored = inputs_.mirror_count > 0;
  inputs_.mirror_count = mirror_count;
  bool is_mirrored = inputs_.mirror_count > 0;
  if (was_mirrored != is_mirrored)
    SetPropertyTreesNeedRebuild();
  SetNeedsPushProperties();
}

ElementListType Layer::GetElementTypeForAnimation() const {
  return ElementListType::ACTIVE;
}

void Layer::AddDrawableDescendants(int num) {
  DCHECK_GE(num_descendants_that_draw_content_, 0);
  DCHECK_GE(num_descendants_that_draw_content_ + num, 0);
  if (num == 0)
    return;
  num_descendants_that_draw_content_ += num;
  SetNeedsCommit();
  if (parent())
    parent()->AddDrawableDescendants(num);
}

void Layer::RunMicroBenchmark(MicroBenchmark* benchmark) {}

void Layer::SetElementId(ElementId id) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.element_id == id)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "Layer::SetElementId",
               "element", id.AsValue().release());
  bool should_register_element =
      layer_tree_host() &&
      (!layer_tree_host()->IsUsingLayerLists() || inputs_.scrollable);
  if (should_register_element && inputs_.element_id) {
    layer_tree_host_->UnregisterElement(inputs_.element_id,
                                        ElementListType::ACTIVE);
  }

  inputs_.element_id = id;

  if (should_register_element && inputs_.element_id) {
    layer_tree_host_->RegisterElement(inputs_.element_id,
                                      ElementListType::ACTIVE, this);
  }

  SetNeedsCommit();
}

gfx::Transform Layer::ScreenSpaceTransform() const {
  DCHECK_NE(transform_tree_index_, TransformTree::kInvalidNodeId);
  return draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host_->property_trees()->transform_tree);
}

}  // namespace cc
