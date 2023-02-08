// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include <array>
#include <type_traits>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
namespace {

struct Context {
  const std::vector<sk_sp<SkImage>> sk_planes_;
};

void ReleaseContext(SkImage::ReleaseContext context) {
  auto* texture_context = static_cast<Context*>(context);
  delete texture_context;
}

// Creates a SkImage backed by the YUV textures corresponding to |plane_images|.
// The layout is specified by |plane_images_format|). The backend textures are
// first extracted out of the |plane_images| (and work is flushed on each one).
// Note that we assume that the image is opaque (no alpha plane). Then, a
// SkImage is created out of those textures using the
// SkImage::MakeFromYUVATextures() API. Finally, |image_color_space| is the
// color space of the resulting image after applying |yuv_color_space|
// (converting from YUV to RGB). This is assumed to be sRGB if nullptr.
//
// On success, the resulting SkImage is
// returned. On failure, nullptr is returned (e.g., if one of the backend
// textures is invalid or a Skia error occurs).
sk_sp<SkImage> MakeYUVImageFromUploadedPlanes(
    GrDirectContext* context,
    const std::vector<sk_sp<SkImage>>& plane_images,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    SkYUVColorSpace yuv_color_space,
    sk_sp<SkColorSpace> image_color_space) {
  // 1) Extract the textures.
  DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, plane_config);
  DCHECK_NE(SkYUVAInfo::Subsampling::kUnknown, subsampling);
  DCHECK_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config)),
            plane_images.size());
  DCHECK_LE(plane_images.size(),
            base::checked_cast<size_t>(SkYUVAInfo::kMaxPlanes));
  std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> plane_backend_textures;
  for (size_t plane = 0u; plane < plane_images.size(); plane++) {
    plane_backend_textures[plane] = plane_images[plane]->getBackendTexture(
        true /* flushPendingGrContextIO */);
    if (!plane_backend_textures[plane].isValid()) {
      DLOG(ERROR) << "Invalid backend texture found";
      return nullptr;
    }
  }

  // 2) Create the YUV image.
  SkYUVAInfo yuva_info(plane_images[0]->dimensions(), plane_config, subsampling,
                       yuv_color_space);
  GrYUVABackendTextures yuva_backend_textures(
      yuva_info, plane_backend_textures.data(), kTopLeft_GrSurfaceOrigin);
  Context* ctx = new Context{plane_images};
  sk_sp<SkImage> image = SkImage::MakeFromYUVATextures(
      context, yuva_backend_textures, std::move(image_color_space),
      ReleaseContext, ctx);
  if (!image) {
    DLOG(ERROR) << "Could not create YUV image";
    return nullptr;
  }

  return image;
}

base::CheckedNumeric<uint32_t> SafeSizeForPixmap(const SkPixmap& pixmap) {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(pixmap.colorType());
  safe_size += PaintOpWriter::SerializedSize(pixmap.width());
  safe_size += PaintOpWriter::SerializedSize(pixmap.height());
  safe_size += PaintOpWriter::SerializedSize(pixmap.colorSpace());
  safe_size += PaintOpWriter::SerializedSize(pixmap.rowBytes());
  safe_size += 16u;  // The max of GetAlignmentForColorType().
  safe_size += PaintOpWriter::SerializedSizeOfBytes(pixmap.computeByteSize());
  return safe_size;
}

size_t GetAlignmentForColorType(SkColorType color_type) {
  size_t bpp = SkColorTypeBytesPerPixel(color_type);
  if (bpp <= 4)
    return 4;
  if (bpp <= 16)
    return 16;
  NOTREACHED();
  return 0;
}

bool WritePixmap(PaintOpWriter& writer, const SkPixmap& pixmap) {
  if (pixmap.width() == 0 || pixmap.height() == 0) {
    DLOG(ERROR) << "Cannot write empty pixmap";
    return false;
  }
  writer.Write(pixmap.colorType());
  writer.Write(pixmap.width());
  writer.Write(pixmap.height());
  writer.Write(pixmap.colorSpace());
  size_t data_size = pixmap.computeByteSize();
  if (data_size == SIZE_MAX) {
    DLOG(ERROR) << "Size overflow writing pixmap";
    return false;
  }
  writer.WriteSize(pixmap.rowBytes());
  writer.WriteSize(data_size);
  // The memory for the pixmap must be aligned to a byte boundary, or mipmap
  // generation can fail.
  // https://crbug.com/863659, https://crbug.com/1300188
  writer.AlignMemory(GetAlignmentForColorType(pixmap.colorType()));
  writer.WriteData(data_size, pixmap.addr());
  return true;
}

bool ReadPixmap(PaintOpReader& reader, SkPixmap& pixmap) {
  if (!reader.valid())
    return false;

  SkColorType color_type = kUnknown_SkColorType;
  reader.Read(&color_type);
  const size_t alignment = GetAlignmentForColorType(color_type);
  if (color_type == kUnknown_SkColorType ||
      color_type == kRGB_101010x_SkColorType ||
      color_type > kLastEnum_SkColorType) {
    DLOG(ERROR) << "Invalid color type";
    return false;
  }
  int width = 0;
  reader.Read(&width);
  int height = 0;
  reader.Read(&height);
  if (width == 0 || height == 0) {
    DLOG(ERROR) << "Empty width or height";
    return false;
  }

  sk_sp<SkColorSpace> color_space;
  reader.Read(&color_space);
  auto image_info = SkImageInfo::Make(width, height, color_type,
                                      kPremul_SkAlphaType, color_space);
  size_t row_bytes = 0;
  reader.ReadSize(&row_bytes);
  if (row_bytes < image_info.minRowBytes()) {
    DLOG(ERROR) << "Row bytes " << row_bytes << " less than minimum "
                << image_info.minRowBytes();
    return false;
  }
  size_t data_size = 0;
  reader.ReadSize(&data_size);
  if (image_info.computeByteSize(row_bytes) > data_size) {
    DLOG(ERROR) << "Data size too small";
    return false;
  }

  reader.AlignMemory(alignment);
  const volatile void* data = reader.ExtractReadableMemory(data_size);
  if (!reader.valid()) {
    DLOG(ERROR) << "Failed to read pixels";
    return false;
  }
  if (reinterpret_cast<uintptr_t>(data) % alignment) {
    DLOG(ERROR) << "Pixel pointer not aligned";
    return false;
  }

  // Const-cast away the "volatile" on |pixel_data|. We specifically understand
  // that a malicious caller may change our pixels under us, and are OK with
  // this as the worst case scenario is visual corruption.
  pixmap = SkPixmap(image_info, const_cast<const void*>(data), row_bytes);
  return true;
}

size_t TargetColorParamsSize(
    const absl::optional<TargetColorParams>& target_color_params) {
  // bool for whether or not there are going to be parameters.
  size_t target_color_params_size = PaintOpWriter::SerializedSize<bool>();
  if (target_color_params) {
    // The target color space.
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->color_space.ToSkColorSpace().get());
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->sdr_max_luminance_nits);
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->hdr_max_luminance_relative);
    target_color_params_size +=
        PaintOpWriter::SerializedSize(target_color_params->enable_tone_mapping);
    // bool for whether or not there is HDR metadata.
    target_color_params_size += PaintOpWriter::SerializedSize<bool>();
    if (auto& hdr_metadata = target_color_params->hdr_metadata) {
      // The minimum and maximum luminance.
      target_color_params_size +=
          PaintOpWriter::SerializedSize(hdr_metadata->max_content_light_level);
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->max_frame_average_light_level);
      // The x and y coordinates for primaries and white point.
      target_color_params_size += PaintOpWriter::SerializedSizeOfElements(
          &hdr_metadata->color_volume_metadata.primaries.fRX, 4 * 2);
      // The CLL and FALL
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->color_volume_metadata.luminance_max);
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->color_volume_metadata.luminance_min);
    }
  }
  return target_color_params_size;
}

void WriteTargetColorParams(
    PaintOpWriter& writer,
    const absl::optional<TargetColorParams>& target_color_params) {
  const bool has_target_color_params = !!target_color_params;
  writer.Write(has_target_color_params);
  if (target_color_params) {
    writer.Write(target_color_params->color_space.ToSkColorSpace().get());
    writer.Write(target_color_params->sdr_max_luminance_nits);
    writer.Write(target_color_params->hdr_max_luminance_relative);
    writer.Write(target_color_params->enable_tone_mapping);

    const bool has_hdr_metadata = !!target_color_params->hdr_metadata;
    writer.Write(has_hdr_metadata);
    if (target_color_params->hdr_metadata) {
      const auto& hdr_metadata = target_color_params->hdr_metadata;
      writer.Write(hdr_metadata->max_content_light_level);
      writer.Write(hdr_metadata->max_frame_average_light_level);

      const auto& color_volume = hdr_metadata->color_volume_metadata;
      writer.Write(color_volume.primaries.fRX);
      writer.Write(color_volume.primaries.fRY);
      writer.Write(color_volume.primaries.fGX);
      writer.Write(color_volume.primaries.fGY);
      writer.Write(color_volume.primaries.fBX);
      writer.Write(color_volume.primaries.fBY);
      writer.Write(color_volume.primaries.fWX);
      writer.Write(color_volume.primaries.fWY);
      writer.Write(color_volume.luminance_max);
      writer.Write(color_volume.luminance_min);
    }
  }
}

bool ReadTargetColorParams(
    PaintOpReader& reader,
    absl::optional<TargetColorParams>& target_color_params) {
  bool has_target_color_params = false;
  reader.Read(&has_target_color_params);
  if (!has_target_color_params) {
    target_color_params = absl::nullopt;
    return true;
  }

  target_color_params = TargetColorParams();
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);
  if (!target_color_space)
    return false;

  target_color_params->color_space = gfx::ColorSpace(*target_color_space);
  reader.Read(&target_color_params->sdr_max_luminance_nits);
  reader.Read(&target_color_params->hdr_max_luminance_relative);
  reader.Read(&target_color_params->enable_tone_mapping);

  bool has_hdr_metadata = false;
  reader.Read(&has_hdr_metadata);
  if (has_hdr_metadata) {
    gfx::HDRMetadata hdr_metadata;
    unsigned max_content_light_level = 0;
    unsigned max_frame_average_light_level = 0;
    reader.Read(&max_content_light_level);
    reader.Read(&max_frame_average_light_level);

    SkColorSpacePrimaries primaries = SkNamedPrimariesExt::kInvalid;
    float luminance_max = 0;
    float luminance_min = 0;
    reader.Read(&primaries.fRX);
    reader.Read(&primaries.fRY);
    reader.Read(&primaries.fGX);
    reader.Read(&primaries.fGY);
    reader.Read(&primaries.fBX);
    reader.Read(&primaries.fBY);
    reader.Read(&primaries.fWX);
    reader.Read(&primaries.fWY);
    reader.Read(&luminance_max);
    reader.Read(&luminance_min);

    target_color_params->hdr_metadata = gfx::HDRMetadata(
        gfx::ColorVolumeMetadata(primaries, luminance_max, luminance_min),
        max_content_light_level, max_frame_average_light_level);
  }
  return true;
}

}  // namespace

size_t NumberOfPlanesForYUVDecodeFormat(YUVDecodeFormat format) {
  switch (format) {
    case YUVDecodeFormat::kYUVA4:
      return 4u;
    case YUVDecodeFormat::kYUV3:
    case YUVDecodeFormat::kYVU3:
      return 3u;
    case YUVDecodeFormat::kYUV2:
      return 2u;
    case YUVDecodeFormat::kUnknown:
      return 0u;
  }
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap* pixmap,
    bool needs_mips,
    absl::optional<TargetColorParams> target_color_params)
    : needs_mips_(needs_mips),
      target_color_params_(target_color_params),
      id_(GetNextId()),
      pixmap_(pixmap),
      decoded_color_space_(nullptr) {
  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(needs_mips_);
  safe_size += TargetColorParamsSize(target_color_params_);
  safe_size += PaintOpWriter::SerializedSize(plane_config_);
  safe_size += SafeSizeForPixmap(*pixmap_);
  size_ = safe_size.ValueOrDefault(0);
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap yuva_pixmaps[],
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    const SkColorSpace* decoded_color_space,
    SkYUVColorSpace yuv_color_space,
    bool needs_mips,
    absl::optional<TargetColorParams> target_color_params)
    : needs_mips_(needs_mips),
      target_color_params_(target_color_params),
      plane_config_(plane_config),
      id_(GetNextId()),
      pixmap_(nullptr),
      decoded_color_space_(decoded_color_space),
      subsampling_(subsampling),
      yuv_color_space_(yuv_color_space) {
  yuv_pixmaps_.emplace(std::array<const SkPixmap*, SkYUVAInfo::kMaxPlanes>());
  size_t num_yuva_pixmaps =
      static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config));
  DCHECK_GT(num_yuva_pixmaps, 0U);
  DCHECK_LE(num_yuva_pixmaps, yuv_pixmaps_->size());
  for (size_t i = 0; i < num_yuva_pixmaps; ++i) {
    yuv_pixmaps_->at(i) = &yuva_pixmaps[i];
  }
  DCHECK(IsYuv());

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(needs_mips_);
  safe_size += TargetColorParamsSize(target_color_params_);
  safe_size += PaintOpWriter::SerializedSize(plane_config_);
  safe_size += PaintOpWriter::SerializedSize(subsampling_);
  safe_size += PaintOpWriter::SerializedSize(yuv_color_space_);
  safe_size += PaintOpWriter::SerializedSize(decoded_color_space_.get());
  for (size_t i = 0; i < num_yuva_pixmaps; ++i) {
    safe_size += SafeSizeForPixmap(*yuv_pixmaps_->at(i));
  }
  size_ = safe_size.ValueOrDefault(0);
}

ClientImageTransferCacheEntry::~ClientImageTransferCacheEntry() = default;

// static
base::AtomicSequenceNumber ClientImageTransferCacheEntry::s_next_id_;

uint32_t ClientImageTransferCacheEntry::SerializedSize() const {
  return size_;
}

uint32_t ClientImageTransferCacheEntry::Id() const {
  return id_;
}

void ClientImageTransferCacheEntry::ValidateYUVDataBeforeSerializing() const {
  DCHECK(!pixmap_);
  DCHECK_NE(subsampling_, SkYUVAInfo::Subsampling::kUnknown);
  DCHECK_LE(yuv_pixmaps_->size(), static_cast<size_t>(SkYUVAInfo::kMaxPlanes));
  size_t num_planes = static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config_));
  DCHECK_LE(num_planes, yuv_pixmaps_->size());
  for (size_t i = 0; i < num_planes; ++i) {
    DCHECK(yuv_pixmaps_->at(i));
    const SkPixmap* plane = yuv_pixmaps_->at(i);
    DCHECK_GT(plane->width(), 0);
    DCHECK_GT(plane->height(), 0);
    DCHECK_GT(plane->rowBytes(), 0u);
  }
}

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());
  // We don't need to populate the SerializeOptions here since the writer is
  // only used for serializing primitives.
  PaintOp::SerializeOptions options;
  PaintOpWriter writer(data.data(), data.size(), options);

  writer.Write(needs_mips_);
  WriteTargetColorParams(writer, target_color_params_);
  writer.Write(plane_config_);

  if (plane_config_ != SkYUVAInfo::PlaneConfig::kUnknown) {
    ValidateYUVDataBeforeSerializing();
    writer.Write(subsampling_);
    int num_planes = SkYUVAInfo::NumPlanes(plane_config_);
    writer.Write(yuv_color_space_);
    writer.Write(decoded_color_space_);
    for (int i = 0; i < num_planes; ++i) {
      DCHECK(yuv_pixmaps_->at(i));
      if (!WritePixmap(writer, *yuv_pixmaps_->at(i)))
        return false;
    }
  } else {
    if (!WritePixmap(writer, *pixmap_))
      return false;
  }

  // Size can't be 0 after serialization unless the writer has become invalid.
  if (writer.size() == 0u)
    return false;
  return true;
}

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

bool ServiceImageTransferCacheEntry::BuildFromHardwareDecodedImage(
    GrDirectContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    SkYUVColorSpace yuv_color_space,
    size_t buffer_byte_size,
    bool needs_mips) {
  context_ = context;
  size_ = buffer_byte_size;

  // 1) Generate mipmap chains if requested.
  if (needs_mips) {
    DCHECK(plane_sizes_.empty());
    base::CheckedNumeric<size_t> safe_total_size(0u);
    for (size_t plane = 0; plane < plane_images.size(); plane++) {
      plane_images[plane] = plane_images[plane]->makeTextureImage(
          context_, GrMipMapped::kYes, skgpu::Budgeted::kNo);
      if (!plane_images[plane]) {
        DLOG(ERROR) << "Could not generate mipmap chain for plane " << plane;
        return false;
      }
      plane_sizes_.push_back(plane_images[plane]->textureSize());
      safe_total_size += plane_sizes_.back();
    }
    if (!safe_total_size.AssignIfValid(&size_)) {
      DLOG(ERROR) << "Could not calculate the total image size";
      return false;
    }
  }
  plane_images_ = std::move(plane_images);
  plane_config_ = plane_config;
  subsampling_ = subsampling;
  yuv_color_space_ = yuv_color_space;

  // 2) Create a SkImage backed by |plane_images|.
  // TODO(andrescj): support embedded color profiles for hardware decodes and
  // pass the color space to MakeYUVImageFromUploadedPlanes.
  image_ = MakeYUVImageFromUploadedPlanes(context_, plane_images_, plane_config,
                                          subsampling, yuv_color_space,
                                          SkColorSpace::MakeSRGB());
  if (!image_)
    return false;

  // 3) Fill out the rest of the information.
  has_mips_ = needs_mips;
  fits_on_gpu_ = true;
  return true;
}

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(
    GrDirectContext* context,
    base::span<const uint8_t> data) {
  context_ = context;
  const int32_t max_size = context_->maxTextureSize();

  // We don't need to populate the DeSerializeOptions here since the reader is
  // only used for de-serializing primitives.
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions options(nullptr, nullptr, nullptr,
                                      &scratch_buffer, false, nullptr);
  PaintOpReader reader(data.data(), data.size(), options);

  // Parameters common to RGBA and YUVA images.
  bool needs_mips = false;
  reader.Read(&needs_mips);
  has_mips_ = needs_mips;
  absl::optional<TargetColorParams> target_color_params;
  ReadTargetColorParams(reader, target_color_params);
  plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  reader.Read(&plane_config_);

  const GrMipMapped mip_mapped_for_upload =
      has_mips_ && !target_color_params ? GrMipMapped::kYes : GrMipMapped::kNo;
  SkPixmap rgba_pixmap;
  sk_sp<SkImage> rgba_pixmap_image;
  if (plane_config_ != SkYUVAInfo::PlaneConfig::kUnknown) {
    SkYUVAInfo::Subsampling subsampling = SkYUVAInfo::Subsampling::kUnknown;
    reader.Read(&subsampling);
    if (subsampling == SkYUVAInfo::Subsampling::kUnknown) {
      DLOG(ERROR) << "Invalid subsampling";
      return false;
    }
    subsampling_ = subsampling;
    SkYUVColorSpace yuv_color_space = kIdentity_SkYUVColorSpace;
    reader.Read(&yuv_color_space);
    yuv_color_space_ = yuv_color_space;
    sk_sp<SkColorSpace> decoded_color_space;
    reader.Read(&decoded_color_space);

    int num_planes = SkYUVAInfo::NumPlanes(plane_config_);
    // Read in each plane and reconstruct pixmaps.
    for (int i = 0; i < num_planes; i++) {
      SkPixmap pixmap;
      if (!ReadPixmap(reader, pixmap)) {
        DLOG(ERROR) << "Failed to read plane pixmap";
        return false;
      }
      pixmap.setColorSpace(decoded_color_space);

      // In the GpuImageDecodeCache, we should veto YUV decoding if the planes
      // would be too big. Check again here for the case a malicious renderer .
      fits_on_gpu_ = pixmap.width() <= max_size && pixmap.height() <= max_size;
      if (!fits_on_gpu_) {
        DLOG(ERROR) << "Plane pixmap too large";
        return false;
      }

      sk_sp<SkImage> plane = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
      if (!plane) {
        DLOG(ERROR) << "Failed to create image from plane pixmap";
        return false;
      }
      plane = plane->makeTextureImage(context_, mip_mapped_for_upload,
                                      skgpu::Budgeted::kNo);
      if (!plane) {
        DLOG(ERROR) << "Failed to upload plane pixmap to texture image";
        return false;
      }
      DCHECK(plane->isTextureBacked());
      plane->getBackendTexture(/*flushPendingGrContextIO=*/true);
      plane_sizes_.push_back(plane->textureSize());
      plane_images_.push_back(std::move(plane));
    }
    DCHECK(yuv_color_space_.has_value());
    image_ = MakeYUVImageFromUploadedPlanes(
        context_, plane_images_, plane_config_, subsampling_.value(),
        yuv_color_space_.value(), decoded_color_space);
    if (!image_) {
      DLOG(ERROR) << "Failed to make YUV image from planes.";
      return false;
    }
  } else {
    if (!ReadPixmap(reader, rgba_pixmap)) {
      DLOG(ERROR) << "Failed to read pixmap";
      return false;
    }
    rgba_pixmap_image = SkImage::MakeFromRaster(rgba_pixmap, nullptr, nullptr);
    if (!rgba_pixmap_image) {
      DLOG(ERROR) << "Failed to create image from plane pixmap";
      return false;
    }
    fits_on_gpu_ =
        rgba_pixmap.width() <= max_size && rgba_pixmap.height() <= max_size;
    if (fits_on_gpu_) {
      image_ = rgba_pixmap_image->makeTextureImage(
          context, mip_mapped_for_upload, skgpu::Budgeted::kNo);
      if (!image_) {
        DLOG(ERROR) << "Failed to upload pixmap to texture image";
        return false;
      }
    } else {
      // If the image is on the CPU, no work is needed to generate mips.
      has_mips_ = true;
      image_ = rgba_pixmap_image;
    }
  }
  CHECK(image_);

  // Perform color conversion.
  if (target_color_params) {
    auto target_color_space = target_color_params->color_space.ToSkColorSpace();
    if (!target_color_space) {
      DLOG(ERROR) << "Invalid target color space.";
      return false;
    }

    // TODO(https://crbug.com/1286088): Pass a shared cache as a parameter.
    gfx::ColorConversionSkFilterCache cache;
    image_ = cache.ConvertImage(image_, target_color_space,
                                target_color_params->hdr_metadata,
                                target_color_params->sdr_max_luminance_nits,
                                target_color_params->hdr_max_luminance_relative,
                                target_color_params->enable_tone_mapping,
                                fits_on_gpu_ ? context_ : nullptr);
    if (!image_) {
      DLOG(ERROR) << "Failed image color conversion";
      return false;
    }

    // Color conversion converts to RGBA. Remove all YUV state.
    plane_images_.clear();
    plane_sizes_.clear();
    plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
    plane_sizes_.clear();
    subsampling_ = absl::nullopt;
    yuv_color_space_ = absl::nullopt;

    // If mipmaps were requested, create them after color conversion.
    if (has_mips_ && fits_on_gpu_) {
      image_ = image_->makeTextureImage(context, GrMipMapped::kYes,
                                        skgpu::Budgeted::kNo);
      if (!image_) {
        DLOG(ERROR) << "Failed to generate mipmaps after color conversion";
        return false;
      }
    }
  }

  // If `image_` is still pointing at the original data from `rgba_pixmap`, make
  // a copy of it, because `rgba_pixmap` is directly referencing the transfer
  // buffer's memory, and will go away after this this call.
  if (image_ == rgba_pixmap_image) {
    image_ = SkImage::MakeRasterCopy(rgba_pixmap);
    if (!image_) {
      DLOG(ERROR) << "Failed to create raster copy";
      return false;
    }
  }

  size_ = image_->textureSize();
  return true;
}

const sk_sp<SkImage>& ServiceImageTransferCacheEntry::GetPlaneImage(
    size_t index) const {
  DCHECK_GE(index, 0u);
  DCHECK_LT(index, plane_images_.size());
  DCHECK(plane_images_.at(index));
  return plane_images_.at(index);
}

void ServiceImageTransferCacheEntry::EnsureMips() {
  if (has_mips_)
    return;

  DCHECK(fits_on_gpu_);
  if (is_yuv()) {
    DCHECK(image_);
    DCHECK(yuv_color_space_.has_value());
    DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, plane_config_);
    DCHECK_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config_)),
              plane_images_.size());

    // We first do all the work with local variables. Then, if everything
    // succeeds, we update the object's state. That way, we don't leave it in an
    // inconsistent state if one step of mip generation fails.
    std::vector<sk_sp<SkImage>> mipped_planes;
    std::vector<size_t> mipped_plane_sizes;
    for (size_t plane = 0; plane < plane_images_.size(); plane++) {
      DCHECK(plane_images_.at(plane));
      sk_sp<SkImage> mipped_plane = plane_images_.at(plane)->makeTextureImage(
          context_, GrMipMapped::kYes, skgpu::Budgeted::kNo);
      if (!mipped_plane)
        return;
      mipped_planes.push_back(std::move(mipped_plane));
      mipped_plane_sizes.push_back(mipped_planes.back()->textureSize());
    }
    sk_sp<SkImage> mipped_image = MakeYUVImageFromUploadedPlanes(
        context_, mipped_planes, plane_config_, subsampling_.value(),
        yuv_color_space_.value(),
        image_->refColorSpace() /* image_color_space */);
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to create YUV image from mipmapped planes";
      return;
    }
    // Note that we cannot update |size_| because the transfer cache keeps track
    // of a total size that is not updated after EnsureMips(). The original size
    // is used when the image is deleted from the cache.
    plane_images_ = std::move(mipped_planes);
    plane_sizes_ = std::move(mipped_plane_sizes);
    image_ = std::move(mipped_image);
  } else {
    sk_sp<SkImage> mipped_image = image_->makeTextureImage(
        context_, GrMipMapped::kYes, skgpu::Budgeted::kNo);
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to mipmapped image";
      return;
    }
    image_ = std::move(mipped_image);
  }
  has_mips_ = true;
}

}  // namespace cc
