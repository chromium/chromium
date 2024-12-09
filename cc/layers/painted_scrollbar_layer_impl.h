// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_

#include <memory>
#include <vector>

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/nine_patch_generator.h"
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
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  gfx::Rect GetEnclosingVisibleRectInTargetSpace() const override;
  gfx::Rect ComputeThumbQuadRect() const override;
  gfx::Rect ComputeHitTestableThumbQuadRect() const override;
  gfx::Rect ComputeHitTestableExpandedThumbQuadRect() const override;

  void SetJumpOnTrackClick(bool jump_on_track_click);
  void SetSupportsDragSnapBack(bool supports_drag_snap_back);
  void SetBackButtonRect(gfx::Rect back_button_rect);
  void SetForwardButtonRect(gfx::Rect forward_button_rect);
  void SetThumbThickness(int thumb_thickness);
  void SetThumbLength(int thumb_length);
  void SetTrackRect(gfx::Rect track_rect);
  void SetScrollbarPaintedOpacity(float opacity);
  void SetThumbColor(SkColor4f thumb_color);
  void SetTrackAndButtonsImageBounds(const gfx::Size& bounds);
  void SetTrackAndButtonsAperture(const gfx::Rect& aperture);

  void set_uses_nine_patch_track_and_buttons(bool uses_nine_patch) {
    uses_nine_patch_track_and_buttons_ = uses_nine_patch;
  }
  void set_track_and_buttons_ui_resource_id(UIResourceId uid) {
    track_and_buttons_ui_resource_id_ = uid;
  }
  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }
  float OverlayScrollbarOpacity() const override;

  void set_internal_contents_scale_and_bounds(float content_scale,
                                              const gfx::Size& content_bounds) {
    internal_contents_scale_ = content_scale;
    internal_content_bounds_ = content_bounds;
  }

  bool JumpOnTrackClick() const override;
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
  void AppendThumbQuads(viz::CompositorRenderPass* render_pass,
                        AppendQuadsData* append_quads_data) const;
  void AppendTrackAndButtonsQuads(viz::CompositorRenderPass* render_pass,
                                  AppendQuadsData* append_quads_data);
  void AppendNinePatchScaledTrackAndButtons(
      viz::CompositorRenderPass* render_pass,
      viz::SharedQuadState* shared_quad_state,
      gfx::Rect& track_and_buttons_quad_rect);
  // Expand the scrollbar thumb's hit testable rect to be able to capture the
  // thumb across the entire width of the track rect.
  gfx::Rect ExpandSolidColorThumb(gfx::Rect thumb_rect) const;
  // Position composited scrollbar thumb in the center of the track.
  gfx::Rect CenterSolidColorThumb(gfx::Rect thumb_rect) const;

  UIResourceId track_and_buttons_ui_resource_id_ = 0;
  UIResourceId thumb_ui_resource_id_ = 0;

  // This is relevant in case of Mac overlay scrollbars because they fade out by
  // animating the opacity via Blink paint.
  float painted_opacity_ = 1.f;

  float internal_contents_scale_ = 1.f;
  gfx::Size internal_content_bounds_;

  bool jump_on_track_click_ = false;
  bool supports_drag_snap_back_ = false;
  int thumb_thickness_ = 0;
  int thumb_length_ = 0;
  gfx::Rect back_button_rect_;
  gfx::Rect forward_button_rect_;
  gfx::Rect track_rect_;
  std::optional<SkColor4f> thumb_color_;

  bool uses_nine_patch_track_and_buttons_ = false;
  gfx::Size track_and_buttons_image_bounds_;
  gfx::Rect track_and_buttons_aperture_;
  NinePatchGenerator track_and_buttons_patch_generator_;
  std::vector<NinePatchGenerator::Patch> track_and_buttons_patches_;
};

}  // namespace cc
#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
