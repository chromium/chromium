// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/adapters.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace cc {

LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting::
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      const Layer* page_scale_layer,
                                      const Layer* inner_viewport_scroll_layer,
                                      const Layer* outer_viewport_scroll_layer,
                                      TransformNode* page_scale_transform_node)
    : root_layer(root_layer),
      device_viewport_size(device_viewport_size),
      device_transform(device_transform),
      device_scale_factor(device_scale_factor),
      page_scale_factor(page_scale_factor),
      page_scale_layer(page_scale_layer),
      inner_viewport_scroll_layer(inner_viewport_scroll_layer),
      outer_viewport_scroll_layer(outer_viewport_scroll_layer),
      page_scale_transform_node(page_scale_transform_node) {}

LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting::
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform)
    : CalcDrawPropsMainInputsForTesting(root_layer,
                                        device_viewport_size,
                                        device_transform,
                                        1.f,
                                        1.f,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr) {}

LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting::
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size)
    : CalcDrawPropsMainInputsForTesting(root_layer,
                                        device_viewport_size,
                                        gfx::Transform()) {}

LayerTreeHostCommon::CalcDrawPropsImplInputs::CalcDrawPropsImplInputs(
    LayerImpl* root_layer,
    const gfx::Size& device_viewport_size,
    const gfx::Transform& device_transform,
    float device_scale_factor,
    float page_scale_factor,
    const LayerImpl* page_scale_layer,
    const LayerImpl* inner_viewport_scroll_layer,
    const LayerImpl* outer_viewport_scroll_layer,
    const gfx::Vector2dF& elastic_overscroll,
    const ElementId elastic_overscroll_element_id,
    int max_texture_size,
    bool can_adjust_raster_scales,
    RenderSurfaceList* render_surface_list,
    PropertyTrees* property_trees,
    TransformNode* page_scale_transform_node)
    : root_layer(root_layer),
      device_viewport_size(device_viewport_size),
      device_transform(device_transform),
      device_scale_factor(device_scale_factor),
      page_scale_factor(page_scale_factor),
      page_scale_layer(page_scale_layer),
      inner_viewport_scroll_layer(inner_viewport_scroll_layer),
      outer_viewport_scroll_layer(outer_viewport_scroll_layer),
      elastic_overscroll(elastic_overscroll),
      elastic_overscroll_element_id(elastic_overscroll_element_id),
      max_texture_size(max_texture_size),
      can_adjust_raster_scales(can_adjust_raster_scales),
      render_surface_list(render_surface_list),
      property_trees(property_trees),
      page_scale_transform_node(page_scale_transform_node) {}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      float device_scale_factor,
                                      RenderSurfaceList* render_surface_list)
    : CalcDrawPropsImplInputs(root_layer,
                              device_viewport_size,
                              device_transform,
                              device_scale_factor,
                              1.f,
                              nullptr,
                              nullptr,
                              nullptr,
                              gfx::Vector2dF(),
                              ElementId(),
                              std::numeric_limits<int>::max() / 2,
                              false,
                              render_surface_list,
                              GetPropertyTrees(root_layer),
                              nullptr) {
  DCHECK(root_layer);
  DCHECK(render_surface_list);
}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      RenderSurfaceList* render_surface_list)
    : CalcDrawPropsImplInputsForTesting(root_layer,
                                        device_viewport_size,
                                        device_transform,
                                        1.f,
                                        render_surface_list) {}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      RenderSurfaceList* render_surface_list)
    : CalcDrawPropsImplInputsForTesting(root_layer,
                                        device_viewport_size,
                                        gfx::Transform(),
                                        1.f,
                                        render_surface_list) {}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      float device_scale_factor,
                                      RenderSurfaceList* render_surface_list)
    : CalcDrawPropsImplInputsForTesting(root_layer,
                                        device_viewport_size,
                                        gfx::Transform(),
                                        device_scale_factor,
                                        render_surface_list) {}

bool LayerTreeHostCommon::ScrollUpdateInfo::operator==(
    const LayerTreeHostCommon::ScrollUpdateInfo& other) const {
  return element_id == other.element_id && scroll_delta == other.scroll_delta;
}

LayerTreeHostCommon::ScrollbarsUpdateInfo::ScrollbarsUpdateInfo()
    : element_id(), hidden(true) {}

LayerTreeHostCommon::ScrollbarsUpdateInfo::ScrollbarsUpdateInfo(ElementId id,
                                                                bool hidden)
    : element_id(id), hidden(hidden) {}

bool LayerTreeHostCommon::ScrollbarsUpdateInfo::operator==(
    const LayerTreeHostCommon::ScrollbarsUpdateInfo& other) const {
  return element_id == other.element_id && hidden == other.hidden;
}

ScrollAndScaleSet::ScrollAndScaleSet()
    : page_scale_delta(1.f),
      top_controls_delta(0.f),
      browser_controls_constraint(BrowserControlsState::kBoth),
      browser_controls_constraint_changed(false),
      has_scrolled_by_wheel(false),
      has_scrolled_by_touch(false) {}

ScrollAndScaleSet::~ScrollAndScaleSet() = default;

static inline void SetMaskLayersContributeToDrawnRenderSurface(
    RenderSurfaceImpl* surface,
    PropertyTrees* property_trees) {
  LayerImpl* mask_layer = surface->MaskLayer();
  if (mask_layer) {
    mask_layer->set_contributes_to_drawn_render_surface(true);
    draw_property_utils::ComputeMaskDrawProperties(mask_layer, property_trees);
  }
}

static inline void ClearMaskLayersContributeToDrawnRenderSurface(
    RenderSurfaceImpl* surface) {
  LayerImpl* mask_layer = surface->MaskLayer();
  if (mask_layer)
    mask_layer->set_contributes_to_drawn_render_surface(false);
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
int LayerTreeHostCommon::CalculateLayerJitter(LayerImpl* layer) {
  float jitter = 0.f;
  layer->performance_properties().translation_from_last_frame = 0.f;
  layer->performance_properties().last_commit_screen_space_transform =
      layer->draw_properties().screen_space_transform;

  if (!layer->visible_layer_rect().IsEmpty()) {
    if (layer->draw_properties().screen_space_transform.ApproximatelyEqual(
            layer->performance_properties()
                .last_commit_screen_space_transform)) {
      float translation_from_last_commit =
          TranslationFromActiveTreeLayerScreenSpaceTransform(layer);
      if (translation_from_last_commit > 0.f) {
        layer->performance_properties().num_fixed_point_hits++;
        layer->performance_properties().translation_from_last_frame =
            translation_from_last_commit;
        if (layer->performance_properties().num_fixed_point_hits >
            layer->layer_tree_impl()->kFixedPointHitsThreshold) {
          // Jitter = Translation from fixed point * sqrt(Area of the layer).
          // The square root of the area is used instead of the area to match
          // the dimensions of both terms on the rhs.
          jitter += translation_from_last_commit *
                    sqrt(layer->visible_layer_rect().size().GetArea());
        }
      } else {
        layer->performance_properties().num_fixed_point_hits = 0;
      }
    }
  }
  return jitter;
}

enum PropertyTreeOption { BUILD_PROPERTY_TREES, DONT_BUILD_PROPERTY_TREES };

static void AddSurfaceToRenderSurfaceList(
    RenderSurfaceImpl* render_surface,
    RenderSurfaceList* render_surface_list,
    PropertyTrees* property_trees) {
  // |render_surface| must appear after its target, so first make sure its
  // target is in the list.
  RenderSurfaceImpl* target = render_surface->render_target();
  bool is_root =
      render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
  if (!is_root && !target->is_render_surface_list_member()) {
    AddSurfaceToRenderSurfaceList(target, render_surface_list, property_trees);
  }
  render_surface->ClearAccumulatedContentRect();
  render_surface_list->push_back(render_surface);
  render_surface->set_is_render_surface_list_member(true);
  if (is_root) {
    // The root surface does not contribute to any other surface, it has no
    // target.
    render_surface->set_contributes_to_drawn_surface(false);
  } else {
    bool contributes_to_drawn_surface =
        property_trees->effect_tree.ContributesToDrawnSurface(
            render_surface->EffectTreeIndex());
    render_surface->set_contributes_to_drawn_surface(
        contributes_to_drawn_surface);
  }

  draw_property_utils::ComputeSurfaceDrawProperties(property_trees,
                                                    render_surface);

  // Ignore occlusion from outside the surface when surface contents need to be
  // fully drawn. Layers with copy-request need to be complete.  We could be
  // smarter about layers with filters that move pixels and exclude regions
  // where both layers and the filters are occluded, but this seems like
  // overkill.
  // TODO(senorblanco): make this smarter for the SkImageFilter case (check for
  // pixel-moving filters)
  const FilterOperations& filters = render_surface->Filters();
  bool is_occlusion_immune = render_surface->HasCopyRequest() ||
                             render_surface->ShouldCacheRenderSurface() ||
                             filters.HasReferenceFilter() ||
                             filters.HasFilterThatMovesPixels();
  if (is_occlusion_immune) {
    render_surface->SetNearestOcclusionImmuneAncestor(render_surface);
  } else if (is_root) {
    render_surface->SetNearestOcclusionImmuneAncestor(nullptr);
  } else {
    render_surface->SetNearestOcclusionImmuneAncestor(
        render_surface->render_target()->nearest_occlusion_immune_ancestor());
  }
}

static bool SkipForInvertibility(const LayerImpl* layer,
                                 PropertyTrees* property_trees) {
  const TransformNode* transform_node =
      property_trees->transform_tree.Node(layer->transform_tree_index());
  const EffectNode* effect_node =
      property_trees->effect_tree.Node(layer->effect_tree_index());
  bool non_root_copy_request =
      effect_node->closest_ancestor_with_copy_request_id >
      EffectTree::kContentsRootNodeId;
  gfx::Transform from_target;
  // If there is a copy request, we check the invertibility of the transform
  // between the node corresponding to the layer and the node corresponding to
  // the copy request. Otherwise, we are interested in the invertibility of
  // screen space transform which is already cached on the transform node.
  return non_root_copy_request
             ? !property_trees->GetFromTarget(
                   layer->transform_tree_index(),
                   effect_node->closest_ancestor_with_copy_request_id,
                   &from_target)
             : !transform_node->ancestors_are_invertible;
}

static void ComputeInitialRenderSurfaceList(
    LayerTreeImpl* layer_tree_impl,
    PropertyTrees* property_trees,
    RenderSurfaceList* render_surface_list) {
  EffectTree& effect_tree = property_trees->effect_tree;
  for (int i = EffectTree::kContentsRootNodeId;
       i < static_cast<int>(effect_tree.size()); ++i) {
    if (RenderSurfaceImpl* render_surface = effect_tree.GetRenderSurface(i)) {
      render_surface->set_is_render_surface_list_member(false);
      render_surface->reset_num_contributors();
      ClearMaskLayersContributeToDrawnRenderSurface(render_surface);
    }
  }

  RenderSurfaceImpl* root_surface =
      effect_tree.GetRenderSurface(EffectTree::kContentsRootNodeId);
  // The root surface always gets added to the render surface  list.
  AddSurfaceToRenderSurfaceList(root_surface, render_surface_list,
                                property_trees);
  // For all non-skipped layers, add their target to the render surface list if
  // it's not already been added, and add their content rect to the target
  // surface's accumulated content rect.
  for (LayerImpl* layer : *layer_tree_impl) {
    DCHECK(layer);
    layer->EnsureValidPropertyTreeIndices();

    layer->set_contributes_to_drawn_render_surface(false);
    layer->set_raster_even_if_not_drawn(false);

    bool is_root = layer_tree_impl->IsRootLayer(layer);

    bool skip_draw_properties_computation =
        draw_property_utils::LayerShouldBeSkippedForDrawPropertiesComputation(
            layer, property_trees->transform_tree, property_trees->effect_tree);

    bool skip_for_invertibility = SkipForInvertibility(layer, property_trees);

    bool skip_layer = !is_root && (skip_draw_properties_computation ||
                                   skip_for_invertibility);

    layer->set_raster_even_if_not_drawn(skip_for_invertibility &&
                                        !skip_draw_properties_computation);
    if (skip_layer)
      continue;

    bool layer_is_drawn =
        property_trees->effect_tree.Node(layer->effect_tree_index())->is_drawn;
    bool layer_should_be_drawn = draw_property_utils::LayerNeedsUpdate(
        layer, layer_is_drawn, property_trees);
    if (!layer_should_be_drawn)
      continue;

    RenderSurfaceImpl* render_target = layer->render_target();
    if (!render_target->is_render_surface_list_member()) {
      AddSurfaceToRenderSurfaceList(render_target, render_surface_list,
                                    property_trees);
    }

    layer->set_contributes_to_drawn_render_surface(true);

    // The layer contributes its drawable content rect to its render target.
    render_target->AccumulateContentRectFromContributingLayer(layer);
    render_target->increment_num_contributors();
  }
}

static void ComputeSurfaceContentRects(PropertyTrees* property_trees,
                                       RenderSurfaceList* render_surface_list,
                                       int max_texture_size) {
  // Walk the list backwards, accumulating each surface's content rect into its
  // target's content rect.
  for (RenderSurfaceImpl* render_surface :
       base::Reversed(*render_surface_list)) {
    if (render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId) {
      // The root surface's content rect is always the entire viewport.
      render_surface->SetContentRectToViewport();
      continue;
    }

    // Now all contributing drawable content rect has been accumulated to this
    // render surface, calculate the content rect.
    render_surface->CalculateContentRectFromAccumulatedContentRect(
        max_texture_size);

    // Now the render surface's content rect is calculated correctly, it could
    // contribute to its render target.
    RenderSurfaceImpl* render_target = render_surface->render_target();
    DCHECK(render_target->is_render_surface_list_member());
    render_target->AccumulateContentRectFromContributingRenderSurface(
        render_surface);
    render_target->increment_num_contributors();
  }
}

static void ComputeListOfNonEmptySurfaces(
    LayerTreeImpl* layer_tree_impl,
    PropertyTrees* property_trees,
    RenderSurfaceList* initial_surface_list,
    RenderSurfaceList* final_surface_list) {
  // Walk the initial surface list forwards. The root surface and each
  // surface with a non-empty content rect go into the final render surface
  // layer list. Surfaces with empty content rects or whose target isn't in
  // the final list do not get added to the final list.
  bool removed_surface = false;
  for (RenderSurfaceImpl* surface : *initial_surface_list) {
    bool is_root =
        surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
    RenderSurfaceImpl* target_surface = surface->render_target();
    if (!is_root && (surface->content_rect().IsEmpty() ||
                     !target_surface->is_render_surface_list_member())) {
      surface->set_is_render_surface_list_member(false);
      removed_surface = true;
      target_surface->decrement_num_contributors();
      continue;
    }
    SetMaskLayersContributeToDrawnRenderSurface(surface, property_trees);
    final_surface_list->push_back(surface);
  }
  if (removed_surface) {
    for (LayerImpl* layer : *layer_tree_impl) {
      if (layer->contributes_to_drawn_render_surface()) {
        RenderSurfaceImpl* render_target = layer->render_target();
        if (!render_target->is_render_surface_list_member()) {
          layer->set_contributes_to_drawn_render_surface(false);
          render_target->decrement_num_contributors();
        }
      }
    }
  }
}

static void CalculateRenderSurfaceLayerList(
    LayerTreeImpl* layer_tree_impl,
    PropertyTrees* property_trees,
    RenderSurfaceList* render_surface_list,
    const int max_texture_size) {
  RenderSurfaceList initial_render_surface_list;

  // First compute a list that might include surfaces that later turn out to
  // have an empty content rect. After surface content rects are computed,
  // produce a final list that omits empty surfaces.
  ComputeInitialRenderSurfaceList(layer_tree_impl, property_trees,
                                  &initial_render_surface_list);
  ComputeSurfaceContentRects(property_trees, &initial_render_surface_list,
                             max_texture_size);
  ComputeListOfNonEmptySurfaces(layer_tree_impl, property_trees,
                                &initial_render_surface_list,
                                render_surface_list);
}

void CalculateDrawPropertiesInternal(
    LayerTreeHostCommon::CalcDrawPropsImplInputs* inputs,
    PropertyTreeOption property_tree_option) {
  inputs->render_surface_list->clear();

  LayerImplList visible_layer_list;
  switch (property_tree_option) {
    case BUILD_PROPERTY_TREES: {
      // The translation from layer to property trees is an intermediate
      // state. We will eventually get these data passed directly to the
      // compositor.
      PropertyTreeBuilder::BuildPropertyTrees(
          inputs->root_layer, inputs->page_scale_layer,
          inputs->inner_viewport_scroll_layer,
          inputs->outer_viewport_scroll_layer,
          inputs->elastic_overscroll_element_id, inputs->elastic_overscroll,
          inputs->page_scale_factor, inputs->device_scale_factor,
          gfx::Rect(inputs->device_viewport_size), inputs->device_transform,
          inputs->property_trees);
      draw_property_utils::UpdatePropertyTreesAndRenderSurfaces(
          inputs->root_layer, inputs->property_trees,
          inputs->can_adjust_raster_scales);

      // Property trees are normally constructed on the main thread and
      // passed to compositor thread. Source to parent updates on them are not
      // allowed in the compositor thread. Some tests build them on the
      // compositor thread, so we need to explicitly disallow source to parent
      // updates when they are built on compositor thread.
      inputs->property_trees->transform_tree
          .set_source_to_parent_updates_allowed(false);
      break;
    }
    case DONT_BUILD_PROPERTY_TREES: {
      // Since page scale and elastic overscroll are SyncedProperties, changes
      // on the active tree immediately affect the pending tree, so instead of
      // trying to update property trees whenever these values change, we
      // update property trees before using them.

      // When the page scale layer is also the root layer, the node should also
      // store the combined scale factor and not just the page scale factor.
      // TODO(bokan): Need to implement this behavior for
      // BlinkGeneratedPropertyTrees. i.e. (no page scale layer). Ideally by
      // not baking these into the page scale layer.
      bool combine_dsf_and_psf = inputs->page_scale_layer == inputs->root_layer;
      float device_scale_factor_for_page_scale_node = 1.f;
      gfx::Transform device_transform_for_page_scale_node;
      if (combine_dsf_and_psf) {
        DCHECK(
            !inputs->root_layer->layer_tree_impl()->settings().use_layer_lists);
        device_transform_for_page_scale_node = inputs->device_transform;
        device_scale_factor_for_page_scale_node = inputs->device_scale_factor;
      }

      // We should never be setting a non-unit page scale factor on an oopif
      // subframe ... if we attempt this log it and fail.
      // TODO(wjmaclean): Remove as part of conditions for closing the bug.
      // https://crbug.com/845097
      if (inputs->page_scale_factor !=
              inputs->property_trees->transform_tree.page_scale_factor() &&
          !inputs->page_scale_transform_node) {
        LOG(ERROR) << "Setting PageScale on subframe: new psf = "
                   << inputs->page_scale_factor << ", old psf = "
                   << inputs->property_trees->transform_tree.page_scale_factor()
                   << ", in_oopif = "
                   << inputs->root_layer->layer_tree_impl()
                          ->settings()
                          .is_layer_tree_for_subframe;
        NOTREACHED();
      }

      draw_property_utils::UpdatePageScaleFactor(
          inputs->property_trees, inputs->page_scale_transform_node,
          inputs->page_scale_factor, device_scale_factor_for_page_scale_node,
          device_transform_for_page_scale_node);
      draw_property_utils::UpdateElasticOverscroll(
          inputs->property_trees, inputs->elastic_overscroll_element_id,
          inputs->elastic_overscroll);
      // Similarly, the device viewport and device transform are shared
      // by both trees.
      PropertyTrees* property_trees = inputs->property_trees;
      property_trees->clip_tree.SetViewportClip(
          gfx::RectF(gfx::SizeF(inputs->device_viewport_size)));
      float page_scale_factor_for_root =
          combine_dsf_and_psf ? inputs->page_scale_factor : 1.f;
      property_trees->transform_tree.SetRootTransformsAndScales(
          inputs->device_scale_factor, page_scale_factor_for_root,
          inputs->device_transform, inputs->root_layer->position());
      draw_property_utils::UpdatePropertyTreesAndRenderSurfaces(
          inputs->root_layer, inputs->property_trees,
          inputs->can_adjust_raster_scales);
      break;
    }
  }

  {
    TRACE_EVENT0("cc", "draw_property_utils::FindLayersThatNeedUpdates");
    draw_property_utils::FindLayersThatNeedUpdates(
        inputs->root_layer->layer_tree_impl(), inputs->property_trees,
        &visible_layer_list);
  }

  {
    TRACE_EVENT1("cc",
                 "draw_property_utils::ComputeDrawPropertiesOfVisibleLayers",
                 "visible_layers", visible_layer_list.size());
    draw_property_utils::ComputeDrawPropertiesOfVisibleLayers(
        &visible_layer_list, inputs->property_trees);
  }

  {
    TRACE_EVENT0("cc", "CalculateRenderSurfaceLayerList");
    CalculateRenderSurfaceLayerList(
        inputs->root_layer->layer_tree_impl(), inputs->property_trees,
        inputs->render_surface_list, inputs->max_texture_size);
  }

  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(inputs->property_trees->effect_tree.GetRenderSurface(
      EffectTree::kContentsRootNodeId));
}

void LayerTreeHostCommon::CalculateDrawPropertiesForTesting(
    CalcDrawPropsMainInputsForTesting* inputs) {
  LayerList update_layer_list;
  PropertyTrees* property_trees =
      inputs->root_layer->layer_tree_host()->property_trees();
  gfx::Vector2dF elastic_overscroll;
  PropertyTreeBuilder::BuildPropertyTrees(
      inputs->root_layer, inputs->page_scale_layer,
      inputs->inner_viewport_scroll_layer, inputs->outer_viewport_scroll_layer,
      ElementId(), elastic_overscroll, inputs->page_scale_factor,
      inputs->device_scale_factor, gfx::Rect(inputs->device_viewport_size),
      inputs->device_transform, property_trees);
  draw_property_utils::UpdatePropertyTrees(
      inputs->root_layer->layer_tree_host(), property_trees);
  draw_property_utils::FindLayersThatNeedUpdates(
      inputs->root_layer->layer_tree_host(), property_trees,
      &update_layer_list);
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsImplInputs* inputs) {
  CalculateDrawPropertiesInternal(inputs, DONT_BUILD_PROPERTY_TREES);
}

void LayerTreeHostCommon::CalculateDrawPropertiesForTesting(
    CalcDrawPropsImplInputsForTesting* inputs) {
  CalculateDrawPropertiesInternal(inputs, inputs->property_trees->needs_rebuild
                                              ? BUILD_PROPERTY_TREES
                                              : DONT_BUILD_PROPERTY_TREES);
}

PropertyTrees* GetPropertyTrees(Layer* layer) {
  return layer->layer_tree_host()->property_trees();
}

PropertyTrees* GetPropertyTrees(LayerImpl* layer) {
  return layer->layer_tree_impl()->property_trees();
}

}  // namespace cc
