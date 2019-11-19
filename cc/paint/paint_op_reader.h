// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_READER_H_
#define CC_PAINT_PAINT_OP_READER_H_

#include <vector>

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"

namespace cc {

class PaintShader;

// PaintOpReader takes garbage |memory| and clobbers it with successive
// read functions.
class CC_PAINT_EXPORT PaintOpReader {
 public:
  // The DeserializeOptions passed to the reader must set all fields if it can
  // be used to for deserializing images, paint records or text blobs.
  PaintOpReader(const volatile void* memory,
                size_t size,
                const PaintOp::DeserializeOptions& options,
                bool enable_security_constraints = false)
      : memory_(static_cast<const volatile char*>(memory) +
                PaintOpWriter::HeaderBytes()),
        remaining_bytes_(size - PaintOpWriter::HeaderBytes()),
        options_(options),
        enable_security_constraints_(enable_security_constraints) {
    if (size < PaintOpWriter::HeaderBytes())
      valid_ = false;
  }

  static void FixupMatrixPostSerialization(SkMatrix* matrix);
  static bool ReadAndValidateOpHeader(const volatile void* input,
                                      size_t input_size,
                                      uint8_t* type,
                                      uint32_t* skip);

  bool valid() const { return valid_; }
  size_t remaining_bytes() const { return remaining_bytes_; }

  void ReadData(size_t bytes, void* data);
  void ReadSize(size_t* size);

  void Read(SkScalar* data);
  void Read(uint8_t* data);
  void Read(uint32_t* data);
  void Read(uint64_t* data);
  void Read(int32_t* data);
  void Read(SkRect* rect);
  void Read(SkIRect* rect);
  void Read(SkRRect* rect);

  void Read(SkPath* path);
  void Read(PaintFlags* flags);
  void Read(PaintImage* image);
  void Read(sk_sp<SkData>* data);
  void Read(sk_sp<SkTextBlob>* blob);
  void Read(sk_sp<PaintFilter>* filter);
  void Read(sk_sp<PaintShader>* shader);
  void Read(SkMatrix* matrix);
  void Read(SkColorType* color_type);
  void Read(SkImageInfo* info);
  void Read(sk_sp<SkColorSpace>* color_space);
  void Read(SkYUVColorSpace* yuv_color_space);

  void Read(SkClipOp* op) {
    uint8_t value = 0u;
    Read(&value);
    *op = static_cast<SkClipOp>(value);
  }
  void Read(PaintCanvas::AnnotationType* type) {
    uint8_t value = 0u;
    Read(&value);
    *type = static_cast<PaintCanvas::AnnotationType>(value);
  }
  void Read(PaintCanvas::SrcRectConstraint* constraint) {
    uint8_t value = 0u;
    Read(&value);
    *constraint = static_cast<PaintCanvas::SrcRectConstraint>(value);
  }
  void Read(SkFilterQuality* quality) {
    uint8_t value = 0u;
    Read(&value);
    if (value > static_cast<uint8_t>(kLast_SkFilterQuality)) {
      SetInvalid();
      return;
    }
    *quality = static_cast<SkFilterQuality>(value);
  }
  void Read(bool* data) {
    uint8_t value = 0u;
    Read(&value);
    *data = !!value;
  }

  // Returns a pointer to the next block of memory of size |bytes|, and treats
  // this memory as read (advancing the reader). Returns nullptr if |bytes|
  // would exceed the available budfer.
  const volatile void* ExtractReadableMemory(size_t bytes);

  // Aligns the memory to the given alignment.
  void AlignMemory(size_t alignment);

 private:
  template <typename T>
  void ReadSimple(T* val);

  template <typename T>
  void ReadFlattenable(sk_sp<T>* val);

  void SetInvalid();

  // The main entry point is Read(sk_sp<PaintFilter>* filter) which calls one of
  // the following functions depending on read type.
  void ReadColorFilterPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadBlurPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadDropShadowPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadMagnifierPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadComposePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadAlphaThresholdPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadXfermodePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadArithmeticPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadMatrixConvolutionPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadDisplacementMapEffectPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadImagePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadRecordPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadMergePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadMorphologyPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadOffsetPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadTilePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadTurbulencePaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadPaintFlagsPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadMatrixPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingDistantPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingPointPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingSpotPaintFilter(
      sk_sp<PaintFilter>* filter,
      const base::Optional<PaintFilter::CropRect>& crop_rect);

  // Returns the size of the read record, 0 if error.
  size_t Read(sk_sp<PaintRecord>* record);

  void Read(SkRegion* region);
  uint8_t* CopyScratchSpace(size_t bytes);

  const volatile char* memory_ = nullptr;
  size_t remaining_bytes_ = 0u;
  bool valid_ = true;
  const PaintOp::DeserializeOptions& options_;

  // Indicates that the data was serialized with the following constraints:
  // 1) PaintRecords and SkDrawLoopers are ignored.
  // 2) Images are decoded and only the bitmap is serialized.
  // If set to true, the above constraints are validated during deserialization
  // and the data types specified above are ignored.
  const bool enable_security_constraints_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_READER_H_
