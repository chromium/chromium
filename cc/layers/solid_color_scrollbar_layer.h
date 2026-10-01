// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_

#include <memory>
#include <optional>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"

namespace cc {

// A solid color scrollbar that can be fully drawn on the impl thread. In
// practice, this is used for overlay scrollbars on Android.
class CC_EXPORT SolidColorScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  static scoped_refptr<SolidColorScrollbarLayer> CreateOrReuse(
      scoped_refptr<Scrollbar> scrollbar,
      SolidColorScrollbarLayer* existing_layer);

  static scoped_refptr<SolidColorScrollbarLayer> Create(
      scoped_refptr<Scrollbar> scrollbar);

  SolidColorScrollbarLayer(const SolidColorScrollbarLayer&) = delete;
  SolidColorScrollbarLayer& operator=(const SolidColorScrollbarLayer&) = delete;

  // Layer overrides.
  bool OpacityCanAnimateOnImplThread() const override;
  void SetOpacity(float opacity) override;
  bool Update() override;
  void SetNeedsDisplayRect(const gfx::Rect& rect) override;
  void SetLayerTreeHost(LayerTreeHost* host) override;

  int thumb_thickness() const { return thumb_thickness_; }
  int track_start() const { return track_start_; }

  ScrollbarLayerType GetScrollbarLayerType() const override;

 protected:
  void PushDirtyPropertiesTo(LayerImpl* layer,
                             uint8_t dirty_flag,
                             CommitState& commit_state) override;

 private:
  explicit SolidColorScrollbarLayer(scoped_refptr<Scrollbar> scrollbar);
  ~SolidColorScrollbarLayer() override;

  ProtectedSequenceForbidden<scoped_refptr<Scrollbar>> scrollbar_;
  int thumb_thickness_;
  int track_start_;
  ProtectedSequenceReadable<std::optional<SkColor4f>> color_;
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
