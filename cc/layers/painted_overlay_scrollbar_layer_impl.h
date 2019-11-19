// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/nine_patch_generator.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT PaintedOverlayScrollbarLayerImpl
    : public ScrollbarLayerImplBase {
 public:
  static std::unique_ptr<PaintedOverlayScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation,
      bool is_left_side_vertical_scrollbar);
  PaintedOverlayScrollbarLayerImpl(const PaintedOverlayScrollbarLayerImpl&) =
      delete;
  PaintedOverlayScrollbarLayerImpl& operator=(
      const PaintedOverlayScrollbarLayerImpl&) = delete;
  ~PaintedOverlayScrollbarLayerImpl() override;

  // LayerImpl implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void SetThumbThickness(int thumb_thickness);
  void SetThumbLength(int thumb_length);
  void SetTrackStart(int track_start);
  void SetTrackLength(int track_length);

  void SetImageBounds(const gfx::Size& bounds);
  void SetAperture(const gfx::Rect& aperture);

  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }

  void set_track_ui_resource_id(UIResourceId uid) {
    track_ui_resource_id_ = uid;
  }

  bool HasFindInPageTickmarks() const override;

 protected:
  PaintedOverlayScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                                   int id,
                                   ScrollbarOrientation orientation,
                                   bool is_left_side_vertical_scrollbar);

  // ScrollbarLayerImplBase implementation.
  int ThumbThickness() const override;
  int ThumbLength() const override;
  float TrackLength() const override;
  int TrackStart() const override;
  bool IsThumbResizable() const override;

 private:
  const char* LayerTypeAsString() const override;

  void AppendThumbQuads(viz::RenderPass* render_pass,
                        AppendQuadsData* append_quads_data,
                        viz::SharedQuadState* shared_quad_state);

  void AppendTrackQuads(viz::RenderPass* render_pass,
                        AppendQuadsData* append_quads_data,
                        viz::SharedQuadState* shared_quad_state);

  UIResourceId thumb_ui_resource_id_;
  UIResourceId track_ui_resource_id_;

  int thumb_thickness_;
  int thumb_length_;
  int track_start_;
  int track_length_;

  gfx::Size image_bounds_;
  gfx::Rect aperture_;

  NinePatchGenerator quad_generator_;
};

}  // namespace cc
#endif  // CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_IMPL_H_
