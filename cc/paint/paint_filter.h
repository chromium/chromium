// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FILTER_H_
#define CC_PAINT_PAINT_FILTER_H_

#include "base/containers/stack_container.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/scoped_raster_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPoint3.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkDisplacementMapEffect.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkMatrixConvolutionImageFilter.h"

namespace viz {
class GLRenderer;
class SkiaRenderer;
class SoftwareRenderer;
}  // namespace viz

namespace cc {
class ImageProvider;

class CC_PAINT_EXPORT PaintFilter : public SkRefCnt {
 public:
  enum class Type : uint32_t {
    // For serialization purposes, we reserve one enum to indicate that there
    // was no PaintFilter, ie the filter is "null".
    kNullFilter,
    kColorFilter,
    kBlur,
    kDropShadow,
    kMagnifier,
    kCompose,
    kAlphaThreshold,
    kXfermode,
    kArithmetic,
    kMatrixConvolution,
    kDisplacementMapEffect,
    kImage,
    kPaintRecord,
    kMerge,
    kMorphology,
    kOffset,
    kTile,
    kTurbulence,
    kPaintFlags,
    kMatrix,
    kLightingDistant,
    kLightingPoint,
    kLightingSpot,
    // Update the following if kLightingSpot is not the max anymore.
    kMaxFilterType = kLightingSpot
  };
  enum class LightingType : uint32_t {
    kDiffuse,
    kSpecular,
    // Update the following if kSpecular is not the max anymore.
    kMaxLightingType = kSpecular
  };

  using MapDirection = SkImageFilter::MapDirection;
  using CropRect = SkImageFilter::CropRect;

  PaintFilter(const PaintFilter&) = delete;
  ~PaintFilter() override;

  PaintFilter& operator=(const PaintFilter&) = delete;

  static std::string TypeToString(Type type);

  // Returns the size required to serialize the |filter|. Note that |filter| can
  // be nullptr.
  static size_t GetFilterSize(const PaintFilter* filter);

  Type type() const { return type_; }
  SkIRect filter_bounds(const SkIRect& src,
                        const SkMatrix& ctm,
                        MapDirection direction) const {
    if (!cached_sk_filter_)
      return SkIRect::MakeEmpty();
    return cached_sk_filter_->filterBounds(src, ctm, direction);
  }
  int count_inputs() const {
    if (!cached_sk_filter_)
      return 0;
    return cached_sk_filter_->countInputs();
  }
  const CropRect* crop_rect() const {
    return base::OptionalOrNullptr(crop_rect_);
  }

  bool has_discardable_images() const { return has_discardable_images_; }
  ImageAnalysisState image_analysis_state() const {
    return image_analysis_state_;
  }
  void set_has_animated_images(bool has_animated_images) {
    image_analysis_state_ = has_animated_images
                                ? ImageAnalysisState::kAnimatedImages
                                : ImageAnalysisState::kNoAnimatedImages;
  }

  virtual size_t SerializedSize() const = 0;

  // Returns a snaphot of the PaintFilter with images replaced using
  // |image_provider|. Note that this may return the same filter if the filter
  // has no images.
  sk_sp<PaintFilter> SnapshotWithImages(ImageProvider* image_provider) const;

  // Note that this operation is potentially slow. It also only compares things
  // that are easy to compare. As an example, it doesn't compare equality of
  // images, rather only its existence. This is meant to be used only by tests
  // and fuzzers.
  // TODO(vmpstr): Rename this and places that its used to something like
  // EqualsForTesting.
  bool operator==(const PaintFilter& other) const;
  bool operator!=(const PaintFilter& other) const { return !(*this == other); }

 protected:
  PaintFilter(Type type,
              const CropRect* crop_rect,
              bool has_discardable_images);

  static sk_sp<SkImageFilter> GetSkFilter(const PaintFilter* paint_filter) {
    return paint_filter ? paint_filter->cached_sk_filter_ : nullptr;
  }
  const sk_sp<SkImageFilter>& cached_sk_filter() const {
    return cached_sk_filter_;
  }

  size_t BaseSerializedSize() const;
  virtual sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const = 0;

  // This should be created by each sub-class at construction time, to ensure
  // that subsequent access to the filter is thread-safe.
  sk_sp<SkImageFilter> cached_sk_filter_;

 private:
  // For cached skia filter access in SkPaint conversions. Mostly used during
  // raster.
  friend class PaintFlags;
  friend class viz::GLRenderer;
  friend class viz::SkiaRenderer;
  friend class viz::SoftwareRenderer;

  const Type type_;
  base::Optional<CropRect> crop_rect_;
  const bool has_discardable_images_;

  ImageAnalysisState image_analysis_state_ = ImageAnalysisState::kNoAnalysis;
};

class CC_PAINT_EXPORT ColorFilterPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kColorFilter;
  ColorFilterPaintFilter(sk_sp<SkColorFilter> color_filter,
                         sk_sp<PaintFilter> input,
                         const CropRect* crop_rect = nullptr);
  ~ColorFilterPaintFilter() override;

  const sk_sp<SkColorFilter>& color_filter() const { return color_filter_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const ColorFilterPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  sk_sp<SkColorFilter> color_filter_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT BlurPaintFilter final : public PaintFilter {
 public:
  using TileMode = SkBlurImageFilter::TileMode;
  static constexpr Type kType = Type::kBlur;
  BlurPaintFilter(SkScalar sigma_x,
                  SkScalar sigma_y,
                  TileMode tile_mode,
                  sk_sp<PaintFilter> input,
                  const CropRect* crop_rect = nullptr);
  ~BlurPaintFilter() override;

  const sk_sp<PaintFilter>& input() const { return input_; }

  SkScalar sigma_x() const { return sigma_x_; }
  SkScalar sigma_y() const { return sigma_y_; }
  TileMode tile_mode() const { return tile_mode_; }

  size_t SerializedSize() const override;
  bool operator==(const BlurPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkScalar sigma_x_;
  SkScalar sigma_y_;
  TileMode tile_mode_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT DropShadowPaintFilter final : public PaintFilter {
 public:
  using ShadowMode = SkDropShadowImageFilter::ShadowMode;
  static constexpr Type kType = Type::kDropShadow;
  DropShadowPaintFilter(SkScalar dx,
                        SkScalar dy,
                        SkScalar sigma_x,
                        SkScalar sigma_y,
                        SkColor color,
                        ShadowMode shadow_mode,
                        sk_sp<PaintFilter> input,
                        const CropRect* crop_rect = nullptr);
  ~DropShadowPaintFilter() override;

  SkScalar dx() const { return dx_; }
  SkScalar dy() const { return dy_; }
  SkScalar sigma_x() const { return sigma_x_; }
  SkScalar sigma_y() const { return sigma_y_; }
  SkColor color() const { return color_; }
  ShadowMode shadow_mode() const { return shadow_mode_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const DropShadowPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkScalar dx_;
  SkScalar dy_;
  SkScalar sigma_x_;
  SkScalar sigma_y_;
  SkColor color_;
  ShadowMode shadow_mode_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT MagnifierPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMagnifier;
  MagnifierPaintFilter(const SkRect& src_rect,
                       SkScalar inset,
                       sk_sp<PaintFilter> input,
                       const CropRect* crop_rect = nullptr);
  ~MagnifierPaintFilter() override;

  const SkRect& src_rect() const { return src_rect_; }
  SkScalar inset() const { return inset_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const MagnifierPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkRect src_rect_;
  SkScalar inset_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT ComposePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kCompose;
  ComposePaintFilter(sk_sp<PaintFilter> outer, sk_sp<PaintFilter> inner);
  ~ComposePaintFilter() override;

  const sk_sp<PaintFilter>& outer() const { return outer_; }
  const sk_sp<PaintFilter>& inner() const { return inner_; }

  size_t SerializedSize() const override;
  bool operator==(const ComposePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  sk_sp<PaintFilter> outer_;
  sk_sp<PaintFilter> inner_;
};

class CC_PAINT_EXPORT AlphaThresholdPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kAlphaThreshold;
  AlphaThresholdPaintFilter(const SkRegion& region,
                            SkScalar inner_min,
                            SkScalar outer_max,
                            sk_sp<PaintFilter> input,
                            const CropRect* crop_rect = nullptr);
  ~AlphaThresholdPaintFilter() override;

  const SkRegion& region() const { return region_; }
  SkScalar inner_min() const { return inner_min_; }
  SkScalar outer_max() const { return outer_max_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const AlphaThresholdPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkRegion region_;
  SkScalar inner_min_;
  SkScalar outer_max_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT XfermodePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kXfermode;
  XfermodePaintFilter(SkBlendMode blend_mode,
                      sk_sp<PaintFilter> background,
                      sk_sp<PaintFilter> foreground,
                      const CropRect* crop_rect = nullptr);
  ~XfermodePaintFilter() override;

  SkBlendMode blend_mode() const { return blend_mode_; }
  const sk_sp<PaintFilter>& background() const { return background_; }
  const sk_sp<PaintFilter>& foreground() const { return foreground_; }

  size_t SerializedSize() const override;
  bool operator==(const XfermodePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkBlendMode blend_mode_;
  sk_sp<PaintFilter> background_;
  sk_sp<PaintFilter> foreground_;
};

class CC_PAINT_EXPORT ArithmeticPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kArithmetic;
  ArithmeticPaintFilter(float k1,
                        float k2,
                        float k3,
                        float k4,
                        bool enforce_pm_color,
                        sk_sp<PaintFilter> background,
                        sk_sp<PaintFilter> foreground,
                        const CropRect* crop_rect = nullptr);
  ~ArithmeticPaintFilter() override;

  float k1() const { return k1_; }
  float k2() const { return k2_; }
  float k3() const { return k3_; }
  float k4() const { return k4_; }
  bool enforce_pm_color() const { return enforce_pm_color_; }
  const sk_sp<PaintFilter>& background() const { return background_; }
  const sk_sp<PaintFilter>& foreground() const { return foreground_; }

  size_t SerializedSize() const override;
  bool operator==(const ArithmeticPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  float k1_;
  float k2_;
  float k3_;
  float k4_;
  bool enforce_pm_color_;
  sk_sp<PaintFilter> background_;
  sk_sp<PaintFilter> foreground_;
};

class CC_PAINT_EXPORT MatrixConvolutionPaintFilter final : public PaintFilter {
 public:
  using TileMode = SkMatrixConvolutionImageFilter::TileMode;
  static constexpr Type kType = Type::kMatrixConvolution;
  MatrixConvolutionPaintFilter(const SkISize& kernel_size,
                               const SkScalar* kernel,
                               SkScalar gain,
                               SkScalar bias,
                               const SkIPoint& kernel_offset,
                               TileMode tile_mode,
                               bool convolve_alpha,
                               sk_sp<PaintFilter> input,
                               const CropRect* crop_rect = nullptr);
  ~MatrixConvolutionPaintFilter() override;

  const SkISize& kernel_size() const { return kernel_size_; }
  SkScalar kernel_at(size_t i) const { return kernel_[i]; }
  SkScalar gain() const { return gain_; }
  SkScalar bias() const { return bias_; }
  SkIPoint kernel_offset() const { return kernel_offset_; }
  TileMode tile_mode() const { return tile_mode_; }
  bool convolve_alpha() const { return convolve_alpha_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const MatrixConvolutionPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkISize kernel_size_;
  base::StackVector<SkScalar, 3> kernel_;
  SkScalar gain_;
  SkScalar bias_;
  SkIPoint kernel_offset_;
  TileMode tile_mode_;
  bool convolve_alpha_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT DisplacementMapEffectPaintFilter final
    : public PaintFilter {
 public:
  using ChannelSelectorType = SkDisplacementMapEffect::ChannelSelectorType;
  static constexpr Type kType = Type::kDisplacementMapEffect;
  DisplacementMapEffectPaintFilter(ChannelSelectorType channel_x,
                                   ChannelSelectorType channel_y,
                                   SkScalar scale,
                                   sk_sp<PaintFilter> displacement,
                                   sk_sp<PaintFilter> color,
                                   const CropRect* crop_rect = nullptr);
  ~DisplacementMapEffectPaintFilter() override;

  ChannelSelectorType channel_x() const { return channel_x_; }
  ChannelSelectorType channel_y() const { return channel_y_; }
  SkScalar scale() const { return scale_; }
  const sk_sp<PaintFilter>& displacement() const { return displacement_; }
  const sk_sp<PaintFilter>& color() const { return color_; }

  size_t SerializedSize() const override;
  bool operator==(const DisplacementMapEffectPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  ChannelSelectorType channel_x_;
  ChannelSelectorType channel_y_;
  SkScalar scale_;
  sk_sp<PaintFilter> displacement_;
  sk_sp<PaintFilter> color_;
};

class CC_PAINT_EXPORT ImagePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kImage;
  ImagePaintFilter(PaintImage image,
                   const SkRect& src_rect,
                   const SkRect& dst_rect,
                   SkFilterQuality filter_quality);
  ~ImagePaintFilter() override;

  const PaintImage& image() const { return image_; }
  const SkRect& src_rect() const { return src_rect_; }
  const SkRect& dst_rect() const { return dst_rect_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }

  size_t SerializedSize() const override;
  bool operator==(const ImagePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  PaintImage image_;
  SkRect src_rect_;
  SkRect dst_rect_;
  SkFilterQuality filter_quality_;
};

class CC_PAINT_EXPORT RecordPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kPaintRecord;
  RecordPaintFilter(sk_sp<PaintRecord> record, const SkRect& record_bounds);
  ~RecordPaintFilter() override;

  const sk_sp<PaintRecord>& record() const { return record_; }
  SkRect record_bounds() const { return record_bounds_; }

  size_t SerializedSize() const override;
  bool operator==(const RecordPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  RecordPaintFilter(sk_sp<PaintRecord> record,
                    const SkRect& record_bounds,
                    ImageProvider* image_provider);

  sk_sp<PaintRecord> record_;
  SkRect record_bounds_;
};

class CC_PAINT_EXPORT MergePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMerge;
  MergePaintFilter(const sk_sp<PaintFilter>* const filters,
                   int count,
                   const CropRect* crop_rect = nullptr);
  ~MergePaintFilter() override;

  size_t input_count() const { return inputs_->size(); }
  const PaintFilter* input_at(size_t i) const {
    DCHECK_LT(i, input_count());
    return inputs_[i].get();
  }

  size_t SerializedSize() const override;
  bool operator==(const MergePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  MergePaintFilter(const sk_sp<PaintFilter>* const filters,
                   int count,
                   const CropRect* crop_rect,
                   ImageProvider* image_provider);
  base::StackVector<sk_sp<PaintFilter>, 2> inputs_;
};

class CC_PAINT_EXPORT MorphologyPaintFilter final : public PaintFilter {
 public:
  enum class MorphType : uint32_t { kDilate, kErode, kMaxMorphType = kErode };
  static constexpr Type kType = Type::kMorphology;
  MorphologyPaintFilter(MorphType morph_type,
                        int radius_x,
                        int radius_y,
                        sk_sp<PaintFilter> input,
                        const CropRect* crop_rect = nullptr);
  ~MorphologyPaintFilter() override;

  MorphType morph_type() const { return morph_type_; }
  int radius_x() const { return radius_x_; }
  int radius_y() const { return radius_y_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const MorphologyPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  MorphType morph_type_;
  int radius_x_;
  int radius_y_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT OffsetPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kOffset;
  OffsetPaintFilter(SkScalar dx,
                    SkScalar dy,
                    sk_sp<PaintFilter> input,
                    const CropRect* crop_rect = nullptr);
  ~OffsetPaintFilter() override;

  SkScalar dx() const { return dx_; }
  SkScalar dy() const { return dy_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const OffsetPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkScalar dx_;
  SkScalar dy_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT TilePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kTile;
  TilePaintFilter(const SkRect& src,
                  const SkRect& dst,
                  sk_sp<PaintFilter> input);
  ~TilePaintFilter() override;

  const SkRect& src() const { return src_; }
  const SkRect& dst() const { return dst_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const TilePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkRect src_;
  SkRect dst_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT TurbulencePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kTurbulence;
  enum class TurbulenceType : uint32_t {
    kTurbulence,
    kFractalNoise,
    kMaxTurbulenceType = kFractalNoise
  };
  TurbulencePaintFilter(TurbulenceType turbulence_type,
                        SkScalar base_frequency_x,
                        SkScalar base_frequency_y,
                        int num_octaves,
                        SkScalar seed,
                        const SkISize* tile_size,
                        const CropRect* crop_rect = nullptr);
  ~TurbulencePaintFilter() override;

  TurbulenceType turbulence_type() const { return turbulence_type_; }
  SkScalar base_frequency_x() const { return base_frequency_x_; }
  SkScalar base_frequency_y() const { return base_frequency_y_; }
  int num_octaves() const { return num_octaves_; }
  SkScalar seed() const { return seed_; }
  SkISize tile_size() const { return tile_size_; }

  size_t SerializedSize() const override;
  bool operator==(const TurbulencePaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  TurbulenceType turbulence_type_;
  SkScalar base_frequency_x_;
  SkScalar base_frequency_y_;
  int num_octaves_;
  SkScalar seed_;
  SkISize tile_size_;
};

class CC_PAINT_EXPORT PaintFlagsPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kPaintFlags;
  explicit PaintFlagsPaintFilter(PaintFlags flags,
                                 const CropRect* crop_rect = nullptr);
  ~PaintFlagsPaintFilter() override;

  const PaintFlags& flags() const { return flags_; }

  size_t SerializedSize() const override;
  bool operator==(const PaintFlagsPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  PaintFlagsPaintFilter(PaintFlags flags,
                        ImageProvider* image_provider,
                        const CropRect* crop_rect);

  PaintFlags flags_;
  base::Optional<ScopedRasterFlags> raster_flags_;
};

class CC_PAINT_EXPORT MatrixPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMatrix;
  MatrixPaintFilter(const SkMatrix& matrix,
                    SkFilterQuality filter_quality,
                    sk_sp<PaintFilter> input);
  ~MatrixPaintFilter() override;

  const SkMatrix& matrix() const { return matrix_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const MatrixPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  SkMatrix matrix_;
  SkFilterQuality filter_quality_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingDistantPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingDistant;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingDistantPaintFilter(LightingType lighting_type,
                             const SkPoint3& direction,
                             SkColor light_color,
                             SkScalar surface_scale,
                             SkScalar kconstant,
                             SkScalar shininess,
                             sk_sp<PaintFilter> input,
                             const CropRect* crop_rect = nullptr);
  ~LightingDistantPaintFilter() override;

  LightingType lighting_type() const { return lighting_type_; }
  const SkPoint3& direction() const { return direction_; }
  SkColor light_color() const { return light_color_; }
  SkScalar surface_scale() const { return surface_scale_; }
  SkScalar kconstant() const { return kconstant_; }
  SkScalar shininess() const { return shininess_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const LightingDistantPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  LightingType lighting_type_;
  SkPoint3 direction_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingPointPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingPoint;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingPointPaintFilter(LightingType lighting_type,
                           const SkPoint3& location,
                           SkColor light_color,
                           SkScalar surface_scale,
                           SkScalar kconstant,
                           SkScalar shininess,
                           sk_sp<PaintFilter> input,
                           const CropRect* crop_rect = nullptr);
  ~LightingPointPaintFilter() override;

  LightingType lighting_type() const { return lighting_type_; }
  const SkPoint3& location() const { return location_; }
  SkColor light_color() const { return light_color_; }
  SkScalar surface_scale() const { return surface_scale_; }
  SkScalar kconstant() const { return kconstant_; }
  SkScalar shininess() const { return shininess_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const LightingPointPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  LightingType lighting_type_;
  SkPoint3 location_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingSpotPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingSpot;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingSpotPaintFilter(LightingType lighting_type,
                          const SkPoint3& location,
                          const SkPoint3& target,
                          SkScalar specular_exponent,
                          SkScalar cutoff_angle,
                          SkColor light_color,
                          SkScalar surface_scale,
                          SkScalar kconstant,
                          SkScalar shininess,
                          sk_sp<PaintFilter> input,
                          const CropRect* crop_rect = nullptr);
  ~LightingSpotPaintFilter() override;

  LightingType lighting_type() const { return lighting_type_; }
  const SkPoint3& location() const { return location_; }
  const SkPoint3& target() const { return target_; }
  SkScalar specular_exponent() const { return specular_exponent_; }
  SkScalar cutoff_angle() const { return cutoff_angle_; }
  SkColor light_color() const { return light_color_; }
  SkScalar surface_scale() const { return surface_scale_; }
  SkScalar kconstant() const { return kconstant_; }
  SkScalar shininess() const { return shininess_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

  size_t SerializedSize() const override;
  bool operator==(const LightingSpotPaintFilter& other) const;

 protected:
  sk_sp<PaintFilter> SnapshotWithImagesInternal(
      ImageProvider* image_provider) const override;

 private:
  LightingType lighting_type_;
  SkPoint3 location_;
  SkPoint3 target_;
  SkScalar specular_exponent_;
  SkScalar cutoff_angle_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_FILTER_H_
