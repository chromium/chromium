// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/hash/hash.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
base::AtomicSequenceNumber g_next_image_id;
base::AtomicSequenceNumber g_next_image_content_id;
base::AtomicSequenceNumber g_next_generator_client_id;
}  // namespace

const PaintImage::Id PaintImage::kNonLazyStableId = -1;
const size_t PaintImage::kDefaultFrameIndex = 0u;
const PaintImage::Id PaintImage::kInvalidId = -2;
const PaintImage::ContentId PaintImage::kInvalidContentId = -1;
const PaintImage::GeneratorClientId PaintImage::kDefaultGeneratorClientId = 0;

ImageHeaderMetadata::ImageHeaderMetadata() = default;
ImageHeaderMetadata::ImageHeaderMetadata(const ImageHeaderMetadata& other) =
    default;
ImageHeaderMetadata& ImageHeaderMetadata::operator=(
    const ImageHeaderMetadata& other) = default;
ImageHeaderMetadata::ImageHeaderMetadata::~ImageHeaderMetadata() = default;

PaintImage::PaintImage() = default;
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;

PaintImage& PaintImage::operator=(const PaintImage& other) = default;
PaintImage& PaintImage::operator=(PaintImage&& other) = default;

bool PaintImage::operator==(const PaintImage& other) const {
  if (sk_image_ != other.sk_image_)
    return false;
  if (paint_record_ != other.paint_record_)
    return false;
  if (paint_record_rect_ != other.paint_record_rect_)
    return false;
  if (content_id_ != other.content_id_)
    return false;
  if (paint_image_generator_ != other.paint_image_generator_)
    return false;
  if (id_ != other.id_)
    return false;
  if (animation_type_ != other.animation_type_)
    return false;
  if (completion_state_ != other.completion_state_)
    return false;
  if (subset_rect_ != other.subset_rect_)
    return false;
  if (is_multipart_ != other.is_multipart_)
    return false;
  return true;
}

// static
PaintImage::DecodingMode PaintImage::GetConservative(DecodingMode one,
                                                     DecodingMode two) {
  if (one == two)
    return one;
  if (one == DecodingMode::kSync || two == DecodingMode::kSync)
    return DecodingMode::kSync;
  if (one == DecodingMode::kUnspecified || two == DecodingMode::kUnspecified)
    return DecodingMode::kUnspecified;
  DCHECK_EQ(one, DecodingMode::kAsync);
  DCHECK_EQ(two, DecodingMode::kAsync);
  return DecodingMode::kAsync;
}

// static
PaintImage::Id PaintImage::GetNextId() {
  return g_next_image_id.GetNext();
}

// static
PaintImage::ContentId PaintImage::GetNextContentId() {
  return g_next_image_content_id.GetNext();
}

// static
PaintImage::GeneratorClientId PaintImage::GetNextGeneratorClientId() {
  // These IDs must start from 1, since 0 is the kDefaultGeneratorClientId.
  return g_next_generator_client_id.GetNext() + 1;
}

// static
PaintImage PaintImage::CreateFromBitmap(SkBitmap bitmap) {
  if (bitmap.drawsNothing())
    return PaintImage();

  return PaintImageBuilder::WithDefault()
      .set_id(PaintImage::GetNextId())
      .set_image(SkImage::MakeFromBitmap(bitmap),
                 PaintImage::GetNextContentId())
      .TakePaintImage();
}

const sk_sp<SkImage>& PaintImage::GetSkImage() const {
  return cached_sk_image_;
}

PaintImage PaintImage::MakeSubset(const gfx::Rect& subset) const {
  DCHECK(!subset.IsEmpty());

  // If the subset is the same as the image bounds, we can return the same
  // image.
  gfx::Rect bounds(width(), height());
  if (bounds == subset)
    return *this;

  DCHECK(bounds.Contains(subset))
      << "Subset should not be greater than the image bounds";
  PaintImage result(*this);
  result.subset_rect_ = subset;
  // Store the subset from the original image.
  result.subset_rect_.Offset(subset_rect_.x(), subset_rect_.y());

  // Creating the |cached_sk_image_| using the SkImage from the original
  // PaintImage is an optimization to allow re-use of the original decode for
  // image subsets in skia, for cases that rely on skia's image decode cache.
  result.cached_sk_image_ =
      GetSkImage()->makeSubset(gfx::RectToSkIRect(subset));
  return result;
}

void PaintImage::CreateSkImage() {
  DCHECK(!cached_sk_image_);

  if (sk_image_) {
    cached_sk_image_ = sk_image_;
  } else if (paint_record_) {
    cached_sk_image_ = SkImage::MakeFromPicture(
        ToSkPicture(paint_record_, gfx::RectToSkRect(paint_record_rect_)),
        SkISize::Make(paint_record_rect_.width(), paint_record_rect_.height()),
        nullptr, nullptr, SkImage::BitDepth::kU8, SkColorSpace::MakeSRGB());
  } else if (paint_image_generator_) {
    cached_sk_image_ =
        SkImage::MakeFromGenerator(std::make_unique<SkiaPaintImageGenerator>(
            paint_image_generator_, kDefaultFrameIndex,
            kDefaultGeneratorClientId));
  }

  if (!subset_rect_.IsEmpty() && cached_sk_image_) {
    cached_sk_image_ =
        cached_sk_image_->makeSubset(gfx::RectToSkIRect(subset_rect_));
  }
}

SkISize PaintImage::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  // TODO(vmpstr): In some cases we do not support decoding to any other
  // size than the original. See the comment in CanDecodeFromGenerator()
  // for more detail.
  if (CanDecodeFromGenerator())
    return paint_image_generator_->GetSupportedDecodeSize(requested_size);
  return SkISize::Make(width(), height());
}

bool PaintImage::Decode(void* memory,
                        SkImageInfo* info,
                        sk_sp<SkColorSpace> color_space,
                        size_t frame_index,
                        GeneratorClientId client_id) const {
  // We don't support SkImageInfo's with color spaces on them. Color spaces
  // should always be passed via the |color_space| arg.
  DCHECK(!info->colorSpace());

  // We only support decode to supported decode size.
  DCHECK(info->dimensions() == GetSupportedDecodeSize(info->dimensions()));

  // TODO(vmpstr): In some cases we do not support decoding to any other
  // size than the original. See the comment in CanDecodeFromGenerator()
  // for more detail. For now, fallback to DecodeFromSkImage().
  if (CanDecodeFromGenerator()) {
    return DecodeFromGenerator(memory, info, std::move(color_space),
                               frame_index, client_id);
  }
  return DecodeFromSkImage(memory, info, std::move(color_space), frame_index,
                           client_id);
}

bool PaintImage::DecodeYuv(
    void* planes[SkYUVASizeInfo::kMaxCount],
    size_t frame_index,
    GeneratorClientId client_id,
    const SkYUVASizeInfo& yuva_size_info,
    SkYUVAIndex plane_indices[SkYUVAIndex::kIndexCount]) const {
  DCHECK(plane_indices != nullptr);
  DCHECK(CanDecodeFromGenerator());
  const uint32_t lazy_pixel_ref = unique_id();
  return paint_image_generator_->GetYUVA8Planes(
      yuva_size_info, plane_indices, planes, frame_index, lazy_pixel_ref);
}

bool PaintImage::DecodeFromGenerator(void* memory,
                                     SkImageInfo* info,
                                     sk_sp<SkColorSpace> color_space,
                                     size_t frame_index,
                                     GeneratorClientId client_id) const {
  DCHECK(CanDecodeFromGenerator());
  // First convert the info to have the requested color space, since the decoder
  // will convert this for us.
  *info = info->makeColorSpace(std::move(color_space));
  const uint32_t lazy_pixel_ref = unique_id();
  return paint_image_generator_->GetPixels(*info, memory, info->minRowBytes(),
                                           frame_index, client_id,
                                           lazy_pixel_ref);
}

// TODO(vmpstr): If we're using a subset_rect_ then the info specifies the
// requested size relative to the subset. However, the generator isn't aware
// of this subsetting and would need a size that is relative to the original
// image size. We could still implement this case, but we need to convert the
// requested size into the space of the original image.
bool PaintImage::CanDecodeFromGenerator() const {
  return paint_image_generator_ && subset_rect_.IsEmpty();
}

bool PaintImage::DecodeFromSkImage(void* memory,
                                   SkImageInfo* info,
                                   sk_sp<SkColorSpace> color_space,
                                   size_t frame_index,
                                   GeneratorClientId client_id) const {
  auto image = GetSkImageForFrame(frame_index, client_id);
  DCHECK(image);
  if (color_space) {
    image = image->makeColorSpace(color_space);
    if (!image)
      return false;
  }
  // Note that the readPixels has to happen before converting the info to the
  // given color space, since it can produce incorrect results.
  bool result = image->readPixels(*info, memory, info->minRowBytes(), 0, 0,
                                  SkImage::kDisallow_CachingHint);
  *info = info->makeColorSpace(std::move(color_space));
  return result;
}

bool PaintImage::ShouldAnimate() const {
  return animation_type_ == AnimationType::ANIMATED &&
         repetition_count_ != kAnimationNone && FrameCount() > 1;
}

PaintImage::FrameKey PaintImage::GetKeyForFrame(size_t frame_index) const {
  DCHECK_LT(frame_index, FrameCount());

  // Query the content id that uniquely identifies the content for this frame
  // from the content provider.
  ContentId content_id = kInvalidContentId;
  if (paint_image_generator_)
    content_id = paint_image_generator_->GetContentIdForFrame(frame_index);
  else if (paint_record_ || sk_image_)
    content_id = content_id_;

  DCHECK_NE(content_id, kInvalidContentId);
  return FrameKey(content_id, frame_index, subset_rect_);
}

SkColorType PaintImage::GetColorType() const {
  if (paint_image_generator_)
    return paint_image_generator_->GetSkImageInfo().colorType();
  if (GetSkImage())
    return GetSkImage()->colorType();
  return kUnknown_SkColorType;
}

int PaintImage::width() const {
  return paint_worklet_input_
             ? static_cast<int>(paint_worklet_input_->GetSize().width())
             : GetSkImage()->width();
}

int PaintImage::height() const {
  return paint_worklet_input_
             ? static_cast<int>(paint_worklet_input_->GetSize().height())
             : GetSkImage()->height();
}

const ImageHeaderMetadata* PaintImage::GetImageHeaderMetadata() const {
  if (paint_image_generator_)
    return paint_image_generator_->GetMetadataForDecodeAcceleration();
  return nullptr;
}

bool PaintImage::IsYuv(SkYUVASizeInfo* yuva_size_info,
                       SkYUVAIndex* plane_indices,
                       SkYUVColorSpace* yuv_color_space) const {
  SkYUVASizeInfo temp_yuva_size_info;
  SkYUVAIndex temp_plane_indices[SkYUVAIndex::kIndexCount];
  SkYUVColorSpace temp_yuv_color_space;
  if (!yuva_size_info) {
    yuva_size_info = &temp_yuva_size_info;
  }
  if (!plane_indices) {
    plane_indices = temp_plane_indices;
  }
  if (!yuv_color_space) {
    yuv_color_space = &temp_yuv_color_space;
  }
  // ImageDecoder will fill out the value of |yuv_color_space| depending on
  // the codec's specification.
  return CanDecodeFromGenerator() &&
         paint_image_generator_->QueryYUVA8(yuva_size_info, plane_indices,
                                            yuv_color_space);
}

const std::vector<FrameMetadata>& PaintImage::GetFrameMetadata() const {
  DCHECK_EQ(animation_type_, AnimationType::ANIMATED);
  DCHECK(paint_image_generator_);

  return paint_image_generator_->GetFrameMetadata();
}

size_t PaintImage::FrameCount() const {
  if (!GetSkImage())
    return 0u;
  return paint_image_generator_
             ? paint_image_generator_->GetFrameMetadata().size()
             : 1u;
}

sk_sp<SkImage> PaintImage::GetSkImageForFrame(
    size_t index,
    GeneratorClientId client_id) const {
  DCHECK_LT(index, FrameCount());

  // |client_id| and |index| are only relevant for generator backed images which
  // perform lazy decoding and can be multi-frame.
  if (!paint_image_generator_) {
    DCHECK_EQ(index, kDefaultFrameIndex);
    return GetSkImage();
  }

  // The internally cached SkImage is constructed using the default frame index
  // and GeneratorClientId. Avoid creating a new SkImage.
  if (index == kDefaultFrameIndex && client_id == kDefaultGeneratorClientId)
    return GetSkImage();

  sk_sp<SkImage> image =
      SkImage::MakeFromGenerator(std::make_unique<SkiaPaintImageGenerator>(
          paint_image_generator_, index, client_id));
  if (!subset_rect_.IsEmpty())
    image = image->makeSubset(gfx::RectToSkIRect(subset_rect_));
  return image;
}

std::string PaintImage::ToString() const {
  std::ostringstream str;
  str << "sk_image_: " << sk_image_ << " paint_record_: " << paint_record_
      << " paint_record_rect_: " << paint_record_rect_.ToString()
      << " paint_image_generator_: " << paint_image_generator_
      << " id_: " << id_
      << " animation_type_: " << static_cast<int>(animation_type_)
      << " completion_state_: " << static_cast<int>(completion_state_)
      << " subset_rect_: " << subset_rect_.ToString()
      << " is_multipart_: " << is_multipart_ << " is YUV: " << IsYuv();
  return str.str();
}

PaintImage::FrameKey::FrameKey(ContentId content_id,
                               size_t frame_index,
                               gfx::Rect subset_rect)
    : content_id_(content_id),
      frame_index_(frame_index),
      subset_rect_(subset_rect) {
  size_t original_hash = base::HashInts(static_cast<uint64_t>(content_id_),
                                        static_cast<uint64_t>(frame_index_));
  if (subset_rect_.IsEmpty()) {
    hash_ = original_hash;
  } else {
    size_t subset_hash =
        base::HashInts(static_cast<uint64_t>(
                           base::HashInts(subset_rect_.x(), subset_rect_.y())),
                       static_cast<uint64_t>(base::HashInts(
                           subset_rect_.width(), subset_rect_.height())));
    hash_ = base::HashInts(original_hash, subset_hash);
  }
}

bool PaintImage::FrameKey::operator==(const FrameKey& other) const {
  return content_id_ == other.content_id_ &&
         frame_index_ == other.frame_index_ &&
         subset_rect_ == other.subset_rect_;
}

bool PaintImage::FrameKey::operator!=(const FrameKey& other) const {
  return !(*this == other);
}

std::string PaintImage::FrameKey::ToString() const {
  std::ostringstream str;
  str << "content_id: " << content_id_ << ","
      << "frame_index: " << frame_index_ << ","
      << "subset_rect: " << subset_rect_.ToString();
  return str.str();
}

}  // namespace cc
