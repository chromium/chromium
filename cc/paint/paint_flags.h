// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FLAGS_H_
#define CC_PAINT_PAINT_FLAGS_H_

#include "base/compiler_specific.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "third_party/skia/include/core/SkShader.h"

namespace cc {
class PaintFilter;

class CC_PAINT_EXPORT PaintFlags {
 public:
  PaintFlags();
  PaintFlags(const PaintFlags& flags);
  PaintFlags(PaintFlags&& other);
  ~PaintFlags();

  PaintFlags& operator=(const PaintFlags& other);
  PaintFlags& operator=(PaintFlags&& other);

  enum Style {
    kFill_Style = SkPaint::kFill_Style,
    kStroke_Style = SkPaint::kStroke_Style,
    kStrokeAndFill_Style = SkPaint::kStrokeAndFill_Style,
  };
  bool nothingToDraw() const;
  ALWAYS_INLINE Style getStyle() const {
    return static_cast<Style>(bitfields_.style_);
  }
  ALWAYS_INLINE void setStyle(Style style) { bitfields_.style_ = style; }
  ALWAYS_INLINE SkColor getColor() const { return color_; }
  ALWAYS_INLINE void setColor(SkColor color) { color_ = color; }
  ALWAYS_INLINE uint8_t getAlpha() const { return SkColorGetA(color_); }
  ALWAYS_INLINE void setAlpha(uint8_t a) {
    color_ = SkColorSetARGB(a, SkColorGetR(color_), SkColorGetG(color_),
                            SkColorGetB(color_));
  }
  ALWAYS_INLINE void setBlendMode(SkBlendMode mode) {
    blend_mode_ = static_cast<uint32_t>(mode);
  }
  ALWAYS_INLINE SkBlendMode getBlendMode() const {
    return static_cast<SkBlendMode>(blend_mode_);
  }
  ALWAYS_INLINE bool isAntiAlias() const { return bitfields_.antialias_; }
  ALWAYS_INLINE void setAntiAlias(bool aa) { bitfields_.antialias_ = aa; }
  ALWAYS_INLINE bool isDither() const { return bitfields_.dither_; }
  ALWAYS_INLINE void setDither(bool dither) { bitfields_.dither_ = dither; }
  ALWAYS_INLINE void setFilterQuality(SkFilterQuality quality) {
    bitfields_.filter_quality_ = quality;
  }
  ALWAYS_INLINE SkFilterQuality getFilterQuality() const {
    return static_cast<SkFilterQuality>(bitfields_.filter_quality_);
  }
  ALWAYS_INLINE SkScalar getStrokeWidth() const { return width_; }
  ALWAYS_INLINE void setStrokeWidth(SkScalar width) { width_ = width; }
  ALWAYS_INLINE SkScalar getStrokeMiter() const { return miter_limit_; }
  ALWAYS_INLINE void setStrokeMiter(SkScalar miter) { miter_limit_ = miter; }
  enum Cap {
    kButt_Cap = SkPaint::kButt_Cap,    //!< begin/end contours with no extension
    kRound_Cap = SkPaint::kRound_Cap,  //!< begin/end contours with a
                                       //! semi-circle extension
    kSquare_Cap = SkPaint::kSquare_Cap,  //!< begin/end contours with a half
                                         //! square extension
    kLast_Cap = kSquare_Cap,
    kDefault_Cap = kButt_Cap
  };
  ALWAYS_INLINE Cap getStrokeCap() const {
    return static_cast<Cap>(bitfields_.cap_type_);
  }
  ALWAYS_INLINE void setStrokeCap(Cap cap) { bitfields_.cap_type_ = cap; }
  enum Join {
    kMiter_Join = SkPaint::kMiter_Join,
    kRound_Join = SkPaint::kRound_Join,
    kBevel_Join = SkPaint::kBevel_Join,
    kLast_Join = kBevel_Join,
    kDefault_Join = kMiter_Join
  };
  ALWAYS_INLINE Join getStrokeJoin() const {
    return static_cast<Join>(bitfields_.join_type_);
  }
  ALWAYS_INLINE void setStrokeJoin(Join join) { bitfields_.join_type_ = join; }

  ALWAYS_INLINE const sk_sp<SkColorFilter>& getColorFilter() const {
    return color_filter_;
  }
  ALWAYS_INLINE void setColorFilter(sk_sp<SkColorFilter> filter) {
    color_filter_ = std::move(filter);
  }
  ALWAYS_INLINE const sk_sp<SkMaskFilter>& getMaskFilter() const {
    return mask_filter_;
  }
  ALWAYS_INLINE void setMaskFilter(sk_sp<SkMaskFilter> mask) {
    mask_filter_ = std::move(mask);
  }

  ALWAYS_INLINE const PaintShader* getShader() const { return shader_.get(); }

  // Returns true if the shader has been set on the flags.
  ALWAYS_INLINE bool HasShader() const { return !!shader_; }

  // Returns whether the shader is opaque. Note that it is only valid to call
  // this function if HasShader() returns true.
  ALWAYS_INLINE bool ShaderIsOpaque() const { return shader_->IsOpaque(); }

  ALWAYS_INLINE void setShader(sk_sp<PaintShader> shader) {
    shader_ = std::move(shader);
  }

  ALWAYS_INLINE const sk_sp<SkPathEffect>& getPathEffect() const {
    return path_effect_;
  }
  ALWAYS_INLINE void setPathEffect(sk_sp<SkPathEffect> effect) {
    path_effect_ = std::move(effect);
  }
  bool getFillPath(const SkPath& src,
                   SkPath* dst,
                   const SkRect* cull_rect = nullptr,
                   SkScalar res_scale = 1) const;

  ALWAYS_INLINE const sk_sp<PaintFilter>& getImageFilter() const {
    return image_filter_;
  }
  void setImageFilter(sk_sp<PaintFilter> filter);

  ALWAYS_INLINE const sk_sp<SkDrawLooper>& getLooper() const {
    return draw_looper_;
  }
  ALWAYS_INLINE void setLooper(sk_sp<SkDrawLooper> looper) {
    draw_looper_ = std::move(looper);
  }

  // Returns true if this just represents an opacity blend when
  // used as saveLayer flags.
  bool IsSimpleOpacity() const;
  bool SupportsFoldingAlpha() const;

  // SkPaint does not support loopers, so callers of SkToPaint need
  // to check for loopers manually (see getLooper()).
  SkPaint ToSkPaint() const;

  template <typename Proc>
  void DrawToSk(SkCanvas* canvas, Proc proc) const {
    SkPaint paint = ToSkPaint();
    if (const sk_sp<SkDrawLooper>& looper = getLooper())
      looper->apply(canvas, paint, proc);
    else
      proc(canvas, paint);
  }

  bool IsValid() const;
  bool operator==(const PaintFlags& other) const;
  bool operator!=(const PaintFlags& other) const { return !(*this == other); }

  bool HasDiscardableImages() const;

  size_t GetSerializedSize() const;

 private:
  friend class PaintOpReader;
  friend class PaintOpWriter;

  sk_sp<SkPathEffect> path_effect_;
  sk_sp<PaintShader> shader_;
  sk_sp<SkMaskFilter> mask_filter_;
  sk_sp<SkColorFilter> color_filter_;
  sk_sp<SkDrawLooper> draw_looper_;
  sk_sp<PaintFilter> image_filter_;

  // Match(ish) SkPaint defaults.  SkPaintDefaults is not public, so this
  // just uses these values and ignores any SkUserConfig overrides.
  SkColor color_ = SK_ColorBLACK;
  float width_ = 0.f;
  float miter_limit_ = 4.f;
  uint32_t blend_mode_ = static_cast<uint32_t>(SkBlendMode::kSrcOver);

  struct PaintFlagsBitfields {
    uint32_t antialias_ : 1;
    uint32_t dither_ : 1;
    uint32_t cap_type_ : 2;
    uint32_t join_type_ : 2;
    uint32_t style_ : 2;
    uint32_t filter_quality_ : 2;
  };

  union {
    PaintFlagsBitfields bitfields_;
    uint32_t bitfields_uint_;
  };
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_FLAGS_H_
