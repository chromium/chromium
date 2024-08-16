// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_PAINT_PAINT_OP_H_
#define CC_PAINT_PAINT_OP_H_

#include <stdint.h>

#include <cmath>
#include <iosfwd>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "cc/base/math_util.h"
#include "cc/paint/element_id.h"
#include "cc/paint/node_id.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/refcounted_buffer.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"

class SkImage;
class SkTextBlob;
namespace sktext::gpu {
class Slug;
}

namespace cc {

class DisplayItemList;
class PaintOpWriter;
class PaintOpReader;

class CC_PAINT_EXPORT ThreadsafePath : public SkPath {
 public:
  explicit ThreadsafePath(const SkPath& path) : SkPath(path) {
    updateBoundsCache();
    getGenerationID();
  }
  ThreadsafePath() {
    updateBoundsCache();
    getGenerationID();
  }
};

// See PaintOp::Serialize/Deserialize for comments.  Serialize() of derived
// types don't write the type/serialized_size header because they don't know how
// much data they will need to write. PaintOp::Serialize itself must update the
// header after calling Serialize() of the derived type.
#define HAS_SERIALIZATION_FUNCTIONS()                                         \
  void Serialize(PaintOpWriter& writer, const PaintFlags* flags_to_serialize, \
                 const SkM44& current_ctm, const SkM44& original_ctm) const;  \
  static PaintOp* Deserialize(PaintOpReader& reader, void* output)

enum class PaintOpType : uint8_t {
  kAnnotate,
  kClipPath,
  kClipRect,
  kClipRRect,
  kConcat,
  kCustomData,
  kDrawArc,
  kDrawArcLite,
  kDrawColor,
  kDrawDRRect,
  kDrawImage,
  kDrawImageRect,
  kDrawIRect,
  kDrawLine,
  kDrawLineLite,
  kDrawOval,
  kDrawPath,
  kDrawRecord,
  kDrawRect,
  kDrawRRect,
  kDrawScrollingContents,
  kDrawSkottie,
  kDrawSlug,
  kDrawTextBlob,
  kDrawVertices,
  kNoop,
  kRestore,
  kRotate,
  kSave,
  kSaveLayer,
  kSaveLayerAlpha,
  kSaveLayerFilters,
  kScale,
  kSetMatrix,
  kSetNodeId,
  kTranslate,
  kLastPaintOpType = kTranslate,
};

CC_PAINT_EXPORT std::string PaintOpTypeToString(PaintOpType type);
CC_PAINT_EXPORT std::ostream& operator<<(std::ostream&, PaintOpType);

class CC_PAINT_EXPORT PaintOp {
 public:
  uint8_t type;

  using SerializeOptions = PaintOpBuffer::SerializeOptions;
  using DeserializeOptions = PaintOpBuffer::DeserializeOptions;

  explicit PaintOp(PaintOpType type) : type(static_cast<uint8_t>(type)) {}

  PaintOpType GetType() const { return static_cast<PaintOpType>(type); }

  // Subclasses should provide a static Raster() method which is called from
  // here. The Raster method should take a const PaintOp* parameter. It is
  // static with a pointer to the base type so that we can use it as a function
  // pointer.
  void Raster(SkCanvas* canvas, const PlaybackParams& params) const;
  bool IsDrawOp() const { return g_is_draw_op[type]; }
  bool IsPaintOpWithFlags() const { return g_has_paint_flags[type]; }
  uint16_t AlignedSize() const { return g_type_to_aligned_size[type]; }

  bool EqualsForTesting(const PaintOp& other) const;

  // Indicates how PaintImages are serialized.
  enum class SerializedImageType : uint8_t {
    kNoImage,
    kImageData,
    kTransferCacheEntry,
    kMailbox,
    kLastType = kMailbox
  };

  // Subclasses should provide a Serialize() method called from here.
  // If the op can be serialized to |memory| in no more than |size| bytes,
  // then return the number of bytes written.  If it won't fit, return 0.
  // If |flags_to_serialize| is non-null, it overrides any flags within the op.
  // |current_ctm| is the transform that will affect the op when rasterized.
  // |original_ctm| is the transform that SetMatrixOps must be made relative to.
  size_t Serialize(void* memory,
                   size_t size,
                   const SerializeOptions& options,
                   const PaintFlags* flags_to_serialize,
                   const SkM44& current_ctm,
                   const SkM44& original_ctm) const;

  // Deserializes a PaintOp of this type from a given buffer |input| of
  // at most |input_size| bytes.  Returns null on any errors.
  // The PaintOp is deserialized into the |output| buffer and returned
  // if valid.  nullptr is returned if the deserialization fails.
  // |output_size| must be at least ComputeOpAlignedSize<LargestPaintOp>(),
  // to fit all ops.  The caller is responsible for destroying these ops.
  // After reading, it returns the number of bytes read in |read_bytes|.
  static PaintOp* Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              size_t* read_bytes,
                              const DeserializeOptions& options);
  // Similar to the above, but deserializes into |buffer|.
  static PaintOp* DeserializeIntoPaintOpBuffer(
      const volatile void* input,
      size_t input_size,
      PaintOpBuffer* buffer,
      size_t* read_bytes,
      const DeserializeOptions& options);

  // For draw ops, returns true if a conservative bounding rect can be provided
  // for the op.
  static bool GetBounds(const PaintOp& op, SkRect* rect);

  // Returns the minimum conservative bounding rect that |op| draws to on a
  // canvas. |clip_rect| and |ctm| are the current clip rect and transform on
  // this canvas.
  static gfx::Rect ComputePaintRect(const PaintOp& op,
                                    const SkRect& clip_rect,
                                    const SkMatrix& ctm);

  // Returns true if the op lies outside the current clip and should be skipped.
  // Should only be used with draw ops.
  static bool QuickRejectDraw(const PaintOp& op, const SkCanvas* canvas);

  // Returns true if executing this op will require decoding of any lazy
  // generated images.
  static bool OpHasDiscardableImages(const PaintOp& op);

  // Gets the maximum content color usage of all images in this PaintOp.
  gfx::ContentColorUsage GetContentColorUsage() const;

  // Returns true if the given op type has PaintFlags.
  static bool TypeHasFlags(PaintOpType type);

  int CountSlowPaths() const { return 0; }
  int CountSlowPathsFromFlags() const { return 0; }

  bool HasNonAAPaint() const { return false; }
  bool HasDrawTextOps() const { return false; }
  bool HasSaveLayerOps() const { return false; }
  bool HasSaveLayerAlphaOps() const { return false; }
  // Returns true if effects are present that would break LCD text or be broken
  // by the flags for kSaveLayerAlpha to preserving LCD text.
  bool HasEffectsPreventingLCDTextForSaveLayerAlpha() const { return false; }

  // If `content_color_usage` is not null, the functions should update
  // `*content_color_usage` to be
  // max(*content_color_usage, max_content_color_usage_of_this_op).
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const {
    return false;
  }
  bool HasDiscardableImagesFromFlags(
      gfx::ContentColorUsage* content_color_usage) const {
    return false;
  }

  // Returns the number of bytes used by this op in referenced sub records
  // and display lists.  This doesn't count other objects like paths or blobs.
  size_t AdditionalBytesUsed() const { return 0; }

  // Returns the number of ops in referenced sub records and display lists.
  size_t AdditionalOpCount() const { return 0; }

  // Run the destructor for the derived op type.  Ops are usually contained in
  // memory buffers and so don't have their destructors run automatically.
  void DestroyThis();

  // kDrawColor is more restrictive on the blend modes that can be used.
  static bool IsValidDrawColorSkBlendMode(SkBlendMode mode) {
    return static_cast<uint32_t>(mode) <=
           static_cast<uint32_t>(SkBlendMode::kLastCoeffMode);
  }

  // PaintFlags can have more complex blend modes than kDrawColor.
  static bool IsValidPaintFlagsSkBlendMode(SkBlendMode mode) {
    return static_cast<uint32_t>(mode) <=
           static_cast<uint32_t>(SkBlendMode::kLastMode);
  }

  static bool IsValidSkClipOp(SkClipOp op) {
    return static_cast<uint32_t>(op) <=
           static_cast<uint32_t>(SkClipOp::kMax_EnumValue);
  }

  static bool IsValidPath(const SkPath& path) { return path.isValid(); }

  static bool IsUnsetRect(const SkRect& rect) {
    return rect.fLeft == SK_ScalarInfinity;
  }

  static bool IsValidOrUnsetRect(const SkRect& rect) {
    return IsUnsetRect(rect) || rect.isFinite();
  }

  static constexpr size_t kNumOpTypes =
      static_cast<size_t>(PaintOpType::kLastPaintOpType) + 1;
  static bool g_is_draw_op[kNumOpTypes];
  static bool g_has_paint_flags[kNumOpTypes];
  static uint16_t g_type_to_aligned_size[kNumOpTypes];

  static constexpr bool kIsDrawOp = false;
  static constexpr bool kHasPaintFlags = false;
  static const SkRect kUnsetRect;

  PaintOp(const PaintOp&) = delete;
  PaintOp& operator=(const PaintOp&) = delete;

 protected:
  ~PaintOp() = default;
};

class CC_PAINT_EXPORT PaintOpWithFlags : public PaintOp {
 public:
  static constexpr bool kHasPaintFlags = true;
  PaintOpWithFlags(PaintOpType type, const PaintFlags& flags)
      : PaintOp(type), flags(flags) {}

  int CountSlowPathsFromFlags() const { return flags.getPathEffect() ? 1 : 0; }
  bool HasNonAAPaint() const { return !flags.isAntiAlias(); }
  bool HasDiscardableImagesFromFlags(
      gfx::ContentColorUsage* content_color_usage) const;

  void RasterWithFlags(SkCanvas* canvas,
                       const PaintFlags* flags,
                       const PlaybackParams& params) const;

  // Subclasses should provide a static RasterWithFlags() method which is called
  // from the Raster() method. The RasterWithFlags() should use the SkPaint
  // passed to it, instead of the |flags| member directly, as some callers may
  // provide a modified PaintFlags. The RasterWithFlags() method is static with
  // a const PaintOpWithFlags* parameter so that it can be used as a function
  // pointer.
  PaintFlags flags;

 protected:
  ~PaintOpWithFlags() = default;

  explicit PaintOpWithFlags(PaintOpType type) : PaintOp(type) {}
};

class CC_PAINT_EXPORT AnnotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kAnnotate;
  AnnotateOp(PaintCanvas::AnnotationType annotation_type,
             const SkRect& rect,
             sk_sp<SkData> data);
  ~AnnotateOp();
  static void Raster(const AnnotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return rect.isFinite(); }
  bool EqualsForTesting(const AnnotateOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  PaintCanvas::AnnotationType annotation_type;
  SkRect rect;
  sk_sp<SkData> data;

 private:
  AnnotateOp();
};

class CC_PAINT_EXPORT ClipPathOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kClipPath;
  ClipPathOp(SkPath path,
             SkClipOp op,
             bool antialias,
             UsePaintCache use_paint_cache = UsePaintCache::kEnabled)
      : PaintOp(kType),
        path(path),
        op(op),
        antialias(antialias),
        use_cache(use_paint_cache) {}
  static void Raster(const ClipPathOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && IsValidPath(path); }
  bool EqualsForTesting(const ClipPathOp& other) const;
  int CountSlowPaths() const;
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;
  SkClipOp op;
  bool antialias;
  UsePaintCache use_cache = UsePaintCache::kDisabled;

 private:
  ClipPathOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT ClipRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kClipRect;
  ClipRectOp(const SkRect& rect, SkClipOp op, bool antialias)
      : PaintOp(kType), rect(rect), op(op), antialias(antialias) {}
  static void Raster(const ClipRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rect.isFinite(); }
  bool EqualsForTesting(const ClipRectOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;
  SkClipOp op;
  bool antialias;

 private:
  ClipRectOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT ClipRRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kClipRRect;
  ClipRRectOp(const SkRRect& rrect, SkClipOp op, bool antialias)
      : PaintOp(kType), rrect(rrect), op(op), antialias(antialias) {}
  static void Raster(const ClipRRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rrect.isValid(); }
  bool EqualsForTesting(const ClipRRectOp& other) const;
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;
  SkClipOp op;
  bool antialias;

 private:
  ClipRRectOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT ConcatOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kConcat;
  explicit ConcatOp(const SkM44& matrix) : PaintOp(kType), matrix(matrix) {}
  static void Raster(const ConcatOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const ConcatOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkM44 matrix;

 private:
  ConcatOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT CustomDataOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kCustomData;
  explicit CustomDataOp(uint32_t id) : PaintOp(kType), id(id) {}
  static void Raster(const CustomDataOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const CustomDataOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  // Stores user defined id as a placeholder op.
  uint32_t id;

 private:
  CustomDataOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT DrawColorOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawColor;
  static constexpr bool kIsDrawOp = true;
  DrawColorOp(SkColor4f color, SkBlendMode mode)
      : PaintOp(kType), color(color), mode(mode) {}
  static void Raster(const DrawColorOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidDrawColorSkBlendMode(mode); }
  bool EqualsForTesting(const DrawColorOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkColor4f color;
  SkBlendMode mode;

 private:
  DrawColorOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT DrawDRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawDRRect;
  static constexpr bool kIsDrawOp = true;
  DrawDRRectOp(const SkRRect& outer,
               const SkRRect& inner,
               const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), outer(outer), inner(inner) {}
  static void RasterWithFlags(const DrawDRRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && outer.isValid() && inner.isValid();
  }
  bool EqualsForTesting(const DrawDRRectOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect outer;
  SkRRect inner;

 private:
  DrawDRRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawImageOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawImage;
  static constexpr bool kIsDrawOp = true;
  DrawImageOp(const PaintImage& image, SkScalar left, SkScalar top);
  DrawImageOp(const PaintImage& image,
              SkScalar left,
              SkScalar top,
              const SkSamplingOptions&,
              const PaintFlags* flags);
  ~DrawImageOp();
  static void RasterWithFlags(const DrawImageOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && std::isfinite(scale_adjustment.width()) &&
           std::isfinite(scale_adjustment.height());
  }
  bool EqualsForTesting(const DrawImageOp& other) const;
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const;
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkScalar left;
  SkScalar top;
  SkSamplingOptions sampling;

 private:
  DrawImageOp();

  // Scale that has already been applied to the decoded image during
  // serialization. Used with OOP raster.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
};

class CC_PAINT_EXPORT DrawImageRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawImageRect;
  static constexpr bool kIsDrawOp = true;
  DrawImageRectOp(const PaintImage& image,
                  const SkRect& src,
                  const SkRect& dst,
                  SkCanvas::SrcRectConstraint constraint);
  DrawImageRectOp(const PaintImage& image,
                  const SkRect& src,
                  const SkRect& dst,
                  const SkSamplingOptions&,
                  const PaintFlags* flags,
                  SkCanvas::SrcRectConstraint constraint);
  ~DrawImageRectOp();
  static void RasterWithFlags(const DrawImageRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && src.isFinite() && dst.isFinite() &&
           std::isfinite(scale_adjustment.width()) &&
           std::isfinite(scale_adjustment.height());
  }
  bool EqualsForTesting(const DrawImageRectOp& other) const;
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const;
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkRect src;
  SkRect dst;
  SkSamplingOptions sampling;
  SkCanvas::SrcRectConstraint constraint;

 private:
  DrawImageRectOp();

  // Scale that has already been applied to the decoded image during
  // serialization. Used with OOP raster.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
};

class CC_PAINT_EXPORT DrawIRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawIRect;
  static constexpr bool kIsDrawOp = true;
  DrawIRectOp(const SkIRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rect(rect) {}
  static void RasterWithFlags(const DrawIRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool EqualsForTesting(const DrawIRectOp& other) const;
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkIRect rect;

 private:
  DrawIRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawLineOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawLine;
  static constexpr bool kIsDrawOp = true;
  DrawLineOp(SkScalar x0,
             SkScalar y0,
             SkScalar x1,
             SkScalar y1,
             const PaintFlags& flags,
             bool draw_as_path = false)
      : PaintOpWithFlags(kType, flags),
        x0(x0),
        y0(y0),
        x1(x1),
        y1(y1),
        draw_as_path(draw_as_path) {}
  static void RasterWithFlags(const DrawLineOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool EqualsForTesting(const DrawLineOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  int CountSlowPaths() const;

  SkScalar x0;
  SkScalar y0;
  SkScalar x1;
  SkScalar y1;
  // Used to indicate if rasterization should treat the line as a path.
  // Typically this should be false, but in some situations it can be quicker
  // to raster lines as paths.
  bool draw_as_path;

 private:
  DrawLineOp() : PaintOpWithFlags(kType) {}
};

// TODO(crbug.com/340122178): figure out a better way to unify types.
class CC_PAINT_EXPORT DrawLineLiteOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawLineLite;
  static constexpr bool kIsDrawOp = true;
  DrawLineLiteOp(SkScalar x0,
                 SkScalar y0,
                 SkScalar x1,
                 SkScalar y1,
                 const CorePaintFlags& core_paint_flags)
      : PaintOp(kType),
        x0(x0),
        y0(y0),
        x1(x1),
        y1(y1),
        core_paint_flags(core_paint_flags) {}
  static void Raster(const DrawLineLiteOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return core_paint_flags.IsValid(); }
  bool EqualsForTesting(const DrawLineLiteOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  int CountSlowPaths() const { return 0; }

  SkScalar x0;
  SkScalar y0;
  SkScalar x1;
  SkScalar y1;
  CorePaintFlags core_paint_flags;

 private:
  DrawLineLiteOp() : PaintOp(kType) {}
};

// TODO(crbug.com/340122178): figure out a better way to unify types.
class CC_PAINT_EXPORT DrawArcLiteOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawArcLite;
  static constexpr bool kIsDrawOp = true;
  DrawArcLiteOp(const SkRect& oval,
                SkScalar start_angle_degrees,
                SkScalar sweep_angle_degrees,
                const CorePaintFlags& core_paint_flags)
      : PaintOp(kType),
        oval(oval),
        start_angle_degrees(start_angle_degrees),
        sweep_angle_degrees(sweep_angle_degrees),
        core_paint_flags(core_paint_flags) {}
  static void Raster(const DrawArcLiteOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const {
    return core_paint_flags.IsValid() && oval.isFinite() &&
           std::isfinite(start_angle_degrees) &&
           std::isfinite(sweep_angle_degrees);
  }
  bool EqualsForTesting(const DrawArcLiteOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;
  SkScalar start_angle_degrees;
  SkScalar sweep_angle_degrees;
  CorePaintFlags core_paint_flags;

 private:
  DrawArcLiteOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT DrawArcOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawArc;
  static constexpr bool kIsDrawOp = true;
  DrawArcOp(const SkRect& oval,
            SkScalar start_angle_degrees,
            SkScalar sweep_angle_degrees,
            const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags),
        oval(oval),
        start_angle_degrees(start_angle_degrees),
        sweep_angle_degrees(sweep_angle_degrees) {}
  static void RasterWithFlags(const DrawArcOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  // Actual implementation for rastering.
  void RasterWithFlagsImpl(const PaintFlags* flags, SkCanvas* canvas) const;
  bool IsValid() const {
    return flags.IsValid() && oval.isFinite() &&
           std::isfinite(start_angle_degrees) &&
           std::isfinite(sweep_angle_degrees);
  }
  bool EqualsForTesting(const DrawArcOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;
  SkScalar start_angle_degrees;
  SkScalar sweep_angle_degrees;

 private:
  DrawArcOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawOvalOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawOval;
  static constexpr bool kIsDrawOp = true;
  DrawOvalOp(const SkRect& oval, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), oval(oval) {}
  static void RasterWithFlags(const DrawOvalOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && oval.isFinite(); }
  bool EqualsForTesting(const DrawOvalOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;

 private:
  DrawOvalOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawPathOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawPath;
  static constexpr bool kIsDrawOp = true;
  DrawPathOp(const SkPath& path,
             const PaintFlags& flags,
             UsePaintCache use_paint_cache = UsePaintCache::kEnabled)
      : PaintOpWithFlags(kType, flags),
        path(path),
        sk_path_fill_type(static_cast<uint8_t>(path.getFillType())),
        use_cache(use_paint_cache) {}
  static void RasterWithFlags(const DrawPathOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidPath(path); }
  bool EqualsForTesting(const DrawPathOp& other) const;
  int CountSlowPaths() const;
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;

  // Changing the fill type on an SkPath does not change the
  // generation id. This can lead to caching issues so we explicitly
  // serialize/deserialize this value and set it on the SkPath before handing it
  // to Skia.
  uint8_t sk_path_fill_type;
  UsePaintCache use_cache = UsePaintCache::kDisabled;

 private:
  DrawPathOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawRecordOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawRecord;
  static constexpr bool kIsDrawOp = true;
  explicit DrawRecordOp(PaintRecord record, bool local_ctm = true);
  ~DrawRecordOp();
  static void Raster(const DrawRecordOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const DrawRecordOp& other) const;
  size_t AdditionalBytesUsed() const;
  size_t AdditionalOpCount() const;
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const;
  int CountSlowPaths() const;
  bool HasNonAAPaint() const;
  bool HasDrawTextOps() const;
  bool HasSaveLayerOps() const;
  bool HasSaveLayerAlphaOps() const;
  bool HasEffectsPreventingLCDTextForSaveLayerAlpha() const;
  HAS_SERIALIZATION_FUNCTIONS();

  PaintRecord record;

  // If `local_ctm` is `true`, the transform operations in `record` are local to
  // that recording: any transform changes done by `record` are undone before
  // this `DrawRecordOp` completes and `SetMatrixOp` acts relatively to the
  // transform set on the destination record (to "anchor" `SetMatrixOp` and
  // other multiplicative matrix transforms on the same base transform). If
  // `local_ctm` is `false`, matrix changes done by `record` act as if part of
  // the parent record: transform changes are preserved after this
  // `DrawRecordOp` is rasterized and `SetMatrixOp` ignores parent transforms.
  bool local_ctm = true;
};

class CC_PAINT_EXPORT DrawRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawRect;
  static constexpr bool kIsDrawOp = true;
  DrawRectOp(const SkRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rect(rect) {}
  static void RasterWithFlags(const DrawRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rect.isFinite(); }
  bool EqualsForTesting(const DrawRectOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;

 private:
  DrawRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawRRect;
  static constexpr bool kIsDrawOp = true;
  DrawRRectOp(const SkRRect& rrect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rrect(rrect) {}
  static void RasterWithFlags(const DrawRRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rrect.isValid(); }
  bool EqualsForTesting(const DrawRRectOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;

 private:
  DrawRRectOp() : PaintOpWithFlags(kType) {}
};

// This is used to draw non-composited scrolling contents. The display item
// list should contain painted results beyond the current scroll port like
// composited scrolling contents. During rasterization or serialization, the
// current clip of the canvas and the current scroll offset and will be applied
// to the display item list. This PaintOp doesn't apply the overflow clip of
// the scroller, but the client should emit ClipRectOp.
class CC_PAINT_EXPORT DrawScrollingContentsOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawScrollingContents;
  static constexpr bool kIsDrawOp = true;
  DrawScrollingContentsOp(ElementId scroll_element_id,
                          scoped_refptr<DisplayItemList> display_item_list);
  ~DrawScrollingContentsOp();
  static void Raster(const DrawScrollingContentsOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return scroll_element_id && display_item_list; }
  bool EqualsForTesting(const DrawScrollingContentsOp& other) const;
  size_t AdditionalBytesUsed() const;
  size_t AdditionalOpCount() const;
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const;
  int CountSlowPaths() const;
  bool HasNonAAPaint() const;
  bool HasDrawTextOps() const;
  bool HasSaveLayerOps() const;
  bool HasSaveLayerAlphaOps() const;
  bool HasEffectsPreventingLCDTextForSaveLayerAlpha() const;
  HAS_SERIALIZATION_FUNCTIONS();

  ElementId scroll_element_id;
  scoped_refptr<DisplayItemList> display_item_list;
};

class CC_PAINT_EXPORT DrawVerticesOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawVertices;
  static constexpr bool kIsDrawOp = true;

  DrawVerticesOp(scoped_refptr<RefCountedBuffer<SkPoint>> vertices,
                 scoped_refptr<RefCountedBuffer<SkPoint>> uvs,
                 scoped_refptr<RefCountedBuffer<uint16_t>> indices,
                 const PaintFlags&);
  ~DrawVerticesOp();

  static void RasterWithFlags(const DrawVerticesOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);

  bool IsValid() const {
    return flags.IsValid() && vertices && uvs && indices &&
           vertices->data().size() > 0 &&
           vertices->data().size() == uvs->data().size() &&
           indices->data().size() > 0 && (indices->data().size() % 3) == 0;
  }

  bool EqualsForTesting(const DrawVerticesOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  scoped_refptr<RefCountedBuffer<SkPoint>> vertices;
  scoped_refptr<RefCountedBuffer<SkPoint>> uvs;
  scoped_refptr<RefCountedBuffer<uint16_t>> indices;

 private:
  DrawVerticesOp();
};

class CC_PAINT_EXPORT DrawSkottieOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawSkottie;
  static constexpr bool kIsDrawOp = true;
  DrawSkottieOp(scoped_refptr<SkottieWrapper> skottie,
                SkRect dst,
                float t,
                SkottieFrameDataMap images,
                const SkottieColorMap& color_map,
                SkottieTextPropertyValueMap text_map);
  ~DrawSkottieOp();
  static void Raster(const DrawSkottieOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const {
    return skottie && skottie->is_valid() && !dst.isEmpty() && t >= 0 &&
           t <= 1.f;
  }
  bool EqualsForTesting(const DrawSkottieOp& other) const;
  bool HasDiscardableImages(gfx::ContentColorUsage* content_color_usage) const;
  HAS_SERIALIZATION_FUNCTIONS();

  scoped_refptr<SkottieWrapper> skottie;
  SkRect dst;
  float t;
  // Image to use for each asset in this frame of the animation. If an asset is
  // missing, the most recently used image for that asset (from a previous
  // DrawSkottieOp) gets reused when rendering this frame. Given that image
  // assets generally do not change from frame to frame in most animations, that
  // means in practice, this map is often empty.
  SkottieFrameDataMap images;
  // Node name hashes and corresponding colors to use for dynamic coloration.
  SkottieColorMap color_map;
  SkottieTextPropertyValueMap text_map;

 private:
  SkottieWrapper::FrameDataFetchResult GetImageAssetForRaster(
      SkCanvas* canvas,
      const PlaybackParams& params,
      SkottieResourceIdHash asset_id,
      float t_frame,
      sk_sp<SkImage>& image_out,
      SkSamplingOptions& sampling_out) const;

  DrawSkottieOp();
};

class CC_PAINT_EXPORT DrawSlugOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawSlug;
  static constexpr bool kIsDrawOp = true;
  DrawSlugOp(sk_sp<sktext::gpu::Slug> slug, const PaintFlags& flags);
  ~DrawSlugOp();
  static void SerializeSlugs(
      const sk_sp<sktext::gpu::Slug>& slug,
      const std::vector<sk_sp<sktext::gpu::Slug>>& extra_slugs,
      PaintOpWriter& writer,
      const PaintFlags* flags_to_serialize,
      const SkM44& current_ctm);
  static void RasterWithFlags(const DrawSlugOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool HasDrawTextOps() const { return true; }
  bool EqualsForTesting(const DrawSlugOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<sktext::gpu::Slug> slug;
  std::vector<sk_sp<sktext::gpu::Slug>> extra_slugs;

 private:
  DrawSlugOp();
};

class CC_PAINT_EXPORT DrawTextBlobOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kDrawTextBlob;
  static constexpr bool kIsDrawOp = true;
  DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                 SkScalar x,
                 SkScalar y,
                 const PaintFlags& flags);
  DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                 SkScalar x,
                 SkScalar y,
                 NodeId node_id,
                 const PaintFlags& flags);
  ~DrawTextBlobOp();
  static void RasterWithFlags(const DrawTextBlobOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool HasDrawTextOps() const { return true; }
  bool EqualsForTesting(const DrawTextBlobOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<SkTextBlob> blob;
  mutable sk_sp<sktext::gpu::Slug> slug;
  mutable std::vector<sk_sp<sktext::gpu::Slug>> extra_slugs;
  SkScalar x;
  SkScalar y;
  // This field isn't serialized.
  NodeId node_id = kInvalidNodeId;

 private:
  DrawTextBlobOp();
};

class CC_PAINT_EXPORT NoopOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kNoop;
  NoopOp() : PaintOp(kType) {}
  static void Raster(const NoopOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {}
  bool IsValid() const { return true; }
  bool EqualsForTesting(const NoopOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RestoreOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kRestore;
  RestoreOp() : PaintOp(kType) {}
  static void Raster(const RestoreOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const RestoreOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kRotate;
  explicit RotateOp(SkScalar degrees) : PaintOp(kType), degrees(degrees) {}
  static void Raster(const RotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const RotateOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar degrees;

 private:
  RotateOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT SaveOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSave;
  SaveOp() : PaintOp(kType) {}
  static void Raster(const SaveOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const SaveOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT SaveLayerOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSaveLayer;
  explicit SaveLayerOp(const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), bounds(kUnsetRect) {}
  SaveLayerOp(const SkRect& bounds, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), bounds(bounds) {}
  static void RasterWithFlags(const SaveLayerOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidOrUnsetRect(bounds); }
  bool EqualsForTesting(const SaveLayerOp& other) const;
  bool HasNonAAPaint() const { return false; }
  // We simply assume any effects (or even no effects -- just starting an empty
  // transparent layer) would break LCD text or be broken by the flags for
  // kSaveLayerAlpha to preserve LCD text.
  bool HasEffectsPreventingLCDTextForSaveLayerAlpha() const { return true; }
  bool HasSaveLayerOps() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;

 private:
  SaveLayerOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT SaveLayerAlphaOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSaveLayerAlpha;
  template <class F, class = std::enable_if_t<std::is_same_v<F, float>>>
  explicit SaveLayerAlphaOp(F alpha)
      : PaintOp(kType), bounds(kUnsetRect), alpha(alpha) {}
  template <class F, class = std::enable_if_t<std::is_same_v<F, float>>>
  SaveLayerAlphaOp(const SkRect& bounds, F alpha)
      : PaintOp(kType), bounds(bounds), alpha(alpha) {}
  static void Raster(const SaveLayerAlphaOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidOrUnsetRect(bounds); }
  bool EqualsForTesting(const SaveLayerAlphaOp& other) const;
  bool HasSaveLayerOps() const { return true; }
  bool HasSaveLayerAlphaOps() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;
  float alpha;

 private:
  SaveLayerAlphaOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT SaveLayerFiltersOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSaveLayerFilters;
  explicit SaveLayerFiltersOp(base::span<sk_sp<PaintFilter>> filters,
                              const PaintFlags& flags);
  ~SaveLayerFiltersOp();
  static void RasterWithFlags(const SaveLayerFiltersOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && (!flags.getImageFilter() || filters.empty());
  }
  bool EqualsForTesting(const SaveLayerFiltersOp& other) const;
  bool HasSaveLayerOps() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  std::vector<sk_sp<PaintFilter>> filters;

 private:
  SaveLayerFiltersOp();
};

class CC_PAINT_EXPORT ScaleOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kScale;
  ScaleOp(SkScalar sx, SkScalar sy) : PaintOp(kType), sx(sx), sy(sy) {}
  static void Raster(const ScaleOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const ScaleOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar sx;
  SkScalar sy;

 private:
  ScaleOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT SetMatrixOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSetMatrix;
  explicit SetMatrixOp(const SkM44& matrix) : PaintOp(kType), matrix(matrix) {}
  // This is the only op that needs the original ctm of the SkCanvas
  // used for raster (since kSetMatrix is relative to the recording origin and
  // shouldn't clobber the SkCanvas raster origin).
  //
  // TODO(enne): Find some cleaner way to do this, possibly by making
  // all kSetMatrix calls kConcat??
  static void Raster(const SetMatrixOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const SetMatrixOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkM44 matrix;

 private:
  SetMatrixOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT SetNodeIdOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kSetNodeId;
  explicit SetNodeIdOp(int node_id) : PaintOp(kType), node_id(node_id) {}
  static void Raster(const SetNodeIdOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const SetNodeIdOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  int node_id;

 private:
  SetNodeIdOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT TranslateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::kTranslate;
  TranslateOp(SkScalar dx, SkScalar dy) : PaintOp(kType), dx(dx), dy(dy) {}
  static void Raster(const TranslateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  bool EqualsForTesting(const TranslateOp& other) const;
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar dx;
  SkScalar dy;

 private:
  TranslateOp() : PaintOp(kType) {}
};

#undef HAS_SERIALIZATION_FUNCTIONS

// TODO(vmpstr): Revisit this when sizes of DrawImageRectOp change.
using LargestPaintOp =
    typename std::conditional<(sizeof(DrawImageRectOp) > sizeof(DrawDRRectOp)),
                              DrawImageRectOp,
                              DrawDRRectOp>::type;

// When allocating a buffer for deserialization of a single PaintOp, the buffer
// should be aligned as PaintOpBuffer::kPaintOpAlign, and the size should be
// kLargestPaintOpAlignedSize instead of sizeof(LargestPaintOp).
inline constexpr size_t kLargestPaintOpAlignedSize =
    PaintOpBuffer::ComputeOpAlignedSize<LargestPaintOp>();

// This is declared here for use in gtest-based unit tests but is defined in
// the //cc:test_support target. Depend on that to use this in your unit test.
// This should not be used in production code.
void PrintTo(const PaintOp& rect, std::ostream* os);

CC_PAINT_EXPORT bool AreLiteOpsEnabled();

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_H_
