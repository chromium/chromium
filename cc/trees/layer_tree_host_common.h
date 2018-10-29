// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_COMMON_H_
#define CC_TREES_LAYER_TREE_HOST_COMMON_H_

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"

namespace cc {

class LayerImpl;
class Layer;
class SwapPromise;
class PropertyTrees;

class CC_EXPORT LayerTreeHostCommon {
 public:
  struct CC_EXPORT CalcDrawPropsMainInputsForTesting {
   public:
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      const Layer* page_scale_layer,
                                      const Layer* inner_viewport_scroll_layer,
                                      const Layer* outer_viewport_scroll_layer,
                                      TransformNode* page_scale_transform_node);
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform);
    CalcDrawPropsMainInputsForTesting(Layer* root_layer,
                                      const gfx::Size& device_viewport_size);
    Layer* root_layer;
    gfx::Size device_viewport_size;
    gfx::Transform device_transform;
    float device_scale_factor;
    float page_scale_factor;
    const Layer* page_scale_layer;
    const Layer* inner_viewport_scroll_layer;
    const Layer* outer_viewport_scroll_layer;
    TransformNode* page_scale_transform_node;
  };

  struct CC_EXPORT CalcDrawPropsImplInputs {
   public:
    CalcDrawPropsImplInputs(LayerImpl* root_layer,
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
                            TransformNode* page_scale_transform_node);

    LayerImpl* root_layer;
    gfx::Size device_viewport_size;
    gfx::Transform device_transform;
    float device_scale_factor;
    float page_scale_factor;
    const LayerImpl* page_scale_layer;
    const LayerImpl* inner_viewport_scroll_layer;
    const LayerImpl* outer_viewport_scroll_layer;
    gfx::Vector2dF elastic_overscroll;
    const ElementId elastic_overscroll_element_id;
    int max_texture_size;
    bool can_adjust_raster_scales;
    RenderSurfaceList* render_surface_list;
    PropertyTrees* property_trees;
    TransformNode* page_scale_transform_node;
  };

  struct CC_EXPORT CalcDrawPropsImplInputsForTesting
      : public CalcDrawPropsImplInputs {
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      float device_scale_factor,
                                      RenderSurfaceList* render_surface_list);
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      RenderSurfaceList* render_surface_list);
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      RenderSurfaceList* render_surface_list);
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      float device_scale_factor,
                                      RenderSurfaceList* render_surface_list);
  };

  static int CalculateLayerJitter(LayerImpl* scrolling_layer);
  static void CalculateDrawPropertiesForTesting(
      CalcDrawPropsMainInputsForTesting* inputs);

  static void CalculateDrawProperties(CalcDrawPropsImplInputs* inputs);
  static void CalculateDrawPropertiesForTesting(
      CalcDrawPropsImplInputsForTesting* inputs);

  template <typename Function>
  static void CallFunctionForEveryLayer(LayerTreeHost* host,
                                        const Function& function);

  template <typename Function>
  static void CallFunctionForEveryLayer(LayerTreeImpl* layer,
                                        const Function& function);

  struct CC_EXPORT ScrollUpdateInfo {
    ElementId element_id;
    gfx::ScrollOffset scroll_delta;

    bool operator==(const ScrollUpdateInfo& other) const;
  };

  // Used to communicate scrollbar visibility from Impl thread to Blink.
  // Scrollbar input is handled by Blink but the compositor thread animates
  // opacity on scrollbars to fade them out when they're overlay. Blink needs
  // to be told when they're faded out so it can stop handling input for
  // invisible scrollbars.
  struct CC_EXPORT ScrollbarsUpdateInfo {
    ElementId element_id;
    bool hidden;

    ScrollbarsUpdateInfo();
    ScrollbarsUpdateInfo(ElementId element_id, bool hidden);

    bool operator==(const ScrollbarsUpdateInfo& other) const;
  };
};

struct CC_EXPORT ScrollAndScaleSet {
  ScrollAndScaleSet();
  ~ScrollAndScaleSet();

  // The inner viewport scroll delta is kept separate since it's special.
  // Because the inner (visual) viewport's maximum offset depends on the
  // current page scale, the two must be committed at the same time to prevent
  // clamping.
  LayerTreeHostCommon::ScrollUpdateInfo inner_viewport_scroll;

  std::vector<LayerTreeHostCommon::ScrollUpdateInfo> scrolls;
  float page_scale_delta;
  gfx::Vector2dF elastic_overscroll_delta;
  float top_controls_delta;
  std::vector<LayerTreeHostCommon::ScrollbarsUpdateInfo> scrollbars;
  std::vector<std::unique_ptr<SwapPromise>> swap_promises;
  BrowserControlsState browser_controls_constraint;
  bool browser_controls_constraint_changed;
  bool has_scrolled_by_wheel;
  bool has_scrolled_by_touch;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollAndScaleSet);
};

template <typename Function>
void LayerTreeHostCommon::CallFunctionForEveryLayer(LayerTreeHost* host,
                                                    const Function& function) {
  for (auto* layer : *host) {
    function(layer);
    if (PictureLayer* mask_layer = layer->mask_layer())
      function(mask_layer);
  }
}

template <typename Function>
void LayerTreeHostCommon::CallFunctionForEveryLayer(LayerTreeImpl* tree_impl,
                                                    const Function& function) {
  for (auto* layer : *tree_impl)
    function(layer);

  for (int id : tree_impl->property_trees()->effect_tree.mask_layer_ids()) {
    function(tree_impl->LayerById(id));
  }
}

CC_EXPORT PropertyTrees* GetPropertyTrees(Layer* layer);
CC_EXPORT PropertyTrees* GetPropertyTrees(LayerImpl* layer);

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_COMMON_H_
