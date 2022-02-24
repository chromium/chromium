// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include <array>
#include <type_traits>
#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"

namespace cc {
namespace {

// TODO(https://crbug.com/1286076): Plumb the true parameters in here.
constexpr float kTempMaxLuminanceNits = 100.f;
constexpr float kTempHDRMaxLuminanceRelative = 1.f;

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

// TODO(ericrk): Replace calls to this with calls to SkImage::makeTextureImage,
// once that function handles colorspaces. https://crbug.com/834837
sk_sp<SkImage> MakeTextureImage(
    GrDirectContext* context,
    sk_sp<SkImage> source_image,
    absl::optional<TargetColorParams> target_color_params,
    GrMipMapped mip_mapped) {
  // Step 1: Upload image and generate mips if necessary. If we will be applying
  // a color-space conversion, don't generate mips yet, instead do it after
  // conversion, in step 3.
  // NOTE: |target_color_space| is only passed over the transfer cache if needed
  // (non-null, different from the source color space).
  bool add_mips_after_color_conversion =
      target_color_params && mip_mapped == GrMipMapped::kYes;
  sk_sp<SkImage> uploaded_image = source_image->makeTextureImage(
      context, add_mips_after_color_conversion ? GrMipMapped::kNo : mip_mapped,
      SkBudgeted::kNo);

  // Step 2: Apply a color-space conversion if necessary.
  if (uploaded_image && target_color_params) {
    // TODO(https://crbug.com/1286088): Pass a shared cache as a parameter.
    gfx::ColorConversionSkFilterCache cache;
    uploaded_image = cache.ConvertImage(
        uploaded_image, target_color_params->color_space.ToSkColorSpace(),
        target_color_params->sdr_max_luminance_nits,
        target_color_params->hdr_max_luminance_relative, context);
  }

  // Step 3: If we had a colorspace conversion, we couldn't mipmap in step 1, so
  // add mips here.
  if (uploaded_image && add_mips_after_color_conversion) {
    uploaded_image = uploaded_image->makeTextureImage(
        context, GrMipMapped::kYes, SkBudgeted::kNo);
  }

  return uploaded_image;
}

base::CheckedNumeric<uint32_t> SafeSizeForPixmap(const SkPixmap& pixmap) {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += sizeof(uint64_t);  // color type
  safe_size += sizeof(uint64_t);  // width
  safe_size += sizeof(uint64_t);  // height
  safe_size += sizeof(uint64_t);  // has color space
  if (pixmap.colorSpace())
    safe_size += pixmap.colorSpace()->writeToMemory(nullptr);  // color space
  safe_size += sizeof(uint64_t);                               // row bytes
  safe_size += sizeof(uint64_t);                               // data size
  safe_size += sizeof(16u);                                    // alignment
  safe_size += pixmap.computeByteSize();                       // data
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
  uint32_t width;
  reader.Read(&width);
  uint32_t height;
  reader.Read(&height);
  if (width == 0 || height == 0) {
    DLOG(ERROR) << "Empty width or height";
    return false;
  }

  sk_sp<SkColorSpace> color_space;
  reader.Read(&color_space);
  auto image_info = SkImageInfo::Make(width, height, color_type,
                                      kPremul_SkAlphaType, color_space);
  size_t row_bytes;
  reader.ReadSize(&row_bytes);
  if (row_bytes < image_info.minRowBytes()) {
    DLOG(ERROR) << "Row bytes " << row_bytes << " less than minimum "
                << image_info.minRowBytes();
    return false;
  }
  size_t data_size;
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
    const SkColorSpace* target_color_space,
    bool needs_mips)
    : needs_mips_(needs_mips),
      id_(GetNextId()),
      pixmap_(pixmap),
      target_color_space_(target_color_space),
      decoded_color_space_(nullptr) {
  size_t target_color_space_size =
      target_color_space ? target_color_space->writeToMemory(nullptr) : 0u;
  size_t pixmap_color_space_size =
      pixmap_->colorSpace() ? pixmap_->colorSpace()->writeToMemory(nullptr)
                            : 0u;

  // x64 has 8-byte alignment for uint64_t even though x86 has 4-byte
  // alignment.  Always use 8 byte alignment.
  const size_t align = sizeof(uint64_t);

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::HeaderBytes();
  safe_size += sizeof(uint32_t);  // is_yuv
  safe_size += sizeof(uint32_t);  // color type
  safe_size += sizeof(uint32_t);  // width
  safe_size += sizeof(uint32_t);  // height
  safe_size += sizeof(uint32_t);  // has mips
  safe_size += sizeof(uint64_t) + align;  // pixels size + alignment
  safe_size += sizeof(uint64_t) + align;  // row bytes + alignment
  safe_size += target_color_space_size + sizeof(uint64_t) + align;
  safe_size += pixmap_color_space_size + sizeof(uint64_t) + align;
  // Include 4 bytes of padding so we can always align our data pointer to a
  // 4-byte boundary.
  safe_size += 4;
  safe_size += pixmap_->computeByteSize();
  size_ = safe_size.ValueOrDefault(0);
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap yuva_pixmaps[],
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    const SkColorSpace* decoded_color_space,
    SkYUVColorSpace yuv_color_space,
    bool needs_mips)
    : needs_mips_(needs_mips),
      plane_config_(plane_config),
      id_(GetNextId()),
      pixmap_(nullptr),
      target_color_space_(nullptr),
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
  size_t decoded_color_space_size =
      decoded_color_space ? decoded_color_space->writeToMemory(nullptr) : 0u;

  // x64 has 8-byte alignment for uint64_t even though x86 has 4-byte
  // alignment.  Always use 8 byte alignment.
  const size_t align = sizeof(uint64_t);

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::HeaderBytes();

  safe_size += sizeof(uint32_t);  // has mips
  safe_size += sizeof(uint64_t);  // target color space stub (is nullptr)

  safe_size += sizeof(uint32_t);  // plane_config
  safe_size += sizeof(uint32_t);  // subsampling
  safe_size += sizeof(uint32_t);  // YUVA color matrix for YUVA image
  safe_size += decoded_color_space_size + align;  // SkColorSpace for YUVA image
  for (size_t i = 0; i < num_yuva_pixmaps; ++i)
    safe_size += SafeSizeForPixmap(*yuv_pixmaps_->at(i));
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

  writer.Write(static_cast<uint32_t>(needs_mips_ ? 1 : 0));
  writer.Write(target_color_space_);
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
          context_, GrMipMapped::kYes, SkBudgeted::kNo);
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
  uint32_t needs_mips;
  reader.Read(&needs_mips);
  has_mips_ = needs_mips;
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);
  absl::optional<TargetColorParams> target_color_params;
  if (target_color_space) {
    target_color_params = TargetColorParams();
    target_color_params->color_space = gfx::ColorSpace(*target_color_space);
    target_color_params->sdr_max_luminance_nits = kTempMaxLuminanceNits;
    target_color_params->hdr_max_luminance_relative =
        kTempHDRMaxLuminanceRelative;
  }
  plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  reader.Read(&plane_config_);

  if (plane_config_ != SkYUVAInfo::PlaneConfig::kUnknown) {
    SkYUVAInfo::Subsampling subsampling = SkYUVAInfo::Subsampling::kUnknown;
    reader.Read(&subsampling);
    if (subsampling == SkYUVAInfo::Subsampling::kUnknown) {
      DLOG(ERROR) << "Invalid subsampling";
      return false;
    }
    subsampling_ = subsampling;
    SkYUVColorSpace yuv_color_space;
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

      DCHECK(!target_color_params);
      sk_sp<SkImage> plane = MakeSkImage(pixmap, target_color_params);
      if (!plane) {
        DLOG(ERROR) << "Failed to upload plane pixmap";
        return false;
      }
      DCHECK(plane->isTextureBacked());

      plane_sizes_.push_back(plane->textureSize());
      size_ += plane_sizes_.back();

      // |plane_images_| must be set for use in EnsureMips(), memory dumps, and
      // unit tests.
      plane_images_.push_back(std::move(plane));
    }
    DCHECK(yuv_color_space_.has_value());
    image_ = MakeYUVImageFromUploadedPlanes(
        context_, plane_images_, plane_config_, subsampling_.value(),
        yuv_color_space_.value(), decoded_color_space);
  } else {
    SkPixmap pixmap;
    if (!ReadPixmap(reader, pixmap)) {
      DLOG(ERROR) << "Failed to read pixmap";
      return false;
    }
    fits_on_gpu_ = pixmap.width() <= max_size && pixmap.height() <= max_size;
    image_ = MakeSkImage(pixmap, target_color_params);
    if (image_)
      size_ = image_->textureSize();
  }

  return true;
}

sk_sp<SkImage> ServiceImageTransferCacheEntry::MakeSkImage(
    const SkPixmap& pixmap,
    absl::optional<TargetColorParams> target_color_params) {
  DCHECK(context_);
  sk_sp<SkImage> image;
  if (fits_on_gpu_) {
    image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    if (!image)
      return nullptr;
    image = MakeTextureImage(context_, std::move(image), target_color_params,
                             has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo);
  } else {
    // If the image is on the CPU, no work is needed to generate mips.
    has_mips_ = true;
    sk_sp<SkImage> original =
        SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
    if (!original)
      return nullptr;
    if (target_color_params) {
      // TODO(https://crbug.com/1286088): Pass a shared cache as a parameter.
      gfx::ColorConversionSkFilterCache cache;
      image = cache.ConvertImage(
          original, target_color_params->color_space.ToSkColorSpace(),
          target_color_params->sdr_max_luminance_nits,
          target_color_params->hdr_max_luminance_relative, /*context=*/nullptr);
      // If color space conversion is a noop, use original data.
      if (image == original)
        image = SkImage::MakeRasterCopy(pixmap);
    } else {
      // No color conversion to do, use original data.
      image = SkImage::MakeRasterCopy(pixmap);
    }
  }

  // Make sure the GPU work to create the backing texture is issued.
  if (image)
    image->getBackendTexture(true /* flushPendingGrContextIO */);

  return image;
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
          context_, GrMipMapped::kYes, SkBudgeted::kNo);
      if (!mipped_plane)
        return;
      mipped_planes.push_back(std::move(mipped_plane));
      mipped_plane_sizes.push_back(mipped_planes.back()->textureSize());
    }
    sk_sp<SkImage> mipped_image = MakeYUVImageFromUploadedPlanes(
        context_, mipped_planes, plane_config_, subsampling_.value(),
        yuv_color_space_.value(),
        image_->refColorSpace() /* image_color_space */);
    if (!mipped_image)
      return;
    // Note that we cannot update |size_| because the transfer cache keeps track
    // of a total size that is not updated after EnsureMips(). The original size
    // is used when the image is deleted from the cache.
    plane_images_ = std::move(mipped_planes);
    plane_sizes_ = std::move(mipped_plane_sizes);
    image_ = std::move(mipped_image);
    has_mips_ = true;
    return;
  }

  sk_sp<SkImage> mipped_image =
      image_->makeTextureImage(context_, GrMipMapped::kYes, SkBudgeted::kNo);
  if (!mipped_image)
    return;
  image_ = std::move(mipped_image);
  has_mips_ = true;
}

}  // namespace cc
