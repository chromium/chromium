// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_

#include <memory>
#include <string>
#include <vector>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public Layer {
 public:
  static scoped_refptr<HeadsUpDisplayLayer> Create();

  HeadsUpDisplayLayer(const HeadsUpDisplayLayer&) = delete;
  HeadsUpDisplayLayer& operator=(const HeadsUpDisplayLayer&) = delete;

  void UpdateLocationAndSize(const gfx::Size& device_viewport,
                             float device_scale_factor);

  const std::vector<gfx::Rect>& LayoutShiftRects() const;
  void SetLayoutShiftRects(const std::vector<gfx::Rect>& rects);

  void SetLayerTreeHost(LayerTreeHost* host) override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  // Layer overrides.
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

 protected:
  HeadsUpDisplayLayer();
  bool HasDrawableContent() const override;

 private:
  ~HeadsUpDisplayLayer() override;

  ProtectedSequenceWritable<sk_sp<SkTypeface>> typeface_;
  ProtectedSequenceWritable<std::vector<gfx::Rect>> layout_shift_rects_;

  std::string paused_debugger_message_;
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
