// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_

#include <memory>

#include "base/functional/function_ref.h"
#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

// Generic scrollbar layer for cases not covered by NinPatchThumbScrollbarLayer
// or SolidColorScrollbarLayer. This is not used for legacy webkit custom
// scrollbars. In practice, this is used for
// - overlay and non-overlay scrollbars on MacOS,
// - non-overlay aura scrollbars on non-MacOS desktop platforms,
// - overlay and non-overlay fluent scrollbars on non-MacOS desktop platforms.
class CC_EXPORT PaintedScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

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
    return internal_content_bounds_.Read(*this);
  }

  ScrollbarLayerType GetScrollbarLayerType() const override;

 protected:
  explicit PaintedScrollbarLayer(scoped_refptr<Scrollbar> scrollbar);
  ~PaintedScrollbarLayer() override;

  // For unit tests
  UIResourceId track_and_buttons_resource_id() {
    if (const auto* resource = track_and_buttons_resource_.Read(*this)) {
      return resource->id();
    }
    return 0;
  }
  UIResourceId thumb_resource_id() {
    if (const auto* thumb_resource = thumb_resource_.Read(*this))
      return thumb_resource->id();
    return 0;
  }
  bool UpdateInternalContentScale();
  bool UpdateGeometry();

 private:
  gfx::Size LayerSizeToContentSize(const gfx::Size& layer_size) const;
  bool UpdateThumbIfNeeded();
  bool UpdateTrackAndButtonsIfNeeded();

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
      base::FunctionRef<void(PaintCanvas&)> paint_function);

  ProtectedSequenceForbidden<scoped_refptr<Scrollbar>> scrollbar_;
  ProtectedSequenceReadable<ElementId> scroll_element_id_;

  ProtectedSequenceReadable<float> internal_contents_scale_;
  ProtectedSequenceReadable<gfx::Size> internal_content_bounds_;

  // Snapshot of properties taken in UpdateGeometry and used in
  // PushPropertiesTo.
  ProtectedSequenceReadable<gfx::Size> thumb_size_;
  ProtectedSequenceReadable<gfx::Rect> track_rect_;
  ProtectedSequenceReadable<gfx::Rect> back_button_rect_;
  ProtectedSequenceReadable<gfx::Rect> forward_button_rect_;
  ProtectedSequenceReadable<float> painted_opacity_;
  ProtectedSequenceReadable<bool> has_thumb_;
  ProtectedSequenceReadable<bool> jump_on_track_click_;
  ProtectedSequenceReadable<std::optional<SkColor4f>> thumb_color_;
  ProtectedSequenceReadable<gfx::Rect> track_and_buttons_aperture_;

  const bool supports_drag_snap_back_;
  const bool is_overlay_;
  const bool is_web_test_;
  const bool uses_nine_patch_track_and_buttons_;
  const bool uses_solid_color_thumb_;

  ProtectedSequenceReadable<std::unique_ptr<ScopedUIResource>>
      track_and_buttons_resource_;
  ProtectedSequenceReadable<std::unique_ptr<ScopedUIResource>> thumb_resource_;
};

}  // namespace cc

#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
