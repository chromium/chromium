// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/color_filter.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/memory/values_equivalent.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorTable.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"

namespace cc {

namespace {

class MatrixColorFilter final : public ColorFilter {
 public:
  explicit MatrixColorFilter(const float matrix[20])
      : ColorFilter(Type::kMatrix, SkColorFilters::Matrix(matrix)) {}

 private:
  size_t SerializedDataSize() const override {
    float matrix[20];
    return PaintOpWriter::SerializedSizeOfElements(matrix, 20);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    // The identity matrix will be used if the constructor failed to create
    // sk_color_filter_ due to invalid matrix values.
    float matrix[20] = {1, 0, 0, 0, 0,   // row 0
                        0, 1, 0, 0, 0,   // row 1
                        0, 0, 1, 0, 0,   // row 2
                        0, 0, 0, 1, 0};  // row 3
    if (sk_color_filter_) {
      sk_color_filter_->asAColorMatrix(matrix);
    }
    for (float f : matrix) {
      writer.Write(f);
    }
  }
};

class BlendColorFilter final : public ColorFilter {
 public:
  BlendColorFilter(const SkColor4f& color, SkBlendMode blend_mode)
      : ColorFilter(Type::kBlend,
                    SkColorFilters::Blend(color, nullptr, blend_mode)),
        color_(color),
        blend_mode_(blend_mode) {}

 private:
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
  SRGBToLinearGammaColorFilter()
      : ColorFilter(Type::kSRGBToLinearGamma,
                    SkColorFilters::SRGBToLinearGamma()) {}
};

class LinearToSRGBGammaColorFilter final : public ColorFilter {
 public:
  LinearToSRGBGammaColorFilter()
      : ColorFilter(Type::kLinearToSRGBGamma,
                    SkColorFilters::LinearToSRGBGamma()) {}
};

class LumaColorFilter final : public ColorFilter {
 public:
  LumaColorFilter() : ColorFilter(Type::kLuma, SkLumaColorFilter::Make()) {}
};

class TableColorFilter : public ColorFilter {
 public:
  explicit TableColorFilter(sk_sp<SkColorTable> table)
      : ColorFilter(Type::kTableARGB, SkColorFilters::Table(table)),
        table_(std::move(table)) {}

 private:
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSizeOfBytes(256 * 4);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    writer.WriteData(256, table_->alphaTable());
    writer.WriteData(256, table_->redTable());
    writer.WriteData(256, table_->greenTable());
    writer.WriteData(256, table_->blueTable());
  }

 private:
  sk_sp<SkColorTable> table_;
};

class HighContrastColorFilter final : public ColorFilter {
 public:
  explicit HighContrastColorFilter(const SkHighContrastConfig& config)
      : ColorFilter(Type::kHighContrast, SkHighContrastFilter::Make(config)),
        config_(config) {}

 private:
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

ColorFilter::ColorFilter(Type type, sk_sp<SkColorFilter> sk_color_filter)
    : type_(type), sk_color_filter_(std::move(sk_color_filter)) {
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
  return MakeTable(SkColorTable::Make(a_table, r_table, g_table, b_table));
}

sk_sp<ColorFilter> ColorFilter::MakeTable(sk_sp<SkColorTable> table) {
  return sk_make_sp<TableColorFilter>(std::move(table));
}

sk_sp<ColorFilter> ColorFilter::MakeLuma() {
  return sk_make_sp<LumaColorFilter>();
}

sk_sp<ColorFilter> ColorFilter::MakeHighContrast(
    const SkHighContrastConfig& config) {
  return sk_make_sp<HighContrastColorFilter>(config);
}

SkColor4f ColorFilter::FilterColor(const SkColor4f& color) const {
  return sk_color_filter_
             ? sk_color_filter_->filterColor4f(color, nullptr, nullptr)
             : color;
}
bool ColorFilter::EqualsForTesting(const ColorFilter& other) const {
  return type_ == other.type_;
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
      reader.ReadData(a_table);
      reader.ReadData(r_table);
      reader.ReadData(g_table);
      reader.ReadData(b_table);
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
  }
}

}  // namespace cc
