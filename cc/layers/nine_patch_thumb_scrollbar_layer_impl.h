// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_IMPL_H_

#include <memory>
#include <vector>

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/nine_patch_generator.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT NinePatchThumbScrollbarLayerImpl
    : public ScrollbarLayerImplBase {
 public:
  static std::unique_ptr<NinePatchThumbScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation,
      bool is_left_side_vertical_scrollbar);
  NinePatchThumbScrollbarLayerImpl(const NinePatchThumbScrollbarLayerImpl&) =
      delete;
  NinePatchThumbScrollbarLayerImpl& operator=(
      const NinePatchThumbScrollbarLayerImpl&) = delete;
  ~NinePatchThumbScrollbarLayerImpl() override;

  // LayerImpl implementation.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void SetThumbThickness(int32_t thumb_thickness);
  void SetMinimumThumbLength(int32_t minimum_thumb_length);
  void SetTrackStart(int32_t track_start);
  void SetTrackLength(int32_t track_length);

  int32_t thumb_thickness() const { return thumb_thickness_; }
  int32_t minimum_thumb_length() const { return minimum_thumb_length_; }
  int32_t track_start() const { return track_start_; }
  int32_t track_length() const { return track_length_; }

  void SetImageBounds(const gfx::Size& bounds);
  void SetAperture(const gfx::Rect& aperture);

  const gfx::Size& image_bounds() const { return image_bounds_; }
  const gfx::Rect& aperture() const { return aperture_; }

  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }

  void set_track_and_buttons_ui_resource_id(UIResourceId uid) {
    track_and_buttons_ui_resource_id_ = uid;
  }

  UIResourceId thumb_ui_resource_id() const { return thumb_ui_resource_id_; }

  UIResourceId track_and_buttons_ui_resource_id() const {
    return track_and_buttons_ui_resource_id_;
  }

 protected:
  NinePatchThumbScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                                   int id,
                                   ScrollbarOrientation orientation,
                                   bool is_left_side_vertical_scrollbar);

  // ScrollbarLayerImplBase implementation.
  int32_t ThumbThickness() const override;
  int32_t MinimumThumbLength() const override;
  float TrackLength() const override;
  int32_t TrackStart() const override;
  bool IsThumbResizable() const override;

 private:
  void AppendThumbQuads(viz::CompositorRenderPass* render_pass,
                        AppendQuadsData* append_quads_data,
                        viz::SharedQuadState* shared_quad_state);

  void AppendTrackAndButtonsQuads(viz::CompositorRenderPass* render_pass,
                                  AppendQuadsData* append_quads_data,
                                  viz::SharedQuadState* shared_quad_state);

  UIResourceId thumb_ui_resource_id_ = 0;
  UIResourceId track_and_buttons_ui_resource_id_ = 0;

  int32_t thumb_thickness_ = 0;
  int32_t minimum_thumb_length_ = 0;
  int32_t track_start_ = 0;
  int32_t track_length_ = 0;

  gfx::Size image_bounds_;
  gfx::Rect aperture_;

  NinePatchGenerator quad_generator_;
  std::vector<NinePatchGenerator::Patch> patches_;
};

}  // namespace cc
#endif  // CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_IMPL_H_
