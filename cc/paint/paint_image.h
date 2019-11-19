// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "cc/paint/frame_metadata.h"
#include "cc/paint/image_animation_count.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/core/SkYUVASizeInfo.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PaintImageGenerator;
class PaintOpBuffer;
class PaintWorkletInput;
using PaintRecord = PaintOpBuffer;

enum class ImageType { kPNG, kJPEG, kWEBP, kGIF, kICO, kBMP, kInvalid };

enum class YUVSubsampling { k410, k411, k420, k422, k440, k444, kUnknown };

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

  // The visible size of the image (i.e., the area that contains meaningful
  // pixels).
  gfx::Size image_size;

  // The size of the area containing coded data, if known. For example, if the
  // |image_size| for a 4:2:0 JPEG is 12x31, its coded size should be 16x32
  // because the size of a minimum-coded unit for 4:2:0 is 16x16.
  // A zero-initialized |coded_size| indicates an invalid image.
  base::Optional<gfx::Size> coded_size;

  // Whether the image embeds an ICC color profile.
  bool has_embedded_color_profile = false;

  // Whether all the data was received prior to starting decoding work.
  bool all_data_received_prior_to_decode = false;

  // For JPEGs only: whether the image is progressive (as opposed to baseline).
  base::Optional<bool> jpeg_is_progressive;

  // For WebPs only: whether this is a simple-format lossy image. See
  // https://developers.google.com/speed/webp/docs/riff_container#simple_file_format_lossy.
  base::Optional<bool> webp_is_non_extended_lossy;
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
    FrameKey(ContentId content_id, size_t frame_index, gfx::Rect subset_rect);
    bool operator==(const FrameKey& other) const;
    bool operator!=(const FrameKey& other) const;

    size_t hash() const { return hash_; }
    std::string ToString() const;
    size_t frame_index() const { return frame_index_; }
    ContentId content_id() const { return content_id_; }

   private:
    ContentId content_id_;
    size_t frame_index_;
    // TODO(khushalsagar): Remove this when callers take care of subsetting.
    gfx::Rect subset_rect_;

    size_t hash_;
  };

  struct CC_PAINT_EXPORT FrameKeyHash {
    size_t operator()(const FrameKey& frame_key) const {
      return frame_key.hash();
    }
  };

  enum class AnimationType { ANIMATED, VIDEO, STATIC };
  enum class CompletionState { DONE, PARTIALLY_DONE };
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

  bool operator==(const PaintImage& other) const;
  bool operator!=(const PaintImage& other) const { return !(*this == other); }

  // Returns the smallest size that is at least as big as the requested_size
  // such that we can decode to exactly that scale. If the requested size is
  // larger than the image, this returns the image size. Any returned value is
  // guaranteed to be stable. That is,
  // GetSupportedDecodeSize(GetSupportedDecodeSize(size)) is guaranteed to be
  // GetSupportedDecodeSize(size).
  SkISize GetSupportedDecodeSize(const SkISize& requested_size) const;

  // Decode the image into RGBX into the given memory for the given SkImageInfo.
  // - Size in |info| must be supported.
  // - The amount of memory allocated must be at least
  //   |info|.minRowBytes() * |info|.height()
  // Returns true on success and false on failure. Updates |info| to match the
  // requested color space, if provided.
  // Note that for non-lazy images this will do a copy or readback if the image
  // is texture backed.
  bool Decode(void* memory,
              SkImageInfo* info,
              sk_sp<SkColorSpace> color_space,
              size_t frame_index,
              GeneratorClientId client_id) const;

  // Decode the image into YUV into |planes| for the given SkYUVASizeInfo.
  //  - Elements of the |planes| array are pointers to some underlying memory
  //    for each plane. It is assumed to have been split up by a call to
  //    SkYUVASizeInfo::computePlanes with the given |yuva_size_info|.
  //  - The amount of memory allocated must be at least
  //    |yuva_size_info|.computeTotalBytes(), though there are places in the
  //    code that assume YUV420 without alpha because it is currently the only
  //    subsampling supported for direct YUV rendering.
  //  - The dimensions of YUV planes are tracked in |yuva_size_info|.
  //    This struct is initialized by QueryYUVA8 in calls to
  //    PaintImage::IsYuv(), including within this method.
  //  - The |frame_index| parameter will be passed along to
  //    ImageDecoder::DecodeToYUV but for multi-frame YUV support, ImageDecoder
  //    needs a separate YUV frame buffer cache.
  //  - The mapping of source planes to channels is tracked by |plane_indices|.
  //    This struct is initialized by QueryYUVA8 in calls to
  //    PaintImage::IsYuv(), including within this method.
  bool DecodeYuv(void* planes[SkYUVASizeInfo::kMaxCount],
                 size_t frame_index,
                 GeneratorClientId client_id,
                 const SkYUVASizeInfo& yuva_size_info,
                 SkYUVAIndex* plane_indices) const;

  Id stable_id() const { return id_; }
  const sk_sp<SkImage>& GetSkImage() const;
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }
  bool is_multipart() const { return is_multipart_; }
  bool is_high_bit_depth() const { return is_high_bit_depth_; }
  int repetition_count() const { return repetition_count_; }
  bool ShouldAnimate() const;
  AnimationSequenceId reset_animation_sequence_id() const {
    return reset_animation_sequence_id_;
  }
  DecodingMode decoding_mode() const { return decoding_mode_; }
  PaintImage::ContentId content_id() const { return content_id_; }

  // TODO(vmpstr): Don't get the SkImage here if you don't need to.
  uint32_t unique_id() const {
    return paint_worklet_input_ ? 0 : GetSkImage()->uniqueID();
  }
  explicit operator bool() const {
    return paint_worklet_input_ || !!GetSkImage();
  }
  bool IsLazyGenerated() const {
    return paint_worklet_input_ ? false : GetSkImage()->isLazyGenerated();
  }
  bool IsPaintWorklet() const { return !!paint_worklet_input_; }
  bool IsTextureBacked() const {
    return paint_worklet_input_ ? false : GetSkImage()->isTextureBacked();
  }
  int width() const;
  int height() const;
  SkColorSpace* color_space() const {
    return paint_worklet_input_ ? nullptr : GetSkImage()->colorSpace();
  }
  const gfx::Rect subset_rect() const { return subset_rect_; }

  // Returns whether this image will be decoded and rendered from YUV data
  // and fills out plane size info, plane index info, and the matrix for
  // conversion from YUV to RGB in, respectively, |yuva_size_info|,
  // |plane_indices|, and |yuv_color_space| if any are provided.
  bool IsYuv(SkYUVASizeInfo* yuva_size_info = nullptr,
             SkYUVAIndex* plane_indices = nullptr,
             SkYUVColorSpace* yuv_color_space = nullptr) const;

  // Returns the color type of this image.
  SkColorType GetColorType() const;

  // Returns general information about the underlying image. Returns nullptr if
  // there is no available |paint_image_generator_|.
  const ImageHeaderMetadata* GetImageHeaderMetadata() const;

  // Returns a unique id for the pixel data for the frame at |frame_index|.
  FrameKey GetKeyForFrame(size_t frame_index) const;

  // Returns the metadata for each frame of a multi-frame image. Should only be
  // used with animated images.
  const std::vector<FrameMetadata>& GetFrameMetadata() const;

  // Returns the total number of frames known to exist in this image.
  size_t FrameCount() const;

  // Returns an SkImage for the frame at |index|.
  sk_sp<SkImage> GetSkImageForFrame(size_t index,
                                    GeneratorClientId client_id) const;

  const scoped_refptr<PaintWorkletInput>& paint_worklet_input() const {
    return paint_worklet_input_;
  }

  std::string ToString() const;

 private:
  friend class PaintImageBuilder;
  FRIEND_TEST_ALL_PREFIXES(PaintImageTest, Subsetting);

  // Used internally for PaintImages created at raster.
  static const Id kNonLazyStableId;
  friend class ScopedRasterFlags;
  friend class PaintOpReader;

  bool CanDecodeFromGenerator() const;

  bool DecodeFromGenerator(void* memory,
                           SkImageInfo* info,
                           sk_sp<SkColorSpace> color_space,
                           size_t frame_index,
                           GeneratorClientId client_id) const;
  bool DecodeFromSkImage(void* memory,
                         SkImageInfo* info,
                         sk_sp<SkColorSpace> color_space,
                         size_t frame_index,
                         GeneratorClientId client_id) const;
  void CreateSkImage();
  PaintImage MakeSubset(const gfx::Rect& subset) const;

  sk_sp<SkImage> sk_image_;
  sk_sp<PaintRecord> paint_record_;
  gfx::Rect paint_record_rect_;

  ContentId content_id_ = kInvalidContentId;

  sk_sp<PaintImageGenerator> paint_image_generator_;

  Id id_ = 0;
  AnimationType animation_type_ = AnimationType::STATIC;
  CompletionState completion_state_ = CompletionState::DONE;
  int repetition_count_ = kAnimationNone;

  // If non-empty, holds the subset of this image relative to the original image
  // at the origin.
  gfx::Rect subset_rect_;

  // Whether the data fetched for this image is a part of a multpart response.
  bool is_multipart_ = false;

  // Whether this image has more than 8 bits per color channel.
  bool is_high_bit_depth_ = false;

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
  scoped_refptr<PaintWorkletInput> paint_worklet_input_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
