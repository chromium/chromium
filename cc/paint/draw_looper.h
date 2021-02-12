// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DRAW_LOOPER_H_
#define CC_PAINT_DRAW_LOOPER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/stl_util.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;
class SkPaint;

namespace cc {

/*
 *  Optional collection of modifiers to paint/canvas to facilitate drawing
 *  a given primitive multiple times. Often this is used for shadows.
 */
class CC_PAINT_EXPORT DrawLooper : public SkRefCnt {
 public:
  enum Flags {
    // If set, apply layer's offset after the canvas' transformation matrix.
    // If clear, pre-translate with layer's offset
    kPostTransformFlag = 1 << 0,

    // If set, set the paint's alpha to the layer's alpha
    // If clear, set the paint's alpha to opaque
    kOverrideAlphaFlag = 1 << 1,

    // If set, ignore all modifiers but still translate using layer's offset
    kDontModifyPaintFlag = 1 << 2,
  };

  ~DrawLooper() override;

  //  The callback will be invoked for each "layer" in the looper, each time
  //  with a modified canvas and paint (depending on what the looper wanted
  //  to change). These can each be drawn directly, as the looper will callback
  //  logically bottom to top visually.
  template <typename DrawProc>
  void Apply(SkCanvas* canvas, const SkPaint& paint, DrawProc proc) const {
    // Our array is stored top to bottom
    //  e.g. layers_[0] is the top (will be drawn last)
    //       layers_[N-1] is on bottom, and will be drawn first
    //
    //  Hence, since we must draw in painter's order (bottom to top), we
    //  walk the array in reverse.
    //
    //  Each time through the loop, we make a copy of the draw's paint, modify
    //  it as indicated by the layer info, modify the canvas' translate, and
    //  then call back to issue the actual draw.
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      SkAutoCanvasRestore acr(canvas, true);
      SkPaint p(paint);
      it->Apply(canvas, &p);
      proc(canvas, p);
    }
  }

  bool operator==(const DrawLooper& other) const;
  bool operator!=(const DrawLooper& other) const { return !(*this == other); }

  static size_t GetSerializedSize(const DrawLooper* looper);

 private:
  // Keep this in sync with the fields in Flags
  // Used to mask out illegal bits when constructing Layer
  enum {
    kAllFlagsMask =
        kPostTransformFlag | kOverrideAlphaFlag | kDontModifyPaintFlag,
  };

  struct Layer {
    SkPoint offset;
    float blurSigma;
    SkColor color;
    uint32_t flags;

    bool operator==(const Layer& other) const;

    void Apply(SkCanvas* canvas, SkPaint* paint) const;
  };
  std::vector<Layer> layers_;

  explicit DrawLooper(std::vector<Layer> l);

  void UpdateForLayer(const Layer& layer,
                      SkCanvas* canvas,
                      SkPaint* paint) const;

  friend class DrawLooperBuilder;
  friend class PaintOpReader;
  friend class PaintOpWriter;
};

class CC_PAINT_EXPORT DrawLooperBuilder {
 public:
  DrawLooperBuilder();
  ~DrawLooperBuilder();

  void AddUnmodifiedContent(bool addOnTop = false);
  void AddShadow(SkPoint offset,
                 float blurSigma,
                 SkColor color,
                 uint32_t flags,
                 bool addOnTop = false);
  sk_sp<DrawLooper> Detach();

 private:
  std::vector<DrawLooper::Layer> layers_;
};

}  // namespace cc

#endif  // CC_PAINT_DRAW_LOOPER_H_
