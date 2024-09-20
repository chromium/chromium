// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/tone_map_util.h"
#include "cc/paint/transfer_cache_entry.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

class GrDirectContext;
class SkColorSpace;
class SkImage;
class SkPixmap;

namespace cc {

static constexpr uint32_t kInvalidImageTransferCacheEntryId =
    static_cast<uint32_t>(-1);

enum class YUVDecodeFormat {
  kYUV3,   // e.g., YUV 4:2:0, 4:2:2, or 4:4:4 as 3 planes.
  kYUVA4,  // e.g., YUV 4:2:0 as 3 planes plus an alpha plane.
  kYVU3,   // e.g., YVU 4:2:0, 4:2:2, or 4:4:4 as 3 planes.
  kYUV2,   // e.g., YUV 4:2:0 as NV12 (2 planes).
  kUnknown,
  kMaxValue = kUnknown,
};

CC_PAINT_EXPORT size_t NumberOfPlanesForYUVDecodeFormat(YUVDecodeFormat format);

// Client/ServiceImageTransferCacheEntry implement a transfer cache entry
// for transferring image data. On the client side, this is a CPU SkPixmap,
// on the service side the image is uploaded and is a GPU SkImage.
class CC_PAINT_EXPORT ClientImageTransferCacheEntry final
    : public ClientTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  // Abstraction around RGBA vs YUVA images.
  struct CC_PAINT_EXPORT Image {
    Image();
    Image(const Image&);
    Image& operator=(const Image&);

    // Constructor for RGBA images.
    explicit Image(const SkPixmap* pixmap);

    // Constructor for YUVA images.
    Image(const SkPixmap yuva_pixmaps[],
          const SkYUVAInfo& yuva_info,
          const SkColorSpace* color_space);

    // The pixmaps for each plane.
    std::array<const SkPixmap*, SkYUVAInfo::kMaxPlanes> pixmaps = {
        nullptr, nullptr, nullptr, nullptr};

    // The YUVA parameters. These should be unchanged for RGBA images.
    SkYUVAInfo::PlaneConfig yuv_plane_config =
        SkYUVAInfo::PlaneConfig::kUnknown;
    SkYUVAInfo::Subsampling yuv_subsampling = SkYUVAInfo::Subsampling::kUnknown;
    SkYUVColorSpace yuv_color_space = kIdentity_SkYUVColorSpace;

    // The color space that will be assigned to the image when it is
    // deserialized.
    raw_ptr<const SkColorSpace> color_space = nullptr;
  };

  ClientImageTransferCacheEntry(
      const Image& image,
      bool needs_mips,
      const std::optional<gfx::HDRMetadata>& hdr_metadata = std::nullopt,
      sk_sp<SkColorSpace> target_color_space = nullptr);
  ClientImageTransferCacheEntry(const Image& image,
                                const Image& gainmap_image,
                                const SkGainmapInfo& gainmap_info,
                                bool needs_mips);
  ~ClientImageTransferCacheEntry() final;

  uint32_t Id() const final;

  // ClientTransferCacheEntry implementation:
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

  static uint32_t GetNextId() { return s_next_id_.GetNext(); }
  bool IsYuv() const {
    return image_.yuv_plane_config != SkYUVAInfo::PlaneConfig::kUnknown;
  }
  bool IsValid() const { return size_ > 0; }

 private:
  void ComputeSize();

  const bool needs_mips_ = false;
  sk_sp<SkColorSpace> target_color_space_;
  const uint32_t id_;
  uint32_t size_ = 0;
  static base::AtomicSequenceNumber s_next_id_;

  Image image_;

  // The gainmap image and parameters. Either both or neither of these must
  // be specified.
  std::optional<Image> gainmap_image_;
  std::optional<SkGainmapInfo> gainmap_info_;

  // The HDR metadata for non-gainmap HDR metadata.
  std::optional<gfx::HDRMetadata> hdr_metadata_;
};

class CC_PAINT_EXPORT ServiceImageTransferCacheEntry final
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  ServiceImageTransferCacheEntry();
  ~ServiceImageTransferCacheEntry() final;

  ServiceImageTransferCacheEntry(ServiceImageTransferCacheEntry&& other);
  ServiceImageTransferCacheEntry& operator=(
      ServiceImageTransferCacheEntry&& other);

  // Populates this entry using the result of a hardware decode. The assumption
  // is that |plane_images| are backed by textures that are in turn backed by a
  // buffer (dmabuf in Chrome OS) containing the planes of the decoded image.
  // |plane_images_format| indicates the planar layout of |plane_images|.
  // |buffer_byte_size| is the size of the buffer. We assume the following:
  //
  // - The backing textures don't have mipmaps. We will generate the mipmaps if
  //   |needs_mips| is true.
  // - The conversion from YUV to RGB will be performed according to
  //   |yuv_color_space|.
  // - The colorspace of the resulting RGB image is sRGB.
  //
  // Returns true if the entry can be built, false otherwise.
  bool BuildFromHardwareDecodedImage(GrDirectContext* gr_context,
                                     std::vector<sk_sp<SkImage>> plane_images,
                                     SkYUVAInfo::PlaneConfig plane_config,
                                     SkYUVAInfo::Subsampling subsampling,
                                     SkYUVColorSpace yuv_color_space,
                                     size_t buffer_byte_size,
                                     bool needs_mips);

  // ServiceTransferCacheEntry implementation:
  size_t CachedSize() const final;
  bool Deserialize(GrDirectContext* gr_context,
                   skgpu::graphite::Recorder* graphite_recorder,
                   base::span<const uint8_t> data) final;

  const sk_sp<SkImage>& image() const { return image_; }

  bool HasGainmap() const { return gainmap_image_ != nullptr; }
  const sk_sp<SkImage>& gainmap_image() const { return gainmap_image_; }
  const SkGainmapInfo& gainmap_info() const { return gainmap_info_; }

  // Ensures the cached image has mips.
  void EnsureMips();

  // Used in tests and for registering each texture for memory dumps.
  bool has_mips() const;
  const std::vector<sk_sp<SkImage>>& plane_images() const {
    return plane_images_;
  }
  const sk_sp<SkImage>& GetPlaneImage(size_t index) const;
  const std::vector<size_t>& GetPlaneCachedSizes() const {
    return plane_sizes_;
  }
  bool is_yuv() const { return !plane_images_.empty(); }
  size_t num_planes() const { return plane_images_.size(); }
  bool fits_on_gpu() const;

  const std::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }

 private:
  raw_ptr<GrDirectContext, DanglingUntriaged> gr_context_ = nullptr;
  raw_ptr<skgpu::graphite::Recorder> graphite_recorder_ = nullptr;
  sk_sp<SkImage> image_;

  // HDR local tone mapping may be done with a gainmap.
  bool has_gainmap_ = false;
  sk_sp<SkImage> gainmap_image_;
  SkGainmapInfo gainmap_info_;

  // HDR metadata used by global tone map application and (potentially but not
  // yet) gain map application.
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  // The value of `size_` is computed during deserialization and never updated
  // (even if the size of the image changes due to mipmaps being requested).
  size_t size_ = 0;

  // The individual planes that are used by `image_` when `image_` is a YUVA
  // image. The planes are kept around for use in EnsureMips(), memory dumps,
  // and unit tests.
  std::optional<SkYUVAInfo> yuva_info_;
  std::vector<sk_sp<SkImage>> plane_images_;
  std::vector<size_t> plane_sizes_;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
