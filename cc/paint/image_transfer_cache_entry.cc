// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/image_transfer_cache_entry.h"

#include <algorithm>
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
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace cc {
namespace {

struct Context {
  const std::vector<sk_sp<SkImage>> sk_planes_;
};

void ReleaseContext(SkImages::ReleaseContext context) {
  auto* texture_context = static_cast<Context*>(context);
  delete texture_context;
}

bool IsYUVAInfoValid(SkYUVAInfo::PlaneConfig plane_config,
                     SkYUVAInfo::Subsampling subsampling,
                     SkYUVColorSpace yuv_color_space) {
  if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown) {
    return subsampling == SkYUVAInfo::Subsampling::kUnknown &&
           yuv_color_space == kIdentity_SkYUVColorSpace;
  }
  return subsampling != SkYUVAInfo::Subsampling::kUnknown;
}

int NumPixmapsForYUVConfig(SkYUVAInfo::PlaneConfig plane_config) {
  return std::max(SkYUVAInfo::NumPlanes(plane_config), 1);
}

// Creates a SkImage backed by the YUV textures corresponding to |plane_images|.
// The layout is specified by |plane_images_format|). The backend textures are
// first extracted out of the |plane_images| (and work is flushed on each one).
// Note that we assume that the image is opaque (no alpha plane). Then, a
// SkImage is created out of those textures using the
// SkImages::TextureFromYUVATextures() API. Finally, |image_color_space| is the
// color space of the resulting image after applying |yuv_color_space|
// (converting from YUV to RGB). This is assumed to be sRGB if nullptr.
//
// On success, the resulting SkImage is
// returned. On failure, nullptr is returned (e.g., if one of the backend
// textures is invalid or a Skia error occurs).
sk_sp<SkImage> MakeYUVImageFromUploadedPlanes(
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder,
    const std::vector<sk_sp<SkImage>>& plane_images,
    const SkYUVAInfo& yuva_info,
    sk_sp<SkColorSpace> image_color_space) {
  // 1) Extract the textures.
  DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, yuva_info.planeConfig());
  DCHECK_NE(SkYUVAInfo::Subsampling::kUnknown, yuva_info.subsampling());
  DCHECK_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(yuva_info.planeConfig())),
            plane_images.size());
  DCHECK_LE(plane_images.size(),
            base::checked_cast<size_t>(SkYUVAInfo::kMaxPlanes));

  if (graphite_recorder) {
    sk_sp<SkImage> image = SkImages::TextureFromYUVAImages(
        graphite_recorder, yuva_info, plane_images, image_color_space);
    if (!image) {
      DLOG(ERROR) << "Could not create YUV image";
      return nullptr;
    }
    return image;
  }

  std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> plane_backend_textures;
  for (size_t plane = 0u; plane < plane_images.size(); plane++) {
    if (!SkImages::GetBackendTextureFromImage(
            plane_images[plane], &plane_backend_textures[plane],
            true /* flushPendingGrContextIO */)) {
      DLOG(ERROR) << "Invalid backend texture found";
      return nullptr;
    }
  }

  // 2) Create the YUV image.
  GrYUVABackendTextures yuva_backend_textures(
      yuva_info, plane_backend_textures.data(), kTopLeft_GrSurfaceOrigin);
  Context* ctx = new Context{plane_images};
  sk_sp<SkImage> image = SkImages::TextureFromYUVATextures(
      gr_context, yuva_backend_textures, std::move(image_color_space),
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
  safe_size += PaintOpWriter::SerializedSize(pixmap.rowBytes());
  safe_size += 16u;  // The max of GetAlignmentForColorType().
  safe_size += PaintOpWriter::SerializedSizeOfBytes(pixmap.computeByteSize());
  return safe_size;
}

base::CheckedNumeric<uint32_t> SafeSizeForImage(
    const ClientImageTransferCacheEntry::Image& image) {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(image.yuv_plane_config);
  safe_size += PaintOpWriter::SerializedSize(image.yuv_subsampling);
  safe_size += PaintOpWriter::SerializedSize(image.yuv_color_space);
  safe_size += PaintOpWriter::SerializedSize(image.color_space.get());
  const int num_pixmaps = NumPixmapsForYUVConfig(image.yuv_plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    safe_size += SafeSizeForPixmap(*image.pixmaps.at(i));
  }
  return safe_size;
}

size_t GetAlignmentForColorType(SkColorType color_type) {
  size_t bpp = SkColorTypeBytesPerPixel(color_type);
  if (bpp <= 4)
    return 4;
  if (bpp <= 16)
    return 16;
  NOTREACHED();
}

bool WritePixmap(PaintOpWriter& writer, const SkPixmap& pixmap) {
  if (pixmap.width() == 0 || pixmap.height() == 0) {
    DLOG(ERROR) << "Cannot write empty pixmap";
    return false;
  }
  DCHECK_GT(pixmap.width(), 0);
  DCHECK_GT(pixmap.height(), 0);
  DCHECK_GT(pixmap.rowBytes(), 0u);
  writer.Write(pixmap.colorType());
  writer.Write(pixmap.width());
  writer.Write(pixmap.height());
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

  auto image_info =
      SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
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

bool WriteImage(PaintOpWriter& writer,
                const ClientImageTransferCacheEntry::Image& image) {
  DCHECK(IsYUVAInfoValid(image.yuv_plane_config, image.yuv_subsampling,
                         image.yuv_color_space));
  writer.Write(image.color_space);
  writer.Write(image.yuv_plane_config);
  writer.Write(image.yuv_subsampling);
  writer.Write(image.yuv_color_space);
  const int num_pixmaps = NumPixmapsForYUVConfig(image.yuv_plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    if (!WritePixmap(writer, *image.pixmaps.at(i))) {
      return false;
    }
  }
  return true;
}

sk_sp<SkImage> ReadImage(
    PaintOpReader& reader,
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder,
    bool mip_mapped_for_upload,
    std::optional<SkYUVAInfo>* out_yuva_info = nullptr,
    std::vector<sk_sp<SkImage>>* out_yuva_plane_images = nullptr) {
  int max_size;
  if (gr_context) {
    max_size = gr_context->maxTextureSize();
  } else if (graphite_recorder) {
    max_size = graphite_recorder->maxTextureSize();
  } else {
    // Allow a nullptr context for testing using the software renderer.
    max_size = 0;
  }

  sk_sp<SkColorSpace> color_space;
  reader.Read(&color_space);

  SkYUVAInfo::PlaneConfig plane_config = SkYUVAInfo::PlaneConfig::kUnknown;
  reader.Read(&plane_config);
  if (plane_config < SkYUVAInfo::PlaneConfig::kUnknown ||
      plane_config > SkYUVAInfo::PlaneConfig::kLast) {
    DLOG(ERROR) << "Invalid plane config";
    return nullptr;
  }

  SkYUVAInfo::Subsampling subsampling = SkYUVAInfo::Subsampling::kUnknown;
  reader.Read(&subsampling);
  if (subsampling < SkYUVAInfo::Subsampling::kUnknown ||
      subsampling > SkYUVAInfo::Subsampling::kLast) {
    DLOG(ERROR) << "Invalid subsampling";
    return nullptr;
  }

  SkYUVColorSpace yuv_color_space = kIdentity_SkYUVColorSpace;
  reader.Read(&yuv_color_space);
  if (yuv_color_space < kJPEG_Full_SkYUVColorSpace ||
      yuv_color_space > kLastEnum_SkYUVColorSpace) {
    DLOG(ERROR) << "Invalid YUV color space";
    return nullptr;
  }

  if (!IsYUVAInfoValid(plane_config, subsampling, yuv_color_space)) {
    DLOG(ERROR) << "Invalid YUV configuration";
    return nullptr;
  }

  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes];
  bool fits_on_gpu = true;
  const int num_pixmaps = NumPixmapsForYUVConfig(plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    if (!ReadPixmap(reader, pixmaps[i])) {
      DLOG(ERROR) << "Failed to read pixmap";
      return nullptr;
    }
    fits_on_gpu &=
        pixmaps[i].width() <= max_size && pixmaps[i].height() <= max_size;

    // This is likely unnecessary for YUVA images (the pixmaps of the individual
    // planes should be ignored), but is left here to avoid behavior changes
    // during refactoring.
    pixmaps[i].setColorSpace(color_space);
  }

  if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown) {
    // Point `image_` directly to the data in the transfer cache.
    auto image = SkImages::RasterFromPixmap(pixmaps[0], nullptr, nullptr);
    if (!image) {
      DLOG(ERROR) << "Failed to create image from pixmap";
      return nullptr;
    }

    // Upload to the GPU if the image will fit.
    if (fits_on_gpu) {
      if (gr_context) {
        image = SkImages::TextureFromImage(gr_context, image,
                                           mip_mapped_for_upload
                                               ? skgpu::Mipmapped::kYes
                                               : skgpu::Mipmapped::kNo,
                                           skgpu::Budgeted::kNo);
      } else {
        CHECK(graphite_recorder);
        SkImage::RequiredProperties props{.fMipmapped = mip_mapped_for_upload};
        image = SkImages::TextureFromImage(graphite_recorder, image, props);
      }

      if (!image) {
        DLOG(ERROR) << "Failed to upload pixmap to texture image.";
        return nullptr;
      }
      DCHECK(image->isTextureBacked());
    }

    if (out_yuva_info) {
      *out_yuva_info = std::nullopt;
    }
    if (out_yuva_plane_images) {
      out_yuva_plane_images->clear();
    }
    return image;
  } else {
    if (!fits_on_gpu) {
      DLOG(ERROR) << "YUVA images must fit in the GPU texture size limit";
      return nullptr;
    }

    // Upload the planes to the GPU.
    std::vector<sk_sp<SkImage>> plane_images;
    for (int i = 0; i < num_pixmaps; i++) {
      sk_sp<SkImage> plane =
          SkImages::RasterFromPixmap(pixmaps[i], nullptr, nullptr);
      if (!plane) {
        DLOG(ERROR) << "Failed to create image from plane pixmap";
        return nullptr;
      }
      if (gr_context) {
        plane = SkImages::TextureFromImage(gr_context, plane,
                                           mip_mapped_for_upload
                                               ? skgpu::Mipmapped::kYes
                                               : skgpu::Mipmapped::kNo,
                                           skgpu::Budgeted::kNo);
        // Uploading pixels is a heavy operation that might take long and lead
        // to yields to higher priority scheduler sequences. To ensure upload is
        // done, perform flush for Ganesh in DDL mode (no-op if image is null).
        SkImages::GetBackendTextureFromImage(plane, /*outTexture=*/nullptr,
                                             /*flushPendingGrContextIO=*/true);
      } else {
        CHECK(graphite_recorder);
        SkImage::RequiredProperties props{.fMipmapped = mip_mapped_for_upload};
        // Graphite is like Ganesh in DDL mode but Graphite has lower CPU
        // overhead with modern APIs leading to lesser scheduling concerns.
        // Also, eventually we want to move tile raster off the GPU main thread.
        // Based on these reasons, its okay to not flush for Graphite here.
        // TODO(crbug.com/40922674): Revisit flushes for Graphite here if yield
        // to scheduler is needed.
        plane = SkImages::TextureFromImage(graphite_recorder, plane, props);
      }
      if (!plane) {
        DLOG(ERROR) << "Failed to upload plane pixmap to texture image";
        return nullptr;
      }
      CHECK(plane->isTextureBacked());
      plane_images.push_back(std::move(plane));
    }
    SkYUVAInfo yuva_info(plane_images[0]->dimensions(), plane_config,
                         subsampling, yuv_color_space);

    // Build the YUV image from its planes.
    auto image = MakeYUVImageFromUploadedPlanes(
        gr_context, graphite_recorder, plane_images, yuva_info, color_space);
    if (!image) {
      DLOG(ERROR) << "Failed to make YUV image from planes.";
      return nullptr;
    }

    if (out_yuva_info) {
      *out_yuva_info = yuva_info;
    }
    if (out_yuva_plane_images) {
      *out_yuva_plane_images = std::move(plane_images);
    }
    return image;
  }
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

////////////////////////////////////////////////////////////////////////////////
// ClientImageTransferCacheEntry::Image

ClientImageTransferCacheEntry::Image::Image() {}
ClientImageTransferCacheEntry::Image::Image(const Image&) = default;
ClientImageTransferCacheEntry::Image&
ClientImageTransferCacheEntry::Image::operator=(const Image&) = default;

ClientImageTransferCacheEntry::Image::Image(const SkPixmap* pixmap)
    : color_space(pixmap->colorSpace()) {
  DCHECK(pixmap);
  pixmaps[0] = pixmap;
}

ClientImageTransferCacheEntry::Image::Image(const SkPixmap yuva_pixmaps[],
                                            const SkYUVAInfo& yuva_info,
                                            const SkColorSpace* color_space)
    : yuv_plane_config(yuva_info.planeConfig()),
      yuv_subsampling(yuva_info.subsampling()),
      yuv_color_space(yuva_info.yuvColorSpace()),
      color_space(color_space) {
  // The size of the first plane must equal the size specified in the
  // SkYUVAInfo.
  DCHECK(yuva_info.dimensions() == yuva_pixmaps[0].dimensions());
  // We fail to serialize some parameters.
  DCHECK_EQ(yuva_info.origin(), kTopLeft_SkEncodedOrigin);
  DCHECK_EQ(yuva_info.sitingX(), SkYUVAInfo::Siting::kCentered);
  DCHECK_EQ(yuva_info.sitingY(), SkYUVAInfo::Siting::kCentered);
  DCHECK(IsYUVAInfoValid(yuv_plane_config, yuv_subsampling, yuv_color_space));
  for (int i = 0; i < SkYUVAInfo::NumPlanes(yuv_plane_config); ++i) {
    pixmaps[i] = &yuva_pixmaps[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClientImageTransferCacheEntry

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const Image& image,
    bool needs_mips,
    const std::optional<gfx::HDRMetadata>& hdr_metadata,
    sk_sp<SkColorSpace> target_color_space)
    : needs_mips_(needs_mips),
      target_color_space_(target_color_space),
      id_(GetNextId()),
      image_(image),
      hdr_metadata_(hdr_metadata) {
  ComputeSize();
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const Image& image,
    const Image& gainmap_image,
    const SkGainmapInfo& gainmap_info,
    bool needs_mips)
    : needs_mips_(needs_mips),
      id_(GetNextId()),
      image_(image),
      gainmap_image_(gainmap_image),
      gainmap_info_(gainmap_info) {
  ComputeSize();
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

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());
  // We don't need to populate the SerializeOptions here since the writer is
  // only used for serializing primitives.
  PaintOp::SerializeOptions options;
  PaintOpWriter writer(data.data(), data.size(), options);

  DCHECK_EQ(gainmap_image_.has_value(), gainmap_info_.has_value());
  bool has_gainmap = gainmap_image_.has_value();
  writer.Write(has_gainmap);
  writer.Write(needs_mips_);
  writer.Write(hdr_metadata_.has_value());
  if (hdr_metadata_.has_value()) {
    writer.Write(hdr_metadata_.value());
  }
  writer.Write(target_color_space_.get());
  WriteImage(writer, image_);

  if (has_gainmap) {
    WriteImage(writer, gainmap_image_.value());
    writer.Write(gainmap_info_.value());
  }

  // Size can't be 0 after serialization unless the writer has become invalid.
  if (writer.size() == 0u)
    return false;
  return true;
}

void ClientImageTransferCacheEntry::ComputeSize() {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize<bool>();  // has_gainmap
  safe_size += PaintOpWriter::SerializedSize<bool>();  // needs_mips
  safe_size += PaintOpWriter::SerializedSize<bool>();  // has_hdr_metadata
  if (hdr_metadata_.has_value()) {
    safe_size += PaintOpWriter::SerializedSize(hdr_metadata_.value());
  }
  safe_size += PaintOpWriter::SerializedSize(target_color_space_.get());
  safe_size += SafeSizeForImage(image_);
  if (gainmap_image_) {
    DCHECK(gainmap_info_);
    safe_size += SafeSizeForImage(gainmap_image_.value());
    if (gainmap_info_.has_value()) {
      safe_size += PaintOpWriter::SerializedSize(gainmap_info_.value());
    }
  }

  size_ = safe_size.ValueOrDefault(0);
}

////////////////////////////////////////////////////////////////////////////////
// ServiceImageTransferCacheEntry

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

bool ServiceImageTransferCacheEntry::BuildFromHardwareDecodedImage(
    GrDirectContext* gr_context,
    std::vector<sk_sp<SkImage>> plane_images,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    SkYUVColorSpace yuv_color_space,
    size_t buffer_byte_size,
    bool needs_mips) {
  // Only supported on Ganesh for now since this code path is only used on CrOS.
  CHECK(gr_context);
  gr_context_ = gr_context;
  size_ = buffer_byte_size;

  // 1) Generate mipmap chains if requested.
  if (needs_mips) {
    DCHECK(plane_sizes_.empty());
    base::CheckedNumeric<size_t> safe_total_size(0u);
    for (size_t plane = 0; plane < plane_images.size(); plane++) {
      plane_images[plane] = SkImages::TextureFromImage(
          gr_context_, plane_images[plane], skgpu::Mipmapped::kYes,
          skgpu::Budgeted::kNo);
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
  if (static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config)) !=
      plane_images_.size()) {
    DLOG(ERROR) << "Expected " << SkYUVAInfo::NumPlanes(plane_config)
                << " planes, got " << plane_images_.size();
    return false;
  }
  yuva_info_ = SkYUVAInfo(plane_images_[0]->dimensions(), plane_config,
                          subsampling, yuv_color_space);

  // 2) Create a SkImage backed by |plane_images|.
  // TODO(andrescj): support embedded color profiles for hardware decodes and
  // pass the color space to MakeYUVImageFromUploadedPlanes.
  image_ = MakeYUVImageFromUploadedPlanes(
      gr_context_, /*graphite_recorder=*/nullptr, plane_images_,
      yuva_info_.value(), SkColorSpace::MakeSRGB());
  if (!image_) {
    return false;
  }
  DCHECK(image_->isTextureBacked());
  return true;
}

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder,
    base::span<const uint8_t> data) {
  gr_context_ = gr_context;
  graphite_recorder_ = graphite_recorder;

  // We don't need to populate the DeSerializeOptions here since the reader is
  // only used for de-serializing primitives.
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions options{.scratch_buffer = scratch_buffer};
  PaintOpReader reader(data.data(), data.size(), options);

  // Parameters common to RGBA and YUVA images.
  reader.Read(&has_gainmap_);
  bool needs_mips = false;
  reader.Read(&needs_mips);
  bool has_hdr_metadata = false;
  reader.Read(&has_hdr_metadata);
  if (has_hdr_metadata) {
    gfx::HDRMetadata hdr_metadata_value;
    reader.Read(&hdr_metadata_value);
    hdr_metadata_ = hdr_metadata_value;
  }
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);

  const bool mip_mapped_for_upload = needs_mips && !target_color_space;

  // Deserialize the image.
  image_ = ReadImage(reader, gr_context, graphite_recorder,
                     mip_mapped_for_upload, &yuva_info_, &plane_images_);
  if (!image_) {
    DLOG(ERROR) << "Failed to deserialize image.";
    return false;
  }

  // If the image doesn't fit in the GPU texture limits, then it is still on
  // the CPU, pointing at memory in the transfer buffer.
  sk_sp<SkImage> image_referencing_transfer_buffer;
  if (!image_->isTextureBacked()) {
    image_referencing_transfer_buffer = image_;
  }
  for (const auto& plane_image : plane_images_) {
    plane_sizes_.push_back(plane_image->textureSize());
  }

  // Read the gainmap image, if one was specified to exist.
  sk_sp<SkImage> gainmap_image_referencing_transfer_buffer;
  if (has_gainmap_) {
    gainmap_image_ =
        ReadImage(reader, gr_context, graphite_recorder, mip_mapped_for_upload);
    if (!gainmap_image_) {
      DLOG(ERROR) << "Failed to deserialize gainmap image.";
      return false;
    }
    if (!gainmap_image_->isTextureBacked()) {
      gainmap_image_referencing_transfer_buffer = gainmap_image_;
    }
    reader.Read(&gainmap_info_);
  }

  // Determine if this image will be tone mapped.
  const bool is_tone_mapped =
      has_gainmap_ || ToneMapUtil::UseGlobalToneMapFilter(image_->colorSpace());

  // Perform color conversion (if no tone mapping is needed).
  if (target_color_space && !is_tone_mapped) {
    if (graphite_recorder_) {
      SkImage::RequiredProperties props{.fMipmapped = needs_mips};
      image_ =
          image_->makeColorSpace(graphite_recorder_, target_color_space, props);
    } else {
      // TODO(crbug.com/40267231): It's possible for both `gr_context` and
      // `graphite_recorder` to be nullptr if `image_` is not texture backed.
      // Need to handle this case (currently just goes through gr_context path
      // with nullptr context).
      image_ = image_->makeColorSpace(gr_context_, target_color_space);
      if (needs_mips && gr_context_ && image_ && image_->isTextureBacked()) {
        image_ = SkImages::TextureFromImage(
            gr_context, image_, skgpu::Mipmapped::kYes, skgpu::Budgeted::kNo);
      }
    }
    if (!image_) {
      DLOG(ERROR) << "Failed image color conversion.";
      return false;
    }

    // Color conversion converts to RGBA. Remove all YUV state.
    yuva_info_ = std::nullopt;
    plane_images_.clear();
    plane_sizes_.clear();

    // Ensure mipmaps were created if requested.
    if (image_->isTextureBacked()) {
      DCHECK_EQ(needs_mips, image_->hasMipmaps());
    }
  }

  // If `image_` or `gainmap_image_` is still directly referencing the transfer
  // buffer's memory, make a copy of it (because the memory will go away after
  // this this call).
  auto copy_from_transfer_buffer =
      [](sk_sp<SkImage>& image,
         sk_sp<SkImage> image_referencing_transfer_buffer) {
        if (!image || image != image_referencing_transfer_buffer) {
          return true;
        }
        SkPixmap pixmap;
        if (!image->peekPixels(&pixmap)) {
          NOTREACHED()
              << "Image should be referencing transfer buffer SkPixmap";
        }
        image = SkImages::RasterFromPixmapCopy(pixmap);
        if (!image) {
          DLOG(ERROR) << "Failed to create raster copy";
          return false;
        }
        return true;
      };
  if (!copy_from_transfer_buffer(image_, image_referencing_transfer_buffer)) {
    return false;
  }
  if (!copy_from_transfer_buffer(gainmap_image_,
                                 gainmap_image_referencing_transfer_buffer)) {
    return false;
  }

  size_ = image_->textureSize();
  if (gainmap_image_) {
    size_ += gainmap_image_->textureSize();
  }
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
  if (!image_ || !image_->isTextureBacked()) {
    return;
  }
  if (image_->hasMipmaps()) {
    return;
  }

  if (is_yuv()) {
    DCHECK(image_);
    DCHECK(yuva_info_.has_value());
    DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, yuva_info_->planeConfig());
    DCHECK_EQ(
        static_cast<size_t>(SkYUVAInfo::NumPlanes(yuva_info_->planeConfig())),
        plane_images_.size());

    // We first do all the work with local variables. Then, if everything
    // succeeds, we update the object's state. That way, we don't leave it in an
    // inconsistent state if one step of mip generation fails.
    std::vector<sk_sp<SkImage>> mipped_planes;
    std::vector<size_t> mipped_plane_sizes;
    for (size_t plane = 0; plane < plane_images_.size(); plane++) {
      CHECK(plane_images_.at(plane));
      sk_sp<SkImage> mipped_plane;
      if (gr_context_) {
        mipped_plane = SkImages::TextureFromImage(
            gr_context_, plane_images_.at(plane), skgpu::Mipmapped::kYes,
            skgpu::Budgeted::kNo);
      } else {
        CHECK(graphite_recorder_);
        SkImage::RequiredProperties props{.fMipmapped = true};
        mipped_plane = SkImages::TextureFromImage(
            graphite_recorder_, plane_images_.at(plane), props);
      }
      if (!mipped_plane) {
        DLOG(ERROR) << "Failed to mipmap plane.";
        return;
      }
      mipped_planes.push_back(std::move(mipped_plane));
      mipped_plane_sizes.push_back(mipped_planes.back()->textureSize());
    }
    sk_sp<SkImage> mipped_image = MakeYUVImageFromUploadedPlanes(
        gr_context_, graphite_recorder_, mipped_planes, yuva_info_.value(),
        image_->refColorSpace() /* image_color_space */);
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to create YUV image from mipmapped planes.";
      return;
    }
    // Note that we cannot update |size_| because the transfer cache keeps track
    // of a total size that is not updated after EnsureMips(). The original size
    // is used when the image is deleted from the cache.
    plane_images_ = std::move(mipped_planes);
    plane_sizes_ = std::move(mipped_plane_sizes);
    image_ = std::move(mipped_image);
  } else {
    sk_sp<SkImage> mipped_image;
    if (gr_context_) {
      mipped_image = SkImages::TextureFromImage(
          gr_context_, image_, skgpu::Mipmapped::kYes, skgpu::Budgeted::kNo);
    } else {
      CHECK(graphite_recorder_);
      SkImage::RequiredProperties props{.fMipmapped = true};
      mipped_image =
          SkImages::TextureFromImage(graphite_recorder_, image_, props);
    }
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to mipmapped image";
      return;
    }
    image_ = std::move(mipped_image);
  }
}

bool ServiceImageTransferCacheEntry::has_mips() const {
  return (image_ && image_->isTextureBacked()) ? image_->hasMipmaps() : true;
}

bool ServiceImageTransferCacheEntry::fits_on_gpu() const {
  return image_ && image_->isTextureBacked();
}

}  // namespace cc
