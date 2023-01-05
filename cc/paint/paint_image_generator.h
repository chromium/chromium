// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_GENERATOR_H_
#define CC_PAINT_PAINT_IMAGE_GENERATOR_H_

#include <vector>

#include "cc/paint/frame_metadata.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace cc {

// PaintImage Generator is a wrapper to provide a lazily decoded PaintImage to
// the compositor.
// Note that the implementation of this class must ensure thread safety, it can
// be called from any thread.
class CC_PAINT_EXPORT PaintImageGenerator : public SkRefCnt {
 public:
  PaintImageGenerator(const PaintImageGenerator&) = delete;
  ~PaintImageGenerator() override;

  PaintImageGenerator& operator=(const PaintImageGenerator&) = delete;

  // Returns a reference to the encoded content of this image.
  virtual sk_sp<SkData> GetEncodedData() const = 0;

  // Decode into the given SkPixmap. This will modify the pixels pointed to by
  // `dst_pixmap`, but will not modify any of its properties (e.g, its
  // SkImageInfo).
  //
  // TODO(khushalsagar): |lazy_pixel_ref| is only present for
  // DecodingImageGenerator tracing needs. Remove it.
  virtual bool GetPixels(SkPixmap dst_pixmap,
                         size_t frame_index,
                         PaintImage::GeneratorClientId client_id,
                         uint32_t lazy_pixel_ref) = 0;

  // Returns true if the generator supports YUV decoding, providing the details
  // about planar configuration and conversion to RGB in |info|.
  // |supported_data_types| indicates the allowed bit depth and types allowed
  // for Y, U, V, and A values.
  virtual bool QueryYUVA(
      const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
      SkYUVAPixmapInfo* info) const = 0;

  // Decodes to YUV, storing planar data in the SkPixmaps in the provided
  // |pixmaps|. The method should only be used if QueryYUVA returns true.
  // SkPixmaps owned by |pixmaps| have been configured as indicated by
  // QueryYUVA.
  //
  // TODO(khushalsagar): |lazy_pixel_ref| is only present for
  // DecodingImageGenerator tracing needs. Remove it.
  virtual bool GetYUVAPlanes(const SkYUVAPixmaps& pixmaps,
                             size_t frame_index,
                             uint32_t lazy_pixel_ref,
                             PaintImage::GeneratorClientId client_id) = 0;

  // Returns the smallest size that is at least as big as the requested size,
  // such that we can decode to exactly that scale.
  virtual SkISize GetSupportedDecodeSize(const SkISize& requested_size) const;

  // Returns the content id to key the decoded output produced by this
  // generator for a frame at |frame_index|. The generator promises that
  // the output for repeated calls to decode a frame will be consistent across
  // all generators for a PaintImage, if this function returns the same id.
  virtual PaintImage::ContentId GetContentIdForFrame(size_t frame_index) const;

  const SkImageInfo& GetSkImageInfo() const { return info_; }
  const std::vector<FrameMetadata>& GetFrameMetadata() const { return frames_; }

  // Returns the information required to decide whether or not hardware
  // acceleration can be used to decode this image.
  virtual const ImageHeaderMetadata* GetMetadataForDecodeAcceleration() const;

 protected:
  // |info| is the info for this paint image generator.
  PaintImageGenerator(const SkImageInfo& info,
                      std::vector<FrameMetadata> frames = {FrameMetadata()});

 private:
  const SkImageInfo info_;
  const PaintImage::ContentId generator_content_id_;
  const std::vector<FrameMetadata> frames_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_GENERATOR_H_
