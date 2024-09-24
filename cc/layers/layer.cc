// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
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
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

struct SameSizeAsLayer : public base::RefCounted<SameSizeAsLayer>,
                         public ProtectedSequenceSynchronizer {
 private:
  SameSizeAsLayer();
  ~SameSizeAsLayer() override;

  void* pointers[2];
  struct {
    LayerList children;
    gfx::Size bounds;
    unsigned bitfields;
    SkColor4f background_color;
    TouchActionRegion touch_action_region;
    ElementId element_id;
    raw_ptr<void> rare_inputs;
  } inputs;
  raw_ptr<void> layer_tree_inputs;
  gfx::Rect update_rect;
  int int_fields[7];
  gfx::Vector2dF offset;
  unsigned bitfields;
  std::unique_ptr<int> debug_info;
};

static_assert(sizeof(Layer) == sizeof(SameSizeAsLayer),
              "Layer should stay small");

base::AtomicSequenceNumber g_next_layer_id;

constexpr gfx::Transform Layer::kIdentityTransform;
constexpr gfx::RoundedCornersF Layer::kNoRoundedCornersF;

LayerDebugInfo::LayerDebugInfo() = default;
LayerDebugInfo::LayerDebugInfo(const LayerDebugInfo&) = default;
LayerDebugInfo::~LayerDebugInfo() = default;

Layer::RareInputs::RareInputs() = default;
Layer::RareInputs::~RareInputs() = default;

Layer::Inputs::Inputs() = default;
Layer::Inputs::~Inputs() = default;

Layer::LayerTreeInputs::LayerTreeInputs() = default;
Layer::LayerTreeInputs::~LayerTreeInputs() = default;

scoped_refptr<Layer> Layer::Create() {
  return base::WrapRefCounted(new Layer());
}

Layer::Layer()
    : parent_(nullptr),
      layer_tree_host_(nullptr),
      // Layer IDs start from 1.
      layer_id_(g_next_layer_id.GetNext() + 1),
      num_descendants_that_draw_content_(0),
      transform_tree_index_(kInvalidPropertyNodeId),
      effect_tree_index_(kInvalidPropertyNodeId),
      clip_tree_index_(kInvalidPropertyNodeId),
      scroll_tree_index_(kInvalidPropertyNodeId),
      property_tree_sequence_number_(-1),
      ignore_set_needs_commit_for_test_(false),
      bitflags_(0u),
      subtree_property_changed_(false) {}

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

Layer::LayerTreeInputs& Layer::EnsureLayerTreeInputs() {
  DCHECK(!IsAttached() || !IsUsingLayerLists());
  auto& layer_tree_inputs = layer_tree_inputs_.Write(*this);
  if (!layer_tree_inputs)
    layer_tree_inputs = std::make_unique<LayerTreeInputs>();
  return *layer_tree_inputs;
}

#if DCHECK_IS_ON()
const Layer::LayerTreeInputs* Layer::layer_tree_inputs() const {
  DCHECK(!IsAttached() || !IsUsingLayerLists());
  return layer_tree_inputs_.Read(*this);
}
#endif

void Layer::SetLayerTreeHost(LayerTreeHost* host) {
  DCHECK(IsPropertyChangeAllowed());
  if (layer_tree_host() == host)
    return;

  bool property_tree_indices_invalid = false;
  auto& inputs = inputs_.Write(*this);
  ElementId element_id = inputs.element_id;
  if (IsAttached()) {
    // These two lines copied from ProtectedSequenceReadable::Write()
    DCHECK(IsOwnerThread());
    layer_tree_host()->WaitForProtectedSequenceCompletion();

    layer_tree_host()->UnregisterLayer(this);
    if (element_id)
      layer_tree_host()->UnregisterElement(element_id);
    if (!IsUsingLayerLists()) {
      layer_tree_host()->property_trees()->set_needs_rebuild(true);
      property_tree_indices_invalid = true;
    }
  }
  if (host) {
    host->RegisterLayer(this);
    if (element_id)
      host->RegisterElement(element_id, this);
    if (!host->IsUsingLayerLists()) {
      host->property_trees()->set_needs_rebuild(true);
      property_tree_indices_invalid = true;
    }
  }

  // See comment in layer.h to learn why this assignment is so weird.
  const_cast<raw_ptr<LayerTreeHost>&>(layer_tree_host_) = host;

  if (property_tree_indices_invalid)
    InvalidatePropertyTreesIndices();

  // When changing hosts, the layer needs to commit its properties to the impl
  // side for the new host.
  SetNeedsPushProperties();

  for (auto child : inputs.children)
    child->SetLayerTreeHost(host);

  if (host && !host->IsUsingLayerLists() &&
      host->mutator_host()->IsElementAnimating(element_id)) {
    host->SetNeedsCommit();
  }
}

void Layer::SetNeedsCommit() {
  if (!IsAttached())
    return;

  SetNeedsPushProperties();

  if (ignore_set_needs_commit_for_test_.Read(*this))
    return;

  layer_tree_host()->SetNeedsCommit();
}

void Layer::SetDebugName(const std::string& name) {
  if (name.empty() && !debug_info_.Read(*this))
    return;
  EnsureDebugInfo().name = name;
}

viz::ViewTransitionElementResourceId Layer::ViewTransitionResourceId() const {
  return viz::ViewTransitionElementResourceId();
}

bool Layer::IsSolidColorLayerForTesting() const {
  return false;
}

void Layer::SetNeedsFullTreeSync() {
  if (!IsAttached())
    return;

  layer_tree_host()->SetNeedsFullTreeSync();
}

void Layer::SetNeedsPushProperties() {
  if (IsAttached())
    layer_tree_host()->AddLayerShouldPushProperties(this);
}

bool Layer::IsPropertyChangeAllowed() const {
  if (!IsAttached())
    return true;
  DCHECK(IsMainThread());

  return !layer_tree_host()->in_paint_layer_contents();
}

void Layer::CaptureContent(const gfx::Rect& rect,
                           std::vector<NodeInfo>* content) const {}

sk_sp<const SkPicture> Layer::GetPicture() const {
  return nullptr;
}

void Layer::SetParent(Layer* layer, RemovalReason reason) {
  DCHECK(!layer || !layer->HasAncestor(this));
  DCHECK(reason == RemovalReason::kNormal || !layer);

  raw_ptr<Layer>& parent = parent_.Write(*this);
  parent = layer;
  if (reason == RemovalReason::kForReadd) {
    // When passing kForReadd, the caller is responsible for calling
    // SetLayerTreeHost.  Deferring this until after the readd means that the
    // single SetLayerTreeHost call will generally set layer_tree_host_ to the
    // same value, and thus be able to optimize away the recursive tree walk.
#if DCHECK_IS_ON()
    DCHECK(allow_remove_for_readd_);
#endif
  } else {
    SetLayerTreeHost(parent ? parent->layer_tree_host() : nullptr);
  }

  SetPropertyTreesNeedRebuild();
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChild(child, inputs_.Read(*this).children.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t index) {
  DCHECK(IsPropertyChangeAllowed());
  AllowRemoveForReadd allow(child.get());
  child->RemoveFromParentForReadd();
  AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                         (child->draws_content() ? 1 : 0));
  child->SetParent(this, RemovalReason::kNormal);
  child->SetSubtreePropertyChanged();

  auto& inputs = inputs_.Write(*this);
  index = std::min(index, inputs.children.size());
  const auto* layer_tree_inputs = layer_tree_inputs_.Read(*this);
  if (layer_tree_inputs && layer_tree_inputs->mask_layer && index &&
      index == inputs.children.size()) {
    // Ensure that the mask layer is always the last child.
    DCHECK_EQ(mask_layer(), inputs.children.back().get());
    index--;
  }
  inputs.children.insert(inputs.children.begin() + index, child);
  SetNeedsFullTreeSync();
}

void Layer::RemoveFromParent() {
  DCHECK(IsPropertyChangeAllowed());
  if (parent_.Read(*this))
    parent_.Write(*this)->RemoveChild(this, RemovalReason::kNormal);
}

void Layer::RemoveFromParentForReadd() {
  DCHECK(IsPropertyChangeAllowed());
  if (parent_.Read(*this))
    parent_.Write(*this)->RemoveChild(this, RemovalReason::kForReadd);
}

void Layer::RemoveChild(Layer* child, RemovalReason reason) {
  const auto* layer_tree_inputs = layer_tree_inputs_.Read(*this);
  if (layer_tree_inputs && child == layer_tree_inputs->mask_layer)
    layer_tree_inputs_.Write(*this)->mask_layer = nullptr;

  auto& inputs = inputs_.Write(*this);
  for (auto iter = inputs.children.begin(); iter != inputs.children.end();
       ++iter) {
    if (iter->get() != child)
      continue;

    child->SetParent(nullptr, reason);
    AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                           (child->draws_content() ? 1 : 0));
    inputs.children.erase(iter);
    SetNeedsFullTreeSync();
    return;
  }
}

bool Layer::GetBitFlag(uint8_t mask) const {
  return bitflags_.Read(*this) & mask;
}

bool Layer::SetBitFlag(bool new_value,
                       uint8_t mask,
                       bool invalidate,
                       bool needs_push) {
  if (GetBitFlag(mask) == new_value)
    return false;
  if (new_value)
    bitflags_.Write(*this) |= mask;
  else
    bitflags_.Write(*this) &= ~mask;
  if (invalidate) {
    SetPropertyTreesNeedRebuild();
    SetNeedsCommit();
  }
  if (needs_push)
    SetNeedsPushProperties();
  return true;
}

void Layer::ReorderChildren(LayerList* new_children_order) {
  auto& inputs = inputs_.Write(*this);
#if DCHECK_IS_ON()
  base::flat_set<Layer*> children_set;
  for (const auto& child : *new_children_order) {
    DCHECK_EQ(child->parent(), this);
    children_set.insert(child.get());
  }
  for (const auto& child : inputs.children)
    DCHECK_GT(children_set.count(child.get()), 0u);
#endif
  inputs.children = std::move(*new_children_order);

  // We do not need to call SetSubtreePropertyChanged for each child here
  // since SetSubtreePropertyChanged includes SetNeedsPushProperties, but this
  // change is not included in properties pushing.
  for (const auto& child : inputs.children)
    child->subtree_property_changed_.Write(*child) = true;

  SetNeedsFullTreeSync();
}

void Layer::ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer) {
  DCHECK(reference);
  DCHECK_EQ(reference->parent(), this);
  DCHECK(IsPropertyChangeAllowed());

  if (reference == new_layer.get())
    return;

  // Find the index of |reference| in |children_|.
  auto& inputs = inputs_.Write(*this);
  auto reference_it = base::ranges::find(inputs.children, reference,
                                         &scoped_refptr<Layer>::get);
  CHECK(reference_it != inputs.children.end(), base::NotFatalUntil::M130);
  size_t reference_index = reference_it - inputs.children.begin();
  reference->RemoveFromParent();

  if (new_layer.get()) {
    InsertChild(new_layer, reference_index);
  }
}

void Layer::SetBounds(const gfx::Size& size) {
  DCHECK(IsPropertyChangeAllowed());
  if (bounds() == size)
    return;
  inputs_.Write(*this).bounds = size;

  if (!IsAttached())
    return;

  // Rounded corner clipping, bounds clipping and mask clipping can result in
  // new areas of subtrees being exposed on a bounds change. Ensure the damaged
  // areas are updated. Also, if the layer subtree (rooted at this layer) is
  // marked as capturable (via a valid SubtreeCaptureId), then the property tree
  // needs rebuild so that |EffectNode::subtree_size| is updated with the new
  // size of this layer.
  if (!IsUsingLayerLists()) {
    if (subtree_capture_id().is_valid() || masks_to_bounds() || mask_layer() ||
        HasMaskFilter()) {
      SetSubtreePropertyChanged();
      SetPropertyTreesNeedRebuild();
    }

    if (scrollable()) {
      auto& scroll_tree =
          layer_tree_host()->property_trees()->scroll_tree_mutable();
      if (auto* scroll_node = scroll_tree.Node(scroll_tree_index_.Read(*this)))
        scroll_node->bounds = inputs_.Read(*this).bounds;
      else
        SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

Layer* Layer::RootLayer() {
  Layer* layer = this;
  while (layer->parent())
    layer = layer->mutable_parent();
  return layer;
}

void Layer::RemoveAllChildren() {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = inputs_.Write(*this);
  while (inputs.children.size()) {
    Layer* layer = inputs.children[0].get();
    DCHECK_EQ(this, layer->parent());
    layer->RemoveFromParent();
  }
}

void Layer::SetChildLayerList(LayerList new_children) {
  DCHECK(IsUsingLayerLists());

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
      child->SetParent(nullptr, RemovalReason::kNormal);
      AddDrawableDescendants(-child->NumDescendantsThatDrawContent() -
                             (child->draws_content() ? 1 : 0));
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
      AllowRemoveForReadd allow(child.get());
      child->RemoveFromParentForReadd();
      AddDrawableDescendants(child->NumDescendantsThatDrawContent() +
                             (child->draws_content() ? 1 : 0));
      child->SetParent(this, RemovalReason::kNormal);
      child->SetSubtreePropertyChanged();
    }
  }

  inputs_.Write(*this).children = std::move(new_children);

  layer_tree_host()->SetNeedsFullTreeSync();
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
  auto& inputs = EnsureLayerTreeInputs();
  if (request->has_source()) {
    const base::UnguessableToken& source = request->source();
    auto it = base::ranges::find_if(
        inputs.copy_requests,
        [&source](const std::unique_ptr<viz::CopyOutputRequest>& x) {
          return x->has_source() && x->source() == source;
        });
    if (it != inputs.copy_requests.end())
      inputs.copy_requests.erase(it);
  }
  inputs.copy_requests.push_back(std::move(request));
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
  if (IsAttached())
    layer_tree_host()->SetHasCopyRequest(true);
}

void Layer::SetBackgroundColor(SkColor4f background_color) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = inputs_.Write(*this);
  if (inputs.background_color == background_color)
    return;
  inputs.background_color = background_color;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetSafeOpaqueBackgroundColor(SkColor4f background_color) {
  DCHECK(IsPropertyChangeAllowed());
  SkColor4f opaque_color = background_color.makeOpaque();
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.safe_opaque_background_color == opaque_color)
    return;
  inputs.safe_opaque_background_color = opaque_color;
  SetNeedsPushProperties();
}

SkColor4f Layer::SafeOpaqueBackgroundColor() const {
  if (contents_opaque()) {
    if (!IsUsingLayerLists()) {
      // In layer tree mode, PropertyTreeBuilder should have calculated the safe
      // opaque background color and called SetSafeOpaqueBackgroundColor().
      DCHECK(layer_tree_inputs());
      DCHECK(layer_tree_inputs()->safe_opaque_background_color.isOpaque());
      return layer_tree_inputs()->safe_opaque_background_color;
    }
    // In layer list mode, the PropertyTreeBuilder algorithm doesn't apply
    // because it depends on the layer tree hierarchy. Instead we use
    // background_color() made opaque.
    return background_color().makeOpaque();
  }
  if (background_color().isOpaque()) {
    // The layer is not opaque while the background color is, meaning that the
    // background color doesn't cover the whole layer. Use
    // SkColors::kTransparent to avoid intrusive checkerboard where the layer is
    // not covered by the background color.
    return SkColors::kTransparent;
  }
  return background_color();
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.masks_to_bounds == masks_to_bounds)
    return;
  inputs.masks_to_bounds = masks_to_bounds;
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
  SetSubtreePropertyChanged();
}

void Layer::SetClipRect(const gfx::Rect& clip_rect) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.clip_rect == clip_rect)
    return;
  inputs.clip_rect = clip_rect;

  // If the clip bounds have been cleared, the property trees needs a rebuild.
  const bool force_rebuild = clip_rect.IsEmpty() || !has_clip_node();

  SetSubtreePropertyChanged();
  if (clip_tree_index() != kInvalidPropertyNodeId && !force_rebuild) {
    PropertyTrees* property_trees = layer_tree_host()->property_trees();
    gfx::RectF effective_clip_rect = EffectiveClipRect();
    if (ClipNode* node =
            property_trees->clip_tree_mutable().Node(clip_tree_index())) {
      node->clip = effective_clip_rect;
      node->clip += offset_to_transform_parent();
      property_trees->clip_tree_mutable().set_needs_update(true);
    }
    if (HasMaskFilter() && effect_tree_index() != kInvalidPropertyNodeId) {
      if (EffectNode* node =
              property_trees->effect_tree_mutable().Node(effect_tree_index())) {
        node->mask_filter_info = gfx::MaskFilterInfo(
            effective_clip_rect, corner_radii(), gradient_mask());
        node->effect_changed = true;
        property_trees->effect_tree_mutable().set_needs_update(true);
      }
    }
  } else {
    SetPropertyTreesNeedRebuild();
  }
  SetNeedsCommit();
}

gfx::RectF Layer::EffectiveClipRect() const {
  // If this does not have a clip rect set, then the subtree is clipped by
  // the bounds.
  const gfx::RectF layer_bounds = gfx::RectF(gfx::SizeF(bounds()));
  if (clip_rect().IsEmpty())
    return layer_bounds;

  const gfx::RectF clip_rect_f(clip_rect());

  // Layer needs to clip to its bounds as well apply a clip rect. Intersect the
  // two to get the effective clip.
  if (masks_to_bounds() || mask_layer() || filters().HasFilterThatMovesPixels())
    return gfx::IntersectRects(layer_bounds, clip_rect_f);

  // Clip rect is the only clip effecting the layer.
  return clip_rect_f;
}

void Layer::SetMaskLayer(scoped_refptr<PictureLayer> mask_layer) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.mask_layer.get() == mask_layer)
    return;
  if (inputs.mask_layer) {
    DCHECK_EQ(this, inputs.mask_layer->parent());
    inputs.mask_layer->RemoveFromParent();
  }
  // Clear mask_layer first and set it later because InsertChild() checks it to
  // ensure the mask layer is the last child.
  inputs.mask_layer = nullptr;
  if (mask_layer) {
    // The mask layer should not have any children.
    DCHECK(mask_layer->children().empty());

    mask_layer->EnsureLayerTreeInputs().position = gfx::PointF();
    mask_layer->SetIsDrawable(true);
    mask_layer->SetBlendMode(SkBlendMode::kDstIn);
    // This flag will be updated in PropertyTreeBuilder.
    mask_layer->SetIsBackdropFilterMask(false);
    AddChild(mask_layer);
  }
  inputs.mask_layer = mask_layer.get();
  SetSubtreePropertyChanged();
}

void Layer::SetFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.filters == filters)
    return;
  inputs.filters = filters;
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetBackdropFilters(const FilterOperations& filters) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.backdrop_filters == filters)
    return;
  inputs.backdrop_filters = filters;

  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetBackdropFilterBounds(const gfx::RRectF& backdrop_filter_bounds) {
  EnsureLayerTreeInputs().backdrop_filter_bounds = backdrop_filter_bounds;
}

void Layer::ClearBackdropFilterBounds() {
  if (layer_tree_inputs())
    layer_tree_inputs_.Write(*this)->backdrop_filter_bounds.reset();
}

void Layer::SetBackdropFilterQuality(const float quality) {
  EnsureLayerTreeInputs().backdrop_filter_quality = quality;
}

void Layer::UpdateMaskFilterInfo(const gfx::RoundedCornersF* corner_radii,
                                 const gfx::LinearGradient* gradient_mask) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();

  if (corner_radii)
    inputs.corner_radii = *corner_radii;

  if (gradient_mask)
    inputs.gradient_mask = *gradient_mask;

  SetSubtreePropertyChanged();
  SetNeedsCommit();
  PropertyTrees* property_trees =
      IsAttached() ? layer_tree_host()->property_trees() : nullptr;
  EffectNode* node = nullptr;
  if (property_trees && effect_tree_index() != kInvalidPropertyNodeId &&
      (node =
           property_trees->effect_tree_mutable().Node(effect_tree_index()))) {
    gfx::RectF effective_clip_rect = EffectiveClipRect();
    effective_clip_rect += offset_to_transform_parent();
    node->mask_filter_info = gfx::MaskFilterInfo(
        effective_clip_rect, inputs.corner_radii, inputs.gradient_mask);
    node->effect_changed = true;
    property_trees->effect_tree_mutable().set_needs_update(true);
  } else {
    SetPropertyTreesNeedRebuild();
  }
}

void Layer::SetRoundedCorner(const gfx::RoundedCornersF& corner_radii) {
  if (EnsureLayerTreeInputs().corner_radii == corner_radii)
    return;

  UpdateMaskFilterInfo(&corner_radii, nullptr);
}

void Layer::SetGradientMask(const gfx::LinearGradient& gradient_mask) {
  if (EnsureLayerTreeInputs().gradient_mask == gradient_mask)
    return;

  UpdateMaskFilterInfo(nullptr, &gradient_mask);
}

void Layer::SetIsFastRoundedCorner(bool enable) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.is_fast_rounded_corner == enable)
    return;
  inputs.is_fast_rounded_corner = enable;

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

  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.opacity == opacity)
    return;
  // We need to force a property tree rebuild when opacity changes from 1 to a
  // non-1 value or vice-versa as render surfaces can change.
  bool force_rebuild = opacity == 1.f || inputs.opacity == 1.f;
  inputs.opacity = opacity;
  SetSubtreePropertyChanged();

  if (IsAttached()) {
    if (!force_rebuild) {
      PropertyTrees* property_trees = layer_tree_host()->property_trees();
      if (EffectNode* node =
              property_trees->effect_tree_mutable().Node(effect_tree_index())) {
        node->opacity = opacity;
        node->effect_changed = true;
        property_trees->effect_tree_mutable().set_needs_update(true);
      }
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  SetNeedsCommit();
}

float Layer::EffectiveOpacity() const {
  if (!layer_tree_inputs())
    return 1.0f;
  return layer_tree_inputs()->hide_layer_and_subtree
             ? 0.f
             : layer_tree_inputs()->opacity;
}

bool Layer::OpacityCanAnimateOnImplThread() const {
  return false;
}

void Layer::SetBlendMode(SkBlendMode blend_mode) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.blend_mode == blend_mode)
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
  }

  inputs.blend_mode = blend_mode;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
}

void Layer::SetHitTestOpaqueness(HitTestOpaqueness opaqueness) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = inputs_.Write(*this);
  if (inputs.hit_test_opaqueness == opaqueness) {
    return;
  }
  inputs.hit_test_opaqueness = opaqueness;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetHitTestable(bool hit_testable) {
  SetHitTestOpaqueness(hit_testable ? HitTestOpaqueness::kMixed
                                    : HitTestOpaqueness::kTransparent);
}

void Layer::SetContentsOpaque(bool opaque) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.Read(*this).contents_opaque == opaque)
    return;
  auto& inputs = inputs_.Write(*this);
  inputs.contents_opaque = opaque;
  inputs.contents_opaque_for_text = opaque;
  SetNeedsCommit();
  SetSubtreePropertyChanged();
  SetPropertyTreesNeedRebuild();
}

void Layer::SetContentsOpaqueForText(bool opaque) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.Read(*this).contents_opaque_for_text == opaque)
    return;
  DCHECK(!contents_opaque() || opaque);
  inputs_.Write(*this).contents_opaque_for_text = opaque;
  SetNeedsCommit();
}

void Layer::SetPosition(const gfx::PointF& position) {
  DCHECK(!IsAttached() || !IsUsingLayerLists());

  // The mask layer should always be at the same location as the masked layer
  // which is its parent, so its position should be always zero.
  if (parent() && parent()->mask_layer() == this) {
    DCHECK(this->position().IsOrigin());
    return;
  }

  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.position == position)
    return;
  inputs.position = position;

  if (!IsAttached())
    return;

  SetSubtreePropertyChanged();

  if (has_transform_node()) {
    TransformNode* transform_node =
        layer_tree_host()->property_trees()->transform_tree_mutable().Node(
            transform_tree_index_.Read(*this));
    // We should never set root layer's position to non-zero.
    DCHECK(parent());
    transform_node->post_translation =
        position.OffsetFromOrigin() + parent()->offset_to_transform_parent();
    transform_node->needs_local_transform_update = true;
    transform_node->transform_changed = true;
    layer_tree_host()
        ->property_trees()
        ->transform_tree_mutable()
        .set_needs_update(true);
  } else {
    SetPropertyTreesNeedRebuild();
  }

  SetNeedsCommit();
}

bool Are2dAxisAligned(const gfx::Transform& a, const gfx::Transform& b) {
  if (a.IsScaleOrTranslation() && b.IsScaleOrTranslation()) {
    return true;
  }

  gfx::Transform inverse;
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
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.transform == transform)
    return;

  SetSubtreePropertyChanged();
  if (IsAttached()) {
    if (has_transform_node()) {
      TransformNode* transform_node =
          layer_tree_host()->property_trees()->transform_tree_mutable().Node(
              transform_tree_index_.Read(*this));
      // We need to trigger a rebuild if we could have affected 2d axis
      // alignment. We'll check to see if transform and inputs_.transform are
      // axis align with respect to one another.
      DCHECK_EQ(transform_tree_index(), transform_node->id);
      bool preserves_2d_axis_alignment =
          Are2dAxisAligned(inputs.transform, transform);
      transform_node->local = transform;
      transform_node->needs_local_transform_update = true;
      transform_node->transform_changed = true;
      layer_tree_host()
          ->property_trees()
          ->transform_tree_mutable()
          .set_needs_update(true);
      if (!preserves_2d_axis_alignment)
        SetPropertyTreesNeedRebuild();
    } else {
      SetPropertyTreesNeedRebuild();
    }
  }

  inputs.transform = transform;
  SetNeedsCommit();
}

void Layer::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.transform_origin == transform_origin)
    return;
  inputs.transform_origin = transform_origin;

  if (!IsAttached())
    return;

  SetSubtreePropertyChanged();

  if (has_transform_node()) {
    TransformNode* transform_node =
        layer_tree_host()->property_trees()->transform_tree_mutable().Node(
            transform_tree_index_.Read(*this));
    DCHECK_EQ(transform_tree_index(), transform_node->id);
    transform_node->origin = transform_origin;
    transform_node->needs_local_transform_update = true;
    transform_node->transform_changed = true;
    layer_tree_host()
        ->property_trees()
        ->transform_tree_mutable()
        .set_needs_update(true);
  } else {
    SetPropertyTreesNeedRebuild();
  }

  SetNeedsCommit();
}

void Layer::SetScrollOffset(const gfx::PointF& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());

  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.scroll_offset == scroll_offset)
    return;
  inputs.scroll_offset = scroll_offset;

  if (!IsAttached())
    return;

  UpdatePropertyTreeScrollOffset();

  SetNeedsCommit();
}

void Layer::SetScrollOffsetFromImplSide(const gfx::PointF& scroll_offset) {
  DCHECK(IsPropertyChangeAllowed());
  // This function only gets called during a BeginMainFrame, so there
  // is no need to call SetNeedsUpdate here.
  DCHECK(IsAttached() && layer_tree_host()->CommitRequested());

  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.scroll_offset == scroll_offset)
    return;
  inputs.scroll_offset = scroll_offset;

  UpdatePropertyTreeScrollOffset();

  if (!inputs.did_scroll_callback.is_null())
    inputs.did_scroll_callback.Run(scroll_offset, element_id());

  // The callback could potentially change the layer structure:
  // "this" may have been destroyed during the process.
}

void Layer::UpdatePropertyTreeScrollOffset() {
  DCHECK(scrollable());
  DCHECK(!IsUsingLayerLists());

  if (scroll_tree_index() == kInvalidPropertyNodeId) {
    // Ensure the property trees just have not been built yet but are marked for
    // being built which will set the correct scroll offset values.
    DCHECK(layer_tree_host()->property_trees()->needs_rebuild());
    return;
  }

  // If a scroll node exists, it should have an associated transform node.
  DCHECK(transform_tree_index() != kInvalidPropertyNodeId);

  auto& property_trees = *layer_tree_host()->property_trees();
  property_trees.scroll_tree_mutable().SetScrollOffset(element_id(),
                                                       scroll_offset());
  auto* transform_node =
      property_trees.transform_tree_mutable().Node(transform_tree_index());
  DCHECK_EQ(transform_tree_index(), transform_node->id);
  transform_node->scroll_offset = scroll_offset();
  transform_node->needs_local_transform_update = true;
  property_trees.transform_tree_mutable().set_needs_update(true);
}

void Layer::SetDidScrollCallback(
    base::RepeatingCallback<void(const gfx::PointF&, const ElementId&)>
        callback) {
  EnsureLayerTreeInputs().did_scroll_callback = std::move(callback);
}

void Layer::SetSubtreeCaptureId(viz::SubtreeCaptureId subtree_id) {
  DCHECK(IsPropertyChangeAllowed());

  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.subtree_capture_id == subtree_id)
    return;

  DCHECK(!inputs.subtree_capture_id.is_valid() || !subtree_id.is_valid())
      << "Not allowed to change from a valid ID to another valid ID, as it may "
         "already be in use.";

  inputs.subtree_capture_id = subtree_id;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetScrollable(const gfx::Size& bounds) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.scrollable && inputs.scroll_container_bounds == bounds)
    return;
  bool was_scrollable = inputs.scrollable;
  inputs.scrollable = true;
  inputs.scroll_container_bounds = bounds;

  if (!IsAttached())
    return;

  auto& scroll_tree =
      layer_tree_host()->property_trees()->scroll_tree_mutable();
  auto* scroll_node = scroll_tree.Node(scroll_tree_index_.Read(*this));
  if (was_scrollable && scroll_node)
    scroll_node->container_bounds = inputs.scroll_container_bounds;
  else
    SetPropertyTreesNeedRebuild();

  SetNeedsCommit();
}

bool Layer::IsScrollbarLayerForTesting() const {
  return false;
}

void Layer::SetMainThreadScrollHitTestRegion(const Region& region) {
  DCHECK(IsPropertyChangeAllowed());
  const auto& rare_inputs = inputs_.Read(*this).rare_inputs;
  if (!rare_inputs && region.IsEmpty())
    return;
  if (rare_inputs &&
      rare_inputs->main_thread_scroll_hit_test_region == region) {
    return;
  }
  EnsureRareInputs().main_thread_scroll_hit_test_region = region;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetNonCompositedScrollHitTestRects(
    std::vector<ScrollHitTestRect> rects) {
  DCHECK(IsPropertyChangeAllowed());
  const auto& rare_inputs = inputs_.Read(*this).rare_inputs;
  if (!rare_inputs && rects.empty()) {
    return;
  }
  if (rare_inputs &&
      rare_inputs->non_composited_scroll_hit_test_rects == rects) {
    return;
  }
  EnsureRareInputs().non_composited_scroll_hit_test_rects = std::move(rects);
  SetNeedsCommit();
}

void Layer::SetTouchActionRegion(TouchActionRegion touch_action_region) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.Read(*this).touch_action_region == touch_action_region)
    return;

  inputs_.Write(*this).touch_action_region = std::move(touch_action_region);
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void Layer::SetCaptureBounds(viz::RegionCaptureBounds bounds) {
  DCHECK(IsPropertyChangeAllowed());
  const auto& rare_inputs = inputs_.Read(*this).rare_inputs;
  if (!rare_inputs && bounds.IsEmpty())
    return;
  if (rare_inputs && rare_inputs->capture_bounds == bounds)
    return;
  EnsureRareInputs().capture_bounds = std::move(bounds);
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
  SetSubtreePropertyChanged();
}

void Layer::SetWheelEventRegion(Region wheel_event_region) {
  DCHECK(IsPropertyChangeAllowed());
  const auto& rare_inputs = inputs_.Read(*this).rare_inputs;
  if (!rare_inputs && wheel_event_region.IsEmpty())
    return;
  if (rare_inputs && rare_inputs->wheel_event_region == wheel_event_region)
    return;
  EnsureRareInputs().wheel_event_region = std::move(wheel_event_region);
  SetNeedsCommit();
}

RenderSurfaceReason Layer::GetRenderSurfaceReason() const {
  if (!IsAttached())
    return RenderSurfaceReason::kNone;
  const PropertyTrees* property_trees = layer_tree_host()->property_trees();
  DCHECK(!property_trees->needs_rebuild());
  const EffectNode* effect_node =
      property_trees->effect_tree().Node(this->effect_tree_index());

  // Effect node can also be the effect node of an ancestor layer.
  // Check if this effect node was created for this layer specifically.
  if (!effect_node ||
      (parent_.Read(*this) &&
       this->effect_tree_index() == parent_.Read(*this)->effect_tree_index())) {
    return RenderSurfaceReason::kNone;
  }
  return effect_node->render_surface_reason;
}

void Layer::SetTransformTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (transform_tree_index_.Read(*this) == index)
    return;
  SetHasTransformNode(index != kInvalidPropertyNodeId);
  transform_tree_index_.Write(*this) = index;
  SetNeedsPushProperties();
}

int Layer::transform_tree_index(const PropertyTrees& property_trees) const {
  if (property_trees.sequence_number() !=
      property_tree_sequence_number_.Read(*this)) {
    return kInvalidPropertyNodeId;
  }
  return transform_tree_index_.Read(*this);
}

bool Layer::transform_tree_index_is_valid(
    const PropertyTrees& property_trees) const {
  return transform_tree_index_.Read(*this) != kInvalidPropertyNodeId &&
         property_trees.sequence_number() ==
             property_tree_sequence_number_.Read(*this);
}

int Layer::transform_tree_index() const {
  if (!IsAttached())
    return kInvalidPropertyNodeId;
  return transform_tree_index(*layer_tree_host()->property_trees());
}

void Layer::SetClipTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (clip_tree_index_.Read(*this) == index)
    return;
  clip_tree_index_.Write(*this) = index;
  SetNeedsPushProperties();
}

int Layer::clip_tree_index(const PropertyTrees& property_trees) const {
  if (property_trees.sequence_number() !=
      property_tree_sequence_number_.Read(*this)) {
    return kInvalidPropertyNodeId;
  }
  return clip_tree_index_.Read(*this);
}

bool Layer::clip_tree_index_is_valid(
    const PropertyTrees& property_trees) const {
  return clip_tree_index_.Read(*this) != kInvalidPropertyNodeId &&
         property_trees.sequence_number() ==
             property_tree_sequence_number_.Read(*this);
}

int Layer::clip_tree_index() const {
  if (!IsAttached())
    return kInvalidPropertyNodeId;
  return clip_tree_index(*layer_tree_host()->property_trees());
}

void Layer::SetEffectTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (effect_tree_index_.Read(*this) == index)
    return;
  effect_tree_index_.Write(*this) = index;
  SetNeedsPushProperties();
}

int Layer::effect_tree_index(const PropertyTrees& property_trees) const {
  if (property_trees.sequence_number() !=
      property_tree_sequence_number_.Read(*this)) {
    return kInvalidPropertyNodeId;
  }
  return effect_tree_index_.Read(*this);
}

bool Layer::effect_tree_index_is_valid(
    const PropertyTrees& property_trees) const {
  return effect_tree_index_.Read(*this) != kInvalidPropertyNodeId &&
         property_trees.sequence_number() ==
             property_tree_sequence_number_.Read(*this);
}

int Layer::effect_tree_index() const {
  if (!IsAttached())
    return kInvalidPropertyNodeId;
  return effect_tree_index(*layer_tree_host()->property_trees());
}

void Layer::SetScrollTreeIndex(int index) {
  DCHECK(IsPropertyChangeAllowed());
  if (scroll_tree_index_.Read(*this) == index)
    return;
  scroll_tree_index_.Write(*this) = index;
  SetNeedsPushProperties();
}

int Layer::scroll_tree_index(const PropertyTrees& property_trees) const {
  if (property_trees.sequence_number() !=
      property_tree_sequence_number_.Read(*this)) {
    return kInvalidPropertyNodeId;
  }
  return scroll_tree_index_.Read(*this);
}

bool Layer::scroll_tree_index_is_valid(
    const PropertyTrees& property_trees) const {
  return scroll_tree_index_.Read(*this) != kInvalidPropertyNodeId &&
         property_trees.sequence_number() ==
             property_tree_sequence_number_.Read(*this);
}

int Layer::scroll_tree_index() const {
  if (!IsAttached())
    return kInvalidPropertyNodeId;
  return scroll_tree_index(*layer_tree_host()->property_trees());
}

void Layer::SetOffsetToTransformParent(gfx::Vector2dF offset) {
  if (offset_to_transform_parent_.Read(*this) == offset)
    return;
  offset_to_transform_parent_.Write(*this) = offset;
  SetNeedsPushProperties();
  SetSubtreePropertyChanged();
}

void Layer::InvalidatePropertyTreesIndices() {
  SetTransformTreeIndex(kInvalidPropertyNodeId);
  SetClipTreeIndex(kInvalidPropertyNodeId);
  SetEffectTreeIndex(kInvalidPropertyNodeId);
  SetScrollTreeIndex(kInvalidPropertyNodeId);
}

void Layer::SetPropertyTreesNeedRebuild() {
  if (IsAttached())
    layer_tree_host()->property_trees()->set_needs_rebuild(true);
}

LayerDebugInfo& Layer::EnsureDebugInfo() {
  auto& info = debug_info_.Write(*this);
  if (!info) {
    info = std::make_unique<LayerDebugInfo>();
    // We just enabled debug info collection. Force PushPropertiesTo() to ensure
    // the first layer tree snapshot contains the debug info. Otherwise we will
    // push debug_info when we have other changes to push.
    SetNeedsPushProperties();
  }
  return *info;
}

void Layer::ClearDebugInfo() {
  if (!debug_info_.Read(*this))
    return;

  debug_info_.Write(*this).reset();
  SetNeedsPushProperties();
}

std::string Layer::DebugName() const {
  const auto* info = debug_info_.Read(*this);
  return info ? info->name : "";
}

std::string Layer::ToString() const {
  return base::StringPrintf(
      "layer_id: %d\n"
      "  name: %s\n"
      "  Bounds: %s\n"
      "  ElementId: %s\n"
      "  HitTestOpaqueness: %s\n"
      "  OffsetToTransformParent: %s\n"
      "  clip_tree_index: %d\n"
      "  effect_tree_index: %d\n"
      "  scroll_tree_index: %d\n"
      "  transform_tree_index: %d\n",
      id(), DebugName().c_str(), bounds().ToString().c_str(),
      element_id().ToString().c_str(),
      HitTestOpaquenessToString(hit_test_opaqueness()),
      offset_to_transform_parent().ToString().c_str(), clip_tree_index(),
      effect_tree_index(), scroll_tree_index(), transform_tree_index());
}

void Layer::SetIsDrawable(bool is_drawable) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.Read(*this).is_drawable == is_drawable)
    return;

  inputs_.Write(*this).is_drawable = is_drawable;
  UpdateDrawsContent();
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  DCHECK(IsPropertyChangeAllowed());
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.hide_layer_and_subtree == hide)
    return;

  inputs.hide_layer_and_subtree = hide;
  SetNeedsCommit();
  SetPropertyTreesNeedRebuild();
  SetSubtreePropertyChanged();
}

void Layer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  if (dirty_rect.IsEmpty())
    return;

  SetNeedsPushProperties();
  update_rect_.Write(*this).Union(dirty_rect);

  if (draws_content() && IsAttached() &&
      !ignore_set_needs_commit_for_test_.Read(*this))
    layer_tree_host()->SetNeedsUpdateLayers();
}

bool Layer::RequiresSetNeedsDisplayOnHdrHeadroomChange() const {
  return false;
}

bool Layer::IsSnappedToPixelGridInTarget() const {
  return false;
}

void Layer::PushPropertiesTo(LayerImpl* layer,
                             const CommitState& commit_state,
                             const ThreadUnsafeCommitState& unsafe_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "Layer::PushPropertiesTo");
  DCHECK(IsAttached());

  const PropertyTrees& property_trees = unsafe_state.property_trees;

  // The element id should be set first because other setters may
  // depend on it. Referencing element id on a layer is
  // deprecated. http://crbug.com/709137
  const auto& inputs = inputs_.Read(*this);
  layer->SetElementId(inputs.element_id);
  layer->SetHasTransformNode(has_transform_node());
  layer->SetBackgroundColor(inputs.background_color);
  layer->SetSafeOpaqueBackgroundColor(SafeOpaqueBackgroundColor());
  layer->SetBounds(inputs.bounds);
  layer->SetTransformTreeIndex(transform_tree_index(property_trees));
  layer->SetEffectTreeIndex(effect_tree_index(property_trees));
  layer->SetClipTreeIndex(clip_tree_index(property_trees));
  layer->SetScrollTreeIndex(scroll_tree_index(property_trees));
  layer->SetOffsetToTransformParent(offset_to_transform_parent_.Read(*this));
  layer->SetDrawsContent(draws_content());
  layer->SetHitTestOpaqueness(inputs.hit_test_opaqueness);
  // subtree_property_changed_ is propagated to all descendants while building
  // property trees. So, it is enough to check it only for the current layer.
  if (subtree_property_changed_.Read(*this))
    layer->NoteLayerPropertyChanged();
  layer->set_may_contain_video(may_contain_video());
  layer->SetTouchActionRegion(inputs.touch_action_region);
  layer->SetContentsOpaque(inputs.contents_opaque);
  layer->SetContentsOpaqueForText(inputs.contents_opaque_for_text);
  layer->SetShouldCheckBackfaceVisibility(should_check_backface_visibility());

  // The property trees must be safe to access because they will be used below
  // to call |SetScrollOffsetClobberActiveValue|.
  DCHECK(layer->layer_tree_impl()->lifecycle().AllowsPropertyTreeAccess());

  // When a scroll offset animation is interrupted the new scroll position on
  // the pending tree will clobber any impl-side scrolling occuring on the
  // active tree. To do so, avoid scrolling the pending tree along with it
  // instead of trying to undo that scrolling later.
  if (unsafe_state.mutator_host->ScrollOffsetAnimationWasInterrupted(
          element_id())) {
    PropertyTrees* trees = layer->layer_tree_impl()->property_trees();
    trees->scroll_tree_mutable().SetScrollOffsetClobberActiveValue(
        layer->element_id());
  }

  layer->UnionUpdateRect(update_rect_.Read(*this));
  layer->SetNeedsPushProperties();

  // debug_info_->invalidations, if exist, will be cleared in the function.
  layer->UpdateDebugInfo(debug_info_.Write(*this).get());

  if (inputs.rare_inputs) {
    layer->SetMainThreadScrollHitTestRegion(
        inputs.rare_inputs->main_thread_scroll_hit_test_region);
    layer->SetNonCompositedScrollHitTestRects(
        inputs.rare_inputs->non_composited_scroll_hit_test_rects);
    layer->SetCaptureBounds(inputs.rare_inputs->capture_bounds);
    layer->SetWheelEventHandlerRegion(inputs.rare_inputs->wheel_event_region);
  } else {
    layer->ResetRareProperties();
  }

  // Reset any state that should be cleared for the next update.
  subtree_property_changed_.Write(*this) = false;
  update_rect_.Write(*this) = gfx::Rect();
}

void Layer::TakeCopyRequests(
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests) {
  if (!layer_tree_inputs())
    return;

  auto& layer_tree_inputs = layer_tree_inputs_.Write(*this);
  for (std::unique_ptr<viz::CopyOutputRequest>& request :
       layer_tree_inputs->copy_requests) {
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

  layer_tree_inputs->copy_requests.clear();
}

std::unique_ptr<LayerImpl> Layer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return LayerImpl::Create(tree_impl, id());
}

bool Layer::HasDrawableContent() const {
  return inputs_.Read(*this).is_drawable;
}

void Layer::UpdateDrawsContent() {
  bool value = HasDrawableContent();
  DCHECK(inputs_.Read(*this).is_drawable || !value);
  if (!SetBitFlag(value, kDrawsContentFlagMask, /*invalidate=*/true))
    return;
  if (parent())
    mutable_parent()->AddDrawableDescendants(value ? 1 : -1);
}

int Layer::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_.Read(*this);
}

bool Layer::Update() {
  DCHECK(IsAttached());
  return false;
}

void Layer::SetSubtreePropertyChanged() {
  if (subtree_property_changed_.Read(*this))
    return;
  subtree_property_changed_.Write(*this) = true;
  SetNeedsPushProperties();
}

bool Layer::IsOwnerThread() const {
  return !IsAttached() || layer_tree_host_->IsOwnerThread();
}

bool Layer::IsMainThread() const {
  return IsAttached() && layer_tree_host_->IsMainThread();
}

bool Layer::InProtectedSequence() const {
  return IsAttached() && layer_tree_host_->InProtectedSequence();
}

void Layer::WaitForProtectedSequenceCompletion() const {
  if (IsAttached())
    layer_tree_host_->WaitForProtectedSequenceCompletion();
}

bool Layer::IsUsingLayerLists() const {
  return IsAttached() && layer_tree_host_->IsUsingLayerLists();
}

// On<Property>Animated is called due to an ongoing accelerated animation.
// Since this animation is also being run on the compositor thread, there
// is no need to request a commit to push this value over, so the value is
// set directly rather than by calling Set<Property>.
void Layer::OnFilterAnimated(const FilterOperations& filters) {
  EnsureLayerTreeInputs().filters = filters;
}

void Layer::OnBackdropFilterAnimated(const FilterOperations& backdrop_filters) {
  EnsureLayerTreeInputs().backdrop_filters = backdrop_filters;
}

void Layer::OnOpacityAnimated(float opacity) {
  EnsureLayerTreeInputs().opacity = opacity;
}

void Layer::OnTransformAnimated(const gfx::Transform& transform) {
  EnsureLayerTreeInputs().transform = transform;
}

void Layer::SetTrilinearFiltering(bool trilinear_filtering) {
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.trilinear_filtering == trilinear_filtering)
    return;
  inputs.trilinear_filtering = trilinear_filtering;
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
  auto& inputs = EnsureLayerTreeInputs();
  if (inputs.mirror_count == mirror_count)
    return;

  DCHECK_LE(0, mirror_count);
  bool was_mirrored = inputs.mirror_count > 0;
  inputs.mirror_count = mirror_count;
  bool is_mirrored = inputs.mirror_count > 0;
  if (was_mirrored != is_mirrored)
    SetPropertyTreesNeedRebuild();
  SetNeedsPushProperties();
}

ElementListType Layer::GetElementTypeForAnimation() const {
  return ElementListType::ACTIVE;
}

void Layer::AddDrawableDescendants(int num) {
  DCHECK_GE(num_descendants_that_draw_content_.Read(*this), 0);
  DCHECK_GE(num_descendants_that_draw_content_.Read(*this) + num, 0);
  if (num == 0)
    return;
  num_descendants_that_draw_content_.Write(*this) += num;
  SetNeedsCommit();
  if (parent())
    mutable_parent()->AddDrawableDescendants(num);
}

void Layer::RunMicroBenchmark(MicroBenchmark* benchmark) {}

void Layer::SetElementId(ElementId id) {
  DCHECK(IsPropertyChangeAllowed());
  if (inputs_.Read(*this).element_id == id)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "Layer::SetElementId",
               "element", id.ToString());
  auto& inputs = inputs_.Write(*this);
  if (IsAttached() && inputs.element_id)
    layer_tree_host()->UnregisterElement(inputs.element_id);

  inputs.element_id = id;

  if (IsAttached() && inputs.element_id)
    layer_tree_host()->RegisterElement(inputs.element_id, this);

  SetNeedsCommit();
}

gfx::Transform Layer::ScreenSpaceTransform() const {
  DCHECK_NE(transform_tree_index_.Read(*this), kInvalidPropertyNodeId);
  return draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host()->property_trees()->transform_tree());
}

}  // namespace cc
