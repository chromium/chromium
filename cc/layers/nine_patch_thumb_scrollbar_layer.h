// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_H_
#define CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

// For composited scrollbars with nine-patch thumb. The track is not painted
// unless there are tick marks. In practice, this is used for non-custom
// overlay aura scrollbars on non-Mac desktop platforms.
class CC_EXPORT NinePatchThumbScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  static scoped_refptr<NinePatchThumbScrollbarLayer> CreateOrReuse(
      scoped_refptr<Scrollbar> scrollbar,
      NinePatchThumbScrollbarLayer* existing_layer);
  static scoped_refptr<NinePatchThumbScrollbarLayer> Create(
      scoped_refptr<Scrollbar> scrollbar);

  NinePatchThumbScrollbarLayer(const NinePatchThumbScrollbarLayer&) = delete;
  NinePatchThumbScrollbarLayer& operator=(const NinePatchThumbScrollbarLayer&) =
      delete;

  bool OpacityCanAnimateOnImplThread() const override;
  bool Update() override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  ScrollbarLayerType GetScrollbarLayerType() const override;

 protected:
  explicit NinePatchThumbScrollbarLayer(scoped_refptr<Scrollbar> scrollbar);
  ~NinePatchThumbScrollbarLayer() override;

 private:
  template <typename T>
  bool UpdateProperty(const T value, T* prop) {
    if (*prop == value)
      return false;
    *prop = value;
    SetNeedsPushProperties();
    return true;
  }

  bool PaintThumbIfNeeded();
  bool PaintTickmarks();

  ProtectedSequenceForbidden<scoped_refptr<Scrollbar>> scrollbar_;

  ProtectedSequenceReadable<gfx::Size> thumb_size_;
  ProtectedSequenceReadable<gfx::Rect> track_rect_;
  ProtectedSequenceReadable<gfx::Rect> aperture_;

  ProtectedSequenceReadable<std::unique_ptr<ScopedUIResource>> thumb_resource_;
  ProtectedSequenceReadable<std::unique_ptr<ScopedUIResource>>
      track_and_buttons_resource_;
};

}  // namespace cc

#endif  // CC_LAYERS_NINE_PATCH_THUMB_SCROLLBAR_LAYER_H_
