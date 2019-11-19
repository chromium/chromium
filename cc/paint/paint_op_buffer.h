// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_H_
#define CC_PAINT_PAINT_OP_BUFFER_H_

#include <stdint.h>

#include <limits>
#include <string>
#include <type_traits>

#include "base/callback.h"
#include "base/containers/stack_container.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/optional.h"
#include "cc/base/math_util.h"
#include "cc/paint/node_id.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/color_space.h"

class SkColorSpace;
class SkStrikeClient;
class SkStrikeServer;

// PaintOpBuffer is a reimplementation of SkLiteDL.
// See: third_party/skia/src/core/SkLiteDL.h.
namespace cc {
class ClientPaintCache;
class ImageProvider;
class ServicePaintCache;

class CC_PAINT_EXPORT ThreadsafeMatrix : public SkMatrix {
 public:
  explicit ThreadsafeMatrix(const SkMatrix& matrix) : SkMatrix(matrix) {
    (void)getType();
  }
};

class CC_PAINT_EXPORT ThreadsafePath : public SkPath {
 public:
  explicit ThreadsafePath(const SkPath& path) : SkPath(path) {
    updateBoundsCache();
  }
  ThreadsafePath() { updateBoundsCache(); }
};

// See PaintOp::Serialize/Deserialize for comments.  Derived Serialize types
// don't write the 4 byte type/skip header because they don't know how much
// data they will need to write.  PaintOp::Serialize itself must update it.
#define HAS_SERIALIZATION_FUNCTIONS()                                        \
  static size_t Serialize(const PaintOp* op, void* memory, size_t size,      \
                          const SerializeOptions& options);                  \
  static PaintOp* Deserialize(const volatile void* input, size_t input_size, \
                              void* output, size_t output_size,              \
                              const DeserializeOptions& options)

enum class PaintOpType : uint8_t {
  Annotate,
  ClipPath,
  ClipRect,
  ClipRRect,
  Concat,
  CustomData,
  DrawColor,
  DrawDRRect,
  DrawImage,
  DrawImageRect,
  DrawIRect,
  DrawLine,
  DrawOval,
  DrawPath,
  DrawRecord,
  DrawRect,
  DrawRRect,
  DrawSkottie,
  DrawTextBlob,
  Noop,
  Restore,
  Rotate,
  Save,
  SaveLayer,
  SaveLayerAlpha,
  Scale,
  SetMatrix,
  Translate,
  LastPaintOpType = Translate,
};

CC_PAINT_EXPORT std::string PaintOpTypeToString(PaintOpType type);
CC_PAINT_EXPORT std::ostream& operator<<(std::ostream&, PaintOpType);

struct CC_PAINT_EXPORT PlaybackParams {
  using CustomDataRasterCallback =
      base::RepeatingCallback<void(SkCanvas* canvas, uint32_t id)>;
  using DidDrawOpCallback = base::RepeatingCallback<void()>;

  explicit PlaybackParams(ImageProvider* image_provider);
  PlaybackParams(
      ImageProvider* image_provider,
      const SkMatrix& original_ctm,
      CustomDataRasterCallback custom_callback = CustomDataRasterCallback(),
      DidDrawOpCallback did_draw_op_callback = DidDrawOpCallback());
  ~PlaybackParams();

  PlaybackParams(const PlaybackParams& other);
  PlaybackParams& operator=(const PlaybackParams& other);

  ImageProvider* image_provider;
  SkMatrix original_ctm;
  CustomDataRasterCallback custom_callback;
  DidDrawOpCallback did_draw_op_callback;
};

class CC_PAINT_EXPORT PaintOp {
 public:
  uint32_t type : 8;
  uint32_t skip : 24;

  explicit PaintOp(PaintOpType type) : type(static_cast<uint8_t>(type)) {}

  PaintOpType GetType() const { return static_cast<PaintOpType>(type); }

  // Subclasses should provide a static Raster() method which is called from
  // here. The Raster method should take a const PaintOp* parameter. It is
  // static with a pointer to the base type so that we can use it as a function
  // pointer.
  void Raster(SkCanvas* canvas, const PlaybackParams& params) const;
  bool IsDrawOp() const;
  bool IsPaintOpWithFlags() const;

  bool operator==(const PaintOp& other) const;
  bool operator!=(const PaintOp& other) const { return !(*this == other); }

  struct CC_PAINT_EXPORT SerializeOptions {
    SerializeOptions(ImageProvider* image_provider,
                     TransferCacheSerializeHelper* transfer_cache,
                     ClientPaintCache* paint_cache,
                     SkCanvas* canvas,
                     SkStrikeServer* strike_server,
                     sk_sp<SkColorSpace> color_space,
                     bool can_use_lcd_text,
                     bool context_supports_distance_field_text,
                     int max_texture_size,
                     size_t max_texture_bytes,
                     const SkMatrix& original_ctm);
    SerializeOptions(const SerializeOptions&);
    SerializeOptions& operator=(const SerializeOptions&);
    ~SerializeOptions();

    // Required.
    ImageProvider* image_provider = nullptr;
    TransferCacheSerializeHelper* transfer_cache = nullptr;
    ClientPaintCache* paint_cache = nullptr;
    SkCanvas* canvas = nullptr;
    SkStrikeServer* strike_server = nullptr;
    sk_sp<SkColorSpace> color_space = nullptr;
    bool can_use_lcd_text = false;
    bool context_supports_distance_field_text = true;
    int max_texture_size = 0;
    size_t max_texture_bytes = 0.f;
    SkMatrix original_ctm = SkMatrix::I();

    // Optional.
    // The flags to use when serializing this op. This can be used to override
    // the flags serialized with the op. Valid only for PaintOpWithFlags.
    const PaintFlags* flags_to_serialize = nullptr;
  };

  struct CC_PAINT_EXPORT DeserializeOptions {
    DeserializeOptions(TransferCacheDeserializeHelper* transfer_cache,
                       ServicePaintCache* paint_cache,
                       SkStrikeClient* strike_client,
                       std::vector<uint8_t>* scratch_buffer);
    TransferCacheDeserializeHelper* transfer_cache = nullptr;
    ServicePaintCache* paint_cache = nullptr;
    SkStrikeClient* strike_client = nullptr;
    // Do a DumpWithoutCrashing when serialization fails.
    bool crash_dump_on_failure = false;
    // Used to memcpy Skia flattenables into to avoid TOCTOU issues.
    std::vector<uint8_t>* scratch_buffer = nullptr;
  };

  // Indicates how PaintImages are serialized.
  enum class SerializedImageType : uint8_t {
    kNoImage,
    kImageData,
    kTransferCacheEntry,
    kLastType = kTransferCacheEntry
  };

  // Subclasses should provide a static Serialize() method called from here.
  // If the op can be serialized to |memory| in no more than |size| bytes,
  // then return the number of bytes written.  If it won't fit, return 0.
  size_t Serialize(void* memory,
                   size_t size,
                   const SerializeOptions& options) const;

  // Deserializes a PaintOp of this type from a given buffer |input| of
  // at most |input_size| bytes.  Returns null on any errors.
  // The PaintOp is deserialized into the |output| buffer and returned
  // if valid.  nullptr is returned if the deserialization fails.
  // |output_size| must be at least LargestPaintOp + serialized->skip,
  // to fit all ops.  The caller is responsible for destroying these ops.
  // After reading, it returns the number of bytes read in |read_bytes|.
  static PaintOp* Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              size_t* read_bytes,
                              const DeserializeOptions& options);

  // For draw ops, returns true if a conservative bounding rect can be provided
  // for the op.
  static bool GetBounds(const PaintOp* op, SkRect* rect);

  // Returns true if the op lies outside the current clip and should be skipped.
  // Should only be used with draw ops.
  static bool QuickRejectDraw(const PaintOp* op, const SkCanvas* canvas);

  // Returns true if executing this op will require decoding of any lazy
  // generated images.
  static bool OpHasDiscardableImages(const PaintOp* op);

  // Returns true if the given op type has PaintFlags.
  static bool TypeHasFlags(PaintOpType type);

  int CountSlowPaths() const { return 0; }
  int CountSlowPathsFromFlags() const { return 0; }

  bool HasNonAAPaint() const { return false; }

  bool HasDiscardableImages() const { return false; }
  bool HasDiscardableImagesFromFlags() const { return false; }

  bool HasText() const { return false; }

  // Returns the number of bytes used by this op in referenced sub records
  // and display lists.  This doesn't count other objects like paths or blobs.
  size_t AdditionalBytesUsed() const { return 0; }

  // Returns the number of ops in referenced sub records and display lists.
  size_t AdditionalOpCount() const { return 0; }

  // Run the destructor for the derived op type.  Ops are usually contained in
  // memory buffers and so don't have their destructors run automatically.
  void DestroyThis();

  // DrawColor is more restrictive on the blend modes that can be used.
  static bool IsValidDrawColorSkBlendMode(SkBlendMode mode) {
    return static_cast<uint32_t>(mode) <=
           static_cast<uint32_t>(SkBlendMode::kLastCoeffMode);
  }

  // PaintFlags can have more complex blend modes than DrawColor.
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

  // PaintOp supports having nans, but some tests want to make sure
  // that operator== is true on two objects.  These helpers compare
  // various types in a way where nan == nan is true.
  static bool AreEqualEvenIfNaN(float left, float right) {
    if (std::isnan(left) && std::isnan(right))
      return true;
    return left == right;
  }
  static bool AreSkPointsEqual(const SkPoint& left, const SkPoint& right);
  static bool AreSkPoint3sEqual(const SkPoint3& left, const SkPoint3& right);
  static bool AreSkRectsEqual(const SkRect& left, const SkRect& right);
  static bool AreSkRRectsEqual(const SkRRect& left, const SkRRect& right);
  static bool AreSkMatricesEqual(const SkMatrix& left, const SkMatrix& right);
  static bool AreSkFlattenablesEqual(SkFlattenable* left, SkFlattenable* right);

  static constexpr bool kIsDrawOp = false;
  static constexpr bool kHasPaintFlags = false;
  // Since skip and type fit in a uint32_t, this is the max size of skip.
  static constexpr size_t kMaxSkip = static_cast<size_t>(1 << 24);
  static const SkRect kUnsetRect;
};

class CC_PAINT_EXPORT PaintOpWithFlags : public PaintOp {
 public:
  static constexpr bool kHasPaintFlags = true;
  PaintOpWithFlags(PaintOpType type, const PaintFlags& flags)
      : PaintOp(type), flags(flags) {}

  int CountSlowPathsFromFlags() const { return flags.getPathEffect() ? 1 : 0; }
  bool HasNonAAPaint() const { return !flags.isAntiAlias(); }
  bool HasDiscardableImagesFromFlags() const;

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
  explicit PaintOpWithFlags(PaintOpType type) : PaintOp(type) {}
};

class CC_PAINT_EXPORT AnnotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Annotate;
  AnnotateOp(PaintCanvas::AnnotationType annotation_type,
             const SkRect& rect,
             sk_sp<SkData> data);
  ~AnnotateOp();
  static void Raster(const AnnotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return rect.isFinite(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  PaintCanvas::AnnotationType annotation_type;
  SkRect rect;
  sk_sp<SkData> data;

 private:
  AnnotateOp();
};

class CC_PAINT_EXPORT ClipPathOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipPath;
  ClipPathOp(SkPath path, SkClipOp op, bool antialias)
      : PaintOp(kType), path(path), op(op), antialias(antialias) {}
  static void Raster(const ClipPathOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && IsValidPath(path); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  int CountSlowPaths() const;
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;
  SkClipOp op;
  bool antialias;

 private:
  ClipPathOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT ClipRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipRect;
  ClipRectOp(const SkRect& rect, SkClipOp op, bool antialias)
      : PaintOp(kType), rect(rect), op(op), antialias(antialias) {}
  static void Raster(const ClipRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rect.isFinite(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;
  SkClipOp op;
  bool antialias;
};

class CC_PAINT_EXPORT ClipRRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipRRect;
  ClipRRectOp(const SkRRect& rrect, SkClipOp op, bool antialias)
      : PaintOp(kType), rrect(rrect), op(op), antialias(antialias) {}
  static void Raster(const ClipRRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rrect.isValid(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;
  SkClipOp op;
  bool antialias;
};

class CC_PAINT_EXPORT ConcatOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Concat;
  explicit ConcatOp(const SkMatrix& matrix) : PaintOp(kType), matrix(matrix) {}
  static void Raster(const ConcatOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafeMatrix matrix;
};

class CC_PAINT_EXPORT CustomDataOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::CustomData;
  explicit CustomDataOp(uint32_t id) : PaintOp(kType), id(id) {}
  static void Raster(const CustomDataOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  // Stores user defined id as a placeholder op.
  uint32_t id;
};

class CC_PAINT_EXPORT DrawColorOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawColor;
  static constexpr bool kIsDrawOp = true;
  DrawColorOp(SkColor color, SkBlendMode mode)
      : PaintOp(kType), color(color), mode(mode) {}
  static void Raster(const DrawColorOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidDrawColorSkBlendMode(mode); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkColor color;
  SkBlendMode mode;
};

class CC_PAINT_EXPORT DrawDRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawDRRect;
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
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect outer;
  SkRRect inner;

 private:
  DrawDRRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawImageOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawImage;
  static constexpr bool kIsDrawOp = true;
  DrawImageOp(const PaintImage& image,
              SkScalar left,
              SkScalar top,
              const PaintFlags* flags);
  ~DrawImageOp();
  static void RasterWithFlags(const DrawImageOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && SkScalarIsFinite(scale_adjustment.width()) &&
           SkScalarIsFinite(scale_adjustment.height());
  }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasDiscardableImages() const;
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkScalar left;
  SkScalar top;

 private:
  DrawImageOp();

  // Scale that has already been applied to the decoded image during
  // serialization. Used with OOP raster.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
};

class CC_PAINT_EXPORT DrawImageRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawImageRect;
  static constexpr bool kIsDrawOp = true;
  DrawImageRectOp(const PaintImage& image,
                  const SkRect& src,
                  const SkRect& dst,
                  const PaintFlags* flags,
                  PaintCanvas::SrcRectConstraint constraint);
  ~DrawImageRectOp();
  static void RasterWithFlags(const DrawImageRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && src.isFinite() && dst.isFinite() &&
           SkScalarIsFinite(scale_adjustment.width()) &&
           SkScalarIsFinite(scale_adjustment.height());
  }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasDiscardableImages() const;
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkRect src;
  SkRect dst;
  PaintCanvas::SrcRectConstraint constraint;

 private:
  DrawImageRectOp();

  // Scale that has already been applied to the decoded image during
  // serialization. Used with OOP raster.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
};

class CC_PAINT_EXPORT DrawIRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawIRect;
  static constexpr bool kIsDrawOp = true;
  DrawIRectOp(const SkIRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rect(rect) {}
  static void RasterWithFlags(const DrawIRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkIRect rect;

 private:
  DrawIRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawLineOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawLine;
  static constexpr bool kIsDrawOp = true;
  DrawLineOp(SkScalar x0,
             SkScalar y0,
             SkScalar x1,
             SkScalar y1,
             const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), x0(x0), y0(y0), x1(x1), y1(y1) {}
  static void RasterWithFlags(const DrawLineOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  int CountSlowPaths() const;

  SkScalar x0;
  SkScalar y0;
  SkScalar x1;
  SkScalar y1;

 private:
  DrawLineOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawOvalOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawOval;
  static constexpr bool kIsDrawOp = true;
  DrawOvalOp(const SkRect& oval, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), oval(oval) {}
  static void RasterWithFlags(const DrawOvalOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    // Reproduce SkRRect::isValid without converting.
    return flags.IsValid() && oval.isFinite() && oval.isSorted();
  }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;

 private:
  DrawOvalOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawPathOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawPath;
  static constexpr bool kIsDrawOp = true;
  DrawPathOp(const SkPath& path, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), path(path) {}
  static void RasterWithFlags(const DrawPathOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidPath(path); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  int CountSlowPaths() const;
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;

 private:
  DrawPathOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawRecordOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRecord;
  static constexpr bool kIsDrawOp = true;
  explicit DrawRecordOp(sk_sp<const PaintRecord> record);
  ~DrawRecordOp();
  static void Raster(const DrawRecordOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  size_t AdditionalBytesUsed() const;
  size_t AdditionalOpCount() const;
  bool HasDiscardableImages() const;
  int CountSlowPaths() const;
  bool HasNonAAPaint() const;
  bool HasText() const;
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<const PaintRecord> record;
};

class CC_PAINT_EXPORT DrawRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRect;
  static constexpr bool kIsDrawOp = true;
  DrawRectOp(const SkRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rect(rect) {}
  static void RasterWithFlags(const DrawRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rect.isFinite(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;

 private:
  DrawRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRRect;
  static constexpr bool kIsDrawOp = true;
  DrawRRectOp(const SkRRect& rrect, const PaintFlags& flags)
      : PaintOpWithFlags(kType, flags), rrect(rrect) {}
  static void RasterWithFlags(const DrawRRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rrect.isValid(); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;

 private:
  DrawRRectOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT DrawSkottieOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawSkottie;
  static constexpr bool kIsDrawOp = true;
  DrawSkottieOp(scoped_refptr<SkottieWrapper> skottie, SkRect dst, float t);
  ~DrawSkottieOp();
  static void Raster(const DrawSkottieOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const {
    return !!skottie && !dst.isEmpty() && t >= 0 && t <= 1.f;
  }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  scoped_refptr<SkottieWrapper> skottie;
  SkRect dst;
  float t;

 private:
  DrawSkottieOp();
};

class CC_PAINT_EXPORT DrawTextBlobOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawTextBlob;
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
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasText() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<SkTextBlob> blob;
  SkScalar x;
  SkScalar y;
  // This field isn't serialized.
  NodeId node_id = kInvalidNodeId;

 private:
  DrawTextBlobOp();
};

class CC_PAINT_EXPORT NoopOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Noop;
  NoopOp() : PaintOp(kType) {}
  static void Raster(const NoopOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {}
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RestoreOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Restore;
  RestoreOp() : PaintOp(kType) {}
  static void Raster(const RestoreOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Rotate;
  explicit RotateOp(SkScalar degrees) : PaintOp(kType), degrees(degrees) {}
  static void Raster(const RotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar degrees;
};

class CC_PAINT_EXPORT SaveOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Save;
  SaveOp() : PaintOp(kType) {}
  static void Raster(const SaveOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT SaveLayerOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::SaveLayer;
  SaveLayerOp(const SkRect* bounds, const PaintFlags* flags)
      : PaintOpWithFlags(kType, flags ? *flags : PaintFlags()),
        bounds(bounds ? *bounds : kUnsetRect) {}
  static void RasterWithFlags(const SaveLayerOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidOrUnsetRect(bounds); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;

 private:
  SaveLayerOp() : PaintOpWithFlags(kType) {}
};

class CC_PAINT_EXPORT SaveLayerAlphaOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::SaveLayerAlpha;
  SaveLayerAlphaOp(const SkRect* bounds, uint8_t alpha)
      : PaintOp(kType), bounds(bounds ? *bounds : kUnsetRect), alpha(alpha) {}
  static void Raster(const SaveLayerAlphaOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidOrUnsetRect(bounds); }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;
  uint8_t alpha;
};

class CC_PAINT_EXPORT ScaleOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Scale;
  ScaleOp(SkScalar sx, SkScalar sy) : PaintOp(kType), sx(sx), sy(sy) {}
  static void Raster(const ScaleOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar sx;
  SkScalar sy;

 private:
  ScaleOp() : PaintOp(kType) {}
};

class CC_PAINT_EXPORT SetMatrixOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::SetMatrix;
  explicit SetMatrixOp(const SkMatrix& matrix)
      : PaintOp(kType), matrix(matrix) {}
  // This is the only op that needs the original ctm of the SkCanvas
  // used for raster (since SetMatrix is relative to the recording origin and
  // shouldn't clobber the SkCanvas raster origin).
  //
  // TODO(enne): Find some cleaner way to do this, possibly by making
  // all SetMatrix calls Concat??
  static void Raster(const SetMatrixOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafeMatrix matrix;
};

class CC_PAINT_EXPORT TranslateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Translate;
  TranslateOp(SkScalar dx, SkScalar dy) : PaintOp(kType), dx(dx), dy(dy) {}
  static void Raster(const TranslateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  static bool AreEqual(const PaintOp* left, const PaintOp* right);
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar dx;
  SkScalar dy;
};

#undef HAS_SERIALIZATION_FUNCTIONS

// TODO(vmpstr): Revisit this when sizes of DrawImageRectOp change.
using LargestPaintOp =
    typename std::conditional<(sizeof(DrawImageRectOp) > sizeof(DrawDRRectOp)),
                              DrawImageRectOp,
                              DrawDRRectOp>::type;

class CC_PAINT_EXPORT PaintOpBuffer : public SkRefCnt {
 public:
  enum { kInitialBufferSize = 4096 };
  static constexpr size_t PaintOpAlign = 8;
  static inline size_t ComputeOpSkip(size_t sizeof_op) {
    return MathUtil::UncheckedRoundUp(sizeof_op, PaintOpBuffer::PaintOpAlign);
  }

  PaintOpBuffer();
  PaintOpBuffer(const PaintOpBuffer&) = delete;
  PaintOpBuffer(PaintOpBuffer&& other);
  ~PaintOpBuffer() override;

  PaintOpBuffer& operator=(const PaintOpBuffer&) = delete;
  PaintOpBuffer& operator=(PaintOpBuffer&& other);

  void Reset();

  // Replays the paint op buffer into the canvas.
  void Playback(SkCanvas* canvas) const;
  void Playback(SkCanvas* canvas, const PlaybackParams& params) const;

  // Deserialize PaintOps from |input|. The original content will be
  // overwritten.
  bool Deserialize(const volatile void* input,
                   size_t input_size,
                   const PaintOp::DeserializeOptions& options);

  static sk_sp<PaintOpBuffer> MakeFromMemory(
      const volatile void* input,
      size_t input_size,
      const PaintOp::DeserializeOptions& options);

  // Returns the size of the paint op buffer. That is, the number of ops
  // contained in it.
  size_t size() const { return op_count_; }
  // Returns the number of bytes used by the paint op buffer.
  size_t bytes_used() const {
    return sizeof(*this) + reserved_ + subrecord_bytes_used_;
  }
  // Returns the number of bytes used by paint ops.
  size_t paint_ops_size() const { return used_ + subrecord_bytes_used_; }
  // Returns the total number of ops including sub-records.
  size_t total_op_count() const { return op_count_ + subrecord_op_count_; }

  size_t next_op_offset() const { return used_; }
  int numSlowPaths() const { return num_slow_paths_; }
  bool HasNonAAPaint() const { return has_non_aa_paint_; }
  bool HasDiscardableImages() const { return has_discardable_images_; }
  bool HasText() const { return has_text_; }

  bool operator==(const PaintOpBuffer& other) const;
  bool operator!=(const PaintOpBuffer& other) const {
    return !(*this == other);
  }

  // Resize the PaintOpBuffer to exactly fit the current amount of used space.
  void ShrinkToFit();

  const PaintOp* GetFirstOp() const {
    return reinterpret_cast<const PaintOp*>(data_.get());
  }

  template <typename T, typename... Args>
  void push(Args&&... args) {
    static_assert(std::is_convertible<T, PaintOp>::value, "T not a PaintOp.");
    static_assert(alignof(T) <= PaintOpAlign, "");
    static_assert(sizeof(T) < std::numeric_limits<uint16_t>::max(),
                  "Cannot fit op code in skip");
    uint16_t skip = static_cast<uint16_t>(ComputeOpSkip(sizeof(T)));
    T* op = reinterpret_cast<T*>(AllocatePaintOp(skip));

    new (op) T{std::forward<Args>(args)...};
    DCHECK_EQ(op->type, static_cast<uint32_t>(T::kType));
    op->skip = skip;
    AnalyzeAddedOp(op);
  }

  void UpdateSaveLayerBounds(size_t offset, const SkRect& bounds) {
    CHECK_LT(offset, used_);
    CHECK_LE(offset + sizeof(PaintOp), used_);

    auto* op = reinterpret_cast<PaintOp*>(data_.get() + offset);
    switch (op->GetType()) {
      case SaveLayerOp::kType:
        CHECK_LE(offset + sizeof(SaveLayerOp), used_);
        static_cast<SaveLayerOp*>(op)->bounds = bounds;
        break;
      case SaveLayerAlphaOp::kType:
        CHECK_LE(offset + sizeof(SaveLayerAlphaOp), used_);
        static_cast<SaveLayerAlphaOp*>(op)->bounds = bounds;
        break;
      default:
        NOTREACHED();
    }
  }

  template <typename T>
  void AnalyzeAddedOp(const T* op) {
    static_assert(!std::is_same<T, PaintOp>::value,
                  "AnalyzeAddedOp needs a subtype of PaintOp");

    num_slow_paths_ += op->CountSlowPathsFromFlags();
    num_slow_paths_ += op->CountSlowPaths();

    has_non_aa_paint_ |= op->HasNonAAPaint();

    has_discardable_images_ |= op->HasDiscardableImages();
    has_discardable_images_ |= op->HasDiscardableImagesFromFlags();

    has_text_ |= (op->HasText());

    subrecord_bytes_used_ += op->AdditionalBytesUsed();
    subrecord_op_count_ += op->AdditionalOpCount();
  }

  template <typename T>
  const T* GetOpAtForTesting(size_t index) const {
    size_t i = 0;
    for (PaintOpBuffer::Iterator it(this); it && i <= index; ++it, ++i) {
      if (i == index && (*it)->GetType() == T::kType)
        return static_cast<const T*>(*it);
    }
    return nullptr;
  }

  size_t GetOpOffsetForTracing(const PaintOp* op) const {
    DCHECK_GE(reinterpret_cast<const char*>(op), data_.get());
    size_t result = reinterpret_cast<const char*>(op) - data_.get();
    DCHECK_LT(result, used_);
    return result;
  }

  class CC_PAINT_EXPORT Iterator {
   public:
    explicit Iterator(const PaintOpBuffer* buffer)
        : Iterator(buffer, buffer->data_.get(), 0u) {}

    PaintOp* operator->() const { return reinterpret_cast<PaintOp*>(ptr_); }
    PaintOp* operator*() const { return operator->(); }
    Iterator begin() { return Iterator(buffer_); }
    Iterator end() {
      return Iterator(buffer_, buffer_->data_.get() + buffer_->used_,
                      buffer_->used_);
    }
    bool operator!=(const Iterator& other) {
      // Not valid to compare iterators on different buffers.
      DCHECK_EQ(other.buffer_, buffer_);
      return other.op_offset_ != op_offset_;
    }
    Iterator& operator++() {
      DCHECK(*this);
      const PaintOp* op = **this;
      ptr_ += op->skip;
      op_offset_ += op->skip;

      CHECK_LE(op_offset_, buffer_->used_);
      return *this;
    }
    operator bool() const { return op_offset_ < buffer_->used_; }

   private:
    Iterator(const PaintOpBuffer* buffer, char* ptr, size_t op_offset)
        : buffer_(buffer), ptr_(ptr), op_offset_(op_offset) {}

    const PaintOpBuffer* buffer_ = nullptr;
    char* ptr_ = nullptr;
    size_t op_offset_ = 0;
  };

  class CC_PAINT_EXPORT OffsetIterator {
   public:
    // Offsets and paint op buffer must come from the same DisplayItemList.
    OffsetIterator(const PaintOpBuffer* buffer,
                   const std::vector<size_t>* offsets)
        : buffer_(buffer), ptr_(buffer_->data_.get()), offsets_(offsets) {
      if (!offsets || offsets->empty()) {
        *this = end();
        return;
      }
      op_offset_ = (*offsets)[0];
      ptr_ += op_offset_;
    }

    PaintOp* operator->() const { return reinterpret_cast<PaintOp*>(ptr_); }
    PaintOp* operator*() const { return operator->(); }
    OffsetIterator begin() { return OffsetIterator(buffer_, offsets_); }
    OffsetIterator end() {
      return OffsetIterator(buffer_, buffer_->data_.get() + buffer_->used_,
                            buffer_->used_, offsets_);
    }
    bool operator!=(const OffsetIterator& other) {
      // Not valid to compare iterators on different buffers.
      DCHECK_EQ(other.buffer_, buffer_);
      return other.op_offset_ != op_offset_;
    }
    OffsetIterator& operator++() {
      if (++offsets_index_ >= offsets_->size()) {
        *this = end();
        return *this;
      }

      size_t target_offset = (*offsets_)[offsets_index_];
      // Sanity checks.
      CHECK_GE(target_offset, op_offset_);
      // Debugging crbug.com/738182.
      base::debug::Alias(&target_offset);
      CHECK_LT(target_offset, buffer_->used_);

      // Advance the iterator to the target offset.
      ptr_ += (target_offset - op_offset_);
      op_offset_ = target_offset;

      DCHECK(!*this || (*this)->type <=
                           static_cast<uint32_t>(PaintOpType::LastPaintOpType));
      return *this;
    }

    operator bool() const { return op_offset_ < buffer_->used_; }

   private:
    OffsetIterator(const PaintOpBuffer* buffer,
                   char* ptr,
                   size_t op_offset,
                   const std::vector<size_t>* offsets)
        : buffer_(buffer),
          ptr_(ptr),
          offsets_(offsets),
          op_offset_(op_offset) {}

    const PaintOpBuffer* buffer_ = nullptr;
    char* ptr_ = nullptr;
    const std::vector<size_t>* offsets_;
    size_t op_offset_ = 0;
    size_t offsets_index_ = 0;
  };

  class CC_PAINT_EXPORT CompositeIterator {
   public:
    // Offsets and paint op buffer must come from the same DisplayItemList.
    CompositeIterator(const PaintOpBuffer* buffer,
                      const std::vector<size_t>* offsets);
    CompositeIterator(const CompositeIterator& other);
    CompositeIterator(CompositeIterator&& other);

    PaintOp* operator->() const {
      return using_offsets_ ? **offset_iter_ : **iter_;
    }
    PaintOp* operator*() const {
      return using_offsets_ ? **offset_iter_ : **iter_;
    }
    bool operator==(const CompositeIterator& other) {
      if (using_offsets_ != other.using_offsets_)
        return false;
      return using_offsets_ ? (*offset_iter_ == *other.offset_iter_)
                            : (*iter_ == *other.iter_);
    }
    bool operator!=(const CompositeIterator& other) {
      return !(*this == other);
    }
    CompositeIterator& operator++() {
      if (using_offsets_)
        ++*offset_iter_;
      else
        ++*iter_;
      return *this;
    }
    operator bool() const {
      return using_offsets_ ? !!*offset_iter_ : !!*iter_;
    }

   private:
    bool using_offsets_ = false;
    base::Optional<OffsetIterator> offset_iter_;
    base::Optional<Iterator> iter_;
  };

  class PlaybackFoldingIterator {
   public:
    PlaybackFoldingIterator(const PaintOpBuffer* buffer,
                            const std::vector<size_t>* offsets);
    ~PlaybackFoldingIterator();

    const PaintOp* operator->() const { return current_op_; }
    const PaintOp* operator*() const { return current_op_; }

    PlaybackFoldingIterator& operator++() {
      FindNextOp();
      return *this;
    }

    operator bool() const { return !!current_op_; }

    // Guaranteed to be 255 for all ops without flags.
    uint8_t alpha() const { return current_alpha_; }

   private:
    void FindNextOp();
    const PaintOp* NextUnfoldedOp();

    PaintOpBuffer::CompositeIterator iter_;

    // FIFO queue of paint ops that have been peeked at.
    base::StackVector<const PaintOp*, 3> stack_;
    DrawColorOp folded_draw_color_;
    const PaintOp* current_op_ = nullptr;
    uint8_t current_alpha_ = 255;
  };

 private:
  friend class DisplayItemList;
  friend class PaintOpBufferOffsetsTest;
  friend class SolidColorAnalyzer;

  // Replays the paint op buffer into the canvas. If |indices| is specified, it
  // contains indices in an increasing order and only the indices specified in
  // the vector will be replayed.
  void Playback(SkCanvas* canvas,
                const PlaybackParams& params,
                const std::vector<size_t>* indices) const;

  void ReallocBuffer(size_t new_size);
  // Returns the allocated op.
  void* AllocatePaintOp(size_t skip);

  std::unique_ptr<char, base::AlignedFreeDeleter> data_;
  size_t used_ = 0;
  size_t reserved_ = 0;
  size_t op_count_ = 0;

  // Record paths for veto-to-msaa for gpu raster.
  int num_slow_paths_ = 0;
  // Record additional bytes used by referenced sub-records and display lists.
  size_t subrecord_bytes_used_ = 0;
  // Record total op count of referenced sub-record and display lists.
  size_t subrecord_op_count_ = 0;

  bool has_non_aa_paint_ : 1;
  bool has_discardable_images_ : 1;
  bool has_text_ : 1;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_H_
