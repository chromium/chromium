// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_filter.h"

#include <string>
#include <utility>
#include <vector>

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
#include "third_party/skia/include/core/SkMath.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/effects/SkPerlinNoiseShader.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"

namespace cc {
namespace {
const bool kHasNoDiscardableImages = false;

bool AreScalarsEqual(SkScalar one, SkScalar two) {
  return PaintOp::AreEqualEvenIfNaN(one, two);
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
  return "Unknown";
}

const PaintFilter::CropRect* PaintFilter::GetCropRect() const {
  return base::OptionalToPtr(crop_rect_);
}

size_t PaintFilter::GetFilterSize(const PaintFilter* filter) {
  // A null type is used to indicate no filter.
  if (!filter)
    return sizeof(uint32_t);
  return filter->SerializedSize() + PaintOpWriter::Alignment();
}

size_t PaintFilter::BaseSerializedSize() const {
  size_t total_size = 0u;
  // Filter type.
  total_size += sizeof(uint32_t);
  // Bool to indicate whether crop exists.
  total_size += sizeof(uint32_t);
  if (crop_rect_) {
    // CropRect.
    total_size += sizeof(*crop_rect_);
  }
  return total_size;
}

sk_sp<PaintFilter> PaintFilter::SnapshotWithImages(
    ImageProvider* image_provider) const {
  if (!has_discardable_images_)
    return sk_ref_sp<PaintFilter>(this);
  return SnapshotWithImagesInternal(image_provider);
}

bool PaintFilter::operator==(const PaintFilter& other) const {
  if (type_ != other.type_)
    return false;
  if (!!crop_rect_ != !!other.crop_rect_)
    return false;
  if (crop_rect_) {
    if (!PaintOp::AreSkRectsEqual(*crop_rect_, *other.crop_rect_)) {
      return false;
    }
  }

  switch (type_) {
    case Type::kNullFilter:
      return true;
    case Type::kColorFilter:
      return *static_cast<const ColorFilterPaintFilter*>(this) ==
             static_cast<const ColorFilterPaintFilter&>(other);
    case Type::kBlur:
      return *static_cast<const BlurPaintFilter*>(this) ==
             static_cast<const BlurPaintFilter&>(other);
    case Type::kDropShadow:
      return *static_cast<const DropShadowPaintFilter*>(this) ==
             static_cast<const DropShadowPaintFilter&>(other);
    case Type::kMagnifier:
      return *static_cast<const MagnifierPaintFilter*>(this) ==
             static_cast<const MagnifierPaintFilter&>(other);
    case Type::kCompose:
      return *static_cast<const ComposePaintFilter*>(this) ==
             static_cast<const ComposePaintFilter&>(other);
    case Type::kAlphaThreshold:
      return *static_cast<const AlphaThresholdPaintFilter*>(this) ==
             static_cast<const AlphaThresholdPaintFilter&>(other);
    case Type::kXfermode:
      return *static_cast<const XfermodePaintFilter*>(this) ==
             static_cast<const XfermodePaintFilter&>(other);
    case Type::kArithmetic:
      return *static_cast<const ArithmeticPaintFilter*>(this) ==
             static_cast<const ArithmeticPaintFilter&>(other);
    case Type::kMatrixConvolution:
      return *static_cast<const MatrixConvolutionPaintFilter*>(this) ==
             static_cast<const MatrixConvolutionPaintFilter&>(other);
    case Type::kDisplacementMapEffect:
      return *static_cast<const DisplacementMapEffectPaintFilter*>(this) ==
             static_cast<const DisplacementMapEffectPaintFilter&>(other);
    case Type::kImage:
      return *static_cast<const ImagePaintFilter*>(this) ==
             static_cast<const ImagePaintFilter&>(other);
    case Type::kPaintRecord:
      return *static_cast<const RecordPaintFilter*>(this) ==
             static_cast<const RecordPaintFilter&>(other);
    case Type::kMerge:
      return *static_cast<const MergePaintFilter*>(this) ==
             static_cast<const MergePaintFilter&>(other);
    case Type::kMorphology:
      return *static_cast<const MorphologyPaintFilter*>(this) ==
             static_cast<const MorphologyPaintFilter&>(other);
    case Type::kOffset:
      return *static_cast<const OffsetPaintFilter*>(this) ==
             static_cast<const OffsetPaintFilter&>(other);
    case Type::kTile:
      return *static_cast<const TilePaintFilter*>(this) ==
             static_cast<const TilePaintFilter&>(other);
    case Type::kTurbulence:
      return *static_cast<const TurbulencePaintFilter*>(this) ==
             static_cast<const TurbulencePaintFilter&>(other);
    case Type::kShader:
      return *static_cast<const ShaderPaintFilter*>(this) ==
             static_cast<const ShaderPaintFilter&>(other);
    case Type::kMatrix:
      return *static_cast<const MatrixPaintFilter*>(this) ==
             static_cast<const MatrixPaintFilter&>(other);
    case Type::kLightingDistant:
      return *static_cast<const LightingDistantPaintFilter*>(this) ==
             static_cast<const LightingDistantPaintFilter&>(other);
    case Type::kLightingPoint:
      return *static_cast<const LightingPointPaintFilter*>(this) ==
             static_cast<const LightingPointPaintFilter&>(other);
    case Type::kLightingSpot:
      return *static_cast<const LightingSpotPaintFilter*>(this) ==
             static_cast<const LightingSpotPaintFilter&>(other);
  }
  NOTREACHED();
  return true;
}

ColorFilterPaintFilter::ColorFilterPaintFilter(
    sk_sp<SkColorFilter> color_filter,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      color_filter_(std::move(color_filter)),
      input_(std::move(input)) {
  DCHECK(color_filter_);
  cached_sk_filter_ = SkImageFilters::ColorFilter(
      color_filter_, GetSkFilter(input_.get()), crop_rect);
}

ColorFilterPaintFilter::~ColorFilterPaintFilter() = default;

size_t ColorFilterPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = 0u;
  total_size += BaseSerializedSize();
  total_size += PaintOpWriter::GetFlattenableSize(color_filter_.get());
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ColorFilterPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ColorFilterPaintFilter>(
      color_filter_, Snapshot(input_, image_provider), GetCropRect());
}

bool ColorFilterPaintFilter::operator==(
    const ColorFilterPaintFilter& other) const {
  return PaintOp::AreSkFlattenablesEqual(color_filter_.get(),
                                         other.color_filter_.get()) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

BlurPaintFilter::BlurPaintFilter(SkScalar sigma_x,
                                 SkScalar sigma_y,
                                 SkTileMode tile_mode,
                                 sk_sp<PaintFilter> input,
                                 const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      tile_mode_(tile_mode),
      input_(std::move(input)) {
  cached_sk_filter_ = SkImageFilters::Blur(
      sigma_x, sigma_y, tile_mode_, GetSkFilter(input_.get()), crop_rect);
}

BlurPaintFilter::~BlurPaintFilter() = default;

size_t BlurPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(sigma_x_) + sizeof(sigma_y_) +
      sizeof(tile_mode_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> BlurPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<BlurPaintFilter>(sigma_x_, sigma_y_, tile_mode_,
                                     Snapshot(input_, image_provider),
                                     GetCropRect());
}

bool BlurPaintFilter::operator==(const BlurPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(sigma_x_, other.sigma_x_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_y_, other.sigma_y_) &&
         tile_mode_ == other.tile_mode_ &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

DropShadowPaintFilter::DropShadowPaintFilter(SkScalar dx,
                                             SkScalar dy,
                                             SkScalar sigma_x,
                                             SkScalar sigma_y,
                                             SkColor4f color,
                                             ShadowMode shadow_mode,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      dx_(dx),
      dy_(dy),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      color_(color),
      shadow_mode_(shadow_mode),
      input_(std::move(input)) {
  if (shadow_mode == ShadowMode::kDrawShadowOnly) {
    // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
    cached_sk_filter_ = SkImageFilters::DropShadowOnly(
        dx_, dy_, sigma_x_, sigma_y_, color_.toSkColor(),
        GetSkFilter(input_.get()), crop_rect);
  } else {
    // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
    cached_sk_filter_ = SkImageFilters::DropShadow(
        dx_, dy_, sigma_x_, sigma_y_, color_.toSkColor(),
        GetSkFilter(input_.get()), crop_rect);
  }
}

DropShadowPaintFilter::~DropShadowPaintFilter() = default;

size_t DropShadowPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(dx_) + sizeof(dy_) + sizeof(sigma_x_) +
      sizeof(sigma_y_) + sizeof(color_) + sizeof(shadow_mode_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> DropShadowPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<DropShadowPaintFilter>(
      dx_, dy_, sigma_x_, sigma_y_, color_, shadow_mode_,
      Snapshot(input_, image_provider), GetCropRect());
}

bool DropShadowPaintFilter::operator==(
    const DropShadowPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(dx_, other.dx_) &&
         PaintOp::AreEqualEvenIfNaN(dy_, other.dy_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_x_, other.sigma_x_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_y_, other.sigma_y_) &&
         color_ == other.color_ && shadow_mode_ == other.shadow_mode_ &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

MagnifierPaintFilter::MagnifierPaintFilter(const SkRect& src_rect,
                                           SkScalar inset,
                                           sk_sp<PaintFilter> input,
                                           const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      src_rect_(src_rect),
      inset_(inset),
      input_(std::move(input)) {
  cached_sk_filter_ = SkImageFilters::Magnifier(
      src_rect_, inset_, GetSkFilter(input_.get()), crop_rect);
}

MagnifierPaintFilter::~MagnifierPaintFilter() = default;

size_t MagnifierPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(src_rect_) + sizeof(inset_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MagnifierPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MagnifierPaintFilter>(
      src_rect_, inset_, Snapshot(input_, image_provider), GetCropRect());
}

bool MagnifierPaintFilter::operator==(const MagnifierPaintFilter& other) const {
  return PaintOp::AreSkRectsEqual(src_rect_, other.src_rect_) &&
         PaintOp::AreEqualEvenIfNaN(inset_, other.inset_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

ComposePaintFilter::ComposePaintFilter(sk_sp<PaintFilter> outer,
                                       sk_sp<PaintFilter> inner)
    : PaintFilter(Type::kCompose,
                  nullptr,
                  HasDiscardableImages(outer) || HasDiscardableImages(inner)),
      outer_(std::move(outer)),
      inner_(std::move(inner)) {
  cached_sk_filter_ = SkImageFilters::Compose(GetSkFilter(outer_.get()),
                                              GetSkFilter(inner_.get()));
}

ComposePaintFilter::~ComposePaintFilter() = default;

size_t ComposePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize();
  total_size += GetFilterSize(outer_.get());
  total_size += GetFilterSize(inner_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ComposePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ComposePaintFilter>(Snapshot(outer_, image_provider),
                                        Snapshot(inner_, image_provider));
}

bool ComposePaintFilter::operator==(const ComposePaintFilter& other) const {
  return base::ValuesEquivalent(outer_.get(), other.outer_.get()) &&
         base::ValuesEquivalent(inner_.get(), other.inner_.get());
}

AlphaThresholdPaintFilter::AlphaThresholdPaintFilter(const SkRegion& region,
                                                     SkScalar inner_min,
                                                     SkScalar outer_max,
                                                     sk_sp<PaintFilter> input,
                                                     const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      region_(region),
      inner_min_(inner_min),
      outer_max_(outer_max),
      input_(std::move(input)) {
  cached_sk_filter_ = SkImageFilters::AlphaThreshold(
      region_, inner_min_, outer_max_, GetSkFilter(input_.get()), crop_rect);
}

AlphaThresholdPaintFilter::~AlphaThresholdPaintFilter() = default;

size_t AlphaThresholdPaintFilter::SerializedSize() const {
  size_t region_size = region_.writeToMemory(nullptr);
  base::CheckedNumeric<size_t> total_size;
  total_size = BaseSerializedSize() + sizeof(uint64_t) +
               base::bits::AlignUp(region_size, PaintOpWriter::Alignment()) +
               sizeof(inner_min_) + sizeof(outer_max_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> AlphaThresholdPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<AlphaThresholdPaintFilter>(region_, inner_min_, outer_max_,
                                               Snapshot(input_, image_provider),
                                               GetCropRect());
}

bool AlphaThresholdPaintFilter::operator==(
    const AlphaThresholdPaintFilter& other) const {
  return region_ == other.region_ &&
         PaintOp::AreEqualEvenIfNaN(inner_min_, other.inner_min_) &&
         PaintOp::AreEqualEvenIfNaN(outer_max_, other.outer_max_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

XfermodePaintFilter::XfermodePaintFilter(SkBlendMode blend_mode,
                                         sk_sp<PaintFilter> background,
                                         sk_sp<PaintFilter> foreground,
                                         const CropRect* crop_rect)
    : PaintFilter(
          kType,
          crop_rect,
          HasDiscardableImages(background) || HasDiscardableImages(foreground)),
      blend_mode_(blend_mode),
      background_(std::move(background)),
      foreground_(std::move(foreground)) {
  cached_sk_filter_ =
      SkImageFilters::Blend(blend_mode_, GetSkFilter(background_.get()),
                            GetSkFilter(foreground_.get()), crop_rect);
}

XfermodePaintFilter::~XfermodePaintFilter() = default;

size_t XfermodePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(blend_mode_);
  total_size += GetFilterSize(background_.get());
  total_size += GetFilterSize(foreground_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> XfermodePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<XfermodePaintFilter>(
      blend_mode_, Snapshot(background_, image_provider),
      Snapshot(foreground_, image_provider), GetCropRect());
}

bool XfermodePaintFilter::operator==(const XfermodePaintFilter& other) const {
  return blend_mode_ == other.blend_mode_ &&
         base::ValuesEquivalent(background_.get(), other.background_.get()) &&
         base::ValuesEquivalent(foreground_.get(), other.foreground_.get());
}

ArithmeticPaintFilter::ArithmeticPaintFilter(float k1,
                                             float k2,
                                             float k3,
                                             float k4,
                                             bool enforce_pm_color,
                                             sk_sp<PaintFilter> background,
                                             sk_sp<PaintFilter> foreground,
                                             const CropRect* crop_rect)
    : PaintFilter(
          kType,
          crop_rect,
          HasDiscardableImages(background) || HasDiscardableImages(foreground)),
      k1_(k1),
      k2_(k2),
      k3_(k3),
      k4_(k4),
      enforce_pm_color_(enforce_pm_color),
      background_(std::move(background)),
      foreground_(std::move(foreground)) {
  cached_sk_filter_ = SkImageFilters::Arithmetic(
      k1_, k2_, k3_, k4_, enforce_pm_color_, GetSkFilter(background_.get()),
      GetSkFilter(foreground_.get()), crop_rect);
}

ArithmeticPaintFilter::~ArithmeticPaintFilter() = default;

size_t ArithmeticPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(k1_) + sizeof(k2_) + sizeof(k3_) +
      sizeof(k4_) + sizeof(enforce_pm_color_);
  total_size += GetFilterSize(background_.get());
  total_size += GetFilterSize(foreground_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ArithmeticPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<ArithmeticPaintFilter>(
      k1_, k2_, k3_, k4_, enforce_pm_color_,
      Snapshot(background_, image_provider),
      Snapshot(foreground_, image_provider), GetCropRect());
}

bool ArithmeticPaintFilter::operator==(
    const ArithmeticPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(k1_, other.k1_) &&
         PaintOp::AreEqualEvenIfNaN(k2_, other.k2_) &&
         PaintOp::AreEqualEvenIfNaN(k3_, other.k3_) &&
         PaintOp::AreEqualEvenIfNaN(k4_, other.k4_) &&
         enforce_pm_color_ == other.enforce_pm_color_ &&
         base::ValuesEquivalent(background_.get(), other.background_.get()) &&
         base::ValuesEquivalent(foreground_.get(), other.foreground_.get());
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
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      kernel_size_(kernel_size),
      gain_(gain),
      bias_(bias),
      kernel_offset_(kernel_offset),
      tile_mode_(tile_mode),
      convolve_alpha_(convolve_alpha),
      input_(std::move(input)) {
  auto len = static_cast<size_t>(
      sk_64_mul(kernel_size_.width(), kernel_size_.height()));
  kernel_->reserve(len);
  for (size_t i = 0; i < len; ++i)
    kernel_->push_back(kernel[i]);

  cached_sk_filter_ = SkImageFilters::MatrixConvolution(
      kernel_size_, kernel, gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, GetSkFilter(input_.get()), crop_rect);
}

MatrixConvolutionPaintFilter::~MatrixConvolutionPaintFilter() = default;

size_t MatrixConvolutionPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(kernel_size_) + sizeof(size_t) +
      kernel_->size() * sizeof(SkScalar) + sizeof(gain_) + sizeof(bias_) +
      sizeof(kernel_offset_) + sizeof(tile_mode_) + sizeof(convolve_alpha_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MatrixConvolutionPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MatrixConvolutionPaintFilter>(
      kernel_size_, &kernel_[0], gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, Snapshot(input_, image_provider), GetCropRect());
}

bool MatrixConvolutionPaintFilter::operator==(
    const MatrixConvolutionPaintFilter& other) const {
  return kernel_size_ == other.kernel_size_ &&
         base::ranges::equal(kernel_.container(), other.kernel_.container(),
                             AreScalarsEqual) &&
         PaintOp::AreEqualEvenIfNaN(gain_, other.gain_) &&
         PaintOp::AreEqualEvenIfNaN(bias_, other.bias_) &&
         kernel_offset_ == other.kernel_offset_ &&
         tile_mode_ == other.tile_mode_ &&
         convolve_alpha_ == other.convolve_alpha_ &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

DisplacementMapEffectPaintFilter::DisplacementMapEffectPaintFilter(
    SkColorChannel channel_x,
    SkColorChannel channel_y,
    SkScalar scale,
    sk_sp<PaintFilter> displacement,
    sk_sp<PaintFilter> color,
    const CropRect* crop_rect)
    : PaintFilter(
          kType,
          crop_rect,
          HasDiscardableImages(displacement) || HasDiscardableImages(color)),
      channel_x_(channel_x),
      channel_y_(channel_y),
      scale_(scale),
      displacement_(std::move(displacement)),
      color_(std::move(color)) {
  cached_sk_filter_ = SkImageFilters::DisplacementMap(
      channel_x_, channel_y_, scale_, GetSkFilter(displacement_.get()),
      GetSkFilter(color_.get()), crop_rect);
}

DisplacementMapEffectPaintFilter::~DisplacementMapEffectPaintFilter() = default;

size_t DisplacementMapEffectPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize() +
                                            sizeof(channel_x_) +
                                            sizeof(channel_y_) + sizeof(scale_);
  total_size += GetFilterSize(displacement_.get());
  total_size += GetFilterSize(color_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> DisplacementMapEffectPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<DisplacementMapEffectPaintFilter>(
      channel_x_, channel_y_, scale_, Snapshot(displacement_, image_provider),
      Snapshot(color_, image_provider), GetCropRect());
}

bool DisplacementMapEffectPaintFilter::operator==(
    const DisplacementMapEffectPaintFilter& other) const {
  return channel_x_ == other.channel_x_ && channel_y_ == other.channel_y_ &&
         PaintOp::AreEqualEvenIfNaN(scale_, other.scale_) &&
         base::ValuesEquivalent(displacement_.get(),
                                other.displacement_.get()) &&
         base::ValuesEquivalent(color_.get(), other.color_.get());
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
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(filter_quality));
  cached_sk_filter_ = SkImageFilters::Image(image_.GetSkImage(), src_rect_,
                                            dst_rect_, sampling);
}

ImagePaintFilter::~ImagePaintFilter() = default;

size_t ImagePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(src_rect_) + sizeof(dst_rect_) +
      sizeof(filter_quality_);
  total_size += PaintOpWriter::GetImageSize(image_);
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

bool ImagePaintFilter::operator==(const ImagePaintFilter& other) const {
  return !!image_ == !!other.image_ &&
         PaintOp::AreSkRectsEqual(src_rect_, other.src_rect_) &&
         PaintOp::AreSkRectsEqual(dst_rect_, other.dst_rect_) &&
         filter_quality_ == other.filter_quality_;
}

RecordPaintFilter::RecordPaintFilter(sk_sp<PaintRecord> record,
                                     const SkRect& record_bounds,
                                     const gfx::SizeF& raster_scale,
                                     ScalingBehavior scaling_behavior)
    : RecordPaintFilter(std::move(record),
                        record_bounds,
                        raster_scale,
                        scaling_behavior,
                        nullptr) {}

RecordPaintFilter::RecordPaintFilter(sk_sp<PaintRecord> record,
                                     const SkRect& record_bounds,
                                     const gfx::SizeF& raster_scale,
                                     ScalingBehavior scaling_behavior,
                                     ImageProvider* image_provider)
    : PaintFilter(kType, nullptr, record->HasDiscardableImages()),
      record_(std::move(record)),
      record_bounds_(record_bounds),
      raster_scale_(raster_scale),
      scaling_behavior_(scaling_behavior) {
  DCHECK(raster_scale_.width() > 0.f && raster_scale_.height() > 0.f);
  DCHECK(scaling_behavior == ScalingBehavior::kFixedScale ||
         (raster_scale_.width() == 1.f && raster_scale_.height() == 1.f));

  sk_sp<SkPicture> picture =
      ToSkPicture(record_, record_bounds_, image_provider);

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
    auto image = SkImage::MakeFromPicture(
        std::move(picture), SkISize::Make(width, height), &originAdjust,
        nullptr, SkImage::BitDepth::kU8, SkColorSpace::MakeSRGB());

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
      BaseSerializedSize() + sizeof(record_bounds_) + sizeof(raster_scale_) +
      sizeof(scaling_behavior_) + sizeof(bool);
  total_size += PaintOpWriter::GetRecordSize(record_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> RecordPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_sp<RecordPaintFilter>(
      new RecordPaintFilter(record_, record_bounds_, raster_scale_,
                            scaling_behavior_, image_provider));
}

bool RecordPaintFilter::operator==(const RecordPaintFilter& other) const {
  return !!record_ == !!other.record_ &&
         scaling_behavior_ == other.scaling_behavior_ &&
         raster_scale_ == other.raster_scale_ &&
         PaintOp::AreSkRectsEqual(record_bounds_, other.record_bounds_);
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
    inputs_->push_back(std::move(filter));
    sk_filters.push_back(GetSkFilter(inputs_->back().get()));
  }

  cached_sk_filter_ = SkImageFilters::Merge(
      static_cast<sk_sp<SkImageFilter>*>(sk_filters.data()), count, crop_rect);
}

MergePaintFilter::~MergePaintFilter() = default;

size_t MergePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = 0u;
  for (size_t i = 0; i < input_count(); ++i)
    total_size += GetFilterSize(input_at(i));
  total_size += BaseSerializedSize();
  total_size += sizeof(input_count());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MergePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_sp<MergePaintFilter>(new MergePaintFilter(
      &inputs_[0], inputs_->size(), GetCropRect(), image_provider));
}

bool MergePaintFilter::operator==(const MergePaintFilter& other) const {
  if (inputs_->size() != other.inputs_->size())
    return false;
  for (size_t i = 0; i < inputs_->size(); ++i) {
    if (!base::ValuesEquivalent(inputs_[i].get(), other.inputs_[i].get()))
      return false;
  }
  return true;
}

MorphologyPaintFilter::MorphologyPaintFilter(MorphType morph_type,
                                             float radius_x,
                                             float radius_y,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      morph_type_(morph_type),
      radius_x_(radius_x),
      radius_y_(radius_y),
      input_(std::move(input)) {
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
      BaseSerializedSize() + sizeof(morph_type_) + sizeof(radius_x_) +
      sizeof(radius_y_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MorphologyPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MorphologyPaintFilter>(morph_type_, radius_x_, radius_y_,
                                           Snapshot(input_, image_provider),
                                           GetCropRect());
}

bool MorphologyPaintFilter::operator==(
    const MorphologyPaintFilter& other) const {
  return morph_type_ == other.morph_type_ && radius_x_ == other.radius_x_ &&
         radius_y_ == other.radius_y_ &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

OffsetPaintFilter::OffsetPaintFilter(SkScalar dx,
                                     SkScalar dy,
                                     sk_sp<PaintFilter> input,
                                     const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      dx_(dx),
      dy_(dy),
      input_(std::move(input)) {
  cached_sk_filter_ =
      SkImageFilters::Offset(dx_, dy_, GetSkFilter(input_.get()), crop_rect);
}

OffsetPaintFilter::~OffsetPaintFilter() = default;

size_t OffsetPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(dx_) + sizeof(dy_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> OffsetPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<OffsetPaintFilter>(
      dx_, dy_, Snapshot(input_, image_provider), GetCropRect());
}

bool OffsetPaintFilter::operator==(const OffsetPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(dx_, other.dx_) &&
         PaintOp::AreEqualEvenIfNaN(dy_, other.dy_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

TilePaintFilter::TilePaintFilter(const SkRect& src,
                                 const SkRect& dst,
                                 sk_sp<PaintFilter> input)
    : PaintFilter(kType, nullptr, HasDiscardableImages(input)),
      src_(src),
      dst_(dst),
      input_(std::move(input)) {
  cached_sk_filter_ =
      SkImageFilters::Tile(src_, dst_, GetSkFilter(input_.get()));
}

TilePaintFilter::~TilePaintFilter() = default;

size_t TilePaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(src_) + sizeof(dst_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> TilePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<TilePaintFilter>(src_, dst_,
                                     Snapshot(input_, image_provider));
}

bool TilePaintFilter::operator==(const TilePaintFilter& other) const {
  return PaintOp::AreSkRectsEqual(src_, other.src_) &&
         PaintOp::AreSkRectsEqual(dst_, other.dst_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
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
      shader = SkPerlinNoiseShader::MakeTurbulence(
          base_frequency_x_, base_frequency_y_, num_octaves_, seed_,
          &tile_size_);
      break;
    case TurbulenceType::kFractalNoise:
      shader = SkPerlinNoiseShader::MakeFractalNoise(
          base_frequency_x_, base_frequency_y_, num_octaves_, seed_,
          &tile_size_);
      break;
  }

  cached_sk_filter_ = SkImageFilters::Shader(std::move(shader), crop_rect);
}

TurbulencePaintFilter::~TurbulencePaintFilter() = default;

size_t TurbulencePaintFilter::SerializedSize() const {
  return BaseSerializedSize() + sizeof(turbulence_type_) +
         sizeof(base_frequency_x_) + sizeof(base_frequency_y_) +
         sizeof(num_octaves_) + sizeof(seed_) + sizeof(tile_size_);
}

sk_sp<PaintFilter> TurbulencePaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<TurbulencePaintFilter>(turbulence_type_, base_frequency_x_,
                                           base_frequency_y_, num_octaves_,
                                           seed_, &tile_size_, GetCropRect());
}

bool TurbulencePaintFilter::operator==(
    const TurbulencePaintFilter& other) const {
  return turbulence_type_ == other.turbulence_type_ &&
         PaintOp::AreEqualEvenIfNaN(base_frequency_x_,
                                    other.base_frequency_x_) &&
         PaintOp::AreEqualEvenIfNaN(base_frequency_y_,
                                    other.base_frequency_y_) &&
         num_octaves_ == other.num_octaves_ &&
         PaintOp::AreEqualEvenIfNaN(seed_, other.seed_) &&
         tile_size_ == other.tile_size_;
}

ShaderPaintFilter::ShaderPaintFilter(sk_sp<PaintShader> shader,
                                     uint8_t alpha,
                                     PaintFlags::FilterQuality filter_quality,
                                     SkImageFilters::Dither dither,
                                     const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, shader->has_discardable_images()),
      shader_(std::move(shader)),
      alpha_(alpha),
      filter_quality_(filter_quality),
      dither_(dither) {
  sk_sp<SkShader> sk_shader = shader_->GetSkShader(filter_quality_);
  // Combine the alpha multiply into the SkShader if it's not opaque
  if (alpha < 255) {
    // The blend effectively produces (shader * alpha), the rgb of the secondary
    // color are ignored.
    SkColor4f color{1.0f, 1.0f, 1.0f, alpha / 255.0f};
    // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
    sk_shader = SkShaders::Blend(SkBlendMode::kDstIn, std::move(sk_shader),
                                 SkShaders::Color(color.toSkColor()));
  }

  cached_sk_filter_ =
      SkImageFilters::Shader(std::move(sk_shader), dither, crop_rect);
}

ShaderPaintFilter::~ShaderPaintFilter() = default;

size_t ShaderPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size = BaseSerializedSize();
  total_size += PaintShader::GetSerializedSize(shader_.get());
  total_size += sizeof(alpha_);
  total_size += sizeof(filter_quality_);  // filter quality
  total_size += sizeof(dither_);
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> ShaderPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  PaintFlags orig_flags;
  orig_flags.setShader(shader_);
  orig_flags.setAlpha(alpha_);
  orig_flags.setFilterQuality(filter_quality_);
  orig_flags.setDither(dither_ == SkImageFilters::Dither::kYes);

  ScopedRasterFlags raster_flags(&orig_flags, image_provider, SkMatrix::I(), 0,
                                 1.0f);
  const PaintFlags* snapshot = raster_flags.flags();
  if (snapshot) {
    // Ref the updated paint shader so that it can outlive ScopedRasterFlags
    return sk_make_sp<ShaderPaintFilter>(
        sk_ref_sp(snapshot->getShader()), snapshot->getAlpha(),
        snapshot->getFilterQuality(),
        snapshot->isDither() ? Dither::kYes : Dither::kNo, GetCropRect());
  } else {
    // If decode failed, then just fallback to the solid color
    return sk_make_sp<ShaderPaintFilter>(nullptr, alpha_, filter_quality_,
                                         dither_, GetCropRect());
  }
}

bool ShaderPaintFilter::operator==(const ShaderPaintFilter& other) const {
  DCHECK(shader_ && other.shader_);
  return alpha_ == other.alpha_ && filter_quality_ == other.filter_quality_ &&
         dither_ == other.dither_ && *shader_ == *other.shader_;
}

MatrixPaintFilter::MatrixPaintFilter(const SkMatrix& matrix,
                                     PaintFlags::FilterQuality filter_quality,
                                     sk_sp<PaintFilter> input)
    : PaintFilter(Type::kMatrix, nullptr, HasDiscardableImages(input)),
      matrix_(matrix),
      filter_quality_(filter_quality),
      input_(std::move(input)) {
  cached_sk_filter_ = SkImageFilters::MatrixTransform(
      matrix_, PaintFlags::FilterQualityToSkSamplingOptions(filter_quality_),
      GetSkFilter(input_.get()));
}

MatrixPaintFilter::~MatrixPaintFilter() = default;

size_t MatrixPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(matrix_) + sizeof(filter_quality_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> MatrixPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<MatrixPaintFilter>(matrix_, filter_quality_,
                                       Snapshot(input_, image_provider));
}

bool MatrixPaintFilter::operator==(const MatrixPaintFilter& other) const {
  return PaintOp::AreSkMatricesEqual(matrix_, other.matrix_) &&
         filter_quality_ == other.filter_quality_ &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
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
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      lighting_type_(lighting_type),
      direction_(direction),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::DistantLitDiffuse(
          direction_, light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::DistantLitSpecular(
          direction_, light_color_.toSkColor(), surface_scale_, kconstant_,
          shininess_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingDistantPaintFilter::~LightingDistantPaintFilter() = default;

size_t LightingDistantPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(lighting_type_) + sizeof(direction_) +
      sizeof(light_color_) + sizeof(surface_scale_) + sizeof(kconstant_) +
      sizeof(shininess_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingDistantPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingDistantPaintFilter>(
      lighting_type_, direction_, light_color_, surface_scale_, kconstant_,
      shininess_, Snapshot(input_, image_provider), GetCropRect());
}

bool LightingDistantPaintFilter::operator==(
    const LightingDistantPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(direction_, other.direction_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

LightingPointPaintFilter::LightingPointPaintFilter(LightingType lighting_type,
                                                   const SkPoint3& location,
                                                   SkColor4f light_color,
                                                   SkScalar surface_scale,
                                                   SkScalar kconstant,
                                                   SkScalar shininess,
                                                   sk_sp<PaintFilter> input,
                                                   const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      lighting_type_(lighting_type),
      location_(location),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::PointLitDiffuse(
          location_, light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::PointLitSpecular(
          location_, light_color_.toSkColor(), surface_scale_, kconstant_,
          shininess_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingPointPaintFilter::~LightingPointPaintFilter() = default;

size_t LightingPointPaintFilter::SerializedSize() const {
  base::CheckedNumeric<size_t> total_size =
      BaseSerializedSize() + sizeof(lighting_type_) + sizeof(location_) +
      sizeof(light_color_) + sizeof(surface_scale_) + sizeof(kconstant_) +
      sizeof(shininess_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingPointPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingPointPaintFilter>(
      lighting_type_, location_, light_color_, surface_scale_, kconstant_,
      shininess_, Snapshot(input_, image_provider), GetCropRect());
}

bool LightingPointPaintFilter::operator==(
    const LightingPointPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(location_, other.location_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
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
    : PaintFilter(kType, crop_rect, HasDiscardableImages(input)),
      lighting_type_(lighting_type),
      location_(location),
      target_(target),
      specular_exponent_(specular_exponent),
      cutoff_angle_(cutoff_angle),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
      cached_sk_filter_ = SkImageFilters::SpotLitDiffuse(
          location_, target_, specular_exponent_, cutoff_angle_,
          light_color_.toSkColor(), surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
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
      BaseSerializedSize() + sizeof(lighting_type_) + sizeof(location_) +
      sizeof(target_) + sizeof(specular_exponent_) + sizeof(cutoff_angle_) +
      sizeof(light_color_) + sizeof(surface_scale_) + sizeof(kconstant_) +
      sizeof(shininess_);
  total_size += GetFilterSize(input_.get());
  return total_size.ValueOrDefault(0u);
}

sk_sp<PaintFilter> LightingSpotPaintFilter::SnapshotWithImagesInternal(
    ImageProvider* image_provider) const {
  return sk_make_sp<LightingSpotPaintFilter>(
      lighting_type_, location_, target_, specular_exponent_, cutoff_angle_,
      light_color_, surface_scale_, kconstant_, shininess_,
      Snapshot(input_, image_provider), GetCropRect());
}

bool LightingSpotPaintFilter::operator==(
    const LightingSpotPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(location_, other.location_) &&
         PaintOp::AreSkPoint3sEqual(target_, other.target_) &&
         PaintOp::AreEqualEvenIfNaN(specular_exponent_,
                                    other.specular_exponent_) &&
         PaintOp::AreEqualEvenIfNaN(cutoff_angle_, other.cutoff_angle_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         base::ValuesEquivalent(input_.get(), other.input_.get());
}

}  // namespace cc
