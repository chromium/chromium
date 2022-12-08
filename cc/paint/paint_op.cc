// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skottie_serialization_history.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "third_party/skia/include/private/chromium/GrSlug.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {
// In a future CL, convert DrawImage to explicitly take sampling instead of
// quality
PaintFlags::FilterQuality sampling_to_quality(
    const SkSamplingOptions& sampling) {
  if (sampling.useCubic) {
    return PaintFlags::FilterQuality::kHigh;
  }
  if (sampling.mipmap != SkMipmapMode::kNone) {
    return PaintFlags::FilterQuality::kMedium;
  }
  return sampling.filter == SkFilterMode::kLinear
             ? PaintFlags::FilterQuality::kLow
             : PaintFlags::FilterQuality::kNone;
}

DrawImage CreateDrawImage(const PaintImage& image,
                          const PaintFlags* flags,
                          const SkSamplingOptions& sampling,
                          const SkM44& matrix) {
  if (!image)
    return DrawImage();
  return DrawImage(image, flags->useDarkModeForImage(),
                   SkIRect::MakeWH(image.width(), image.height()),
                   sampling_to_quality(sampling), matrix);
}

bool IsScaleAdjustmentIdentity(const SkSize& scale_adjustment) {
  return std::abs(scale_adjustment.width() - 1.f) < FLT_EPSILON &&
         std::abs(scale_adjustment.height() - 1.f) < FLT_EPSILON;
}

SkRect AdjustSrcRectForScale(SkRect original, SkSize scale_adjustment) {
  if (IsScaleAdjustmentIdentity(scale_adjustment))
    return original;

  float x_scale = scale_adjustment.width();
  float y_scale = scale_adjustment.height();
  return SkRect::MakeXYWH(original.x() * x_scale, original.y() * y_scale,
                          original.width() * x_scale,
                          original.height() * y_scale);
}

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

void DrawImageRect(SkCanvas* canvas,
                   const SkImage* image,
                   const SkRect& src,
                   const SkRect& dst,
                   const SkSamplingOptions& options,
                   const SkPaint* paint,
                   SkCanvas::SrcRectConstraint constraint) {
  if (!image)
    return;
  if (constraint == SkCanvas::kStrict_SrcRectConstraint &&
      options.mipmap != SkMipmapMode::kNone &&
      src.contains(SkRect::Make(image->dimensions()))) {
    SkMatrix m;
    m.setRectToRect(src, dst, SkMatrix::ScaleToFit::kFill_ScaleToFit);
    canvas->save();
    canvas->concat(m);
    canvas->drawImage(image, 0, 0, options, paint);
    canvas->restore();
    return;
  }
  canvas->drawImageRect(image, src, dst, options, paint, constraint);
}

bool GrSlugAreEqual(sk_sp<GrSlug> left, sk_sp<GrSlug> right) {
  if (!left && !right) {
    return true;
  }
  if (left && right) {
    auto left_data = left->serialize();
    auto right_data = right->serialize();
    return left_data->equals(right_data.get());
  }
  return false;
}

}  // namespace

#define TYPES(M)      \
  M(AnnotateOp)       \
  M(ClipPathOp)       \
  M(ClipRectOp)       \
  M(ClipRRectOp)      \
  M(ConcatOp)         \
  M(CustomDataOp)     \
  M(DrawColorOp)      \
  M(DrawDRRectOp)     \
  M(DrawImageOp)      \
  M(DrawImageRectOp)  \
  M(DrawIRectOp)      \
  M(DrawLineOp)       \
  M(DrawOvalOp)       \
  M(DrawPathOp)       \
  M(DrawRecordOp)     \
  M(DrawRectOp)       \
  M(DrawRRectOp)      \
  M(DrawSkottieOp)    \
  M(DrawTextBlobOp)   \
  M(NoopOp)           \
  M(RestoreOp)        \
  M(RotateOp)         \
  M(SaveOp)           \
  M(SaveLayerOp)      \
  M(SaveLayerAlphaOp) \
  M(ScaleOp)          \
  M(SetMatrixOp)      \
  M(SetNodeIdOp)      \
  M(TranslateOp)

static constexpr size_t kNumOpTypes =
    static_cast<size_t>(PaintOpType::LastPaintOpType) + 1;

// Verify that every op is in the TYPES macro.
#define M(T) +1
static_assert(kNumOpTypes == TYPES(M), "Missing op in list");
#undef M

#define M(T) sizeof(T),
static const size_t g_type_to_size[kNumOpTypes] = {TYPES(M)};
#undef M

template <typename T, bool HasFlags>
struct Rasterizer {
  static void RasterWithFlags(const T* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params) {
    static_assert(
        !T::kHasPaintFlags,
        "This function should not be used for a PaintOp that has PaintFlags");
    DCHECK(op->IsValid());
    NOTREACHED();
  }
  static void Raster(const T* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {
    static_assert(
        !T::kHasPaintFlags,
        "This function should not be used for a PaintOp that has PaintFlags");
    DCHECK(op->IsValid());
    T::Raster(op, canvas, params);
  }
};

template <typename T>
struct Rasterizer<T, true> {
  static void RasterWithFlags(const T* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params) {
    static_assert(T::kHasPaintFlags,
                  "This function expects the PaintOp to have PaintFlags");
    DCHECK(op->IsValid());
    T::RasterWithFlags(op, flags, canvas, params);
  }

  static void Raster(const T* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {
    static_assert(T::kHasPaintFlags,
                  "This function expects the PaintOp to have PaintFlags");
    DCHECK(op->IsValid());
    T::RasterWithFlags(op, &op->flags, canvas, params);
  }
};

using RasterFunction = void (*)(const PaintOp* op,
                                SkCanvas* canvas,
                                const PlaybackParams& params);
#define M(T)                                                              \
  [](const PaintOp* op, SkCanvas* canvas, const PlaybackParams& params) { \
    Rasterizer<T, T::kHasPaintFlags>::Raster(static_cast<const T*>(op),   \
                                             canvas, params);             \
  },
static const RasterFunction g_raster_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using RasterWithFlagsFunction = void (*)(const PaintOp* op,
                                         const PaintFlags* flags,
                                         SkCanvas* canvas,
                                         const PlaybackParams& params);
#define M(T)                                                       \
  [](const PaintOp* op, const PaintFlags* flags, SkCanvas* canvas, \
     const PlaybackParams& params) {                               \
    Rasterizer<T, T::kHasPaintFlags>::RasterWithFlags(             \
        static_cast<const T*>(op), flags, canvas, params);         \
  },
static const RasterWithFlagsFunction
    g_raster_with_flags_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using SerializeFunction = size_t (*)(const PaintOp* op,
                                     void* memory,
                                     size_t size,
                                     const PaintOp::SerializeOptions& options,
                                     const PaintFlags* flags_to_serialize,
                                     const SkM44& current_ctm,
                                     const SkM44& original_ctm);

#define M(T) &T::Serialize,
static const SerializeFunction g_serialize_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using DeserializeFunction =
    PaintOp* (*)(const volatile void* input,
                 size_t input_size,
                 void* output,
                 size_t output_size,
                 const PaintOp::DeserializeOptions& options);

#define M(T) &T::Deserialize,
static const DeserializeFunction g_deserialize_functions[kNumOpTypes] = {
    TYPES(M)};
#undef M

using EqualsFunction = bool (*)(const PaintOp* left, const PaintOp* right);
#define M(T) &T::AreEqual,
static const EqualsFunction g_equals_operator[kNumOpTypes] = {TYPES(M)};
#undef M

// Most state ops (matrix, clip, save, restore) have a trivial destructor.
// TODO(enne): evaluate if we need the nullptr optimization or if
// we even need to differentiate trivial destructors here.
using VoidFunction = void (*)(PaintOp* op);
#define M(T)                                           \
  !std::is_trivially_destructible<T>::value            \
      ? [](PaintOp* op) { static_cast<T*>(op)->~T(); } \
      : static_cast<VoidFunction>(nullptr),
static const VoidFunction g_destructor_functions[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T) T::kIsDrawOp,
static bool g_is_draw_op[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T) T::kHasPaintFlags,
static bool g_has_paint_flags[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T)                                         \
  static_assert(sizeof(T) <= sizeof(LargestPaintOp), \
                #T " must be no bigger than LargestPaintOp");
TYPES(M)
#undef M

#define M(T)                                                \
  static_assert(alignof(T) <= PaintOpBuffer::kPaintOpAlign, \
                #T " must have alignment no bigger than PaintOpAlign");
TYPES(M)
#undef M

using AnalyzeOpFunc = void (*)(PaintOpBuffer*, const PaintOp*);
#define M(T)                                           \
  [](PaintOpBuffer* buffer, const PaintOp* op) {       \
    buffer->AnalyzeAddedOp(static_cast<const T*>(op)); \
  },
static const AnalyzeOpFunc g_analyze_op_functions[kNumOpTypes] = {TYPES(M)};
#undef M

#undef TYPES

const SkRect PaintOp::kUnsetRect = {SK_ScalarInfinity, 0, 0, 0};
const size_t PaintOp::kMaxSkip;

std::string PaintOpTypeToString(PaintOpType type) {
  switch (type) {
    case PaintOpType::Annotate:
      return "Annotate";
    case PaintOpType::ClipPath:
      return "ClipPath";
    case PaintOpType::ClipRect:
      return "ClipRect";
    case PaintOpType::ClipRRect:
      return "ClipRRect";
    case PaintOpType::Concat:
      return "Concat";
    case PaintOpType::CustomData:
      return "CustomData";
    case PaintOpType::DrawColor:
      return "DrawColor";
    case PaintOpType::DrawDRRect:
      return "DrawDRRect";
    case PaintOpType::DrawImage:
      return "DrawImage";
    case PaintOpType::DrawImageRect:
      return "DrawImageRect";
    case PaintOpType::DrawIRect:
      return "DrawIRect";
    case PaintOpType::DrawLine:
      return "DrawLine";
    case PaintOpType::DrawOval:
      return "DrawOval";
    case PaintOpType::DrawPath:
      return "DrawPath";
    case PaintOpType::DrawRecord:
      return "DrawRecord";
    case PaintOpType::DrawRect:
      return "DrawRect";
    case PaintOpType::DrawRRect:
      return "DrawRRect";
    case PaintOpType::DrawSkottie:
      return "DrawSkottie";
    case PaintOpType::DrawTextBlob:
      return "DrawTextBlob";
    case PaintOpType::Noop:
      return "Noop";
    case PaintOpType::Restore:
      return "Restore";
    case PaintOpType::Rotate:
      return "Rotate";
    case PaintOpType::Save:
      return "Save";
    case PaintOpType::SaveLayer:
      return "SaveLayer";
    case PaintOpType::SaveLayerAlpha:
      return "SaveLayerAlpha";
    case PaintOpType::Scale:
      return "Scale";
    case PaintOpType::SetMatrix:
      return "SetMatrix";
    case PaintOpType::SetNodeId:
      return "SetNodeId";
    case PaintOpType::Translate:
      return "Translate";
  }
  return "UNKNOWN";
}

std::ostream& operator<<(std::ostream& os, PaintOpType type) {
  return os << PaintOpTypeToString(type);
}

size_t AnnotateOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const AnnotateOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->annotation_type);
  helper.Write(op->rect);
  helper.Write(op->data);
  return helper.size();
}

size_t ClipPathOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const ClipPathOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->path, op->use_cache);
  helper.Write(op->op);
  helper.Write(op->antialias);
  return helper.size();
}

size_t ClipRectOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const ClipRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->rect);
  helper.Write(op->op);
  helper.Write(op->antialias);
  return helper.size();
}

size_t ClipRRectOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const ClipRRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->rrect);
  helper.Write(op->op);
  helper.Write(op->antialias);
  return helper.size();
}

size_t ConcatOp::Serialize(const PaintOp* base_op,
                           void* memory,
                           size_t size,
                           const SerializeOptions& options,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) {
  auto* op = static_cast<const ConcatOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->matrix);
  return helper.size();
}

size_t CustomDataOp::Serialize(const PaintOp* base_op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) {
  auto* op = static_cast<const CustomDataOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->id);
  return helper.size();
}

size_t DrawColorOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const DrawColorOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->color);
  helper.Write(op->mode);
  return helper.size();
}

size_t DrawDRRectOp::Serialize(const PaintOp* base_op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) {
  auto* op = static_cast<const DrawDRRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->outer);
  helper.Write(op->inner);
  return helper.size();
}

size_t DrawImageOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const DrawImageOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);

  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  helper.Write(
      CreateDrawImage(op->image, flags_to_serialize, op->sampling, current_ctm),
      &scale_adjustment);
  helper.AssertAlignment(alignof(SkScalar));
  helper.Write(scale_adjustment.width());
  helper.Write(scale_adjustment.height());

  helper.Write(op->left);
  helper.Write(op->top);
  helper.Write(op->sampling);
  return helper.size();
}

size_t DrawImageRectOp::Serialize(const PaintOp* base_op,
                                  void* memory,
                                  size_t size,
                                  const SerializeOptions& options,
                                  const PaintFlags* flags_to_serialize,
                                  const SkM44& current_ctm,
                                  const SkM44& original_ctm) {
  auto* op = static_cast<const DrawImageRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);

  // This adjustment mirrors DiscardableImageMap::GatherDiscardableImage logic.
  SkM44 matrix = current_ctm * SkM44(SkMatrix::RectToRect(op->src, op->dst));
  // Note that we don't request subsets here since the GpuImageCache has no
  // optimizations for using subsets.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  helper.Write(
      CreateDrawImage(op->image, flags_to_serialize, op->sampling, matrix),
      &scale_adjustment);
  helper.AssertAlignment(alignof(SkScalar));
  helper.Write(scale_adjustment.width());
  helper.Write(scale_adjustment.height());

  helper.Write(op->src);
  helper.Write(op->dst);
  helper.Write(op->sampling);
  helper.Write(op->constraint);
  return helper.size();
}

size_t DrawIRectOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const DrawIRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->rect);
  return helper.size();
}

size_t DrawLineOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const DrawLineOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.AssertAlignment(alignof(SkScalar));
  helper.Write(op->x0);
  helper.Write(op->y0);
  helper.Write(op->x1);
  helper.Write(op->y1);
  return helper.size();
}

size_t DrawOvalOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const DrawOvalOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->oval);
  return helper.size();
}

size_t DrawPathOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const DrawPathOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->path, op->use_cache);
  helper.Write(op->sk_path_fill_type);
  return helper.size();
}

size_t DrawRecordOp::Serialize(const PaintOp* op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) {
  // TODO(enne): these must be flattened.  Serializing this will not do
  // anything.
  NOTREACHED();
  return 0u;
}

size_t DrawRectOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) {
  auto* op = static_cast<const DrawRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->rect);
  return helper.size();
}

size_t DrawRRectOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const DrawRRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->rrect);
  return helper.size();
}

namespace {

template <typename T>
void SerializeSkottieMap(
    const base::flat_map<SkottieResourceIdHash, T>& map,
    PaintOpWriter& helper,
    const base::RepeatingCallback<void(const T&, PaintOpWriter&)>&
        value_serializer) {
  // Write the size of the map first so that we know how many entries to read
  // from the buffer during deserialization.
  helper.WriteSize(map.size());
  for (const auto& [resource_id, val] : map) {
    helper.WriteSize(resource_id.GetUnsafeValue());
    value_serializer.Run(val, helper);
  }
}

void SerializeSkottieFrameData(const SkM44& current_ctm,
                               const SkottieFrameData& frame_data,
                               PaintOpWriter& helper) {
  // |scale_adjustment| is not ultimately used; Skottie handles image
  // scale adjustment internally when rastering.
  SkSize scale_adjustment = SkSize::MakeEmpty();
  DrawImage draw_image;
  if (frame_data.image) {
    draw_image = DrawImage(
        frame_data.image, /*use_dark_mode=*/false,
        SkIRect::MakeWH(frame_data.image.width(), frame_data.image.height()),
        frame_data.quality, current_ctm);
  }
  helper.Write(draw_image, &scale_adjustment);
  helper.Write(frame_data.quality);
}

}  // namespace

size_t DrawSkottieOp::Serialize(const PaintOp* base_op,
                                void* memory,
                                size_t size,
                                const SerializeOptions& options,
                                const PaintFlags* flags_to_serialize,
                                const SkM44& current_ctm,
                                const SkM44& original_ctm) {
  auto* op = static_cast<const DrawSkottieOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->dst);
  helper.Write(SkFloatToScalar(op->t));
  helper.Write(op->skottie);

  SkottieFrameDataMap images_to_serialize = op->images;
  SkottieTextPropertyValueMap text_map_to_serialize = op->text_map;
  if (options.skottie_serialization_history) {
    options.skottie_serialization_history->FilterNewSkottieFrameState(
        *op->skottie, images_to_serialize, text_map_to_serialize);
  }

  SerializeSkottieMap(
      images_to_serialize, helper,
      base::BindRepeating(&SerializeSkottieFrameData, std::cref(current_ctm)));
  SerializeSkottieMap(
      op->color_map, helper,
      base::BindRepeating([](const SkColor& color, PaintOpWriter& helper) {
        helper.Write(color);
      }));
  SerializeSkottieMap(
      text_map_to_serialize, helper,
      base::BindRepeating([](const SkottieTextPropertyValue& text_property_val,
                             PaintOpWriter& helper) {
        helper.WriteSize(text_property_val.text().size());
        // If there is not enough space in the underlying buffer, WriteData()
        // will mark the |helper| as invalid and the buffer will keep growing
        // until a max size is reached (currently 64MB which should be ample for
        // text).
        helper.WriteData(text_property_val.text().size(),
                         text_property_val.text().c_str());
        helper.Write(gfx::RectFToSkRect(text_property_val.box()));
      }));
  return helper.size();
}

size_t DrawTextBlobOp::Serialize(const PaintOp* base_op,
                                 void* memory,
                                 size_t size,
                                 const SerializeOptions& options,
                                 const PaintFlags* flags_to_serialize,
                                 const SkM44& current_ctm,
                                 const SkM44& original_ctm) {
  auto* op = static_cast<const DrawTextBlobOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  unsigned int count = op->extra_slugs.size() + 1;
  helper.Write(count);
  helper.Write(op->slug);
  for (const auto& slug : op->extra_slugs) {
    helper.Write(slug);
  }
  return helper.size();
}

size_t NoopOp::Serialize(const PaintOp* base_op,
                         void* memory,
                         size_t size,
                         const SerializeOptions& options,
                         const PaintFlags* flags_to_serialize,
                         const SkM44& current_ctm,
                         const SkM44& original_ctm) {
  PaintOpWriter helper(memory, size, options);
  return helper.size();
}

size_t RestoreOp::Serialize(const PaintOp* base_op,
                            void* memory,
                            size_t size,
                            const SerializeOptions& options,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) {
  PaintOpWriter helper(memory, size, options);
  return helper.size();
}

size_t RotateOp::Serialize(const PaintOp* base_op,
                           void* memory,
                           size_t size,
                           const SerializeOptions& options,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) {
  auto* op = static_cast<const RotateOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->degrees);
  return helper.size();
}

size_t SaveOp::Serialize(const PaintOp* base_op,
                         void* memory,
                         size_t size,
                         const SerializeOptions& options,
                         const PaintFlags* flags_to_serialize,
                         const SkM44& current_ctm,
                         const SkM44& original_ctm) {
  PaintOpWriter helper(memory, size, options);
  return helper.size();
}

size_t SaveLayerOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const SaveLayerOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  if (!flags_to_serialize)
    flags_to_serialize = &op->flags;
  helper.Write(*flags_to_serialize, current_ctm);
  helper.Write(op->bounds);
  return helper.size();
}

size_t SaveLayerAlphaOp::Serialize(const PaintOp* base_op,
                                   void* memory,
                                   size_t size,
                                   const SerializeOptions& options,
                                   const PaintFlags* flags_to_serialize,
                                   const SkM44& current_ctm,
                                   const SkM44& original_ctm) {
  auto* op = static_cast<const SaveLayerAlphaOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->bounds);
  helper.Write(op->alpha);
  return helper.size();
}

size_t ScaleOp::Serialize(const PaintOp* base_op,
                          void* memory,
                          size_t size,
                          const SerializeOptions& options,
                          const PaintFlags* flags_to_serialize,
                          const SkM44& current_ctm,
                          const SkM44& original_ctm) {
  auto* op = static_cast<const ScaleOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->sx);
  helper.Write(op->sy);
  return helper.size();
}

size_t SetMatrixOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const SetMatrixOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  // Use original_ctm here because SetMatrixOp replaces current_ctm
  helper.Write(original_ctm * op->matrix);
  return helper.size();
}

size_t SetNodeIdOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const SetNodeIdOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->node_id);
  return helper.size();
}

size_t TranslateOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) {
  auto* op = static_cast<const TranslateOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->dx);
  helper.Write(op->dy);
  return helper.size();
}

template <typename T>
void UpdateTypeAndSkip(T* op) {
  op->type = static_cast<uint8_t>(T::kType);
  op->skip = PaintOpBuffer::ComputeOpSkip(sizeof(T));
}

template <typename T>
class PaintOpDeserializer {
 public:
  static_assert(std::is_base_of<PaintOp, T>::value, "T not a PaintOp.");

  explicit PaintOpDeserializer(const volatile void* input,
                               size_t input_size,
                               const PaintOp::DeserializeOptions& options,
                               T* op)
      : reader_(input, input_size, options), op_(op) {
    DCHECK(op_);
  }
  PaintOpDeserializer(const PaintOpDeserializer&) = delete;
  PaintOpDeserializer& operator=(const PaintOpDeserializer&) = delete;

  ~PaintOpDeserializer() {
    DCHECK(!op_)
        << "FinalizeOp must be called before PaintOpDeserializer is destroyed. "
           "type="
        << T::kType;
  }

  PaintOp* FinalizeOp(bool force_invalid = false) {
    DCHECK(op_) << "PaintOp has already been finalized. type=" << T::kType;

    if (force_invalid || !reader_.valid() || !op_->IsValid()) {
      op_->~T();
      op_ = nullptr;
      return nullptr;
    }

    UpdateTypeAndSkip(op_.get());
    T* op_snapshot = op_;
    op_ = nullptr;
    return op_snapshot;
  }

  PaintOp* InvalidateAndFinalizeOp() {
    return FinalizeOp(/*force_invalid=*/true);
  }

  T* operator->() { return op_; }

  template <typename... Args>
  void Read(Args&&... args) {
    reader_.Read(std::forward<Args>(args)...);
  }

  void ReadData(size_t bytes, void* data) { reader_.ReadData(bytes, data); }

  void ReadSize(size_t* size) { reader_.ReadSize(size); }

  void AssertAlignment(size_t alignment) { reader_.AssertAlignment(alignment); }

 private:
  PaintOpReader reader_;
  raw_ptr<T> op_;
};

PaintOp* AnnotateOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(AnnotateOp));
  PaintOpDeserializer<AnnotateOp> deserializer(input, input_size, options,
                                               new (output) AnnotateOp);

  deserializer.Read(&deserializer->annotation_type);
  deserializer.Read(&deserializer->rect);
  deserializer.Read(&deserializer->data);
  return deserializer.FinalizeOp();
}

PaintOp* ClipPathOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipPathOp));
  PaintOpDeserializer<ClipPathOp> deserializer(input, input_size, options,
                                               new (output) ClipPathOp);

  deserializer.Read(&deserializer->path);
  deserializer.Read(&deserializer->op);
  deserializer.Read(&deserializer->antialias);
  return deserializer.FinalizeOp();
}

PaintOp* ClipRectOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipRectOp));
  PaintOpDeserializer<ClipRectOp> deserializer(input, input_size, options,
                                               new (output) ClipRectOp);
  deserializer.Read(&deserializer->rect);
  deserializer.Read(&deserializer->op);
  deserializer.Read(&deserializer->antialias);
  return deserializer.FinalizeOp();
}

PaintOp* ClipRRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipRRectOp));
  PaintOpDeserializer<ClipRRectOp> deserializer(input, input_size, options,
                                                new (output) ClipRRectOp);
  deserializer.Read(&deserializer->rrect);
  deserializer.Read(&deserializer->op);
  deserializer.Read(&deserializer->antialias);
  return deserializer.FinalizeOp();
}

PaintOp* ConcatOp::Deserialize(const volatile void* input,
                               size_t input_size,
                               void* output,
                               size_t output_size,
                               const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ConcatOp));
  PaintOpDeserializer<ConcatOp> deserializer(input, input_size, options,
                                             new (output) ConcatOp);
  deserializer.Read(&deserializer->matrix);
  return deserializer.FinalizeOp();
}

PaintOp* CustomDataOp::Deserialize(const volatile void* input,
                                   size_t input_size,
                                   void* output,
                                   size_t output_size,
                                   const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(CustomDataOp));
  PaintOpDeserializer<CustomDataOp> deserializer(input, input_size, options,
                                                 new (output) CustomDataOp);
  deserializer.Read(&deserializer->id);
  return deserializer.FinalizeOp();
}

PaintOp* DrawColorOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawColorOp));
  PaintOpDeserializer<DrawColorOp> deserializer(input, input_size, options,
                                                new (output) DrawColorOp);
  deserializer.Read(&deserializer->color);
  deserializer.Read(&deserializer->mode);
  return deserializer.FinalizeOp();
}

PaintOp* DrawDRRectOp::Deserialize(const volatile void* input,
                                   size_t input_size,
                                   void* output,
                                   size_t output_size,
                                   const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawDRRectOp));
  PaintOpDeserializer<DrawDRRectOp> deserializer(input, input_size, options,
                                                 new (output) DrawDRRectOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->outer);
  deserializer.Read(&deserializer->inner);
  return deserializer.FinalizeOp();
}

PaintOp* DrawImageOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawImageOp));
  PaintOpDeserializer<DrawImageOp> deserializer(input, input_size, options,
                                                new (output) DrawImageOp);
  deserializer.Read(&deserializer->flags);

  deserializer.Read(&deserializer->image);
  deserializer.AssertAlignment(alignof(SkScalar));
  deserializer.Read(&deserializer->scale_adjustment.fWidth);
  deserializer.Read(&deserializer->scale_adjustment.fHeight);

  deserializer.Read(&deserializer->left);
  deserializer.Read(&deserializer->top);
  deserializer.Read(&deserializer->sampling);
  return deserializer.FinalizeOp();
}

PaintOp* DrawImageRectOp::Deserialize(const volatile void* input,
                                      size_t input_size,
                                      void* output,
                                      size_t output_size,
                                      const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawImageRectOp));
  PaintOpDeserializer<DrawImageRectOp> deserializer(
      input, input_size, options, new (output) DrawImageRectOp);
  deserializer.Read(&deserializer->flags);

  deserializer.Read(&deserializer->image);
  deserializer.AssertAlignment(alignof(SkScalar));
  deserializer.Read(&deserializer->scale_adjustment.fWidth);
  deserializer.Read(&deserializer->scale_adjustment.fHeight);

  deserializer.Read(&deserializer->src);
  deserializer.Read(&deserializer->dst);
  deserializer.Read(&deserializer->sampling);
  deserializer.Read(&deserializer->constraint);
  return deserializer.FinalizeOp();
}

PaintOp* DrawIRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawIRectOp));
  PaintOpDeserializer<DrawIRectOp> deserializer(input, input_size, options,
                                                new (output) DrawIRectOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->rect);
  return deserializer.FinalizeOp();
}

PaintOp* DrawLineOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawLineOp));
  PaintOpDeserializer<DrawLineOp> deserializer(input, input_size, options,
                                               new (output) DrawLineOp);
  deserializer.Read(&deserializer->flags);
  deserializer.AssertAlignment(alignof(SkScalar));
  deserializer.Read(&deserializer->x0);
  deserializer.Read(&deserializer->y0);
  deserializer.Read(&deserializer->x1);
  deserializer.Read(&deserializer->y1);
  return deserializer.FinalizeOp();
}

PaintOp* DrawOvalOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawOvalOp));
  PaintOpDeserializer<DrawOvalOp> deserializer(input, input_size, options,
                                               new (output) DrawOvalOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->oval);
  return deserializer.FinalizeOp();
}

PaintOp* DrawPathOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawPathOp));
  PaintOpDeserializer<DrawPathOp> deserializer(input, input_size, options,
                                               new (output) DrawPathOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->path);
  deserializer.Read(&deserializer->sk_path_fill_type);
  deserializer->path.setFillType(
      static_cast<SkPathFillType>(deserializer->sk_path_fill_type));
  return deserializer.FinalizeOp();
}

PaintOp* DrawRecordOp::Deserialize(const volatile void* input,
                                   size_t input_size,
                                   void* output,
                                   size_t output_size,
                                   const DeserializeOptions& options) {
  // TODO(enne): these must be flattened and not sent directly.
  // TODO(enne): could also consider caching these service side.
  return nullptr;
}

PaintOp* DrawRectOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawRectOp));
  PaintOpDeserializer<DrawRectOp> deserializer(input, input_size, options,
                                               new (output) DrawRectOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->rect);
  return deserializer.FinalizeOp();
}

PaintOp* DrawRRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawRRectOp));
  PaintOpDeserializer<DrawRRectOp> deserializer(input, input_size, options,
                                                new (output) DrawRRectOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->rrect);
  return deserializer.FinalizeOp();
}

namespace {

// |max_map_size| is purely a safety mechanism to prevent disastrous behavior
// (trying to allocate an enormous map, looping for long periods of time, etc)
// in case the serialization buffer is corrupted somehow.
template <typename T>
bool DeserializeSkottieMap(
    base::flat_map<SkottieResourceIdHash, T>& map,
    absl::optional<size_t> max_map_size,
    PaintOpDeserializer<DrawSkottieOp>& deserializer,
    const base::RepeatingCallback<absl::optional<T>(
        PaintOpDeserializer<DrawSkottieOp>&)>& value_deserializer) {
  size_t map_size = 0;
  deserializer.ReadSize(&map_size);
  if (max_map_size && map_size > *max_map_size)
    return false;

  for (size_t i = 0; i < map_size; ++i) {
    size_t resource_id_hash_raw = 0;
    deserializer.ReadSize(&resource_id_hash_raw);
    SkottieResourceIdHash resource_id_hash =
        SkottieResourceIdHash::FromUnsafeValue(resource_id_hash_raw);
    if (!resource_id_hash)
      return false;

    absl::optional<T> value = value_deserializer.Run(deserializer);
    if (!value)
      return false;

    // Duplicate keys should not happen by design, but defend against it
    // gracefully in case the underlying buffer is corrupted.
    bool is_new_entry = map.emplace(resource_id_hash, std::move(*value)).second;
    if (!is_new_entry)
      return false;
  }
  return true;
}

absl::optional<SkottieFrameData> DeserializeSkottieFrameData(
    PaintOpDeserializer<DrawSkottieOp>& deserializer) {
  SkottieFrameData frame_data;
  deserializer.Read(&frame_data.image);
  deserializer.Read(&frame_data.quality);
  return frame_data;
}

absl::optional<SkColor> DeserializeSkottieColor(
    PaintOpDeserializer<DrawSkottieOp>& deserializer) {
  SkColor color = SK_ColorTRANSPARENT;
  deserializer.Read(&color);
  return color;
}

absl::optional<SkottieTextPropertyValue> DeserializeSkottieTextPropertyValue(
    PaintOpDeserializer<DrawSkottieOp>& deserializer) {
  size_t text_size = 0u;
  deserializer.ReadSize(&text_size);
  std::string text(text_size, char());
  deserializer.ReadData(text_size, const_cast<char*>(text.c_str()));
  SkRect box;
  deserializer.Read(&box);
  return SkottieTextPropertyValue(std::move(text), gfx::SkRectToRectF(box));
}

}  // namespace

PaintOp* DrawSkottieOp::Deserialize(const volatile void* input,
                                    size_t input_size,
                                    void* output,
                                    size_t output_size,
                                    const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawSkottieOp));
  PaintOpDeserializer<DrawSkottieOp> deserializer(input, input_size, options,
                                                  new (output) DrawSkottieOp);
  deserializer.Read(&deserializer->dst);

  SkScalar t;
  deserializer.Read(&t);
  deserializer->t = SkScalarToFloat(t);

  deserializer.Read(&deserializer->skottie);
  // The |skottie| object gets used below, so no point in continuing if it's
  // invalid. That can lead to crashing or unexpected behavior.
  if (!deserializer->skottie || !deserializer->skottie->is_valid())
    return deserializer.InvalidateAndFinalizeOp();

  size_t num_assets_in_animation =
      deserializer->skottie->GetImageAssetMetadata().asset_storage().size();
  size_t num_text_nodes_in_animation =
      deserializer->skottie->GetTextNodeNames().size();
  bool deserialized_all_maps =
      DeserializeSkottieMap(
          deserializer->images, /*max_map_size=*/num_assets_in_animation,
          deserializer, base::BindRepeating(&DeserializeSkottieFrameData)) &&
      DeserializeSkottieMap(deserializer->color_map,
                            /*max_map_size=*/absl::nullopt, deserializer,
                            base::BindRepeating(&DeserializeSkottieColor)) &&
      DeserializeSkottieMap(
          deserializer->text_map, /*max_map_size=*/num_text_nodes_in_animation,
          deserializer,
          base::BindRepeating(&DeserializeSkottieTextPropertyValue));
  return deserialized_all_maps ? deserializer.FinalizeOp()
                               : deserializer.InvalidateAndFinalizeOp();
}

PaintOp* DrawTextBlobOp::Deserialize(const volatile void* input,
                                     size_t input_size,
                                     void* output,
                                     size_t output_size,
                                     const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawTextBlobOp));
  PaintOpDeserializer<DrawTextBlobOp> deserializer(input, input_size, options,
                                                   new (output) DrawTextBlobOp);
  deserializer.Read(&deserializer->flags);
  unsigned int count = 0;
  deserializer.Read(&count);
  deserializer.Read(&deserializer->slug);
  deserializer->extra_slugs.resize(count - 1);
  for (auto& slug : deserializer->extra_slugs) {
    deserializer.Read(&slug);
  }
  return deserializer.FinalizeOp();
}

PaintOp* NoopOp::Deserialize(const volatile void* input,
                             size_t input_size,
                             void* output,
                             size_t output_size,
                             const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(NoopOp));
  PaintOpDeserializer<NoopOp> deserializer(input, input_size, options,
                                           new (output) NoopOp);
  return deserializer.FinalizeOp();
}

PaintOp* RestoreOp::Deserialize(const volatile void* input,
                                size_t input_size,
                                void* output,
                                size_t output_size,
                                const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(RestoreOp));
  PaintOpDeserializer<RestoreOp> deserializer(input, input_size, options,
                                              new (output) RestoreOp);
  return deserializer.FinalizeOp();
}

PaintOp* RotateOp::Deserialize(const volatile void* input,
                               size_t input_size,
                               void* output,
                               size_t output_size,
                               const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(RotateOp));
  PaintOpDeserializer<RotateOp> deserializer(input, input_size, options,
                                             new (output) RotateOp);
  deserializer.Read(&deserializer->degrees);
  return deserializer.FinalizeOp();
}

PaintOp* SaveOp::Deserialize(const volatile void* input,
                             size_t input_size,
                             void* output,
                             size_t output_size,
                             const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveOp));
  PaintOpDeserializer<SaveOp> deserializer(input, input_size, options,
                                           new (output) SaveOp);
  return deserializer.FinalizeOp();
}

PaintOp* SaveLayerOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveLayerOp));
  PaintOpDeserializer<SaveLayerOp> deserializer(input, input_size, options,
                                                new (output) SaveLayerOp);
  deserializer.Read(&deserializer->flags);
  deserializer.Read(&deserializer->bounds);
  return deserializer.FinalizeOp();
}

PaintOp* SaveLayerAlphaOp::Deserialize(const volatile void* input,
                                       size_t input_size,
                                       void* output,
                                       size_t output_size,
                                       const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveLayerAlphaOp));
  PaintOpDeserializer<SaveLayerAlphaOp> deserializer(
      input, input_size, options, new (output) SaveLayerAlphaOp);
  deserializer.Read(&deserializer->bounds);
  deserializer.Read(&deserializer->alpha);
  return deserializer.FinalizeOp();
}

PaintOp* ScaleOp::Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ScaleOp));
  PaintOpDeserializer<ScaleOp> deserializer(input, input_size, options,
                                            new (output) ScaleOp);
  deserializer.Read(&deserializer->sx);
  deserializer.Read(&deserializer->sy);
  return deserializer.FinalizeOp();
}

PaintOp* SetMatrixOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SetMatrixOp));
  PaintOpDeserializer<SetMatrixOp> deserializer(input, input_size, options,
                                                new (output) SetMatrixOp);
  deserializer.Read(&deserializer->matrix);
  return deserializer.FinalizeOp();
}

PaintOp* SetNodeIdOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SetNodeIdOp));
  PaintOpDeserializer<SetNodeIdOp> deserializer(input, input_size, options,
                                                new (output) SetNodeIdOp);
  deserializer.Read(&deserializer->node_id);
  return deserializer.FinalizeOp();
}

PaintOp* TranslateOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(TranslateOp));
  PaintOpDeserializer<TranslateOp> deserializer(input, input_size, options,
                                                new (output) TranslateOp);
  deserializer.Read(&deserializer->dx);
  deserializer.Read(&deserializer->dy);
  return deserializer.FinalizeOp();
}

void AnnotateOp::Raster(const AnnotateOp* op,
                        SkCanvas* canvas,
                        const PlaybackParams& params) {
  switch (op->annotation_type) {
    case PaintCanvas::AnnotationType::URL:
      SkAnnotateRectWithURL(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::LINK_TO_DESTINATION:
      SkAnnotateLinkToDestination(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::NAMED_DESTINATION: {
      SkPoint point = SkPoint::Make(op->rect.x(), op->rect.y());
      SkAnnotateNamedDestination(canvas, point, op->data.get());
      break;
    }
  }
}

void ClipPathOp::Raster(const ClipPathOp* op,
                        SkCanvas* canvas,
                        const PlaybackParams& params) {
  canvas->clipPath(op->path, op->op, op->antialias);
}

void ClipRectOp::Raster(const ClipRectOp* op,
                        SkCanvas* canvas,
                        const PlaybackParams& params) {
  canvas->clipRect(op->rect, op->op, op->antialias);
}

void ClipRRectOp::Raster(const ClipRRectOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  canvas->clipRRect(op->rrect, op->op, op->antialias);
}

void ConcatOp::Raster(const ConcatOp* op,
                      SkCanvas* canvas,
                      const PlaybackParams& params) {
  canvas->concat(op->matrix);
}

void CustomDataOp::Raster(const CustomDataOp* op,
                          SkCanvas* canvas,
                          const PlaybackParams& params) {
  if (params.custom_callback)
    params.custom_callback.Run(canvas, op->id);
}

void DrawColorOp::Raster(const DrawColorOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  canvas->drawColor(op->color, op->mode);
}

void DrawDRRectOp::RasterWithFlags(const DrawDRRectOp* op,
                                   const PaintFlags* flags,
                                   SkCanvas* canvas,
                                   const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawDRRect(op->outer, op->inner, p);
  });
}

void DrawImageOp::RasterWithFlags(const DrawImageOp* op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const PlaybackParams& params) {
  DCHECK(!op->image.IsPaintWorklet());
  SkPaint paint = flags ? flags->ToSkPaint() : SkPaint();

  if (!params.image_provider) {
    const bool needs_scale = !IsScaleAdjustmentIdentity(op->scale_adjustment);
    SkAutoCanvasRestore save_restore(canvas, needs_scale);
    if (needs_scale) {
      canvas->scale(1.f / op->scale_adjustment.width(),
                    1.f / op->scale_adjustment.height());
    }
    sk_sp<SkImage> sk_image;
    if (op->image.IsTextureBacked()) {
      sk_image = op->image.GetAcceleratedSkImage();
      DCHECK(sk_image || !canvas->recordingContext());
    }
    if (!sk_image)
      sk_image = op->image.GetSwSkImage();

    canvas->drawImage(sk_image.get(), op->left, op->top, op->sampling, &paint);
    return;
  }

  // Dark mode is applied only for OOP raster during serialization.
  DrawImage draw_image(
      op->image, false, SkIRect::MakeWH(op->image.width(), op->image.height()),
      sampling_to_quality(op->sampling), canvas->getLocalToDevice());
  auto scoped_result = params.image_provider->GetRasterContent(draw_image);
  if (!scoped_result)
    return;

  const auto& decoded_image = scoped_result.decoded_image();
  DCHECK(decoded_image.image());

  DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().width()));
  DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().height()));
  SkSize scale_adjustment = SkSize::Make(
      op->scale_adjustment.width() * decoded_image.scale_adjustment().width(),
      op->scale_adjustment.height() *
          decoded_image.scale_adjustment().height());
  const bool needs_scale = !IsScaleAdjustmentIdentity(scale_adjustment);
  SkAutoCanvasRestore save_restore(canvas, needs_scale);
  if (needs_scale) {
    canvas->scale(1.f / scale_adjustment.width(),
                  1.f / scale_adjustment.height());
  }
  canvas->drawImage(decoded_image.image().get(), op->left, op->top,
                    PaintFlags::FilterQualityToSkSamplingOptions(
                        decoded_image.filter_quality()),
                    &paint);
}

void DrawImageRectOp::RasterWithFlags(const DrawImageRectOp* op,
                                      const PaintFlags* flags,
                                      SkCanvas* canvas,
                                      const PlaybackParams& params) {
  // TODO(crbug.com/931704): make sure to support the case where paint worklet
  // generated images are used in other raster work such as canvas2d.
  if (op->image.IsPaintWorklet()) {
    // When rasterizing on the main thread (e.g. paint invalidation checking,
    // see https://crbug.com/990382), an image provider may not be available, so
    // we should draw nothing.
    if (!params.image_provider)
      return;
    ImageProvider::ScopedResult result =
        params.image_provider->GetRasterContent(DrawImage(op->image));

    // Check that we are not using loopers with paint worklets, since converting
    // PaintFlags to SkPaint drops loopers.
    DCHECK(!flags->getLooper());
    SkPaint paint = flags ? flags->ToSkPaint() : SkPaint();

    DCHECK(IsScaleAdjustmentIdentity(op->scale_adjustment));
    SkAutoCanvasRestore save_restore(canvas, true);
    canvas->concat(SkMatrix::RectToRect(op->src, op->dst));
    canvas->clipRect(op->src);
    canvas->saveLayer(&op->src, &paint);
    // Compositor thread animations can cause PaintWorklet jobs to be dispatched
    // to the worklet thread even after main has torn down the worklet (e.g.
    // because a navigation is happening). In that case the PaintWorklet jobs
    // will fail and there will be no result to raster here. This state is
    // transient as the next main frame commit will remove the PaintWorklets.
    if (result && result.paint_record())
      result.paint_record()->Playback(canvas, params);
    return;
  }

  if (!params.image_provider) {
    SkRect adjusted_src = AdjustSrcRectForScale(op->src, op->scale_adjustment);
    flags->DrawToSk(canvas, [op, adjusted_src](SkCanvas* c, const SkPaint& p) {
      sk_sp<SkImage> sk_image;
      if (op->image.IsTextureBacked()) {
        sk_image = op->image.GetAcceleratedSkImage();
        DCHECK(sk_image || !c->recordingContext());
      }
      if (!sk_image)
        sk_image = op->image.GetSwSkImage();
      DrawImageRect(c, sk_image.get(), adjusted_src, op->dst, op->sampling, &p,
                    op->constraint);
    });
    return;
  }

  SkM44 matrix = canvas->getLocalToDevice() *
                 SkM44(SkMatrix::RectToRect(op->src, op->dst));

  SkIRect int_src_rect;
  op->src.roundOut(&int_src_rect);

  // Dark mode is applied only for OOP raster during serialization.
  DrawImage draw_image(op->image, false, int_src_rect,
                       sampling_to_quality(op->sampling), matrix);
  auto scoped_result = params.image_provider->GetRasterContent(draw_image);
  if (!scoped_result)
    return;

  const auto& decoded_image = scoped_result.decoded_image();
  DCHECK(decoded_image.image());

  SkSize scale_adjustment = SkSize::Make(
      op->scale_adjustment.width() * decoded_image.scale_adjustment().width(),
      op->scale_adjustment.height() *
          decoded_image.scale_adjustment().height());
  SkRect adjusted_src =
      op->src.makeOffset(decoded_image.src_rect_offset().width(),
                         decoded_image.src_rect_offset().height());
  adjusted_src = AdjustSrcRectForScale(adjusted_src, scale_adjustment);
  flags->DrawToSk(canvas, [op, &decoded_image, adjusted_src](SkCanvas* c,
                                                             const SkPaint& p) {
    SkSamplingOptions options = PaintFlags::FilterQualityToSkSamplingOptions(
        decoded_image.filter_quality());
    DrawImageRect(c, decoded_image.image().get(), adjusted_src, op->dst,
                  options, &p, op->constraint);
  });
}

void DrawIRectOp::RasterWithFlags(const DrawIRectOp* op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawIRect(op->rect, p);
  });
}

void DrawLineOp::RasterWithFlags(const DrawLineOp* op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const PlaybackParams& params) {
  SkPaint paint = flags->ToSkPaint();
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawLine(op->x0, op->y0, op->x1, op->y1, p);
  });
}

void DrawOvalOp::RasterWithFlags(const DrawOvalOp* op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawOval(op->oval, p);
  });
}

void DrawPathOp::RasterWithFlags(const DrawPathOp* op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawPath(op->path, p);
  });
}

void DrawRecordOp::Raster(const DrawRecordOp* op,
                          SkCanvas* canvas,
                          const PlaybackParams& params) {
  // Don't use drawPicture here, as it adds an implicit clip.
  // TODO(enne): Temporary CHECK debugging for http://crbug.com/823835
  CHECK(op->record);
  op->record->Playback(canvas, params);
}

void DrawRectOp::RasterWithFlags(const DrawRectOp* op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawRect(op->rect, p);
  });
}

void DrawRRectOp::RasterWithFlags(const DrawRRectOp* op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawRRect(op->rrect, p);
  });
}

void DrawSkottieOp::Raster(const DrawSkottieOp* op,
                           SkCanvas* canvas,
                           const PlaybackParams& params) {
  // Binding unretained references in the callback is safe because Draw()'s API
  // guarantees that the callback is invoked synchronously.
  op->skottie->Draw(
      canvas, op->t, op->dst,
      base::BindRepeating(&DrawSkottieOp::GetImageAssetForRaster,
                          base::Unretained(op), canvas, std::cref(params)),
      op->color_map, op->text_map);
}

SkottieWrapper::FrameDataFetchResult DrawSkottieOp::GetImageAssetForRaster(
    SkCanvas* canvas,
    const PlaybackParams& params,
    SkottieResourceIdHash asset_id,
    float t_frame,
    sk_sp<SkImage>& sk_image,
    SkSamplingOptions& sampling_out) const {
  auto images_iter = images.find(asset_id);
  if (images_iter == images.end())
    return SkottieWrapper::FrameDataFetchResult::NO_UPDATE;

  const SkottieFrameData& frame_data = images_iter->second;
  if (!frame_data.image) {
    sk_image = nullptr;
  } else if (params.image_provider) {
    // There is no use case for applying dark mode filters to skottie images
    // currently.
    DrawImage draw_image(
        frame_data.image, /*use_dark_mode=*/false,
        SkIRect::MakeWH(frame_data.image.width(), frame_data.image.height()),
        frame_data.quality, canvas->getLocalToDevice());
    auto scoped_result = params.image_provider->GetRasterContent(draw_image);
    if (scoped_result) {
      sk_image = scoped_result.decoded_image().image();
      DCHECK(sk_image);
    }
  } else {
    if (frame_data.image.IsTextureBacked()) {
      sk_image = frame_data.image.GetAcceleratedSkImage();
      DCHECK(sk_image || !canvas->recordingContext());
    }
    if (!sk_image)
      sk_image = frame_data.image.GetSwSkImage();
  }
  sampling_out =
      PaintFlags::FilterQualityToSkSamplingOptions(frame_data.quality);
  return SkottieWrapper::FrameDataFetchResult::NEW_DATA_AVAILABLE;
}

void DrawTextBlobOp::RasterWithFlags(const DrawTextBlobOp* op,
                                     const PaintFlags* flags,
                                     SkCanvas* canvas,
                                     const PlaybackParams& params) {
  if (op->node_id)
    SkPDF::SetNodeId(canvas, op->node_id);

  // The PaintOpBuffer could be rasterized with different global matrix. It is
  // used for over scall on Android. So we cannot reuse slugs, they have to be
  // recreated.
  if (params.is_analyzing) {
    const_cast<DrawTextBlobOp*>(op)->slug.reset();
    const_cast<DrawTextBlobOp*>(op)->extra_slugs.clear();
  }

  // flags may contain SkDrawLooper for shadow effect, so we need to convert
  // SkTextBlob to slug for each run.
  size_t i = 0;
  flags->DrawToSk(canvas, [op, &params, &i](SkCanvas* c, const SkPaint& p) {
    if (op->blob) {
      c->drawTextBlob(op->blob.get(), op->x, op->y, p);
      if (params.is_analyzing) {
        auto s = GrSlug::ConvertBlob(c, *op->blob, {op->x, op->y}, p);
        if (i == 0) {
          const_cast<DrawTextBlobOp*>(op)->slug = std::move(s);
        } else {
          const_cast<DrawTextBlobOp*>(op)->extra_slugs.push_back(std::move(s));
        }
      }
    } else if (i < 1 + op->extra_slugs.size()) {
      DCHECK(!params.is_analyzing);
      const auto& draw_slug = i == 0 ? op->slug : op->extra_slugs[i - 1];
      if (draw_slug)
        draw_slug->draw(c);
    }
    ++i;
  });

  if (op->node_id)
    SkPDF::SetNodeId(canvas, 0);
}

void RestoreOp::Raster(const RestoreOp* op,
                       SkCanvas* canvas,
                       const PlaybackParams& params) {
  canvas->restore();
}

void RotateOp::Raster(const RotateOp* op,
                      SkCanvas* canvas,
                      const PlaybackParams& params) {
  canvas->rotate(op->degrees);
}

void SaveOp::Raster(const SaveOp* op,
                    SkCanvas* canvas,
                    const PlaybackParams& params) {
  canvas->save();
}

void SaveLayerOp::RasterWithFlags(const SaveLayerOp* op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const PlaybackParams& params) {
  // See PaintOp::kUnsetRect
  SkPaint paint = flags->ToSkPaint();
  bool unset = op->bounds.left() == SK_ScalarInfinity;
  canvas->saveLayer(unset ? nullptr : &op->bounds, &paint);
}

void SaveLayerAlphaOp::Raster(const SaveLayerAlphaOp* op,
                              SkCanvas* canvas,
                              const PlaybackParams& params) {
  // See PaintOp::kUnsetRect
  bool unset = op->bounds.left() == SK_ScalarInfinity;
  absl::optional<SkPaint> paint;
  if (op->alpha != 1.0f) {
    paint.emplace();
    paint->setAlpha(op->alpha * 255.0f);
  }
  SkCanvas::SaveLayerRec rec(unset ? nullptr : &op->bounds,
                             base::OptionalToPtr(paint));
  if (params.save_layer_alpha_should_preserve_lcd_text.has_value() &&
      *params.save_layer_alpha_should_preserve_lcd_text) {
    rec.fSaveLayerFlags = SkCanvas::kPreserveLCDText_SaveLayerFlag |
                          SkCanvas::kInitWithPrevious_SaveLayerFlag;
  }
  canvas->saveLayer(rec);
}

void ScaleOp::Raster(const ScaleOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {
  canvas->scale(op->sx, op->sy);
}

void SetMatrixOp::Raster(const SetMatrixOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  canvas->setMatrix(params.original_ctm * op->matrix);
}

void SetNodeIdOp::Raster(const SetNodeIdOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  SkPDF::SetNodeId(canvas, op->node_id);
}

void TranslateOp::Raster(const TranslateOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  canvas->translate(op->dx, op->dy);
}

// static
bool PaintOp::AreSkPointsEqual(const SkPoint& left, const SkPoint& right) {
  if (!AreEqualEvenIfNaN(left.fX, right.fX))
    return false;
  if (!AreEqualEvenIfNaN(left.fY, right.fY))
    return false;
  return true;
}

// static
bool PaintOp::AreSkPoint3sEqual(const SkPoint3& left, const SkPoint3& right) {
  if (!AreEqualEvenIfNaN(left.fX, right.fX))
    return false;
  if (!AreEqualEvenIfNaN(left.fY, right.fY))
    return false;
  if (!AreEqualEvenIfNaN(left.fZ, right.fZ))
    return false;
  return true;
}

// static
bool PaintOp::AreSkRectsEqual(const SkRect& left, const SkRect& right) {
  if (!AreEqualEvenIfNaN(left.fLeft, right.fLeft))
    return false;
  if (!AreEqualEvenIfNaN(left.fTop, right.fTop))
    return false;
  if (!AreEqualEvenIfNaN(left.fRight, right.fRight))
    return false;
  if (!AreEqualEvenIfNaN(left.fBottom, right.fBottom))
    return false;
  return true;
}

// static
bool PaintOp::AreSkRRectsEqual(const SkRRect& left, const SkRRect& right) {
  char left_buffer[SkRRect::kSizeInMemory];
  left.writeToMemory(left_buffer);
  char right_buffer[SkRRect::kSizeInMemory];
  right.writeToMemory(right_buffer);
  return !memcmp(left_buffer, right_buffer, SkRRect::kSizeInMemory);
}

// static
bool PaintOp::AreSkMatricesEqual(const SkMatrix& left, const SkMatrix& right) {
  for (int i = 0; i < 9; ++i) {
    if (!AreEqualEvenIfNaN(left.get(i), right.get(i)))
      return false;
  }

  // If a serialized matrix says it is identity, then the original must have
  // those values, as the serialization process clobbers the matrix values.
  if (left.isIdentity()) {
    if (SkMatrix::I() != left)
      return false;
    if (SkMatrix::I() != right)
      return false;
  }

  if (left.getType() != right.getType())
    return false;

  return true;
}

// static
bool PaintOp::AreSkM44sEqual(const SkM44& left, const SkM44& right) {
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      if (!AreEqualEvenIfNaN(left.rc(r, c), right.rc(r, c)))
        return false;
    }
  }

  return true;
}

// static
bool PaintOp::AreSkFlattenablesEqual(SkFlattenable* left,
                                     SkFlattenable* right) {
  if (!right || !left)
    return !right && !left;

  sk_sp<SkData> left_data = left->serialize();
  sk_sp<SkData> right_data = right->serialize();
  if (left_data->size() != right_data->size())
    return false;
  if (!left_data->equals(right_data.get()))
    return false;
  return true;
}

bool AnnotateOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const AnnotateOp*>(base_left);
  auto* right = static_cast<const AnnotateOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->annotation_type != right->annotation_type)
    return false;
  if (!AreSkRectsEqual(left->rect, right->rect))
    return false;
  if (!left->data != !right->data)
    return false;
  if (left->data) {
    if (left->data->size() != right->data->size())
      return false;
    if (0 !=
        memcmp(left->data->data(), right->data->data(), right->data->size()))
      return false;
  }
  return true;
}

bool ClipPathOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const ClipPathOp*>(base_left);
  auto* right = static_cast<const ClipPathOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->path != right->path)
    return false;
  if (left->op != right->op)
    return false;
  if (left->antialias != right->antialias)
    return false;
  return true;
}

bool ClipRectOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const ClipRectOp*>(base_left);
  auto* right = static_cast<const ClipRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreSkRectsEqual(left->rect, right->rect))
    return false;
  if (left->op != right->op)
    return false;
  if (left->antialias != right->antialias)
    return false;
  return true;
}

bool ClipRRectOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const ClipRRectOp*>(base_left);
  auto* right = static_cast<const ClipRRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreSkRRectsEqual(left->rrect, right->rrect))
    return false;
  if (left->op != right->op)
    return false;
  if (left->antialias != right->antialias)
    return false;
  return true;
}

bool ConcatOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const ConcatOp*>(base_left);
  auto* right = static_cast<const ConcatOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  return AreSkM44sEqual(left->matrix, right->matrix);
}

bool CustomDataOp::AreEqual(const PaintOp* base_left,
                            const PaintOp* base_right) {
  auto* left = static_cast<const CustomDataOp*>(base_left);
  auto* right = static_cast<const CustomDataOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  return left->id == right->id;
}

bool DrawColorOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const DrawColorOp*>(base_left);
  auto* right = static_cast<const DrawColorOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  return left->color == right->color;
}

bool DrawDRRectOp::AreEqual(const PaintOp* base_left,
                            const PaintOp* base_right) {
  auto* left = static_cast<const DrawDRRectOp*>(base_left);
  auto* right = static_cast<const DrawDRRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreSkRRectsEqual(left->outer, right->outer))
    return false;
  if (!AreSkRRectsEqual(left->inner, right->inner))
    return false;
  return true;
}

bool DrawImageOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const DrawImageOp*>(base_left);
  auto* right = static_cast<const DrawImageOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  // TODO(enne): Test PaintImage equality once implemented
  if (!AreEqualEvenIfNaN(left->left, right->left))
    return false;
  if (!AreEqualEvenIfNaN(left->top, right->top))
    return false;

  // scale_adjustment intentionally omitted because it is added during
  // serialization based on raster scale.
  return true;
}

bool DrawImageRectOp::AreEqual(const PaintOp* base_left,
                               const PaintOp* base_right) {
  auto* left = static_cast<const DrawImageRectOp*>(base_left);
  auto* right = static_cast<const DrawImageRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  // TODO(enne): Test PaintImage equality once implemented
  if (!AreSkRectsEqual(left->src, right->src))
    return false;
  if (!AreSkRectsEqual(left->dst, right->dst))
    return false;

  // scale_adjustment intentionally omitted because it is added during
  // serialization based on raster scale.
  return true;
}

bool DrawIRectOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const DrawIRectOp*>(base_left);
  auto* right = static_cast<const DrawIRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (left->rect != right->rect)
    return false;
  return true;
}

bool DrawLineOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const DrawLineOp*>(base_left);
  auto* right = static_cast<const DrawLineOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreEqualEvenIfNaN(left->x0, right->x0))
    return false;
  if (!AreEqualEvenIfNaN(left->y0, right->y0))
    return false;
  if (!AreEqualEvenIfNaN(left->x1, right->x1))
    return false;
  if (!AreEqualEvenIfNaN(left->y1, right->y1))
    return false;
  return true;
}

bool DrawOvalOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const DrawOvalOp*>(base_left);
  auto* right = static_cast<const DrawOvalOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreSkRectsEqual(left->oval, right->oval))
    return false;
  return true;
}

bool DrawPathOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const DrawPathOp*>(base_left);
  auto* right = static_cast<const DrawPathOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (left->path != right->path)
    return false;
  return true;
}

bool DrawRecordOp::AreEqual(const PaintOp* base_left,
                            const PaintOp* base_right) {
  auto* left = static_cast<const DrawRecordOp*>(base_left);
  auto* right = static_cast<const DrawRecordOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!left->record != !right->record)
    return false;
  if (*left->record != *right->record)
    return false;
  return true;
}

bool DrawRectOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const DrawRectOp*>(base_left);
  auto* right = static_cast<const DrawRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreSkRectsEqual(left->rect, right->rect))
    return false;
  return true;
}

bool DrawRRectOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const DrawRRectOp*>(base_left);
  auto* right = static_cast<const DrawRRectOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreSkRRectsEqual(left->rrect, right->rrect))
    return false;
  return true;
}

bool DrawSkottieOp::AreEqual(const PaintOp* base_left,
                             const PaintOp* base_right) {
  auto* left = static_cast<const DrawSkottieOp*>(base_left);
  auto* right = static_cast<const DrawSkottieOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  // TODO(malaykeshav): Verify the skottie objects of each PaintOb are equal
  // based on the serialized bytes.
  if (left->t != right->t)
    return false;
  if (!AreSkRectsEqual(left->dst, right->dst))
    return false;
  if (left->images.size() != right->images.size())
    return false;

  auto left_iter = left->images.begin();
  auto right_iter = right->images.begin();
  for (; left_iter != left->images.end(); ++left_iter, ++right_iter) {
    if (left_iter->first != right_iter->first ||
        // PaintImage's comparison operator compares the underlying SkImage's
        // pointer address. This does not necessarily hold in cases where the
        // image's content may be the same, but it got realloacted to a
        // different spot somewhere in memory via the transfer cache. The next
        // best thing is to just compare the dimensions of the PaintImage.
        left_iter->second.image.width() != right_iter->second.image.width() ||
        left_iter->second.image.height() != right_iter->second.image.height() ||
        left_iter->second.quality != right_iter->second.quality) {
      return false;
    }
  }

  if (left->color_map != right->color_map)
    return false;

  if (left->text_map != right->text_map)
    return false;

  return true;
}

bool DrawTextBlobOp::AreEqual(const PaintOp* base_left,
                              const PaintOp* base_right) {
  auto* left = static_cast<const DrawTextBlobOp*>(base_left);
  auto* right = static_cast<const DrawTextBlobOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreEqualEvenIfNaN(left->x, right->x))
    return false;
  if (!AreEqualEvenIfNaN(left->y, right->y))
    return false;
  if (left->node_id != right->node_id)
    return false;
  return GrSlugAreEqual(left->slug, right->slug);
}

bool NoopOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  return true;
}

bool RestoreOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  return true;
}

bool RotateOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const RotateOp*>(base_left);
  auto* right = static_cast<const RotateOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreEqualEvenIfNaN(left->degrees, right->degrees))
    return false;
  return true;
}

bool SaveOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  return true;
}

bool SaveLayerOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const SaveLayerOp*>(base_left);
  auto* right = static_cast<const SaveLayerOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (left->flags != right->flags)
    return false;
  if (!AreSkRectsEqual(left->bounds, right->bounds))
    return false;
  return true;
}

bool SaveLayerAlphaOp::AreEqual(const PaintOp* base_left,
                                const PaintOp* base_right) {
  auto* left = static_cast<const SaveLayerAlphaOp*>(base_left);
  auto* right = static_cast<const SaveLayerAlphaOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreSkRectsEqual(left->bounds, right->bounds))
    return false;
  if (left->alpha != right->alpha)
    return false;
  return true;
}

bool ScaleOp::AreEqual(const PaintOp* base_left, const PaintOp* base_right) {
  auto* left = static_cast<const ScaleOp*>(base_left);
  auto* right = static_cast<const ScaleOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreEqualEvenIfNaN(left->sx, right->sx))
    return false;
  if (!AreEqualEvenIfNaN(left->sy, right->sy))
    return false;
  return true;
}

bool SetMatrixOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const SetMatrixOp*>(base_left);
  auto* right = static_cast<const SetMatrixOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreSkM44sEqual(left->matrix, right->matrix))
    return false;
  return true;
}

bool SetNodeIdOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const SetNodeIdOp*>(base_left);
  auto* right = static_cast<const SetNodeIdOp*>(base_right);

  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  return left->node_id == right->node_id;
}

bool TranslateOp::AreEqual(const PaintOp* base_left,
                           const PaintOp* base_right) {
  auto* left = static_cast<const TranslateOp*>(base_left);
  auto* right = static_cast<const TranslateOp*>(base_right);
  DCHECK(left->IsValid());
  DCHECK(right->IsValid());
  if (!AreEqualEvenIfNaN(left->dx, right->dx))
    return false;
  if (!AreEqualEvenIfNaN(left->dy, right->dy))
    return false;
  return true;
}

bool PaintOp::IsDrawOp() const {
  return g_is_draw_op[type];
}

bool PaintOp::IsPaintOpWithFlags() const {
  return g_has_paint_flags[type];
}

bool PaintOp::operator==(const PaintOp& other) const {
  if (GetType() != other.GetType())
    return false;
  return g_equals_operator[type](this, &other);
}

// static
bool PaintOp::TypeHasFlags(PaintOpType type) {
  return g_has_paint_flags[static_cast<uint8_t>(type)];
}

void PaintOp::Raster(SkCanvas* canvas, const PlaybackParams& params) const {
  g_raster_functions[type](this, canvas, params);
}

size_t PaintOp::Serialize(void* memory,
                          size_t size,
                          const SerializeOptions& options,
                          const PaintFlags* flags_to_serialize,
                          const SkM44& current_ctm,
                          const SkM44& original_ctm) const {
  // Need at least enough room for a skip/type header.
  if (size < 4)
    return 0u;

  DCHECK_EQ(0u,
            reinterpret_cast<uintptr_t>(memory) % PaintOpBuffer::kPaintOpAlign);

  size_t written = g_serialize_functions[type](this, memory, size, options,
                                               flags_to_serialize, current_ctm,
                                               original_ctm);
  DCHECK_LE(written, size);
  if (written < 4)
    return 0u;

  size_t aligned_written = ((written + PaintOpBuffer::kPaintOpAlign - 1) &
                            ~(PaintOpBuffer::kPaintOpAlign - 1));
  if (aligned_written >= kMaxSkip)
    return 0u;
  if (aligned_written > size)
    return 0u;

  // Update skip and type now that the size is known.
  uint32_t bytes_to_skip = static_cast<uint32_t>(aligned_written);
  static_cast<uint32_t*>(memory)[0] = type | bytes_to_skip << 8;
  return bytes_to_skip;
}

PaintOp* PaintOp::Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              size_t* read_bytes,
                              const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(LargestPaintOp));

  uint8_t type;
  uint32_t skip;
  if (!PaintOpReader::ReadAndValidateOpHeader(input, input_size, &type, &skip))
    return nullptr;

  *read_bytes = skip;
  return g_deserialize_functions[type](input, skip, output, output_size,
                                       options);
}

PaintOp* PaintOp::DeserializeIntoPaintOpBuffer(
    const volatile void* input,
    size_t input_size,
    PaintOpBuffer* buffer,
    size_t* read_bytes,
    const DeserializeOptions& options) {
  uint8_t type;
  uint32_t skip;
  if (!PaintOpReader::ReadAndValidateOpHeader(input, input_size, &type,
                                              &skip)) {
    return nullptr;
  }

  size_t op_skip = PaintOpBuffer::ComputeOpSkip(g_type_to_size[type]);
  if (auto* op = g_deserialize_functions[type](
          input, skip, buffer->AllocatePaintOp(op_skip), op_skip, options)) {
    g_analyze_op_functions[type](buffer, op);
    *read_bytes = skip;
    return op;
  }

  // The last allocated op has already been destroyed if it failed to
  // deserialize. Update the buffer's op tracking to exclude it to avoid
  // access during cleanup at destruction.
  buffer->used_ -= op_skip;
  buffer->op_count_--;
  return nullptr;
}

// static
bool PaintOp::GetBounds(const PaintOp& op, SkRect* rect) {
  DCHECK(op.IsDrawOp());

  switch (op.GetType()) {
    case PaintOpType::DrawColor:
      return false;
    case PaintOpType::DrawDRRect: {
      const auto& rect_op = static_cast<const DrawDRRectOp&>(op);
      *rect = rect_op.outer.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawImage: {
      const auto& image_op = static_cast<const DrawImageOp&>(op);
      *rect = SkRect::MakeXYWH(image_op.left, image_op.top,
                               image_op.image.width(), image_op.image.height());
      rect->sort();
      return true;
    }
    case PaintOpType::DrawImageRect: {
      const auto& image_rect_op = static_cast<const DrawImageRectOp&>(op);
      *rect = image_rect_op.dst;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawIRect: {
      const auto& rect_op = static_cast<const DrawIRectOp&>(op);
      *rect = SkRect::Make(rect_op.rect);
      rect->sort();
      return true;
    }
    case PaintOpType::DrawLine: {
      const auto& line_op = static_cast<const DrawLineOp&>(op);
      rect->setLTRB(line_op.x0, line_op.y0, line_op.x1, line_op.y1);
      rect->sort();
      return true;
    }
    case PaintOpType::DrawOval: {
      const auto& oval_op = static_cast<const DrawOvalOp&>(op);
      *rect = oval_op.oval;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawPath: {
      const auto& path_op = static_cast<const DrawPathOp&>(op);
      *rect = path_op.path.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRect: {
      const auto& rect_op = static_cast<const DrawRectOp&>(op);
      *rect = rect_op.rect;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRRect: {
      const auto& rect_op = static_cast<const DrawRRectOp&>(op);
      *rect = rect_op.rrect.rect();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRecord:
      return false;
    case PaintOpType::DrawSkottie: {
      const auto& skottie_op = static_cast<const DrawSkottieOp&>(op);
      *rect = skottie_op.dst;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawTextBlob: {
      const auto& text_op = static_cast<const DrawTextBlobOp&>(op);
      *rect = text_op.blob->bounds().makeOffset(text_op.x, text_op.y);
      rect->sort();
      return true;
    }
    default:
      NOTREACHED();
  }
  return false;
}

// static
gfx::Rect PaintOp::ComputePaintRect(const PaintOp& op,
                                    const SkRect& clip_rect,
                                    const SkMatrix& ctm) {
  gfx::Rect transformed_rect;
  SkRect op_rect;
  if (!op.IsDrawOp() || !PaintOp::GetBounds(op, &op_rect)) {
    // If we can't provide a conservative bounding rect for the op, assume it
    // covers the complete current clip.
    // TODO(khushalsagar): See if we can do something better for non-draw ops.
    transformed_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(clip_rect));
  } else {
    const PaintFlags* flags =
        op.IsPaintOpWithFlags()
            ? &(static_cast<const PaintOpWithFlags&>(op).flags)
            : nullptr;
    SkRect paint_rect = MapRect(ctm, op_rect);
    if (flags) {
      SkPaint paint = flags->ToSkPaint();
      paint_rect = paint.canComputeFastBounds() && paint_rect.isFinite()
                       ? paint.computeFastBounds(paint_rect, &paint_rect)
                       : clip_rect;
    }
    // Clamp the image rect by the current clip rect.
    if (!paint_rect.intersect(clip_rect))
      return gfx::Rect();

    transformed_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(paint_rect));
  }

  // During raster, we use the device clip bounds on the canvas, which outsets
  // the actual clip by 1 due to the possibility of antialiasing. Account for
  // this here by outsetting the image rect by 1. Note that this only affects
  // queries into the rtree, which will now return images that only touch the
  // bounds of the query rect.
  //
  // Note that it's not sufficient for us to inset the device clip bounds at
  // raster time, since we might be sending a larger-than-one-item display
  // item to skia, which means that skia will internally determine whether to
  // raster the picture (using device clip bounds that are outset).
  transformed_rect.Inset(-1);
  return transformed_rect;
}

// static
bool PaintOp::QuickRejectDraw(const PaintOp& op, const SkCanvas* canvas) {
  if (!op.IsDrawOp())
    return false;

  SkRect rect;
  if (!PaintOp::GetBounds(op, &rect))
    return false;
  if (!rect.isFinite())
    return true;

  if (op.IsPaintOpWithFlags()) {
    SkPaint paint = static_cast<const PaintOpWithFlags&>(op).flags.ToSkPaint();
    if (!paint.canComputeFastBounds())
      return false;
    // canvas->quickReject tried to be very fast, and sometimes give a false
    // but conservative result. That's why we need the additional check for
    // |local_op_rect| because it could quickReject could return false even if
    // |local_op_rect| is empty.
    const SkRect& clip_rect = SkRect::Make(canvas->getDeviceClipBounds());
    const SkMatrix& ctm = canvas->getTotalMatrix();
    gfx::Rect local_op_rect = PaintOp::ComputePaintRect(op, clip_rect, ctm);
    if (local_op_rect.IsEmpty())
      return true;
    paint.computeFastBounds(rect, &rect);
  }

  return canvas->quickReject(rect);
}

// static
bool PaintOp::OpHasDiscardableImages(const PaintOp& op) {
  if (op.IsPaintOpWithFlags() && static_cast<const PaintOpWithFlags&>(op)
                                     .HasDiscardableImagesFromFlags()) {
    return true;
  }

  if (op.GetType() == PaintOpType::DrawImage &&
      static_cast<const DrawImageOp&>(op).HasDiscardableImages()) {
    return true;
  } else if (op.GetType() == PaintOpType::DrawImageRect &&
             static_cast<const DrawImageRectOp&>(op).HasDiscardableImages()) {
    return true;
  } else if (op.GetType() == PaintOpType::DrawRecord &&
             static_cast<const DrawRecordOp&>(op).HasDiscardableImages()) {
    return true;
  } else if (op.GetType() == PaintOpType::DrawSkottie &&
             static_cast<const DrawSkottieOp&>(op).HasDiscardableImages()) {
    return true;
  }

  return false;
}

void PaintOp::DestroyThis() {
  auto func = g_destructor_functions[type];
  if (func)
    func(this);
}

bool PaintOpWithFlags::HasDiscardableImagesFromFlags() const {
  return flags.HasDiscardableImages();
}

void PaintOpWithFlags::RasterWithFlags(SkCanvas* canvas,
                                       const PaintFlags* raster_flags,
                                       const PlaybackParams& params) const {
  g_raster_with_flags_functions[type](this, raster_flags, canvas, params);
}

int ClipPathOp::CountSlowPaths() const {
  return antialias && !path.isConvex() ? 1 : 0;
}

int DrawLineOp::CountSlowPaths() const {
  if (const SkPathEffect* effect = flags.getPathEffect().get()) {
    SkPathEffect::DashInfo info;
    SkPathEffect::DashType dashType = effect->asADash(&info);
    if (flags.getStrokeCap() != PaintFlags::kRound_Cap &&
        dashType == SkPathEffect::kDash_DashType && info.fCount == 2) {
      // The PaintFlags will count this as 1, so uncount that here as
      // this kind of line is special cased and not slow.
      return -1;
    }
  }
  return 0;
}

int DrawPathOp::CountSlowPaths() const {
  // This logic is copied from SkPathCounter instead of attempting to expose
  // that from Skia.
  if (!flags.isAntiAlias() || path.isConvex())
    return 0;

  PaintFlags::Style paintStyle = flags.getStyle();
  const SkRect& pathBounds = path.getBounds();
  if (paintStyle == PaintFlags::kStroke_Style && flags.getStrokeWidth() == 0) {
    // AA hairline concave path is not slow.
    return 0;
  } else if (paintStyle == PaintFlags::kFill_Style &&
             pathBounds.width() < 64.f && pathBounds.height() < 64.f &&
             !path.isVolatile()) {
    // AADF eligible concave path is not slow.
    return 0;
  } else {
    return 1;
  }
}

int DrawRecordOp::CountSlowPaths() const {
  return record->num_slow_paths_up_to_min_for_MSAA();
}

bool DrawRecordOp::HasNonAAPaint() const {
  return record->HasNonAAPaint();
}

bool DrawRecordOp::HasDrawTextOps() const {
  return record->has_draw_text_ops();
}

bool DrawRecordOp::HasSaveLayerOps() const {
  return record->has_save_layer_ops();
}

bool DrawRecordOp::HasSaveLayerAlphaOps() const {
  return record->has_save_layer_alpha_ops();
}

bool DrawRecordOp::HasEffectsPreventingLCDTextForSaveLayerAlpha() const {
  return record->has_effects_preventing_lcd_text_for_save_layer_alpha();
}

AnnotateOp::AnnotateOp() : PaintOp(kType) {}

AnnotateOp::AnnotateOp(PaintCanvas::AnnotationType annotation_type,
                       const SkRect& rect,
                       sk_sp<SkData> data)
    : PaintOp(kType),
      annotation_type(annotation_type),
      rect(rect),
      data(std::move(data)) {}

AnnotateOp::~AnnotateOp() = default;
AnnotateOp::AnnotateOp(const AnnotateOp&) = default;
AnnotateOp& AnnotateOp::operator=(const AnnotateOp&) = default;

DrawImageOp::DrawImageOp() : PaintOpWithFlags(kType) {}

DrawImageOp::DrawImageOp(const PaintImage& image, SkScalar left, SkScalar top)
    : PaintOpWithFlags(kType, PaintFlags()),
      image(image),
      left(left),
      top(top) {}

DrawImageOp::DrawImageOp(const PaintImage& image,
                         SkScalar left,
                         SkScalar top,
                         const SkSamplingOptions& sampling,
                         const PaintFlags* flags)
    : PaintOpWithFlags(kType, flags ? *flags : PaintFlags()),
      image(image),
      left(left),
      top(top),
      sampling(sampling) {}

bool DrawImageOp::HasDiscardableImages() const {
  return image && !image.IsTextureBacked();
}

DrawImageOp::~DrawImageOp() = default;

DrawImageRectOp::DrawImageRectOp() : PaintOpWithFlags(kType) {}

DrawImageRectOp::DrawImageRectOp(const PaintImage& image,
                                 const SkRect& src,
                                 const SkRect& dst,
                                 SkCanvas::SrcRectConstraint constraint)
    : PaintOpWithFlags(kType, PaintFlags()),
      image(image),
      src(src),
      dst(dst),
      constraint(constraint) {}

DrawImageRectOp::DrawImageRectOp(const PaintImage& image,
                                 const SkRect& src,
                                 const SkRect& dst,
                                 const SkSamplingOptions& sampling,
                                 const PaintFlags* flags,
                                 SkCanvas::SrcRectConstraint constraint)
    : PaintOpWithFlags(kType, flags ? *flags : PaintFlags()),
      image(image),
      src(src),
      dst(dst),
      sampling(sampling),
      constraint(constraint) {}

bool DrawImageRectOp::HasDiscardableImages() const {
  return image && !image.IsTextureBacked();
}

DrawImageRectOp::~DrawImageRectOp() = default;

DrawRecordOp::DrawRecordOp(sk_sp<const PaintRecord> record)
    : PaintOp(kType), record(std::move(record)) {}

DrawRecordOp::~DrawRecordOp() = default;
DrawRecordOp::DrawRecordOp(const DrawRecordOp&) = default;
DrawRecordOp& DrawRecordOp::operator=(const DrawRecordOp&) = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record->bytes_used();
}

size_t DrawRecordOp::AdditionalOpCount() const {
  return record->total_op_count();
}

DrawSkottieOp::DrawSkottieOp(scoped_refptr<SkottieWrapper> skottie,
                             SkRect dst,
                             float t,
                             SkottieFrameDataMap images,
                             const SkottieColorMap& color_map,
                             SkottieTextPropertyValueMap text_map)
    : PaintOp(kType),
      skottie(std::move(skottie)),
      dst(dst),
      t(t),
      images(std::move(images)),
      color_map(color_map),
      text_map(std::move(text_map)) {}

DrawSkottieOp::DrawSkottieOp() : PaintOp(kType) {}

DrawSkottieOp::~DrawSkottieOp() = default;
DrawSkottieOp::DrawSkottieOp(const DrawSkottieOp&) = default;
DrawSkottieOp& DrawSkottieOp::operator=(const DrawSkottieOp&) = default;

bool DrawSkottieOp::HasDiscardableImages() const {
  return !images.empty();
}

bool DrawRecordOp::HasDiscardableImages() const {
  return record->HasDiscardableImages();
}

DrawTextBlobOp::DrawTextBlobOp() : PaintOpWithFlags(kType) {}

DrawTextBlobOp::DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                               SkScalar x,
                               SkScalar y,
                               const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags), blob(std::move(blob)), x(x), y(y) {}

DrawTextBlobOp::DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                               SkScalar x,
                               SkScalar y,
                               NodeId node_id,
                               const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags),
      blob(std::move(blob)),
      x(x),
      y(y),
      node_id(node_id) {}

DrawTextBlobOp::~DrawTextBlobOp() = default;
DrawTextBlobOp::DrawTextBlobOp(const DrawTextBlobOp&) = default;
DrawTextBlobOp& DrawTextBlobOp::operator=(const DrawTextBlobOp&) = default;

}  // namespace cc
