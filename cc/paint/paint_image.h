// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/deferred_paint_record.h"
#include "cc/paint/frame_metadata.h"
#include "cc/paint/image_animation_count.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_record.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

class SkBitmap;
class SkColorSpace;
struct SkISize;

namespace blink {
class VideoFrame;
}

namespace cc {

class PaintImageGenerator;
class PaintWorkletInput;
class TextureBacking;

enum class ImageType { kPNG, kJPEG, kWEBP, kGIF, kICO, kBMP, kAVIF, kInvalid };

// An encoded image may include several auxiliary images within it. This enum
// is used to index those images. Auxiliary images can have different sizes and
// pixel formats from the default image.
enum class AuxImage : size_t {
  // The default image that decoders unaware of independent auxiliary images
  // will decode.
  kDefault = 0,
  // The UltraHDR or (equivalently) ISO 21496-1 gainmap image.
  kGainmap = 1
};

static constexpr std::array<AuxImage, 2> kAllAuxImages = {AuxImage::kDefault,
                                                          AuxImage::kGainmap};
constexpr size_t AuxImageIndex(AuxImage aux_image) {
  return static_cast<size_t>(aux_image);
}
static constexpr size_t kAuxImageCount = 2;
static constexpr size_t kAuxImageIndexDefault =
    AuxImageIndex(AuxImage::kDefault);
static constexpr size_t kAuxImageIndexGainmap =
    AuxImageIndex(AuxImage::kGainmap);
constexpr const char* AuxImageName(AuxImage aux_image) {
  switch (aux_image) {
    case AuxImage::kDefault:
      return "default";
    case AuxImage::kGainmap:
      return "gainmap";
  }
}

enum class YUVSubsampling { k410, k411, k420, k422, k440, k444, kUnknown };

enum class YUVIndex { kY, kU, kV };

// Should match the number of YUVIndex values.
static constexpr int kNumYUVPlanes = 3;

struct CC_PAINT_EXPORT ImageHeaderMetadata {
 public:
  ImageHeaderMetadata();
  ImageHeaderMetadata(const ImageHeaderMetadata& other);
  ImageHeaderMetadata& operator=(const ImageHeaderMetadata& other);
  ~ImageHeaderMetadata();

  // The image type, e.g., JPEG or WebP.
  ImageType image_type = ImageType::kInvalid;

  // The subsampling format used for the chroma planes, e.g., YUV 4:2:0.
  YUVSubsampling yuv_subsampling = YUVSubsampling::kUnknown;

  // The HDR metadata included with the image, if present.
  std::optional<gfx::HDRMetadata> hdr_metadata;

  // The visible size of the image (i.e., the area that contains meaningful
  // pixels).
  gfx::Size image_size;

  // The size of the area containing coded data, if known. For example, if the
  // |image_size| for a 4:2:0 JPEG is 12x31, its coded size should be 16x32
  // because the size of a minimum-coded unit for 4:2:0 is 16x16.
  // A zero-initialized |coded_size| indicates an invalid image.
  std::optional<gfx::Size> coded_size;

  // Whether the image embeds an ICC color profile.
  bool has_embedded_color_profile = false;

  // Whether all the data was received prior to starting decoding work.
  bool all_data_received_prior_to_decode = false;

  // For JPEGs only: whether the image is progressive (as opposed to baseline).
  std::optional<bool> jpeg_is_progressive;

  // For WebPs only: whether this is a simple-format lossy image. See
  // https://developers.google.com/speed/webp/docs/riff_container#simple_file_format_lossy.
  std::optional<bool> webp_is_non_extended_lossy;
};

// A representation of an image for the compositor.  This is the most abstract
// form of images, and represents what is known at paint time.  Note that aside
// from default construction, it can only be constructed using a
// PaintImageBuilder, or copied/moved into using operator=.  PaintImage can
// be backed by different kinds of content, such as a lazy generator, a paint
// record, a bitmap, or a texture.
//
// If backed by a generator, this image may not be decoded and information like
// the animation frame, the target colorspace, or the scale at which it will be
// used are not known yet.  A DrawImage is a PaintImage with those decisions
// known but that might not have been decoded yet.  A DecodedDrawImage is a
// DrawImage that has been decoded/scaled/uploaded with all of those parameters
// applied.
//
// The PaintImage -> DrawImage -> DecodedDrawImage -> PaintImage (via SkImage)
// path can be used to create a PaintImage that is snapshotted at a particular
// scale or animation frame.
class CC_PAINT_EXPORT PaintImage {
 public:
  using Id = int;
  using AnimationSequenceId = uint32_t;

  // A ContentId is used to identify the content for which images which can be
  // lazily generated (generator/record backed images). As opposed to Id, which
  // stays constant for the same image, the content id can be updated when the
  // backing encoded data for this image changes. For instance, in the case of
  // images which can be progressively updated as more encoded data is received.
  using ContentId = int;

  // A GeneratorClientId can be used to namespace different clients that are
  // using the output of a PaintImageGenerator.
  //
  // This is used to allow multiple compositors to simultaneously decode the
  // same image. Each compositor is assigned a unique GeneratorClientId which is
  // passed through to the decoder from PaintImage::Decode. Internally the
  // decoder ensures that requestes from different clients are executed in
  // parallel. This is particularly important for animated images, where
  // compositors displaying the same image can request decodes for different
  // frames from this image.
  using GeneratorClientId = int;
  static const GeneratorClientId kDefaultGeneratorClientId;

  // The default frame index to use if no index is provided. For multi-frame
  // images, this would imply the first frame of the animation.
  static const size_t kDefaultFrameIndex;

  static const Id kInvalidId;
  static const ContentId kInvalidContentId;

  class CC_PAINT_EXPORT FrameKey {
   public:
    FrameKey(ContentId content_id, size_t frame_index);
    bool operator==(const FrameKey& other) const;
    bool operator!=(const FrameKey& other) const;

    size_t hash() const { return hash_; }
    std::string ToString() const;
    size_t frame_index() const { return frame_index_; }
    ContentId content_id() const { return content_id_; }

   private:
    ContentId content_id_;
    size_t frame_index_;

    size_t hash_;
  };

  struct CC_PAINT_EXPORT FrameKeyHash {
    size_t operator()(const FrameKey& frame_key) const {
      return frame_key.hash();
    }
  };

  enum class AnimationType { kAnimated, kVideo, kStatic };
  enum class CompletionState { kDone, kPartiallyDone };
  enum class DecodingMode {
    // No preference has been specified. The compositor may choose to use sync
    // or async decoding. See CheckerImageTracker for the default behaviour.
    kUnspecified,

    // It's preferred to display this image synchronously with the rest of the
    // content updates, skipping any heuristics.
    kSync,

    // Async is preferred. The compositor may decode async if it meets the
    // heuristics used to avoid flickering (for instance vetoing of multipart
    // response, animated, partially loaded images) and would be performant. See
    // CheckerImageTracker for all heuristics used.
    kAsync
  };

  // Returns the more conservative mode out of the two given ones.
  static DecodingMode GetConservative(DecodingMode one, DecodingMode two);

  static Id GetNextId();
  static ContentId GetNextContentId();
  static GeneratorClientId GetNextGeneratorClientId();

  // Creates a PaintImage wrapping |bitmap|. Note that the pixels will be copied
  // unless the bitmap is marked immutable.
  static PaintImage CreateFromBitmap(SkBitmap bitmap);

  PaintImage();
  PaintImage(const PaintImage& other);
  PaintImage(PaintImage&& other);
  ~PaintImage();

  PaintImage& operator=(const PaintImage& other);
  PaintImage& operator=(PaintImage&& other);

  // For testing only. Checks if `this` and `other` are the same image, i.e.
  // share the same underlying image. `a.IsSameForTesting(b)` will be true after
  // `PaintImage b = a;`.
  bool IsSameForTesting(const PaintImage& other) const;

  // Returns the smallest size that is at least as big as the requested_size
  // such that we can decode to exactly that scale. If the requested size is
  // larger than the image, this returns the image size. Any returned value is
  // guaranteed to be stable. That is,
  // GetSupportedDecodeSize(GetSupportedDecodeSize(size)) is guaranteed to be
  // GetSupportedDecodeSize(size).
  SkISize GetSupportedDecodeSize(const SkISize& requested_size,
                                 AuxImage aux_image = AuxImage::kDefault) const;

  // Decode the image into RGBX into the pixels of the specified SkPixmap.
  // Returns true on success and false on failure. Note that for non-lazy images
  // this will do a copy or readback if the image is texture backed.
  bool Decode(SkPixmap pixmap,
              size_t frame_index,
              AuxImage aux_image,
              GeneratorClientId client_id) const;

  // Decode the image into YUV into |pixmaps|.
  //  - SkPixmaps owned by |pixmaps| are preallocated to store the
  //    planar data. They must have have color types, row bytes,
  //    and sizes as indicated by PaintImage::IsYuv().
  //  - The |frame_index| parameter will be passed along to
  //    ImageDecoder::DecodeToYUV but for multi-frame YUV support, ImageDecoder
  //    needs a separate YUV frame buffer cache.
  bool DecodeYuv(const SkYUVAPixmaps& pixmaps,
                 size_t frame_index,
                 AuxImage aux_image,
                 GeneratorClientId client_id) const;

  // Returns the SkImage associated with this PaintImage. If PaintImage is
  // texture backed, this API will always do a readback from GPU to CPU memory,
  // so avoid using it unless actual pixels are needed. For other cases, prefer
  // using PaintImage APIs directly or use GetSkImageInfo() for metadata about
  // the SkImage.
  sk_sp<SkImage> GetSwSkImage() const;

  // Reads this image's pixels into caller-owned |dst_pixels|
  bool readPixels(const SkImageInfo& dst_info,
                  void* dst_pixels,
                  size_t dst_row_bytes,
                  int src_x,
                  int src_y) const;

  // Returned mailbox must not outlive this PaintImage.
  gpu::Mailbox GetMailbox() const;

  Id stable_id() const { return id_; }
  SkImageInfo GetSkImageInfo(AuxImage aux_image = AuxImage::kDefault) const;
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }
  bool is_multipart() const { return is_multipart_; }
  bool is_high_bit_depth() const { return is_high_bit_depth_; }
  bool may_be_lcp_candidate() const { return may_be_lcp_candidate_; }
  bool no_cache() const { return no_cache_; }
  int repetition_count() const { return repetition_count_; }
  bool ShouldAnimate() const;
  AnimationSequenceId reset_animation_sequence_id() const {
    return reset_animation_sequence_id_;
  }
  DecodingMode decoding_mode() const { return decoding_mode_; }

  explicit operator bool() const {
    return deferred_paint_record_ || cached_sk_image_ || texture_backing_;
  }
  bool IsLazyGenerated() const {
    return paint_record_ || paint_image_generator_;
  }
  bool IsDeferredPaintRecord() const { return !!deferred_paint_record_; }
  bool IsPaintWorklet() const {
    return deferred_paint_record_ &&
           deferred_paint_record_->IsPaintWorkletInput();
  }
  bool NeedsLayer() const;
  bool IsTextureBacked() const;
  // Skia internally buffers commands and flushes them as necessary but there
  // are some cases where we need to force a flush.
  void FlushPendingSkiaOps();
  int width() const { return GetSkImageInfo().width(); }
  int height() const { return GetSkImageInfo().height(); }
  SkColorSpace* color_space() const {
    return IsPaintWorklet() ? nullptr : GetSkImageInfo().colorSpace();
  }
  gfx::Size GetSize(AuxImage aux_image) const;
  SkISize GetSkISize(AuxImage aux_image) const {
    return GetSkImageInfo(aux_image).dimensions();
  }
  bool GetReinterpretAsSRGB() const { return reinterpret_as_srgb_; }

  gfx::ContentColorUsage GetContentColorUsage(bool* is_hlg = nullptr) const;

  // Returns whether this image will be decoded and rendered from YUV data
  // and fills out |info|. |supported_data_types| indicates the bit depths and
  // data types allowed. If successful, the caller can use |info| to allocate
  // SkPixmaps to pass DecodeYuv() and render with the correct YUV->RGB
  // transformation.
  bool IsYuv(const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
             AuxImage aux_image,
             SkYUVAPixmapInfo* info = nullptr) const;

  // Get metadata associated with this image.
  SkColorType GetColorType() const { return GetSkImageInfo().colorType(); }
  SkAlphaType GetAlphaType() const { return GetSkImageInfo().alphaType(); }

  // Returns general information about the underlying image. Returns nullptr if
  // there is no available |paint_image_generator_|.
  const ImageHeaderMetadata* GetImageHeaderMetadata() const;

  // Returns a unique id for the pixel data for the frame at |frame_index|.
  FrameKey GetKeyForFrame(size_t frame_index) const;

  PaintImage::ContentId GetContentIdForFrame(size_t frame_index) const;

  // Returns the metadata for each frame of a multi-frame image. Should only be
  // used with animated images.
  const std::vector<FrameMetadata>& GetFrameMetadata() const;

  // Returns the total number of frames known to exist in this image.
  size_t FrameCount() const;

  // Returns an SkImage for the frame at |index|.
  sk_sp<SkImage> GetSkImageForFrame(size_t index,
                                    GeneratorClientId client_id) const;

  const scoped_refptr<PaintWorkletInput> GetPaintWorkletInput() const;

  const scoped_refptr<DeferredPaintRecord>& deferred_paint_record() const {
    return deferred_paint_record_;
  }

  bool IsOpaque() const;
  bool HasGainmap() const {
    DCHECK_EQ(gainmap_paint_image_generator_ != nullptr ||
                  gainmap_sk_image_ != nullptr,
              gainmap_info_.has_value());
    return gainmap_info_.has_value();
  }
  const SkGainmapInfo& GetGainmapInfo() const {
    DCHECK(HasGainmap());
    return gainmap_info_.value();
  }

  std::optional<gfx::HDRMetadata> GetHDRMetadata() const {
    if (const auto* image_metadata = GetImageHeaderMetadata()) {
      return image_metadata->hdr_metadata;
    }
    return std::nullopt;
  }

  std::string ToString() const;

 private:
  friend class PaintImageBuilder;
  FRIEND_TEST_ALL_PREFIXES(PaintImageTest, Subsetting);

  // Used internally for PaintImages created at raster.
  static const Id kNonLazyStableId;
  friend class ScopedRasterFlags;
  friend class PaintOpReader;

  friend class PlaybackImageProvider;
  friend class DrawImageRectOp;
  friend class DrawImageOp;
  friend class DrawSkottieOp;
  friend class ToneMapUtil;

  // TODO(crbug.com/40110279): Remove these once GetSkImage()
  // is fully removed.
  friend class ImagePaintFilter;
  friend class PaintShader;
  friend class blink::VideoFrame;

  bool DecodeFromSkImage(SkPixmap pixmap,
                         size_t frame_index,
                         GeneratorClientId client_id) const;
  void CreateSkImage();

  // Only supported in non-OOPR contexts by friend callers.
  sk_sp<SkImage> GetAcceleratedSkImage() const;

  // GetSkImage() is being deprecated, see crbug.com/1031051.
  // Prefer using GetSwSkImage() or GetSkImageInfo().
  const sk_sp<SkImage>& GetSkImage() const;

  sk_sp<SkImage> sk_image_;
  std::optional<PaintRecord> paint_record_;
  gfx::Rect paint_record_rect_;

  ContentId content_id_ = kInvalidContentId;

  sk_sp<PaintImageGenerator> paint_image_generator_;

  // If true, then this images will be reinterpreted as being sRGB during paint.
  // This is used by createImageBitmap's colorSpaceConversion:"none".
  bool reinterpret_as_srgb_ = false;

  // The target HDR headroom for gainmap and global tone map application.
  float target_hdr_headroom_ = 1.f;

  // Gainmap HDR metadata.
  sk_sp<SkImage> gainmap_sk_image_;
  sk_sp<PaintImageGenerator> gainmap_paint_image_generator_;
  std::optional<SkGainmapInfo> gainmap_info_;

  // HDR metadata used by global tone map application and (potentially but not
  // yet) gain map application.
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  sk_sp<TextureBacking> texture_backing_;

  Id id_ = 0;
  AnimationType animation_type_ = AnimationType::kStatic;
  CompletionState completion_state_ = CompletionState::kDone;
  int repetition_count_ = kAnimationNone;

  // Whether the data fetched for this image is a part of a multpart response.
  bool is_multipart_ = false;

  // Whether this image has more than 8 bits per color channel.
  bool is_high_bit_depth_ = false;

  // Whether this image may untimately be a candidate for Largest Contentful
  // Paint. The final LCP contribution of an image is unknown until we present
  // it, but this flag is intended for metrics on when we do not present the
  // image when the system claims.
  bool may_be_lcp_candidate_ = false;

  // Indicates that the image is unlikely to be re-used past the first frame it
  // appears in. Used as a hint to avoid caching it downstream, but is not a
  // mandate.
  bool no_cache_ = false;

  // An incrementing sequence number maintained by the painter to indicate if
  // this animation should be reset in the compositor. Incrementing this number
  // will reset this animation in the compositor for the first frame which has a
  // recording with a PaintImage storing the updated sequence id.
  AnimationSequenceId reset_animation_sequence_id_ = 0u;

  DecodingMode decoding_mode_ = DecodingMode::kSync;

  // The |cached_sk_image_| can be derived/created from other inputs present in
  // the PaintImage but we always construct it at creation time for 2 reasons:
  // 1) This ensures that the underlying SkImage is shared across PaintImage
  //    copies, which is necessary to allow reuse of decodes from this image in
  //    skia's cache.
  // 2) Ensures that accesses to it are thread-safe.
  sk_sp<SkImage> cached_sk_image_;

  // The input parameters that are needed to execute the JS paint callback.
  scoped_refptr<DeferredPaintRecord> deferred_paint_record_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
