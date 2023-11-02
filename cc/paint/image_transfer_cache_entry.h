// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/target_color_params.h"
#include "cc/paint/transfer_cache_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

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
  ClientImageTransferCacheEntry(
      const SkPixmap* pixmap,
      bool needs_mips,
      absl::optional<TargetColorParams> target_color_params);
  ClientImageTransferCacheEntry(
      const SkPixmap yuva_pixmaps[],
      SkYUVAInfo::PlaneConfig plane_config,
      SkYUVAInfo::Subsampling subsampling,
      const SkColorSpace* decoded_color_space,
      SkYUVColorSpace yuv_color_space,
      bool needs_mips,
      absl::optional<TargetColorParams> target_color_params);
  ~ClientImageTransferCacheEntry() final;

  uint32_t Id() const final;

  // ClientTransferCacheEntry implementation:
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

  static uint32_t GetNextId() { return s_next_id_.GetNext(); }
  bool IsYuv() const { return !!yuv_pixmaps_; }
  bool IsValid() const { return size_ > 0; }

 private:
  const bool needs_mips_ = false;
  absl::optional<TargetColorParams> target_color_params_;
  SkYUVAInfo::PlaneConfig plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  uint32_t id_;
  uint32_t size_ = 0;
  static base::AtomicSequenceNumber s_next_id_;

  // RGBX-only members.
  const raw_ptr<const SkPixmap> pixmap_;

  // YUVA-only members.
  absl::optional<std::array<const SkPixmap*, SkYUVAInfo::kMaxPlanes>>
      yuv_pixmaps_;
  const raw_ptr<const SkColorSpace> decoded_color_space_;
  SkYUVAInfo::Subsampling subsampling_ = SkYUVAInfo::Subsampling::kUnknown;
  SkYUVColorSpace yuv_color_space_;

  // DCHECKs that the appropriate data members are set or not set and have
  // positive size dimensions.
  void ValidateYUVDataBeforeSerializing() const;
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
  bool BuildFromHardwareDecodedImage(GrDirectContext* context,
                                     std::vector<sk_sp<SkImage>> plane_images,
                                     SkYUVAInfo::PlaneConfig plane_config,
                                     SkYUVAInfo::Subsampling subsampling,
                                     SkYUVColorSpace yuv_color_space,
                                     size_t buffer_byte_size,
                                     bool needs_mips);

  // ServiceTransferCacheEntry implementation:
  size_t CachedSize() const final;
  bool Deserialize(GrDirectContext* context,
                   base::span<const uint8_t> data) final;

  bool fits_on_gpu() const { return fits_on_gpu_; }
  const std::vector<sk_sp<SkImage>>& plane_images() const {
    return plane_images_;
  }
  const sk_sp<SkImage>& image() const { return image_; }

  // Ensures the cached image has mips.
  void EnsureMips();
  bool has_mips() const { return has_mips_; }

  // Used in tests and for registering each texture for memory dumps.
  const sk_sp<SkImage>& GetPlaneImage(size_t index) const;
  const std::vector<size_t>& GetPlaneCachedSizes() const {
    return plane_sizes_;
  }
  bool is_yuv() const { return !plane_images_.empty(); }
  size_t num_planes() const {
    return is_yuv() ? SkYUVAInfo::NumPlanes(plane_config_) : 1u;
  }

 private:
  raw_ptr<GrDirectContext> context_ = nullptr;
  // The individual planes that are used by `image_` when `image_` is a YUVA
  // image. The planes are kept around for use in EnsureMips(), memory dumps,
  // and unit tests.
  std::vector<sk_sp<SkImage>> plane_images_;
  SkYUVAInfo::PlaneConfig plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  std::vector<size_t> plane_sizes_;
  sk_sp<SkImage> image_;
  absl::optional<SkYUVAInfo::Subsampling> subsampling_;
  absl::optional<SkYUVColorSpace> yuv_color_space_;
  bool has_mips_ = false;
  // The value of `size_` is computed during deserialization and never updated
  // (even if the size of the image changes due to mipmaps being requested).
  size_t size_ = 0;
  bool fits_on_gpu_ = false;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
