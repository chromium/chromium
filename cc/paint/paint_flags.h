// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FLAGS_H_
#define CC_PAINT_PAINT_FLAGS_H_

#include <utility>

#include "base/compiler_specific.h"
#include "cc/paint/color_filter.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"

class SkCanvas;

namespace cc {
class PaintFilter;
class PaintShader;

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
  };
  bool nothingToDraw() const;
  ALWAYS_INLINE Style getStyle() const {
    return static_cast<Style>(bitfields_.style_);
  }
  ALWAYS_INLINE void setStyle(Style style) { bitfields_.style_ = style; }
  // TODO(crbug.com/1399566): Remove this function
  ALWAYS_INLINE SkColor getColor() const { return color_.toSkColor(); }
  ALWAYS_INLINE SkColor4f getColor4f() const { return color_; }
  ALWAYS_INLINE void setColor(SkColor color) {
    color_ = SkColor4f::FromColor(color);
  }
  ALWAYS_INLINE void setColor(SkColor4f color) { color_ = color; }
  ALWAYS_INLINE float getAlphaf() const { return color_.fA; }
  ALWAYS_INLINE bool isFullyTransparent() const { return color_.fA == 0.0f; }
  ALWAYS_INLINE bool isOpaque() const { return color_.fA >= 1.0f; }
  template <class F, class = std::enable_if_t<std::is_same_v<F, float>>>
  ALWAYS_INLINE void setAlphaf(F a) {
    color_.fA = a;
  }
  ALWAYS_INLINE void setBlendMode(SkBlendMode mode) {
    bitfields_.blend_mode_ = static_cast<uint32_t>(mode);
  }
  ALWAYS_INLINE SkBlendMode getBlendMode() const {
    return static_cast<SkBlendMode>(bitfields_.blend_mode_);
  }
  ALWAYS_INLINE bool isAntiAlias() const { return bitfields_.antialias_; }
  ALWAYS_INLINE void setAntiAlias(bool aa) { bitfields_.antialias_ = aa; }
  ALWAYS_INLINE bool isDither() const { return bitfields_.dither_; }
  ALWAYS_INLINE void setDither(bool dither) { bitfields_.dither_ = dither; }

  enum class FilterQuality {
    kNone,
    kLow,
    kMedium,
    kHigh,
    kLast = kHigh,
  };
  ALWAYS_INLINE void setFilterQuality(FilterQuality quality) {
    bitfields_.filter_quality_ = static_cast<uint32_t>(quality);
  }
  ALWAYS_INLINE FilterQuality getFilterQuality() const {
    return static_cast<FilterQuality>(bitfields_.filter_quality_);
  }
  enum class DynamicRangeLimit {
    kStandard,
    kHigh,
    kConstrainedHigh,
    kLast = kConstrainedHigh,
  };
  ALWAYS_INLINE void setDynamicRangeLimit(DynamicRangeLimit limit) {
    bitfields_.dynamic_range_limit_ = static_cast<uint32_t>(limit);
  }
  ALWAYS_INLINE DynamicRangeLimit getDynamicRangeLimit() const {
    return static_cast<DynamicRangeLimit>(bitfields_.dynamic_range_limit_);
  }
  ALWAYS_INLINE bool useDarkModeForImage() const {
    return bitfields_.use_dark_mode_for_image_;
  }
  ALWAYS_INLINE void setUseDarkModeForImage(bool use_dark_mode_for_image) {
    bitfields_.use_dark_mode_for_image_ = use_dark_mode_for_image;
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

  ALWAYS_INLINE const sk_sp<ColorFilter>& getColorFilter() const {
    return color_filter_;
  }
  ALWAYS_INLINE void setColorFilter(sk_sp<ColorFilter> filter) {
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
  bool ShaderIsOpaque() const;

  void setShader(sk_sp<PaintShader> shader);

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

  // Returns true if this (of a drawOp) allows the sequence
  // saveLayerAlphaf/drawOp/restore to be folded into a single drawOp by baking
  // the alpha in the saveLayerAlphaf into the flags of the drawOp.
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

  static SkSamplingOptions FilterQualityToSkSamplingOptions(
      FilterQuality filter_quality);

  bool IsValid() const;
  bool EqualsForTesting(const PaintFlags& other) const;

  bool HasDiscardableImages() const;

 private:
  friend class PaintOpReader;
  friend class PaintOpWriter;

  sk_sp<SkPathEffect> path_effect_;
  sk_sp<PaintShader> shader_;
  sk_sp<SkMaskFilter> mask_filter_;
  sk_sp<ColorFilter> color_filter_;
  sk_sp<SkDrawLooper> draw_looper_;
  sk_sp<PaintFilter> image_filter_;

  // Match(ish) SkPaint defaults.  SkPaintDefaults is not public, so this
  // just uses these values and ignores any SkUserConfig overrides.
  SkColor4f color_ = SkColors::kBlack;
  float width_ = 0.f;
  float miter_limit_ = 4.f;

  struct PaintFlagsBitfields {
    uint32_t antialias_ : 1;
    uint32_t dither_ : 1;
    uint32_t cap_type_ : 2;
    uint32_t join_type_ : 2;
    uint32_t style_ : 2;
    uint32_t blend_mode_ : 5;
    uint32_t filter_quality_ : 2;
    uint32_t dynamic_range_limit_ : 2;
    // Specifies whether the compositor should use a dark mode filter when
    // rasterizing image on the draw op with this PaintFlags.
    uint32_t use_dark_mode_for_image_ : 1;
  };

  union {
    PaintFlagsBitfields bitfields_;
    uint32_t bitfields_uint_;
  };
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_FLAGS_H_
