// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer_impl.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

NinePatchLayerImpl::NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id)
    : UIResourceLayerImpl(tree_impl, id) {}

NinePatchLayerImpl::~NinePatchLayerImpl() = default;

mojom::LayerType NinePatchLayerImpl::GetLayerType() const {
  return mojom::LayerType::kNinePatch;
}

std::unique_ptr<LayerImpl> NinePatchLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return NinePatchLayerImpl::Create(tree_impl, id());
}

void NinePatchLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  UIResourceLayerImpl::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  layer_impl->quad_generator_ = this->quad_generator_;
}

void NinePatchLayerImpl::SetLayout(const gfx::Rect& aperture,
                                   const gfx::Rect& border,
                                   const gfx::Rect& layer_occlusion,
                                   bool fill_center,
                                   bool nearest_neighbor) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetLayout can be called.
  DCHECK(ui_resource_id_);

  if (!quad_generator_.SetLayout(image_bounds_, bounds(), aperture, border,
                                 layer_occlusion, fill_center,
                                 nearest_neighbor))
    return;

  NoteLayerPropertyChanged();
}

void NinePatchLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                     AppendQuadsData* append_quads_data) {
  DCHECK(!bounds().IsEmpty());
  quad_generator_.CheckGeometryLimitations();

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  bool is_resource =
      ui_resource_id_ &&
      layer_tree_impl()->ResourceIdForUIResource(ui_resource_id_);
  bool are_contents_opaque =
      is_resource ? layer_tree_impl()->IsUIResourceOpaque(ui_resource_id_) ||
                        contents_opaque()
                  : false;
  PopulateSharedQuadState(shared_quad_state, are_contents_opaque);
  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  if (!is_resource)
    return;

  std::vector<NinePatchGenerator::Patch> patches =
      quad_generator_.GeneratePatches();
  quad_generator_.AppendQuadsForCc(this, ui_resource_id_, render_pass,
                                   shared_quad_state, patches);
}

void NinePatchLayerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  LayerImpl::AsValueInto(state);
  quad_generator_.AsValueInto(state);
}

}  // namespace cc
