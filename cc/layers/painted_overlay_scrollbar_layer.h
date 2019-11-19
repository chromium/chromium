// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_H_
#define CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

// For composited overlay scrollbars with nine-patch thumb. For overlay
// scrollbars whose thumb is not nine-patch, use PaintedScrollbarLayer.
class CC_EXPORT PaintedOverlayScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  PaintedOverlayScrollbarLayer(const PaintedOverlayScrollbarLayer&) = delete;
  PaintedOverlayScrollbarLayer& operator=(const PaintedOverlayScrollbarLayer&) =
      delete;
  static scoped_refptr<PaintedOverlayScrollbarLayer> Create(
      scoped_refptr<Scrollbar> scrollbar);

  bool OpacityCanAnimateOnImplThread() const override;
  bool Update() override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer) override;

 protected:
  explicit PaintedOverlayScrollbarLayer(scoped_refptr<Scrollbar> scrollbar);
  ~PaintedOverlayScrollbarLayer() override;

 private:
  template <typename T>
  bool UpdateProperty(T value, T* prop) {
    if (*prop == value)
      return false;
    *prop = value;
    SetNeedsPushProperties();
    return true;
  }

  bool PaintThumbIfNeeded();
  bool PaintTickmarks();

  scoped_refptr<Scrollbar> scrollbar_;

  gfx::Size thumb_size_;
  gfx::Rect track_rect_;
  gfx::Rect aperture_;

  const ScrollbarOrientation orientation_;
  const bool is_left_side_vertical_scrollbar_;

  std::unique_ptr<ScopedUIResource> thumb_resource_;
  std::unique_ptr<ScopedUIResource> track_resource_;
};

}  // namespace cc

#endif  // CC_LAYERS_PAINTED_OVERLAY_SCROLLBAR_LAYER_H_
