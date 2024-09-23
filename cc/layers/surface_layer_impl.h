// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_IMPL_H_
#define CC_LAYERS_SURFACE_LAYER_IMPL_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc {

// This must match surface_layer.h's UpdateSubmissionStateCB.
using UpdateSubmissionStateCB =
    base::RepeatingCallback<void(bool is_visible, base::WaitableEvent*)>;

class CC_EXPORT SurfaceLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<SurfaceLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      UpdateSubmissionStateCB update_submission_state_callback);

  static std::unique_ptr<SurfaceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id);

  SurfaceLayerImpl(const SurfaceLayerImpl&) = delete;
  ~SurfaceLayerImpl() override;

  SurfaceLayerImpl& operator=(const SurfaceLayerImpl&) = delete;

  void SetRange(const viz::SurfaceRange& surface_range,
                std::optional<uint32_t> deadline_in_frames);
  const viz::SurfaceRange& range() const { return surface_range_; }

  std::optional<uint32_t> deadline_in_frames() const {
    return deadline_in_frames_;
  }

  void SetStretchContentToFillBounds(bool stretch_content);
  bool stretch_content_to_fill_bounds() const {
    return stretch_content_to_fill_bounds_;
  }

  void SetSurfaceHitTestable(bool surface_hit_testable);
  bool surface_hit_testable() const { return surface_hit_testable_; }

  void SetHasPointerEventsNone(bool has_pointer_events_none);
  bool has_pointer_events_none() const { return has_pointer_events_none_; }

  void SetIsReflection(bool is_reflection);
  bool is_reflection() const { return is_reflection_; }

  void ResetStateForUpdateSubmissionStateCallback();

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;
  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  bool is_surface_layer() const override;
  gfx::Rect GetEnclosingVisibleRectInTargetSpace() const override;

 protected:
  SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id, UpdateSubmissionStateCB);

 private:
  void GetDebugBorderProperties(SkColor4f* color, float* width) const override;
  void AppendRainbowDebugBorder(viz::CompositorRenderPass* render_pass);
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  UpdateSubmissionStateCB update_submission_state_callback_;
  viz::SurfaceRange surface_range_;
  std::optional<uint32_t> deadline_in_frames_;

  bool stretch_content_to_fill_bounds_ = false;
  bool surface_hit_testable_ = false;
  bool has_pointer_events_none_ = false;
  bool is_reflection_ = false;
  bool will_draw_ = false;
  // This value tracks if a visibility reset took place in the associated
  // SurfaceLayer, so that it can be propagated to the active SurfaceLayerImpl
  // and used to update `will_draw_` on that layer accordingly.
  bool will_draw_needs_reset_ = false;
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_IMPL_H_
