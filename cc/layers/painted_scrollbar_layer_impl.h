// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT PaintedScrollbarLayerImpl : public ScrollbarLayerImplBase {
 public:
  static std::unique_ptr<PaintedScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation,
      bool is_left_side_vertical_scrollbar,
      bool is_overlay);
  PaintedScrollbarLayerImpl(const PaintedScrollbarLayerImpl&) = delete;
  ~PaintedScrollbarLayerImpl() override;

  PaintedScrollbarLayerImpl& operator=(const PaintedScrollbarLayerImpl&) =
      delete;

  // LayerImpl implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  gfx::Rect GetEnclosingRectInTargetSpace() const override;

  void SetSupportsDragSnapBack(bool supports_drag_snap_back);
  void SetBackButtonRect(gfx::Rect back_button_rect);
  void SetForwardButtonRect(gfx::Rect forward_button_rect);
  void SetThumbThickness(int thumb_thickness);
  void SetThumbLength(int thumb_length);
  void SetTrackRect(gfx::Rect track_rect);

  void set_track_ui_resource_id(UIResourceId uid) {
    track_ui_resource_id_ = uid;
  }
  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }

  void set_thumb_opacity(float opacity) { thumb_opacity_ = opacity; }

  void set_internal_contents_scale_and_bounds(float content_scale,
                                              const gfx::Size& content_bounds) {
    internal_contents_scale_ = content_scale;
    internal_content_bounds_ = content_bounds;
  }

  bool SupportsDragSnapBack() const override;
  gfx::Rect BackButtonRect() const override;
  gfx::Rect ForwardButtonRect() const override;
  gfx::Rect BackTrackRect() const override;
  gfx::Rect ForwardTrackRect() const override;
  int ThumbThickness() const override;

  LayerTreeSettings::ScrollbarAnimator GetScrollbarAnimator() const override;

 protected:
  PaintedScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                            int id,
                            ScrollbarOrientation orientation,
                            bool is_left_side_vertical_scrollbar,
                            bool is_overlay);

  // ScrollbarLayerImplBase implementation.
  int ThumbLength() const override;
  float TrackLength() const override;
  int TrackStart() const override;
  bool IsThumbResizable() const override;

 private:
  const char* LayerTypeAsString() const override;

  UIResourceId track_ui_resource_id_;
  UIResourceId thumb_ui_resource_id_;

  float thumb_opacity_;

  float internal_contents_scale_;
  gfx::Size internal_content_bounds_;

  bool supports_drag_snap_back_;
  int thumb_thickness_;
  int thumb_length_;
  gfx::Rect back_button_rect_;
  gfx::Rect forward_button_rect_;
  gfx::Rect track_rect_;
};

}  // namespace cc
#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
