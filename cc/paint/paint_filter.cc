// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_filter.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/values_equivalent.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/scoped_raster_flags.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/effects/SkPerlinNoiseShader.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"

namespace cc {
namespace {
const bool kHasNoDiscardableImages = false;

template <typename T>
bool AreValuesEqualForTesting(const sk_sp<T>& a, const sk_sp<T>& b) {
  return base::ValuesEquivalent(a, b, [](const T& x, const T& y) {
    return x.EqualsForTesting(y);  // IN-TEST
  });
}

template <typename T>
bool AreEqualForTesting(const PaintFilter& a, const PaintFilter& b) {
  return static_cast<const T&>(a).EqualsForTesting(  // IN-TEST
      static_cast<const T&>(b));
}

bool HasDiscardableImages(const sk_sp<PaintFilter>& filter) {
  return filter ? filter->has_discardable_images() : false;
}

bool HasDiscardableImages(const sk_sp<PaintFilter>* const filters, int count) {
  for (int i = 0; i < count; ++i) {
    if (filters[i] && filters[i]->has_discardable_images())
      return true;
  }
  return false;
}

sk_sp<PaintFilter> Snapshot(const sk_sp<PaintFilter>& filter,
                            ImageProvider* image_provider) {
  if (!filter)
    return nullptr;
  return filter->SnapshotWithImages(image_provider);
}

}  // namespace

PaintFilter::PaintFilter(Type type,
                         const CropRect* crop_rect,
                         bool has_discardable_images)
    : type_(type), has_discardable_images_(has_discardable_images) {
  if (crop_rect)
    crop_rect_.emplace(*crop_rect);
}

PaintFilter::~PaintFilter() = default;

// static
std::string PaintFilter::TypeToString(Type type) {
  switch (type) {
    case Type::kNullFilter:
      return "kNullFilter";
    case Type::kColorFilter:
      return "kColorFilter";
    case Type::kBlur:
      return "kBlur";
    case Type::kDropShadow:
      return "kDropShadow";
    case Type::kMagnifier:
      return "kMagnifier";
    case Type::kCompose:
      return "kCompose";
    case Type::kAlphaThreshold:
      return "kAlphaThreshold";
    case Type::kXfermode:
      return "kXfermode";
    case Type::kArithmetic:
      return "kArithmetic";
    case Type::kMatrixConvolution:
      return "kMatrixConvolution";
    case Type::kDisplacementMapEffect:
      return "kDisplacementMapEffect";
    case Type::kImage:
      return "kImage";
    case Type::kPaintRecord:
      return "kPaintRecord";
    case Type::kMerge:
      return "kMerge";
    case Type::kMorphology:
      return "kMorphology";
    case Type::kOffset:
      return "kOffset";
    case Type::kTile:
      return "kTile";
    case Type::kTurbulence:
      return "kTurbulence";
    case Type::kShader:
      return "kShader";
    case Type::kMatrix:
      return "kMatrix";
    case Type::kLightingDistant:
      return "kLightingDistant";
    case Type::kLightingPoint:
      return "kLightingPoint";
    case Type::kLightingSpot:
      return "kLightingSpot";
  }
  NOTREACHED();
}

SkIRect PaintFilter::MapRect(const SkIRect& src,
                             const SkMatrix* ctm,
                             MapDirection direction) const {
  if (!cached_sk_filter_) {
    return SkIRect::MakeEmpty();
  }
  if (direction == MapDirection::kForward_MapDirection && !ctm) {
    // Compared to filterbounds(), computeFastBounds ensures the result rect
    // covers the affected pixels regardless of CTM.
    SkRect rect = cached_sk_filter_->computeFastBounds(SkRect::Make(src));
    SkIRect result;
    rect.roundOut(&result);
    return result;
  }
  CHECK(ctm);
  return cached_sk_filter_->filterBounds(src, *ctm, direction);
}

const PaintFilter::CropRect* PaintFilter::GetCropRect() const {
  return base::OptionalToPtr(crop_rect_);
}

base::CheckedNumeric<size_t> PaintFilter::BaseSerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      PaintOpWriter::SerializedSize(type_);
  // Bool to indicate whether crop exists.
  total_size += PaintOpWriter::SerializedSize<bool>();
  if (crop_rect_) {
    total_size += PaintOpWriter::SerializedSize(*crop_rect_);
  }
  return total_size;
}

sk_sp<PaintFilter> PaintFilter::SnapshotWithImages(
    ImageProvider* image_provider) const {
  if (!has_discardable_images_)
    return sk_ref_sp<PaintFilter>(this);
  return SnapshotWithImagesInternal(image_provider);
}

bool PaintFilter::EqualsForTesting(const PaintFilter& other) const {
  if (type_ != other.type_) {
    return false;
  }
  if (crop_rect_ != other.crop_rect_) {
    return false;
  }

  switch (type_) {
    case Type::kNullFilter:
      return true;
    case Type::kColorFilter:
      return AreEqualForTesting<ColorFilterPaintFilter>(*this, other);
    case Type::kBlur:
      return AreEqualForTesting<BlurPaintFilter>(*this, other);
    case Type::kDropShadow:
      return AreEqualForTesting<DropShadowPaintFilter>(*this, other);
    case Type::kMagnifier:
      return AreEqualForTesting<MagnifierPaintFilter>(*this, other);
    case Type::kCompose:
      return AreEqualForTesting<ComposePaintFilter>(*this, other);
    case Type::kAlphaThreshold:
      return AreEqualForTesting<AlphaThresholdPaintFilter>(*this, other);
    case Type::kXfermode:
      return AreEqualForTesting<XfermodePaintFilter>(*this, other);
    case Type::kArithmetic:
      return AreEqualForTesting<ArithmeticPaintFilter>(*this, other);
    case Type::kMatrixConvolution:
      return AreEqualForTesting<MatrixConvolutionPaintFilter>(*this, other);
    case Type::kDisplacementMapEffect:
      return AreEqualForTesting<DisplacementMapEffectPaintFilter>(*this, other);
    case Type::kImage:
      return AreEqualForTesting<ImagePaintFilter>(*this, other);
    case Type::kPaintRecord:
      return AreEqualForTesting<RecordPaintFilter>(*this, other);
    case Type::kMerge:
      return AreEqualForTesting<MergePaintFilter>(*this, other);
    case Type::kMorphology:
      return AreEqualForTesting<MorphologyPaintFilter>(*this, other);
    case Type::kOffset:
      return AreEqualForTesting<OffsetPaintFilter>(*this, other);
    case Type::kTile:
      return AreEqualForTesting<TilePaintFilter>(*this, other);
    case Type::kTurbulence:
      return AreEqualForTesting<TurbulencePaintFilter>(*this, other);
    case Type::kShader:
      return AreEqualForTesting<ShaderPaintFilter>(*this, other);
    case Type::kMatrix:
      return AreEqualForTesting<MatrixPaintFilter>(*this, other);
    case Type::kLightingDistant:
      return AreEqualForTesting<LightingDistantPaintFilter>(*this, other);
    case Type::kLightingPoint:
      return AreEqualForTesting<LightingPointPaintFilter>(*this, other);
    case Type::kLightingSpot:
      return AreEqualForTesting<LightingSpotPaintFilter>(*this, other);
  }
  NOTREACHED();
}

std::vector<sk_sp<SkImageFilter>> PaintFilter::ToSkImageFilters(
    base::span<const sk_sp<PaintFilter>> filters) {
  std::vector<sk_sp<SkImageFilter>> sk_filters;
  sk_filters.reserve(filters.size());
  for (const sk_sp<PaintFilter>& filter : filters) {
    sk_filters.push_back(GetSkFilter(filter.get()));
  }
  return sk_filters;
}

OneInputPaintFilter::OneInputPaintFilter(Type type,
                                         sk_sp<PaintFilter> input,
                                         const CropRect* crop_rect)
    : PaintFilter(type, crop_rect, HasDiscardableImages(input)),
      input_(std::move(input)) {}

OneInputPaintFilter::~OneInputPaintFilter() = default;

gfx::ContentColorUsage OneInputPaintFilter::GetContentColorUsage() const {
  return has_discardable_images() ? input_->GetContentColorUsage()
                                  : gfx::ContentColorUsage::kSRGB;
}

base::CheckedNumeric<size_t> OneInputPaintFilter::BaseSerializedSize() const {
  return PaintFilter::BaseSerializedSize() +
         PaintOpWriter::SerializedSize(input_.get());
}

bool OneInputPaintFilter::EqualsForTesting(
    const OneInputPaintFilter& other) const {
  return AreValuesEqualForTesting(input_, other.input_);  // IN-TEST
}

TwoInputPaintFilter::TwoInputPaintFilter(Type type,
                                         sk_sp<PaintFilter> first,
                                         sk_sp<PaintFilter> second,
                                         const CropRect* crop_rect)
    : PaintFilter(type,
                  crop_rect,
                  HasDiscardableImages(first) || HasDiscardableImages(second)),
      first_(std::move(first)),
      second_(std::move(second)) {}

TwoInputPaintFilter::~TwoInputPaintFilter() = default;

gfx::ContentColorUsage TwoInputPaintFilter::GetContentColorUsage() const {
  if (!has_discardable_images()) {
    return gfx::ContentColorUsage::kSRGB;
  }
  gfx::ContentColorUsage result =
      first_ ? first_->GetContentColorUsage() : gfx::ContentColorUsage::kSRGB;
  if (second_ && result != gfx::ContentColorUsage::kMaxValue) {
    result = std::max(result, second_->GetContentColorUsage());
  }
  return result;
}

base::CheckedNumeric<size_t> TwoInputPaintFilter::BaseSerializedSize() const {
  return PaintFilter::BaseSerializedSize() +
         PaintOpWriter::SerializedSize(first_.get()) +
         PaintOpWriter::SerializedSize(second_.get());
}

bool TwoInputPaintFilter::EqualsForTesting(
    const TwoInputPaintFilter& other) const {
  return AreValuesEqualForTesting(first_, other.first_) &&  // IN-TEST
         AreValuesEqualForTesting(second_, other.second_);  // IN-TEST
}

ColorFilterPaintFilter::ColorFilterPaintFilter(sk_sp<ColorFilter> color_filter,
                                               sk_sp<PaintFilter> input,
                                               const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      color_filter_(std::move(color_filter)) {
  cached_sk_filter_ = SkImageFilters::ColorFilter(
      color_filter_ ? color_filter_->sk_color_filter_ : nullptr,
      GetSkFilter(input_.get()), crop_rect);
}

ColorFilterPaintFilter::~ColorFilterPaintFilter() = default;

size_t ColorFilterPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize();
  total_size += PaintOpWriter::SerializedSize(color_filter_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ColorFilterPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ColorFilterPaintFilter>(
      color_filter_, Snapshot(input_, image_provider), GetCropRect());
}

bool ColorFilterPaintFilter::EqualsForTesting(
    const ColorFilterPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         AreValuesEqualForTesting(color_filter_,  // IN-TEST
                                  other.color_filter_);
}

BlurPaintFilter::BlurPaintFilter(SkScalar sigma_x,
                                 SkScalar sigma_y,
                                 SkTileMode tile_mode,
                                 sk_sp<PaintFilter> input,
                                 const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      tile_mode_(tile_mode) {
  cached_sk_filter_ = SkImageFilters::Blur(
      sigma_x, sigma_y, tile_mode_, GetSkFilter(input_.get()), crop_rect);
}

BlurPaintFilter::~BlurPaintFilter() = default;

size_t BlurPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(sigma_x_) +
      PaintOpWriter::SerializedSize(sigma_y_) +
      PaintOpWriter::SerializedSize(tile_mode_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> BlurPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<BlurPaintFilter>(sigma_x_, sigma_y_, tile_mode_,
                                     Snapshot(input_, image_provider),
                                     GetCropRect());
}

bool BlurPaintFilter::EqualsForTesting(const BlurPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         sigma_x_ == other.sigma_x_ && sigma_y_ == other.sigma_y_ &&
         tile_mode_ == other.tile_mode_;
}

DropShadowPaintFilter::DropShadowPaintFilter(SkScalar dx,
                                             SkScalar dy,
                                             SkScalar sigma_x,
                                             SkScalar sigma_y,
                                             SkColor4f color,
                                             ShadowMode shadow_mode,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      dx_(dx),
      dy_(dy),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      color_(color),
      shadow_mode_(shadow_mode) {
  if (shadow_mode == ShadowMode::kDrawShadowOnly) {
    // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
    cached_sk_filter_ = SkImageFilters::DropShadowOnly(
        dx_, dy_, sigma_x_, sigma_y_, color_.toSkColor(),
        GetSkFilter(input_.get()), crop_rect);
  } else {
    // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
    cached_sk_filter_ = SkImageFilters::DropShadow(
        dx_, dy_, sigma_x_, sigma_y_, color_.toSkColor(),
        GetSkFilter(input_.get()), crop_rect);
  }
}

DropShadowPaintFilter::~DropShadowPaintFilter() = default;

size_t DropShadowPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(dx_) +
      PaintOpWriter::SerializedSize(dy_) +
      PaintOpWriter::SerializedSize(sigma_x_) +
      PaintOpWriter::SerializedSize(sigma_y_) +
      PaintOpWriter::SerializedSize(color_) +
      PaintOpWriter::SerializedSize(shadow_mode_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> DropShadowPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<DropShadowPaintFilter>(
      dx_, dy_, sigma_x_, sigma_y_, color_, shadow_mode_,
      Snapshot(input_, image_provider), GetCropRect());
}

bool DropShadowPaintFilter::EqualsForTesting(
    const DropShadowPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) && dx_ == other.dx_ &&
         dy_ == other.dy_ && sigma_x_ == other.sigma_x_ &&
         sigma_y_ == other.sigma_y_ && color_ == other.color_ &&
         shadow_mode_ == other.shadow_mode_;
}

MagnifierPaintFilter::MagnifierPaintFilter(const SkRect& lens_bounds,
                                           SkScalar zoom_amount,
                                           SkScalar inset,
                                           sk_sp<PaintFilter> input,
                                           const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      lens_bounds_(lens_bounds),
      zoom_amount_(zoom_amount),
      inset_(inset) {
  // Historically the Skia Magnifier filter always used nearest-neighbor
  // sampling internally, when it was only used for the accessibility
  // magnifier widgets (where NN was preferred and always had an integer zoom
  // amount). However, when the zoom amount is not an integer NN severely
  // degrades visual quality. If more refined control is required, the
  // sampling mode can be exposed and plumbed up to FilterOperation.
  SkFilterMode filter_mode = SkScalarIsInt(zoom_amount) ? SkFilterMode::kNearest
                                                        : SkFilterMode::kLinear;
  cached_sk_filter_ =
      SkImageFilters::Magnifier(lens_bounds_, zoom_amount_, inset_, filter_mode,
                                GetSkFilter(input_.get()), crop_rect);
}

MagnifierPaintFilter::~MagnifierPaintFilter() = default;

size_t MagnifierPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(lens_bounds_) +
      PaintOpWriter::SerializedSize(zoom_amount_) +
      PaintOpWriter::SerializedSize(inset_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MagnifierPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MagnifierPaintFilter>(lens_bounds_, zoom_amount_, inset_,
                                          Snapshot(input_, image_provider),
                                          GetCropRect());
}

bool MagnifierPaintFilter::EqualsForTesting(
    const MagnifierPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         lens_bounds_ == other.lens_bounds_ &&
         zoom_amount_ == other.zoom_amount_ && inset_ == other.inset_;
}

ComposePaintFilter::ComposePaintFilter(sk_sp<PaintFilter> outer,
                                       sk_sp<PaintFilter> inner)
    : TwoInputPaintFilter(Type::kCompose, std::move(outer), std::move(inner)) {
  cached_sk_filter_ = SkImageFilters::Compose(GetSkFilter(first_.get()),
                                              GetSkFilter(second_.get()));
}

ComposePaintFilter::~ComposePaintFilter() = default;

size_t ComposePaintFilter::SerializedSize() const {
  return BaseSerializedSize().ValueOrDefault(0u);
}

sk_sp<PaintFilter> ComposePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ComposePaintFilter>(Snapshot(outer(), image_provider),
                                        Snapshot(inner(), image_provider));
}

bool ComposePaintFilter::EqualsForTesting(
    const ComposePaintFilter& other) const {
  return TwoInputPaintFilter::EqualsForTesting(other);
}

AlphaThresholdPaintFilter::AlphaThresholdPaintFilter(const SkRegion& region,
                                                     sk_sp<PaintFilter> input,
                                                     const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect), region_(region) {
  // Historically, Skia had a specialized AlphaThreshold effect that took an
  // inner and outer alpha threshold. If a pixel inside the region had an alpha
  // lower than the inner threshold, its opacity would be increased to that
  // threshold. If a pixel outside the region had an alpha higher than the
  // outer threshold, its opacity would be lowered to that threshold.
  //
  // The actual usage in chrome used an inner and outer threshold of 0, which
  // has the equivalent behavior of leaving pixels inside the region unmodified,
  // and clearing pixels outside the region to transparent black.

  SkRect cull_rect = SkRect::Make(region.getBounds());
  if (crop_rect) {
    if (!cull_rect.intersect(*crop_rect)) {
      cull_rect = SkRect::MakeEmpty();
    }
  }

  if (region.isRect()) {
    // `cull_rect` can entirely represent the threshold effect, so avoid
    // producing a mask image that has to be blended against and just crop it.
    // TODO(michaelludwig): Replace with a dedicated SkImageFilters::Crop once
    // that has been made public, but Offset(0,0) is equivalent.
    cached_sk_filter_ =
        SkImageFilters::Offset(0, 0, GetSkFilter(input_.get()), cull_rect);
  } else {
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(cull_rect, nullptr);
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->drawRegion(region, SkPaint(SkColors::kBlack));
    sk_sp<SkPicture> shape_mask = recorder.finishRecordingAsPicture();
    // kSrcIn multiplies the source (input_) by the dest's alpha (shape_mask)
    cached_sk_filter_ = SkImageFilters::Blend(
        SkBlendMode::kSrcIn,
        /*dst=*/SkImageFilters::Picture(std::move(shape_mask)),
        /*src=*/GetSkFilter(input_.get()), crop_rect);
  }
}

AlphaThresholdPaintFilter::~AlphaThresholdPaintFilter() = default;

size_t AlphaThresholdPaintFilter::SerializedSize() const {
  size_t region_size = region_.writeToMemory(nullptr);
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSizeOfBytes(region_size);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> AlphaThresholdPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<AlphaThresholdPaintFilter>(
      region_, Snapshot(input_, image_provider), GetCropRect());
}

bool AlphaThresholdPaintFilter::EqualsForTesting(
    const AlphaThresholdPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         region_ == other.region_;
}

XfermodePaintFilter::XfermodePaintFilter(SkBlendMode blend_mode,
                                         sk_sp<PaintFilter> background,
                                         sk_sp<PaintFilter> foreground,
                                         const CropRect* crop_rect)
    : TwoInputPaintFilter(kType,
                          std::move(background),
                          std::move(foreground),
                          crop_rect),
      blend_mode_(blend_mode) {
  cached_sk_filter_ =
      SkImageFilters::Blend(blend_mode_, GetSkFilter(first_.get()),
                            GetSkFilter(second_.get()), crop_rect);
}

XfermodePaintFilter::~XfermodePaintFilter() = default;

size_t XfermodePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(blend_mode_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> XfermodePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<XfermodePaintFilter>(
      blend_mode_, Snapshot(background(), image_provider),
      Snapshot(foreground(), image_provider), GetCropRect());
}

bool XfermodePaintFilter::EqualsForTesting(
    const XfermodePaintFilter& other) const {
  return TwoInputPaintFilter::EqualsForTesting(other) &&
         blend_mode_ == other.blend_mode_;
}

ArithmeticPaintFilter::ArithmeticPaintFilter(float k1,
                                             float k2,
                                             float k3,
                                             float k4,
                                             bool enforce_pm_color,
                                             sk_sp<PaintFilter> background,
                                             sk_sp<PaintFilter> foreground,
                                             const CropRect* crop_rect)
    : TwoInputPaintFilter(kType,
                          std::move(background),
                          std::move(foreground),
                          crop_rect),
      k1_(k1),
      k2_(k2),
      k3_(k3),
      k4_(k4),
      enforce_pm_color_(enforce_pm_color) {
  cached_sk_filter_ = SkImageFilters::Arithmetic(
      k1_, k2_, k3_, k4_, enforce_pm_color_, GetSkFilter(first_.get()),
      GetSkFilter(second_.get()), crop_rect);
}

ArithmeticPaintFilter::~ArithmeticPaintFilter() = default;

size_t ArithmeticPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(k1_) +
      PaintOpWriter::SerializedSize(k2_) + PaintOpWriter::SerializedSize(k3_) +
      PaintOpWriter::SerializedSize(k4_) +
      PaintOpWriter::SerializedSize(enforce_pm_color_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ArithmeticPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ArithmeticPaintFilter>(
      k1_, k2_, k3_, k4_, enforce_pm_color_,
      Snapshot(background(), image_provider),
      Snapshot(foreground(), image_provider), GetCropRect());
}

bool ArithmeticPaintFilter::EqualsForTesting(
    const ArithmeticPaintFilter& other) const {
  return TwoInputPaintFilter::EqualsForTesting(other) && k1_ == other.k1_ &&
         k2_ == other.k2_ && k3_ == other.k3_ && k4_ == other.k4_ &&
         enforce_pm_color_ == other.enforce_pm_color_;
}

MatrixConvolutionPaintFilter::MatrixConvolutionPaintFilter(
    const SkISize& kernel_size,
    const SkScalar* kernel,
    SkScalar gain,
    SkScalar bias,
    const SkIPoint& kernel_offset,
    SkTileMode tile_mode,
    bool convolve_alpha,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      kernel_size_(kernel_size),
      gain_(gain),
      bias_(bias),
      kernel_offset_(kernel_offset),
      tile_mode_(tile_mode),
      convolve_alpha_(convolve_alpha) {
  DCHECK(kernel_size_.width() >= 0 && kernel_size_.height() >= 0);
  auto len = static_cast<size_t>(kernel_size_.width()) *
             static_cast<size_t>(kernel_size_.height());
  kernel_.reserve(len);
  for (size_t i = 0; i < len; ++i)
    kernel_.push_back(kernel[i]);

  cached_sk_filter_ = SkImageFilters::MatrixConvolution(
      kernel_size_, kernel, gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, GetSkFilter(input_.get()), crop_rect);
}

MatrixConvolutionPaintFilter::~MatrixConvolutionPaintFilter() = default;

size_t MatrixConvolutionPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(kernel_size_) +
      PaintOpWriter::SerializedSize<size_t>() +
      PaintOpWriter::SerializedSizeOfElements(kernel_.data(), kernel_.size()) +
      PaintOpWriter::SerializedSize(gain_) +
      PaintOpWriter::SerializedSize(bias_) +
      PaintOpWriter::SerializedSize(kernel_offset_) +
      PaintOpWriter::SerializedSize(tile_mode_) +
      PaintOpWriter::SerializedSize(convolve_alpha_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MatrixConvolutionPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MatrixConvolutionPaintFilter>(
      kernel_size_, &kernel_[0], gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, Snapshot(input_, image_provider), GetCropRect());
}

bool MatrixConvolutionPaintFilter::EqualsForTesting(
    const MatrixConvolutionPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         kernel_size_ == other.kernel_size_ &&
         base::ranges::equal(kernel_, other.kernel_) && gain_ == other.gain_ &&
         bias_ == other.bias_ && kernel_offset_ == other.kernel_offset_ &&
         tile_mode_ == other.tile_mode_ &&
         convolve_alpha_ == other.convolve_alpha_;
}

DisplacementMapEffectPaintFilter::DisplacementMapEffectPaintFilter(
    SkColorChannel channel_x,
    SkColorChannel channel_y,
    SkScalar scale,
    sk_sp<PaintFilter> displacement,
    sk_sp<PaintFilter> color,
    const CropRect* crop_rect)
    : TwoInputPaintFilter(kType,
                          std::move(displacement),
                          std::move(color),
                          crop_rect),
      channel_x_(channel_x),
      channel_y_(channel_y),
      scale_(scale) {
  cached_sk_filter_ = SkImageFilters::DisplacementMap(
      channel_x_, channel_y_, scale_, GetSkFilter(first_.get()),
      GetSkFilter(second_.get()), crop_rect);
}

DisplacementMapEffectPaintFilter::~DisplacementMapEffectPaintFilter() = default;

size_t DisplacementMapEffectPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(channel_x_) +
      PaintOpWriter::SerializedSize(channel_y_) +
      PaintOpWriter::SerializedSize(scale_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> DisplacementMapEffectPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<DisplacementMapEffectPaintFilter>(
      channel_x_, channel_y_, scale_, Snapshot(displacement(), image_provider),
      Snapshot(color(), image_provider), GetCropRect());
}

bool DisplacementMapEffectPaintFilter::EqualsForTesting(
    const DisplacementMapEffectPaintFilter& other) const {
  return TwoInputPaintFilter::EqualsForTesting(other) &&
         channel_x_ == other.channel_x_ && channel_y_ == other.channel_y_ &&
         scale_ == other.scale_;
}

ImagePaintFilter::ImagePaintFilter(PaintImage image,
                                   const SkRect& src_rect,
                                   const SkRect& dst_rect,
                                   PaintFlags::FilterQuality filter_quality)
    : PaintFilter(kType, nullptr, !image.IsTextureBacked()),
      image_(std::move(image)),
      src_rect_(src_rect),
      dst_rect_(dst_rect),
      filter_quality_(filter_quality) {
  PaintFlags::ScalingOperation scale =
      (src_rect.height() < dst_rect.height() &&
       src_rect.width() < dst_rect.width())
          ? PaintFlags::ScalingOperation::kUpscale
          : PaintFlags::ScalingOperation::kUnknown;
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(filter_quality, scale));
  cached_sk_filter_ = SkImageFilters::Image(image_.GetSkImage(), src_rect_,
                                            dst_rect_, sampling);
}

ImagePaintFilter::~ImagePaintFilter() = default;

gfx::ContentColorUsage ImagePaintFilter::GetContentColorUsage() const {
  return image_.GetContentColorUsage();
}

size_t ImagePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(src_rect_) +
      PaintOpWriter::SerializedSize(dst_rect_) +
      PaintOpWriter::SerializedSize(filter_quality_);
  total_size += PaintOpWriter::SerializedSize(image_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ImagePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  DrawImage draw_image(image_, false,
                       SkIRect::MakeWH(image_.width(), image_.height()),
                       filter_quality_, SkM44());
  auto scoped_result = image_provider->GetRasterContent(draw_image);
  if (!scoped_result)
    return nullptr;

  auto decoded_sk_image = sk_ref_sp<SkImage>(
      const_cast<SkImage*>(scoped_result.decoded_image().image().get()));
  PaintImage decoded_paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(image_.stable_id())
          .set_texture_image(decoded_sk_image, PaintImage::GetNextContentId())
          .TakePaintImage();

  return sk_make_sp<ImagePaintFilter>(std::move(decoded_paint_image), src_rect_,
                                      dst_rect_, filter_quality_);
}

bool ImagePaintFilter::EqualsForTesting(const ImagePaintFilter& other) const {
  return !!image_ == !!other.image_ && src_rect_ == other.src_rect_ &&
         dst_rect_ == other.dst_rect_ &&
         filter_quality_ == other.filter_quality_;
}

RecordPaintFilter::RecordPaintFilter(PaintRecord record,
                                     const SkRect& record_bounds,
                                     const gfx::SizeF& raster_scale,
                                     ScalingBehavior scaling_behavior)
    : RecordPaintFilter(std::move(record),
                        record_bounds,
                        raster_scale,
                        scaling_behavior,
                        nullptr) {}

RecordPaintFilter::RecordPaintFilter(PaintRecord record,
                                     const SkRect& record_bounds,
                                     const gfx::SizeF& raster_scale,
                                     ScalingBehavior scaling_behavior,
                                     ImageProvider* image_provider)
    : PaintFilter(kType, nullptr, record.has_discardable_images()),
      record_(std::move(record)),
      record_bounds_(record_bounds),
      raster_scale_(raster_scale),
      scaling_behavior_(scaling_behavior) {
  DCHECK(raster_scale_.width() > 0.f && raster_scale_.height() > 0.f);
  DCHECK(scaling_behavior == ScalingBehavior::kFixedScale ||
         (raster_scale_.width() == 1.f && raster_scale_.height() == 1.f));

  sk_sp<SkPicture> picture =
      record_.ToSkPicture(record_bounds_, image_provider);

  if (scaling_behavior == ScalingBehavior::kRasterAtScale ||
      record_bounds_.isEmpty()) {
    cached_sk_filter_ = SkImageFilters::Picture(std::move(picture));
  } else {
    DCHECK(scaling_behavior == ScalingBehavior::kFixedScale);

    // Convert the record to an image at the scaled resolution, but draw it in
    // the filter DAG at the original record bounds.
    int width = SkScalarCeilToInt(record_bounds.width());
    int height = SkScalarCeilToInt(record_bounds.height());
    SkMatrix originAdjust =
        SkMatrix::Translate(-record_bounds.fLeft, -record_bounds.fTop);
    auto image = SkImages::DeferredFromPicture(
        std::move(picture), SkISize::Make(width, height), &originAdjust,
        nullptr, SkImages::BitDepth::kU8, SkColorSpace::MakeSRGB());

    // Must account for the raster scale when drawing the picture image,
    SkRect src = SkRect::MakeWH(record_bounds.width(), record_bounds.height());
    SkScalar inv_x = 1.f / raster_scale_.width();
    SkScalar inv_y = 1.f / raster_scale_.height();
    SkRect dst = {inv_x * record_bounds.fLeft, inv_y * record_bounds.fTop,
                  inv_x * record_bounds.fRight, inv_y * record_bounds.fBottom};

    // Use Mitchell cubic filter, matching historic
    // PaintFlags::FilterQuality::kHigh
    SkSamplingOptions sampling(SkCubicResampler::Mitchell());
    cached_sk_filter_ =
        SkImageFilters::Image(std::move(image), src, dst, sampling);
  }
}

RecordPaintFilter::~RecordPaintFilter() = default;

gfx::ContentColorUsage RecordPaintFilter::GetContentColorUsage() const {
  return record_.content_color_usage();
}

sk_sp<RecordPaintFilter> RecordPaintFilter::CreateScaledPaintRecord(
    const SkMatrix& ctm,
    int max_texture_size) const {
  // If this is already fixed scale, then this is already good to go, and if
  // the bounds are empty the filter produces no output so keep it as-is.
  if (scaling_behavior_ == ScalingBehavior::kFixedScale ||
      record_bounds_.isEmpty()) {
    return sk_ref_sp<RecordPaintFilter>(this);
  }

  // For creating a deserialized RecordPaintFilter, extract the scale factor at
  // which it would have been rasterized at for the given ctm. This is modeled
  // after PaintShader::CreateScaledPaintRecord.
  SkRect scaled_record_bounds =
      PaintRecord::GetFixedScaleBounds(ctm, record_bounds_, max_texture_size);
  gfx::SizeF raster_scale = {
      scaled_record_bounds.width() / record_bounds_.width(),
      scaled_record_bounds.height() / record_bounds_.height()};
  if (raster_scale.IsEmpty())
    return nullptr;

  return sk_make_sp<RecordPaintFilter>(record_, scaled_record_bounds,
                                       raster_scale,
                                       ScalingBehavior::kFixedScale);
}

size_t RecordPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(record_bounds_) +
      PaintOpWriter::SerializedSize(raster_scale_) +
      PaintOpWriter::SerializedSize(scaling_behavior_);
  total_size += PaintOpWriter::SerializedSize(record_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> RecordPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_sp<RecordPaintFilter>(
      new RecordPaintFilter(record_, record_bounds_, raster_scale_,
                            scaling_behavior_, image_provider));
}

bool RecordPaintFilter::EqualsForTesting(const RecordPaintFilter& other) const {
  return record_.EqualsForTesting(other.record_) &&  // IN-TEST
         scaling_behavior_ == other.scaling_behavior_ &&
         raster_scale_ == other.raster_scale_ &&
         record_bounds_ == other.record_bounds_;
}

MergePaintFilter::MergePaintFilter(const sk_sp<PaintFilter>* const filters,
                                   int count,
                                   const CropRect* crop_rect)
    : MergePaintFilter(filters, count, crop_rect, nullptr) {}

MergePaintFilter::MergePaintFilter(const sk_sp<PaintFilter>* const filters,
                                   int count,
                                   const CropRect* crop_rect,
                                   ImageProvider* image_provider)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(filters, count)) {
  std::vector<sk_sp<SkImageFilter>> sk_filters;
  sk_filters.reserve(count);

  for (int i = 0; i < count; ++i) {
    auto filter =
        image_provider ? Snapshot(filters[i], image_provider) : filters[i];
    inputs_.push_back(std::move(filter));
    sk_filters.push_back(GetSkFilter(inputs_.back().get()));
  }

  cached_sk_filter_ = SkImageFilters::Merge(
      static_cast<sk_sp<SkImageFilter>*>(sk_filters.data()), count, crop_rect);
}

MergePaintFilter::~MergePaintFilter() = default;

gfx::ContentColorUsage MergePaintFilter::GetContentColorUsage() const {
  gfx::ContentColorUsage result = gfx::ContentColorUsage::kSRGB;
  if (!has_discardable_images()) {
    return result;
  }
  for (const auto& input : inputs_) {
    if (input) {
      result = std::max(result, input->GetContentColorUsage());
      if (result == gfx::ContentColorUsage::kMaxValue) {
        break;
      }
    }
  }
  return result;
}

size_t MergePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize();
  total_size += PaintOpWriter::SerializedSize(input_count());
  for (size_t i = 0; i < input_count(); ++i) {
    total_size += PaintOpWriter::SerializedSize(input_at(i));
  }
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MergePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_sp<MergePaintFilter>(new MergePaintFilter(
      &inputs_[0], inputs_.size(), GetCropRect(), image_provider));
}

bool MergePaintFilter::EqualsForTesting(const MergePaintFilter& other) const {
  return base::ranges::equal(
      inputs_, other.inputs_, [](const auto& a, const auto& b) {
        return AreValuesEqualForTesting(a, b);  // IN-TEST
      });
}

MorphologyPaintFilter::MorphologyPaintFilter(MorphType morph_type,
                                             float radius_x,
                                             float radius_y,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      morph_type_(morph_type),
      radius_x_(radius_x),
      radius_y_(radius_y) {
  switch (morph_type_) {
    case MorphType::kDilate:
      cached_sk_filter_ = SkImageFilters::Dilate(
          radius_x_, radius_y_, GetSkFilter(input_.get()), crop_rect);
      break;
    case MorphType::kErode:
      cached_sk_filter_ = SkImageFilters::Erode(
          radius_x_, radius_y_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

MorphologyPaintFilter::~MorphologyPaintFilter() = default;

size_t MorphologyPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(morph_type_) +
      PaintOpWriter::SerializedSize(radius_x_) +
      PaintOpWriter::SerializedSize(radius_y_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MorphologyPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MorphologyPaintFilter>(morph_type_, radius_x_, radius_y_,
                                           Snapshot(input_, image_provider),
                                           GetCropRect());
}

bool MorphologyPaintFilter::EqualsForTesting(
    const MorphologyPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         morph_type_ == other.morph_type_ && radius_x_ == other.radius_x_ &&
         radius_y_ == other.radius_y_;
}

OffsetPaintFilter::OffsetPaintFilter(SkScalar dx,
                                     SkScalar dy,
                                     sk_sp<PaintFilter> input,
                                     const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      dx_(dx),
      dy_(dy) {
  cached_sk_filter_ =
      SkImageFilters::Offset(dx_, dy_, GetSkFilter(input_.get()), crop_rect);
}

OffsetPaintFilter::~OffsetPaintFilter() = default;

size_t OffsetPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize() +
                                            PaintOpWriter::SerializedSize(dx_) +
                                            PaintOpWriter::SerializedSize(dy_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> OffsetPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<OffsetPaintFilter>(
      dx_, dy_, Snapshot(input_, image_provider), GetCropRect());
}

bool OffsetPaintFilter::EqualsForTesting(const OffsetPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) && dx_ == other.dx_ &&
         dy_ == other.dy_;
}

TilePaintFilter::TilePaintFilter(const SkRect& src,
                                 const SkRect& dst,
                                 sk_sp<PaintFilter> input)
    : OneInputPaintFilter(kType, std::move(input)), src_(src), dst_(dst) {
  cached_sk_filter_ =
      SkImageFilters::Tile(src_, dst_, GetSkFilter(input_.get()));
}

TilePaintFilter::~TilePaintFilter() = default;

size_t TilePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(src_) +
      PaintOpWriter::SerializedSize(dst_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> TilePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<TilePaintFilter>(src_, dst_,
                                     Snapshot(input_, image_provider));
}

bool TilePaintFilter::EqualsForTesting(const TilePaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) && src_ == other.src_ &&
         dst_ == other.dst_;
}

TurbulencePaintFilter::TurbulencePaintFilter(TurbulenceType turbulence_type,
                                             SkScalar base_frequency_x,
                                             SkScalar base_frequency_y,
                                             int num_octaves,
                                             SkScalar seed,
                                             const SkISize* tile_size,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, kHasNoDiscardableImages),
      turbulence_type_(turbulence_type),
      base_frequency_x_(base_frequency_x),
      base_frequency_y_(base_frequency_y),
      num_octaves_(num_octaves),
      seed_(seed),
      tile_size_(tile_size ? *tile_size : SkISize::MakeEmpty()) {
  sk_sp<SkShader> shader;
  switch (turbulence_type_) {
    case TurbulenceType::kTurbulence:
      shader = SkShaders::MakeTurbulence(base_frequency_x_, base_frequency_y_,
                                         num_octaves_, seed_, &tile_size_);
      break;
    case TurbulenceType::kFractalNoise:
      shader = SkShaders::MakeFractalNoise(base_frequency_x_, base_frequency_y_,
                                           num_octaves_, seed_, &tile_size_);
      break;
  }

  cached_sk_filter_ = SkImageFilters::Shader(std::move(shader), crop_rect);
}

TurbulencePaintFilter::~TurbulencePaintFilter() = default;

gfx::ContentColorUsage TurbulencePaintFilter::GetContentColorUsage() const {
  return gfx::ContentColorUsage::kSRGB;
}

size_t TurbulencePaintFilter::SerializedSize() const {
  return (BaseSerializedSize() +
          PaintOpWriter::SerializedSize(turbulence_type_) +
          PaintOpWriter::SerializedSize(base_frequency_x_) +
          PaintOpWriter::SerializedSize(base_frequency_y_) +
          PaintOpWriter::SerializedSize(num_octaves_) +
          PaintOpWriter::SerializedSize(seed_) +
          PaintOpWriter::SerializedSize(tile_size_))
      .ValueOrDefault(0u);
}

sk_sp<PaintFilter> TurbulencePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<TurbulencePaintFilter>(turbulence_type_, base_frequency_x_,
                                           base_frequency_y_, num_octaves_,
                                           seed_, &tile_size_, GetCropRect());
}

bool TurbulencePaintFilter::EqualsForTesting(
    const TurbulencePaintFilter& other) const {
  return turbulence_type_ == other.turbulence_type_ &&
         base_frequency_x_ == other.base_frequency_x_ &&
         base_frequency_y_ == other.base_frequency_y_ &&
         num_octaves_ == other.num_octaves_ && seed_ == other.seed_ &&
         tile_size_ == other.tile_size_;
}

ShaderPaintFilter::ShaderPaintFilter(sk_sp<PaintShader> shader,
                                     float alpha,
                                     PaintFlags::FilterQuality filter_quality,
                                     SkImageFilters::Dither dither,
                                     const CropRect* crop_rect)
    : PaintFilter(
          kType,
          crop_rect,
          shader->HasDiscardableImages(/*content_color_usage=*/nullptr)),
      shader_(std::move(shader)),
      alpha_(alpha),
      filter_quality_(filter_quality),
      dither_(dither) {
  sk_sp<SkShader> sk_shader = shader_->GetSkShader(filter_quality_);
  // Combine the alpha multiply into the SkShader if it's not opaque
  if (alpha < 1.0f) {
    // The blend effectively produces (shader * alpha), the rgb of the secondary
    // color are ignored.
    SkColor4f color{1.0f, 1.0f, 1.0f, alpha};
    // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
    sk_shader = SkShaders::Blend(SkBlendMode::kDstIn, std::move(sk_shader),
                                 SkShaders::Color(color.toSkColor()));
  }

  cached_sk_filter_ =
      SkImageFilters::Shader(std::move(sk_shader), dither, crop_rect);
}

ShaderPaintFilter::~ShaderPaintFilter() = default;

gfx::ContentColorUsage ShaderPaintFilter::GetContentColorUsage() const {
  gfx::ContentColorUsage result = gfx::ContentColorUsage::kSRGB;
  if (has_discardable_images()) {
    shader_->HasDiscardableImages(&result);
  }
  return result;
}

size_t ShaderPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize();
  total_size += PaintShader::GetSerializedSize(shader_.get());
  total_size += PaintOpWriter::SerializedSize(alpha_);
  total_size +=
      PaintOpWriter::SerializedSize(filter_quality_);  // filter quality
  total_size += PaintOpWriter::SerializedSize(dither_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ShaderPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  PaintFlags orig_flags;
  orig_flags.setShader(shader_);
  orig_flags.setAlphaf(alpha_);
  orig_flags.setFilterQuality(filter_quality_);
  orig_flags.setDither(dither_ == SkImageFilters::Dither::kYes);

  ScopedRasterFlags raster_flags(&orig_flags, image_provider, SkMatrix::I(), 0,
                                 1.0f);
  const PaintFlags* snapshot = raster_flags.flags();
  if (snapshot) {
    // Ref the updated paint shader so that it can outlive ScopedRasterFlags
    return sk_make_sp<ShaderPaintFilter>(
        sk_ref_sp(snapshot->getShader()), snapshot->getAlphaf(),
        snapshot->getFilterQuality(),
        snapshot->isDither() ? Dither::kYes : Dither::kNo, GetCropRect());
  } else {
    // If decode failed, then just fallback to the solid color
    return sk_make_sp<ShaderPaintFilter>(nullptr, alpha_, filter_quality_,
                                         dither_, GetCropRect());
  }
}

bool ShaderPaintFilter::EqualsForTesting(const ShaderPaintFilter& other) const {
  DCHECK(shader_ && other.shader_);
  return alpha_ == other.alpha_ && filter_quality_ == other.filter_quality_ &&
         dither_ == other.dither_ &&
         shader_->EqualsForTesting(*other.shader_);  // IN-TEST
}

MatrixPaintFilter::MatrixPaintFilter(const SkMatrix& matrix,
                                     PaintFlags::FilterQuality filter_quality,
                                     sk_sp<PaintFilter> input)
    : OneInputPaintFilter(Type::kMatrix, std::move(input)),
      matrix_(matrix),
      filter_quality_(filter_quality) {
  SkSize scale;
  PaintFlags::ScalingOperation scaling_option =
      PaintFlags::ScalingOperation::kUnknown;
  if (matrix_.decomposeScale(&scale)) {
    scaling_option = (scale.width() > 1 && scale.height() > 1)
                         ? PaintFlags::ScalingOperation::kUpscale
                         : PaintFlags::ScalingOperation::kUnknown;
  }

  cached_sk_filter_ = SkImageFilters::MatrixTransform(
      matrix_,
      PaintFlags::FilterQualityToSkSamplingOptions(filter_quality_,
                                                   scaling_option),
      GetSkFilter(input_.get()));
}

MatrixPaintFilter::~MatrixPaintFilter() = default;

size_t MatrixPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(matrix_) +
      PaintOpWriter::SerializedSize(filter_quality_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MatrixPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MatrixPaintFilter>(matrix_, filter_quality_,
                                       Snapshot(input_, image_provider));
}

bool MatrixPaintFilter::EqualsForTesting(const MatrixPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         matrix_ == other.matrix_ && filter_quality_ == other.filter_quality_;
}

LightingDistantPaintFilter::LightingDistantPaintFilter(
    LightingType lighting_type,
    const SkPoint3& direction,
    SkColor4f light_color,
    SkScalar surface_scale,
    SkScalar kconstant,
    SkScalar shininess,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      lighting_type_(lighting_type),
      direction_(direction),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::DistantLitDiffuse(
          direction_, light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::DistantLitSpecular(
          direction_, light_color_.toSkColor(), surface_scale_, kconstant_,
          shininess_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingDistantPaintFilter::~LightingDistantPaintFilter() = default;

size_t LightingDistantPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(lighting_type_) +
      PaintOpWriter::SerializedSize(direction_) +
      PaintOpWriter::SerializedSize(light_color_) +
      PaintOpWriter::SerializedSize(surface_scale_) +
      PaintOpWriter::SerializedSize(kconstant_) +
      PaintOpWriter::SerializedSize(shininess_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingDistantPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingDistantPaintFilter>(
      lighting_type_, direction_, light_color_, surface_scale_, kconstant_,
      shininess_, Snapshot(input_, image_provider), GetCropRect());
}

bool LightingDistantPaintFilter::EqualsForTesting(
    const LightingDistantPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         lighting_type_ == other.lighting_type_ &&
         direction_ == other.direction_ && light_color_ == other.light_color_ &&
         surface_scale_ == other.surface_scale_ &&
         kconstant_ == other.kconstant_ && shininess_ == other.shininess_;
}

LightingPointPaintFilter::LightingPointPaintFilter(LightingType lighting_type,
                                                   const SkPoint3& location,
                                                   SkColor4f light_color,
                                                   SkScalar surface_scale,
                                                   SkScalar kconstant,
                                                   SkScalar shininess,
                                                   sk_sp<PaintFilter> input,
                                                   const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      lighting_type_(lighting_type),
      location_(location),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::PointLitDiffuse(
          location_, light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::PointLitSpecular(
          location_, light_color_.toSkColor(), surface_scale_, kconstant_,
          shininess_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingPointPaintFilter::~LightingPointPaintFilter() = default;

size_t LightingPointPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(lighting_type_) +
      PaintOpWriter::SerializedSize(location_) +
      PaintOpWriter::SerializedSize(light_color_) +
      PaintOpWriter::SerializedSize(surface_scale_) +
      PaintOpWriter::SerializedSize(kconstant_) +
      PaintOpWriter::SerializedSize(shininess_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingPointPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingPointPaintFilter>(
      lighting_type_, location_, light_color_, surface_scale_, kconstant_,
      shininess_, Snapshot(input_, image_provider), GetCropRect());
}

bool LightingPointPaintFilter::EqualsForTesting(
    const LightingPointPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         lighting_type_ == other.lighting_type_ &&
         location_ == other.location_ && light_color_ == other.light_color_ &&
         surface_scale_ == other.surface_scale_ &&
         kconstant_ == other.kconstant_ && shininess_ == other.shininess_;
}

LightingSpotPaintFilter::LightingSpotPaintFilter(LightingType lighting_type,
                                                 const SkPoint3& location,
                                                 const SkPoint3& target,
                                                 SkScalar specular_exponent,
                                                 SkScalar cutoff_angle,
                                                 SkColor4f light_color,
                                                 SkScalar surface_scale,
                                                 SkScalar kconstant,
                                                 SkScalar shininess,
                                                 sk_sp<PaintFilter> input,
                                                 const CropRect* crop_rect)
    : OneInputPaintFilter(kType, std::move(input), crop_rect),
      lighting_type_(lighting_type),
      location_(location),
      target_(target),
      specular_exponent_(specular_exponent),
      cutoff_angle_(cutoff_angle),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::SpotLitDiffuse(
          location_, target_, specular_exponent_, cutoff_angle_,
          light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::SpotLitSpecular(
          location_, target_, specular_exponent_, cutoff_angle_,
          light_color_.toSkColor(), surface_scale_, kconstant_, shininess_,
          GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingSpotPaintFilter::~LightingSpotPaintFilter() = default;

size_t LightingSpotPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + PaintOpWriter::SerializedSize(lighting_type_) +
      PaintOpWriter::SerializedSize(location_) +
      PaintOpWriter::SerializedSize(target_) +
      PaintOpWriter::SerializedSize(specular_exponent_) +
      PaintOpWriter::SerializedSize(cutoff_angle_) +
      PaintOpWriter::SerializedSize(light_color_) +
      PaintOpWriter::SerializedSize(surface_scale_) +
      PaintOpWriter::SerializedSize(kconstant_) +
      PaintOpWriter::SerializedSize(shininess_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingSpotPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingSpotPaintFilter>(
      lighting_type_, location_, target_, specular_exponent_, cutoff_angle_,
      light_color_, surface_scale_, kconstant_, shininess_,
      Snapshot(input_, image_provider), GetCropRect());
}

bool LightingSpotPaintFilter::EqualsForTesting(
    const LightingSpotPaintFilter& other) const {
  return OneInputPaintFilter::EqualsForTesting(other) &&
         lighting_type_ == other.lighting_type_ &&
         location_ == other.location_ && target_ == other.target_ &&
         specular_exponent_ == other.specular_exponent_ &&
         cutoff_angle_ == other.cutoff_angle_ &&
         light_color_ == other.light_color_ &&
         surface_scale_ == other.surface_scale_ &&
         kconstant_ == other.kconstant_ && shininess_ == other.shininess_;
}

}  // namespace cc
