// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/color_filter.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/values_equivalent.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"

namespace cc {

namespace {

class MatrixColorFilter final : public ColorFilter {
 public:
  explicit MatrixColorFilter(const float matrix[20])
      : ColorFilter(Type::kMatrix) {
    std::copy_n(matrix, 20, matrix_);
  }

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkColorFilters::Matrix(matrix_);
  }
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSizeOfElements(matrix_, 20);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    for (float f : matrix_) {
      writer.Write(f);
    }
  }

  float matrix_[20];
};

class BlendColorFilter final : public ColorFilter {
 public:
  BlendColorFilter(const SkColor4f& color, SkBlendMode blend_mode)
      : ColorFilter(Type::kBlend), color_(color), blend_mode_(blend_mode) {}

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkColorFilters::Blend(color_, nullptr, blend_mode_);
  }
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSize(color_) +
           PaintOpWriter::SerializedSize(blend_mode_);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    writer.Write(color_);
    writer.Write(blend_mode_);
  }

  SkColor4f color_;
  SkBlendMode blend_mode_;
};

class SRGBToLinearGammaColorFilter final : public ColorFilter {
 public:
  SRGBToLinearGammaColorFilter() : ColorFilter(Type::kSRGBToLinearGamma) {}

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkColorFilters::SRGBToLinearGamma();
  }
};

class LinearToSRGBGammaColorFilter final : public ColorFilter {
 public:
  LinearToSRGBGammaColorFilter() : ColorFilter(Type::kLinearToSRGBGamma) {}

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkColorFilters::LinearToSRGBGamma();
  }
};

class LumaColorFilter final : public ColorFilter {
 public:
  LumaColorFilter() : ColorFilter(Type::kLuma) {}

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkLumaColorFilter::Make();
  }
};

class TableARGBColorFilter : public ColorFilter {
 public:
  TableARGBColorFilter(const uint8_t a_table[256],
                       const uint8_t r_table[256],
                       const uint8_t g_table[256],
                       const uint8_t b_table[256])
      : ColorFilter(Type::kTableARGB) {
    std::copy_n(a_table, 256, a_table_);
    std::copy_n(r_table, 256, r_table_);
    std::copy_n(g_table, 256, g_table_);
    std::copy_n(b_table, 256, b_table_);
  }

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkColorFilters::TableARGB(a_table_, r_table_, g_table_, b_table_);
  }
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSizeOfBytes(256 * 4);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    writer.WriteData(256, a_table_);
    writer.WriteData(256, r_table_);
    writer.WriteData(256, g_table_);
    writer.WriteData(256, b_table_);
  }

 private:
  uint8_t a_table_[256];
  uint8_t r_table_[256];
  uint8_t g_table_[256];
  uint8_t b_table_[256];
};

class HighContrastColorFilter final : public ColorFilter {
 public:
  explicit HighContrastColorFilter(const SkHighContrastConfig& config)
      : ColorFilter(Type::kHighContrast), config_(config) {}

 private:
  sk_sp<SkColorFilter> CreateSkColorFilter() const override {
    return SkHighContrastFilter::Make(config_);
  }
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSize(config_);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    writer.Write(config_);
  }

  SkHighContrastConfig config_;
};

}  // namespace

ColorFilter::~ColorFilter() = default;

ColorFilter::ColorFilter(Type type) : type_(type) {
  DCHECK_NE(type, Type::kNull);
}

sk_sp<ColorFilter> ColorFilter::MakeMatrix(const float matrix[20]) {
  return sk_make_sp<MatrixColorFilter>(matrix);
}

sk_sp<ColorFilter> ColorFilter::MakeBlend(const SkColor4f& color,
                                          SkBlendMode blend_mode) {
  return sk_make_sp<BlendColorFilter>(color, blend_mode);
}

sk_sp<ColorFilter> ColorFilter::MakeSRGBToLinearGamma() {
  return sk_make_sp<SRGBToLinearGammaColorFilter>();
}

sk_sp<ColorFilter> ColorFilter::MakeLinearToSRGBGamma() {
  return sk_make_sp<LinearToSRGBGammaColorFilter>();
}

sk_sp<ColorFilter> ColorFilter::MakeTableARGB(const uint8_t a_table[256],
                                              const uint8_t r_table[256],
                                              const uint8_t g_table[256],
                                              const uint8_t b_table[256]) {
  return sk_make_sp<TableARGBColorFilter>(a_table, r_table, g_table, b_table);
}

sk_sp<ColorFilter> ColorFilter::MakeLuma() {
  return sk_make_sp<LumaColorFilter>();
}

sk_sp<ColorFilter> ColorFilter::MakeHighContrast(
    const SkHighContrastConfig& config) {
  return sk_make_sp<HighContrastColorFilter>(config);
}

SkColor4f ColorFilter::FilterColor(const SkColor4f& color) const {
  sk_sp<SkColorFilter> filter = GetSkColorFilter();
  return filter ? filter->filterColor4f(color, nullptr, nullptr) : color;
}
bool ColorFilter::EqualsForTesting(const ColorFilter& other) const {
  return type_ == other.type_;
}

sk_sp<SkColorFilter> ColorFilter::GetSkColorFilter() const {
  if (!sk_color_filter_) {
    sk_color_filter_ = CreateSkColorFilter();
  }
  return sk_color_filter_;
}

size_t ColorFilter::SerializedDataSize() const {
  return 0u;
}

void ColorFilter::SerializeData(PaintOpWriter& writer) const {}

sk_sp<ColorFilter> ColorFilter::Deserialize(PaintOpReader& reader, Type type) {
  switch (type) {
    case Type::kMatrix: {
      float matrix[20];
      for (float& f : matrix) {
        reader.Read(&f);
        if (!reader.valid()) {
          return nullptr;
        }
      }
      return MakeMatrix(matrix);
    }
    case Type::kBlend: {
      SkColor4f color;
      SkBlendMode blend_mode;
      reader.Read(&color);
      reader.Read(&blend_mode);
      if (!reader.valid()) {
        return nullptr;
      }
      return MakeBlend(color, blend_mode);
    }
    case Type::kSRGBToLinearGamma:
      return MakeSRGBToLinearGamma();
    case Type::kLinearToSRGBGamma:
      return MakeLinearToSRGBGamma();
    case Type::kLuma:
      return MakeLuma();
    case Type::kTableARGB: {
      uint8_t a_table[256], r_table[256], g_table[256], b_table[256];
      reader.ReadData(256, a_table);
      reader.ReadData(256, r_table);
      reader.ReadData(256, g_table);
      reader.ReadData(256, b_table);
      if (!reader.valid()) {
        return nullptr;
      }
      return MakeTableARGB(a_table, r_table, g_table, b_table);
    }
    case Type::kHighContrast: {
      SkHighContrastConfig config;
      reader.Read(&config);
      return MakeHighContrast(config);
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace cc
