// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

// Generic scrollbar layer for cases not covered by PaintedOverlayScrollbarLayer
// or SolidColorScrollbarLayer. This is not used for CSS-styled scrollbars. In
// practice, this is used for overlay and non-overlay scrollbars on MacOS, as
// well as non-overlay scrollbars on Win/Linux.
class CC_EXPORT PaintedScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  static scoped_refptr<PaintedScrollbarLayer> CreateOrReuse(
      scoped_refptr<Scrollbar> scrollbar,
      PaintedScrollbarLayer* existing_layer);
  static scoped_refptr<PaintedScrollbarLayer> Create(
      scoped_refptr<Scrollbar> scrollbar);

  PaintedScrollbarLayer(const PaintedScrollbarLayer&) = delete;
  PaintedScrollbarLayer& operator=(const PaintedScrollbarLayer&) = delete;

  bool OpacityCanAnimateOnImplThread() const override;
  bool Update() override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  const gfx::Size& internal_content_bounds() const {
    return internal_content_bounds_;
  }

  ScrollbarLayerType GetScrollbarLayerType() const override;

 protected:
  explicit PaintedScrollbarLayer(scoped_refptr<Scrollbar> scrollbar);
  ~PaintedScrollbarLayer() override;

  // For unit tests
  UIResourceId track_resource_id() {
    return track_resource_.get() ? track_resource_->id() : 0;
  }
  UIResourceId thumb_resource_id() {
    return thumb_resource_.get() ? thumb_resource_->id() : 0;
  }
  bool UpdateInternalContentScale();
  bool UpdateThumbAndTrackGeometry();

 private:
  gfx::Size LayerSizeToContentSize(const gfx::Size& layer_size) const;

  template <typename T>
  bool UpdateProperty(T value, T* prop) {
    if (*prop == value)
      return false;
    *prop = value;
    SetNeedsPushProperties();
    return true;
  }

  UIResourceBitmap RasterizeScrollbarPart(
      const gfx::Size& size,
      const gfx::Size& requested_content_size,
      ScrollbarPart part);

  scoped_refptr<Scrollbar> scrollbar_;
  ElementId scroll_element_id_;

  float internal_contents_scale_;
  gfx::Size internal_content_bounds_;

  // Snapshot of properties taken in UpdateThumbAndTrackGeometry and used in
  // PushPropertiesTo.
  gfx::Size thumb_size_;
  gfx::Rect track_rect_;
  gfx::Rect back_button_rect_;
  gfx::Rect forward_button_rect_;
  float painted_opacity_;
  bool has_thumb_;
  bool jump_on_track_click_;

  const bool supports_drag_snap_back_;
  const bool is_overlay_;

  std::unique_ptr<ScopedUIResource> track_resource_;
  std::unique_ptr<ScopedUIResource> thumb_resource_;
};

}  // namespace cc

#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
