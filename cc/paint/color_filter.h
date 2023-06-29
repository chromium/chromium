// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_COLOR_FILTER_H_
#define CC_PAINT_COLOR_FILTER_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;
class SkColorTable;
struct SkHighContrastConfig;

namespace cc {

class PaintOpWriter;
class PaintOpReader;

class CC_PAINT_EXPORT ColorFilter : public SkRefCnt {
 public:
  ~ColorFilter() override;
  ColorFilter(const ColorFilter&) = delete;
  ColorFilter& operator=(const ColorFilter&) = delete;

  static sk_sp<ColorFilter> MakeMatrix(const float matrix[20]);
  static sk_sp<ColorFilter> MakeBlend(const SkColor4f& color,
                                      SkBlendMode blend_mode);
  static sk_sp<ColorFilter> MakeSRGBToLinearGamma();
  static sk_sp<ColorFilter> MakeLinearToSRGBGamma();
  static sk_sp<ColorFilter> MakeLuma();
  static sk_sp<ColorFilter> MakeTableARGB(const uint8_t a_table[256],
                                          const uint8_t r_table[256],
                                          const uint8_t g_table[256],
                                          const uint8_t b_table[256]);
  static sk_sp<ColorFilter> MakeTable(sk_sp<SkColorTable> table);
  static sk_sp<ColorFilter> MakeHighContrast(
      const SkHighContrastConfig& config);

  SkColor4f FilterColor(const SkColor4f& color) const;

  bool EqualsForTesting(const ColorFilter& other) const;

 protected:
  friend class ColorFilterPaintFilter;
  friend class PaintFlags;
  friend class PaintOpReader;
  friend class PaintOpWriter;

  enum class Type {
    // kNull is for serialization purposes only, to indicate a null color
    // filter in a containing object (e.g. PaintFlags).
    kNull,
    kMatrix,
    kBlend,
    kSRGBToLinearGamma,
    kLinearToSRGBGamma,
    kLuma,
    kTableARGB,
    kHighContrast,
    kMaxValue = kHighContrast,
  };

  explicit ColorFilter(Type type, sk_sp<SkColorFilter> sk_color_filter);
  // These functions don't handle type_. It's handled in PaintOpWriter/Reader.
  virtual size_t SerializedDataSize() const;
  virtual void SerializeData(PaintOpWriter& writer) const;
  static sk_sp<ColorFilter> Deserialize(PaintOpReader& reader, Type type);

  Type type_;
  sk_sp<SkColorFilter> sk_color_filter_;
};

}  // namespace cc

#endif  // CC_PAINT_COLOR_FILTER_H_
