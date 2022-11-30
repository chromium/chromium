// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

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
  if (is_multipart_ != other.is_multipart_)
    return false;
  if (texture_backing_ != other.texture_backing_)
    return false;
  if (paint_worklet_input_ != other.paint_worklet_input_)
    return false;
  // Do not check may_be_lcp_candidate_ as it should not affect any rendering
  // operation, only metrics collection.
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

sk_sp<SkImage> PaintImage::GetSwSkImage() const {
  if (texture_backing_) {
    return texture_backing_->GetSkImageViaReadback();
  } else if (cached_sk_image_ && cached_sk_image_->isTextureBacked()) {
    return cached_sk_image_->makeNonTextureImage();
  }
  return cached_sk_image_;
}

sk_sp<SkImage> PaintImage::GetAcceleratedSkImage() const {
  DCHECK(!cached_sk_image_ || cached_sk_image_->isTextureBacked());
  return cached_sk_image_;
}

bool PaintImage::readPixels(const SkImageInfo& dst_info,
                            void* dst_pixels,
                            size_t dst_row_bytes,
                            int src_x,
                            int src_y) const {
  if (texture_backing_) {
    return texture_backing_->readPixels(dst_info, dst_pixels, dst_row_bytes,
                                        src_x, src_y);
  } else if (cached_sk_image_) {
    return cached_sk_image_->readPixels(dst_info, dst_pixels, dst_row_bytes,
                                        src_x, src_y);
  }
  return false;
}

SkImageInfo PaintImage::GetSkImageInfo() const {
  if (paint_image_generator_) {
    return paint_image_generator_->GetSkImageInfo();
  } else if (texture_backing_) {
    return texture_backing_->GetSkImageInfo();
  } else if (cached_sk_image_) {
    return cached_sk_image_->imageInfo();
  } else if (paint_worklet_input_) {
    auto size = paint_worklet_input_->GetSize();
    return SkImageInfo::MakeUnknown(base::ClampCeil(size.width()),
                                    base::ClampCeil(size.height()));
  } else {
    return SkImageInfo::MakeUnknown();
  }
}

gpu::Mailbox PaintImage::GetMailbox() const {
  DCHECK(texture_backing_);
  return texture_backing_->GetMailbox();
}

bool PaintImage::IsOpaque() const {
  if (IsPaintWorklet())
    return paint_worklet_input_->KnownToBeOpaque();
  return GetSkImageInfo().isOpaque();
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
  } else if (texture_backing_) {
    cached_sk_image_ = texture_backing_->GetAcceleratedSkImage();
  }
}

SkISize PaintImage::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  if (paint_image_generator_)
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

  if (paint_image_generator_) {
    return DecodeFromGenerator(memory, info, std::move(color_space),
                               frame_index, client_id);
  }
  return DecodeFromSkImage(memory, info, std::move(color_space), frame_index,
                           client_id);
}

bool PaintImage::DecodeYuv(const SkYUVAPixmaps& pixmaps,
                           size_t frame_index,
                           GeneratorClientId client_id) const {
  DCHECK(pixmaps.isValid());
  DCHECK(paint_image_generator_);
  const uint32_t lazy_pixel_ref = stable_id();
  return paint_image_generator_->GetYUVAPlanes(pixmaps, frame_index,
                                               lazy_pixel_ref, client_id);
}

bool PaintImage::DecodeFromGenerator(void* memory,
                                     SkImageInfo* info,
                                     sk_sp<SkColorSpace> color_space,
                                     size_t frame_index,
                                     GeneratorClientId client_id) const {
  DCHECK(paint_image_generator_);
  // First convert the info to have the requested color space, since the decoder
  // will convert this for us.
  *info = info->makeColorSpace(std::move(color_space));
  const uint32_t lazy_pixel_ref = stable_id();
  return paint_image_generator_->GetPixels(*info, memory, info->minRowBytes(),
                                           frame_index, client_id,
                                           lazy_pixel_ref);
}

bool PaintImage::DecodeFromSkImage(void* memory,
                                   SkImageInfo* info,
                                   sk_sp<SkColorSpace> color_space,
                                   size_t frame_index,
                                   GeneratorClientId client_id) const {
  auto image = GetSkImageForFrame(frame_index, client_id);
  DCHECK(image);
  if (color_space) {
    image = image->makeColorSpace(color_space, nullptr);
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

  return FrameKey(GetContentIdForFrame(frame_index), frame_index);
}

PaintImage::ContentId PaintImage::GetContentIdForFrame(
    size_t frame_index) const {
  if (paint_image_generator_)
    return paint_image_generator_->GetContentIdForFrame(frame_index);

  DCHECK_NE(content_id_, kInvalidContentId);
  return content_id_;
}

bool PaintImage::IsTextureBacked() const {
  if (texture_backing_)
    return true;
  if (cached_sk_image_)
    return cached_sk_image_->isTextureBacked();
  return false;
}

void PaintImage::FlushPendingSkiaOps() {
  if (texture_backing_)
    texture_backing_->FlushPendingSkiaOps();
}

gfx::ContentColorUsage PaintImage::GetContentColorUsage(bool* is_hlg) const {
  if (is_hlg)
    *is_hlg = false;

  // Right now, JS paint worklets can only be in sRGB
  if (paint_worklet_input_)
    return gfx::ContentColorUsage::kSRGB;

  const auto* color_space = GetSkImageInfo().colorSpace();

  // Assume the image will be sRGB if we don't know yet.
  if (!color_space || color_space->isSRGB())
    return gfx::ContentColorUsage::kSRGB;

  skcms_TransferFunction fn;
  if (!color_space->isNumericalTransferFn(&fn)) {
    if (skcms_TransferFunction_isPQish(&fn))
      return gfx::ContentColorUsage::kHDR;

    if (skcms_TransferFunction_isHLGish(&fn)) {
      if (is_hlg)
        *is_hlg = true;
      return gfx::ContentColorUsage::kHDR;
    }
  }

  // If it's not HDR and not SRGB, report it as WCG.
  return gfx::ContentColorUsage::kWideColorGamut;
}

const ImageHeaderMetadata* PaintImage::GetImageHeaderMetadata() const {
  if (paint_image_generator_)
    return paint_image_generator_->GetMetadataForDecodeAcceleration();
  return nullptr;
}

bool PaintImage::IsYuv(
    const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
    SkYUVAPixmapInfo* info) const {
  SkYUVAPixmapInfo temp_info;
  if (!info)
    info = &temp_info;
  // ImageDecoder will fill out the SkYUVColorSpace in |info| depending on the
  // codec's specification.
  return paint_image_generator_ &&
         paint_image_generator_->QueryYUVA(supported_data_types, info);
}

const std::vector<FrameMetadata>& PaintImage::GetFrameMetadata() const {
  DCHECK_EQ(animation_type_, AnimationType::ANIMATED);
  DCHECK(paint_image_generator_);

  return paint_image_generator_->GetFrameMetadata();
}

size_t PaintImage::FrameCount() const {
  if (!*this)
    return 0u;
  return paint_image_generator_
             ? paint_image_generator_->GetFrameMetadata().size()
             : 1u;
}

sk_sp<SkImage> PaintImage::GetSkImageForFrame(
    size_t index,
    GeneratorClientId client_id) const {
  DCHECK_LT(index, FrameCount());
  DCHECK(!IsTextureBacked());

  // |client_id| and |index| are only relevant for generator backed images which
  // perform lazy decoding and can be multi-frame.
  if (!paint_image_generator_) {
    DCHECK_EQ(index, kDefaultFrameIndex);
    return GetSwSkImage();
  }

  // The internally cached SkImage is constructed using the default frame index
  // and GeneratorClientId. Avoid creating a new SkImage.
  if (index == kDefaultFrameIndex && client_id == kDefaultGeneratorClientId)
    return GetSwSkImage();

  sk_sp<SkImage> image =
      SkImage::MakeFromGenerator(std::make_unique<SkiaPaintImageGenerator>(
          paint_image_generator_, index, client_id));
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
      << " is_multipart_: " << is_multipart_
      << " may_be_lcp_candidate_: " << may_be_lcp_candidate_
      << " is YUV: " << IsYuv(SkYUVAPixmapInfo::SupportedDataTypes::All());
  return str.str();
}

PaintImage::FrameKey::FrameKey(ContentId content_id, size_t frame_index)
    : content_id_(content_id), frame_index_(frame_index) {
  hash_ = base::HashInts(static_cast<uint64_t>(content_id_),
                         static_cast<uint64_t>(frame_index_));
}

bool PaintImage::FrameKey::operator==(const FrameKey& other) const {
  return content_id_ == other.content_id_ && frame_index_ == other.frame_index_;
}

bool PaintImage::FrameKey::operator!=(const FrameKey& other) const {
  return !(*this == other);
}

std::string PaintImage::FrameKey::ToString() const {
  std::ostringstream str;
  str << "content_id: " << content_id_ << ","
      << "frame_index: " << frame_index_;
  return str.str();
}

}  // namespace cc
