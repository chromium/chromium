// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
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
  M(DrawSlugOp)       \
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

#define M(T) PaintOpBuffer::ComputeOpAlignedSize<T>(),
static constexpr uint16_t g_type_to_aligned_size[kNumOpTypes] = {TYPES(M)};
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

using SerializeFunction = void (*)(const PaintOp& op,
                                   PaintOpWriter& writer,
                                   const PaintFlags* flags_to_serialize,
                                   const SkM44& current_ctm,
                                   const SkM44& original_ctm);
template <typename T>
void Serialize(const PaintOp& op,
               PaintOpWriter& writer,
               const PaintFlags* flags_to_serialize,
               const SkM44& current_ctm,
               const SkM44& original_ctm) {
  if (T::kHasPaintFlags && !flags_to_serialize) {
    const auto& op_with_flags = static_cast<const PaintOpWithFlags&>(op);
    flags_to_serialize = &op_with_flags.flags;
  }
  const T& op_t = static_cast<const T&>(op);
  DCHECK(op_t.IsValid());
  op_t.Serialize(writer, flags_to_serialize, current_ctm, original_ctm);
}
#define M(T) &Serialize<T>,
static const SerializeFunction g_serialize_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using DeserializeFunction = PaintOp* (*)(PaintOpReader& reader,
                                         void* output,
                                         size_t output_size);
template <typename T>
PaintOp* Deserialize(PaintOpReader& reader, void* output, size_t output_size) {
  DCHECK_GE(output_size, sizeof(T));
  T* op = static_cast<T*>(T::Deserialize(reader, output));
  if (!op) {
    return nullptr;
  }
  if (!reader.valid() || !op->IsValid()) {
    op->~T();
    return nullptr;
  }
  op->aligned_size = PaintOpBuffer::ComputeOpAlignedSize<T>();
  return op;
}
#define M(T) &Deserialize<T>,
static const DeserializeFunction g_deserialize_functions[kNumOpTypes] = {
    TYPES(M)};
#undef M

using AreEqualForTestingFunction = bool (*)(const PaintOp&, const PaintOp&);
template <typename T>
bool AreEqualForTesting(const PaintOp& a, const PaintOp& b) {
  return static_cast<const T&>(a).EqualsForTesting(  // IN-TEST
      static_cast<const T&>(b));
}
#define M(T) &AreEqualForTesting<T>,
static const AreEqualForTestingFunction
    g_equal_for_testing_functions[kNumOpTypes] = {TYPES(M)};
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

}  // namespace

const SkRect PaintOp::kUnsetRect = {SK_ScalarInfinity, 0, 0, 0};

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
    case PaintOpType::DrawSlug:
      return "DrawSlug";
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

void AnnotateOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(annotation_type);
  writer.Write(rect);
  writer.Write(data);
}

void ClipPathOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(path, use_cache);
  writer.Write(op);
  writer.Write(antialias);
}

void ClipRectOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(rect);
  writer.Write(op);
  writer.Write(antialias);
}

void ClipRRectOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(rrect);
  writer.Write(op);
  writer.Write(antialias);
}

void ConcatOp::Serialize(PaintOpWriter& writer,
                         const PaintFlags* flags_to_serialize,
                         const SkM44& current_ctm,
                         const SkM44& original_ctm) const {
  writer.Write(matrix);
}

void CustomDataOp::Serialize(PaintOpWriter& writer,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) const {
  writer.Write(id);
}

void DrawColorOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(color);
  writer.Write(mode);
}

void DrawDRRectOp::Serialize(PaintOpWriter& writer,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(outer);
  writer.Write(inner);
}

void DrawImageOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);

  SkSize serialized_scale_adjustment = SkSize::Make(1.f, 1.f);
  writer.Write(
      CreateDrawImage(image, flags_to_serialize, sampling, current_ctm),
      &serialized_scale_adjustment);
  writer.Write(serialized_scale_adjustment.width());
  writer.Write(serialized_scale_adjustment.height());

  writer.Write(left);
  writer.Write(top);
  writer.Write(sampling);
}

void DrawImageRectOp::Serialize(PaintOpWriter& writer,
                                const PaintFlags* flags_to_serialize,
                                const SkM44& current_ctm,
                                const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);

  // This adjustment mirrors DiscardableImageMap::GatherDiscardableImage logic.
  SkM44 matrix = current_ctm * SkM44(SkMatrix::RectToRect(src, dst));
  // Note that we don't request subsets here since the GpuImageCache has no
  // optimizations for using subsets.
  SkSize serialized_scale_adjustment = SkSize::Make(1.f, 1.f);
  writer.Write(CreateDrawImage(image, flags_to_serialize, sampling, matrix),
               &serialized_scale_adjustment);
  writer.Write(serialized_scale_adjustment.width());
  writer.Write(serialized_scale_adjustment.height());

  writer.Write(src);
  writer.Write(dst);
  writer.Write(sampling);
  writer.Write(constraint);
}

void DrawIRectOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(rect);
}

void DrawLineOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(x0);
  writer.Write(y0);
  writer.Write(x1);
  writer.Write(y1);
  writer.Write(draw_as_path);
}

void DrawOvalOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(oval);
}

void DrawPathOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(path, use_cache);
  writer.Write(sk_path_fill_type);
}

void DrawRecordOp::Serialize(PaintOpWriter& writer,
                             const PaintFlags* flags_to_serialize,
                             const SkM44& current_ctm,
                             const SkM44& original_ctm) const {
  // These are flattened in PaintOpBufferSerializer.
  NOTREACHED();
}

void DrawRectOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(rect);
}

void DrawRRectOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(rrect);
}

namespace {

template <typename T>
void SerializeSkottieMap(
    const base::flat_map<SkottieResourceIdHash, T>& map,
    PaintOpWriter& writer,
    base::FunctionRef<void(const T&, PaintOpWriter&)> value_serializer) {
  // Write the size of the map first so that we know how many entries to read
  // from the buffer during deserialization.
  writer.WriteSize(map.size());
  for (const auto& [resource_id, val] : map) {
    writer.WriteSize(resource_id.GetUnsafeValue());
    value_serializer(val, writer);
  }
}

void SerializeSkottieFrameData(const SkM44& current_ctm,
                               const SkottieFrameData& frame_data,
                               PaintOpWriter& writer) {
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
  writer.Write(draw_image, &scale_adjustment);
  writer.Write(frame_data.quality);
}

}  // namespace

void DrawSkottieOp::Serialize(PaintOpWriter& writer,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) const {
  writer.Write(dst);
  writer.Write(SkFloatToScalar(t));
  writer.Write(skottie);

  SkottieFrameDataMap images_to_serialize = images;
  SkottieTextPropertyValueMap text_map_to_serialize = text_map;
  if (writer.options().skottie_serialization_history) {
    writer.options().skottie_serialization_history->FilterNewSkottieFrameState(
        *skottie, images_to_serialize, text_map_to_serialize);
  }

  SerializeSkottieMap<SkottieFrameData>(
      images_to_serialize, writer,
      [&current_ctm](const SkottieFrameData& frame_data,
                     PaintOpWriter& writer) {
        SerializeSkottieFrameData(current_ctm, frame_data, writer);
      });
  SerializeSkottieMap<SkColor>(
      color_map, writer,
      [](const SkColor& color, PaintOpWriter& writer) { writer.Write(color); });
  SerializeSkottieMap<SkottieTextPropertyValue>(
      text_map_to_serialize, writer,
      [](const SkottieTextPropertyValue& text_property_val,
         PaintOpWriter& writer) {
        writer.WriteSize(text_property_val.text().size());
        // If there is not enough space in the underlying buffer, WriteData()
        // will mark the |helper| as invalid and the buffer will keep growing
        // until a max size is reached (currently 64MB which should be ample for
        // text).
        writer.WriteData(text_property_val.text().size(),
                         text_property_val.text().c_str());
        writer.Write(gfx::RectFToSkRect(text_property_val.box()));
      });
}

void DrawSlugOp::SerializeSlugs(const sk_sp<GrSlug>& slug,
                                const std::vector<sk_sp<GrSlug>>& extra_slugs,
                                PaintOpWriter& writer,
                                const PaintFlags* flags_to_serialize,
                                const SkM44& current_ctm) {
  writer.Write(*flags_to_serialize, current_ctm);
  unsigned int count = extra_slugs.size() + 1;
  writer.Write(count);
  writer.Write(slug);
  for (const auto& extra_slug : extra_slugs) {
    writer.Write(extra_slug);
  }
}

void DrawSlugOp::Serialize(PaintOpWriter& writer,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm) const {
  DrawSlugOp::SerializeSlugs(slug, extra_slugs, writer, flags_to_serialize,
                             current_ctm);
}

void DrawTextBlobOp::Serialize(PaintOpWriter& writer,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) const {
  DrawSlugOp::SerializeSlugs(slug, extra_slugs, writer, flags_to_serialize,
                             current_ctm);
}

void NoopOp::Serialize(PaintOpWriter& writer,
                       const PaintFlags* flags_to_serialize,
                       const SkM44& current_ctm,
                       const SkM44& original_ctm) const {}

void RestoreOp::Serialize(PaintOpWriter& writer,
                          const PaintFlags* flags_to_serialize,
                          const SkM44& current_ctm,
                          const SkM44& original_ctm) const {}

void RotateOp::Serialize(PaintOpWriter& writer,
                         const PaintFlags* flags_to_serialize,
                         const SkM44& current_ctm,
                         const SkM44& original_ctm) const {
  writer.Write(degrees);
}

void SaveOp::Serialize(PaintOpWriter& writer,
                       const PaintFlags* flags_to_serialize,
                       const SkM44& current_ctm,
                       const SkM44& original_ctm) const {}

void SaveLayerOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(bounds);
}

void SaveLayerAlphaOp::Serialize(PaintOpWriter& writer,
                                 const PaintFlags* flags_to_serialize,
                                 const SkM44& current_ctm,
                                 const SkM44& original_ctm) const {
  writer.Write(bounds);
  writer.Write(alpha);
}

void ScaleOp::Serialize(PaintOpWriter& writer,
                        const PaintFlags* flags_to_serialize,
                        const SkM44& current_ctm,
                        const SkM44& original_ctm) const {
  writer.Write(sx);
  writer.Write(sy);
}

void SetMatrixOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  // Use original_ctm here because SetMatrixOp replaces current_ctm
  writer.Write(original_ctm * matrix);
}

void SetNodeIdOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(node_id);
}

void TranslateOp::Serialize(PaintOpWriter& writer,
                            const PaintFlags* flags_to_serialize,
                            const SkM44& current_ctm,
                            const SkM44& original_ctm) const {
  writer.Write(dx);
  writer.Write(dy);
}

PaintOp* AnnotateOp::Deserialize(PaintOpReader& reader, void* output) {
  AnnotateOp* op = new (output) AnnotateOp;
  reader.Read(&op->annotation_type);
  reader.Read(&op->rect);
  reader.Read(&op->data);
  return op;
}

PaintOp* ClipPathOp::Deserialize(PaintOpReader& reader, void* output) {
  ClipPathOp* op = new (output) ClipPathOp;
  reader.Read(&op->path);
  reader.Read(&op->op);
  reader.Read(&op->antialias);
  return op;
}

PaintOp* ClipRectOp::Deserialize(PaintOpReader& reader, void* output) {
  ClipRectOp* op = new (output) ClipRectOp;
  reader.Read(&op->rect);
  reader.Read(&op->op);
  reader.Read(&op->antialias);
  return op;
}

PaintOp* ClipRRectOp::Deserialize(PaintOpReader& reader, void* output) {
  ClipRRectOp* op = new (output) ClipRRectOp;
  reader.Read(&op->rrect);
  reader.Read(&op->op);
  reader.Read(&op->antialias);
  return op;
}

PaintOp* ConcatOp::Deserialize(PaintOpReader& reader, void* output) {
  ConcatOp* op = new (output) ConcatOp;
  reader.Read(&op->matrix);
  return op;
}

PaintOp* CustomDataOp::Deserialize(PaintOpReader& reader, void* output) {
  CustomDataOp* op = new (output) CustomDataOp;
  reader.Read(&op->id);
  return op;
}

PaintOp* DrawColorOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawColorOp* op = new (output) DrawColorOp;
  reader.Read(&op->color);
  reader.Read(&op->mode);
  return op;
}

PaintOp* DrawDRRectOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawDRRectOp* op = new (output) DrawDRRectOp;
  reader.Read(&op->flags);
  reader.Read(&op->outer);
  reader.Read(&op->inner);
  return op;
}

PaintOp* DrawImageOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawImageOp* op = new (output) DrawImageOp;
  reader.Read(&op->flags);

  reader.Read(&op->image);
  reader.Read(&op->scale_adjustment.fWidth);
  reader.Read(&op->scale_adjustment.fHeight);

  reader.Read(&op->left);
  reader.Read(&op->top);
  reader.Read(&op->sampling);

  return op;
}

PaintOp* DrawImageRectOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawImageRectOp* op = new (output) DrawImageRectOp;
  reader.Read(&op->flags);

  reader.Read(&op->image);
  reader.Read(&op->scale_adjustment.fWidth);
  reader.Read(&op->scale_adjustment.fHeight);

  reader.Read(&op->src);
  reader.Read(&op->dst);
  reader.Read(&op->sampling);
  reader.Read(&op->constraint);

  return op;
}

PaintOp* DrawIRectOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawIRectOp* op = new (output) DrawIRectOp;
  reader.Read(&op->flags);
  reader.Read(&op->rect);
  return op;
}

PaintOp* DrawLineOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawLineOp* op = new (output) DrawLineOp;
  reader.Read(&op->flags);
  reader.Read(&op->x0);
  reader.Read(&op->y0);
  reader.Read(&op->x1);
  reader.Read(&op->y1);
  reader.Read(&op->draw_as_path);
  return op;
}

PaintOp* DrawOvalOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawOvalOp* op = new (output) DrawOvalOp;
  reader.Read(&op->flags);
  reader.Read(&op->oval);
  return op;
}

PaintOp* DrawPathOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawPathOp* op = new (output) DrawPathOp;
  reader.Read(&op->flags);
  reader.Read(&op->path);
  reader.Read(&op->sk_path_fill_type);
  op->path.setFillType(static_cast<SkPathFillType>(op->sk_path_fill_type));
  return op;
}

PaintOp* DrawRecordOp::Deserialize(PaintOpReader& reader, void* output) {
  // These are flattened during serialization.
  return nullptr;
}

PaintOp* DrawRectOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawRectOp* op = new (output) DrawRectOp;
  reader.Read(&op->flags);
  reader.Read(&op->rect);
  return op;
}

PaintOp* DrawRRectOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawRRectOp* op = new (output) DrawRRectOp;
  reader.Read(&op->flags);
  reader.Read(&op->rrect);
  return op;
}

namespace {

// |max_map_size| is purely a safety mechanism to prevent disastrous behavior
// (trying to allocate an enormous map, looping for long periods of time, etc)
// in case the serialization buffer is corrupted somehow.
template <typename T>
bool DeserializeSkottieMap(
    base::flat_map<SkottieResourceIdHash, T>& map,
    absl::optional<size_t> max_map_size,
    PaintOpReader& reader,
    base::FunctionRef<T(PaintOpReader& reader)> value_deserializer) {
  size_t map_size = 0;
  reader.ReadSize(&map_size);
  if (max_map_size && map_size > *max_map_size)
    return false;

  for (size_t i = 0; i < map_size; ++i) {
    size_t resource_id_hash_raw = 0;
    reader.ReadSize(&resource_id_hash_raw);
    SkottieResourceIdHash resource_id_hash =
        SkottieResourceIdHash::FromUnsafeValue(resource_id_hash_raw);
    if (!resource_id_hash)
      return false;

    T value = value_deserializer(reader);
    // Duplicate keys should not happen by design, but defend against it
    // gracefully in case the underlying buffer is corrupted.
    bool is_new_entry = map.emplace(resource_id_hash, std::move(value)).second;
    if (!is_new_entry)
      return false;
  }
  return true;
}

SkottieFrameData DeserializeSkottieFrameData(PaintOpReader& reader) {
  SkottieFrameData frame_data;
  reader.Read(&frame_data.image);
  reader.Read(&frame_data.quality);
  return frame_data;
}

SkColor DeserializeSkottieColor(PaintOpReader& reader) {
  SkColor color = SK_ColorTRANSPARENT;
  reader.Read(&color);
  return color;
}

SkottieTextPropertyValue DeserializeSkottieTextPropertyValue(
    PaintOpReader& reader) {
  size_t text_size = 0u;
  reader.ReadSize(&text_size);
  std::string text(text_size, char());
  reader.ReadData(text_size, const_cast<char*>(text.c_str()));
  SkRect box;
  reader.Read(&box);
  return SkottieTextPropertyValue(std::move(text), gfx::SkRectToRectF(box));
}

}  // namespace

PaintOp* DrawSkottieOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawSkottieOp* op = new (output) DrawSkottieOp;
  reader.Read(&op->dst);
  reader.Read(&op->t);

  reader.Read(&op->skottie);
  // The |skottie| object gets used below, so no point in continuing if it's
  // invalid. That can lead to crashing or unexpected behavior.
  if (!op->skottie || !op->skottie->is_valid()) {
    DCHECK(!op->IsValid());
    return op;
  }

  size_t num_assets_in_animation =
      op->skottie->GetImageAssetMetadata().asset_storage().size();
  size_t num_text_nodes_in_animation = op->skottie->GetTextNodeNames().size();
  bool deserialized_all_maps =
      DeserializeSkottieMap<SkottieFrameData>(
          op->images, /*max_map_size=*/num_assets_in_animation, reader,
          DeserializeSkottieFrameData) &&
      DeserializeSkottieMap<SkColor>(op->color_map,
                                     /*max_map_size=*/absl::nullopt, reader,
                                     DeserializeSkottieColor) &&
      DeserializeSkottieMap<SkottieTextPropertyValue>(
          op->text_map, /*max_map_size=*/num_text_nodes_in_animation, reader,
          DeserializeSkottieTextPropertyValue);
  if (!deserialized_all_maps) {
    op->skottie = nullptr;
    DCHECK(!op->IsValid());
  }
  return op;
}

PaintOp* DrawSlugOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawSlugOp* op = new (output) DrawSlugOp;
  reader.Read(&op->flags);
  unsigned int count = 0;
  reader.Read(&count);
  reader.Read(&op->slug);
  op->extra_slugs.resize(count - 1);
  for (auto& extra_slug : op->extra_slugs) {
    reader.Read(&extra_slug);
  }
  return op;
}

PaintOp* DrawTextBlobOp::Deserialize(PaintOpReader& reader, void* output) {
  NOTREACHED();
  return nullptr;
}

PaintOp* NoopOp::Deserialize(PaintOpReader& reader, void* output) {
  return new (output) NoopOp;
}

PaintOp* RestoreOp::Deserialize(PaintOpReader& reader, void* output) {
  return new (output) RestoreOp;
}

PaintOp* RotateOp::Deserialize(PaintOpReader& reader, void* output) {
  RotateOp* op = new (output) RotateOp;
  reader.Read(&op->degrees);
  return op;
}

PaintOp* SaveOp::Deserialize(PaintOpReader& reader, void* output) {
  return new (output) SaveOp;
}

PaintOp* SaveLayerOp::Deserialize(PaintOpReader& reader, void* output) {
  SaveLayerOp* op = new (output) SaveLayerOp;
  reader.Read(&op->flags);
  reader.Read(&op->bounds);
  return op;
}

PaintOp* SaveLayerAlphaOp::Deserialize(PaintOpReader& reader, void* output) {
  SaveLayerAlphaOp* op = new (output) SaveLayerAlphaOp;
  reader.Read(&op->bounds);
  reader.Read(&op->alpha);
  return op;
}

PaintOp* ScaleOp::Deserialize(PaintOpReader& reader, void* output) {
  ScaleOp* op = new (output) ScaleOp;
  reader.Read(&op->sx);
  reader.Read(&op->sy);
  return op;
}

PaintOp* SetMatrixOp::Deserialize(PaintOpReader& reader, void* output) {
  SetMatrixOp* op = new (output) SetMatrixOp;
  reader.Read(&op->matrix);
  return op;
}

PaintOp* SetNodeIdOp::Deserialize(PaintOpReader& reader, void* output) {
  SetNodeIdOp* op = new (output) SetNodeIdOp;
  reader.Read(&op->node_id);
  return op;
}

PaintOp* TranslateOp::Deserialize(PaintOpReader& reader, void* output) {
  TranslateOp* op = new (output) TranslateOp;
  reader.Read(&op->dx);
  reader.Read(&op->dy);
  return op;
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
    if (result && result.has_paint_record()) {
      result.ReleaseAsRecord().Playback(canvas, params);
    }
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
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    if (op->draw_as_path) {
      SkPath path;
      path.moveTo(op->x0, op->y0);
      path.lineTo(op->x1, op->y1);
      c->drawPath(path, p);
    } else {
      c->drawLine(op->x0, op->y0, op->x1, op->y1, p);
    }
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
  op->record.Playback(canvas, params);
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
    op->slug.reset();
    op->extra_slugs.clear();
  }

  // flags may contain SkDrawLooper for shadow effect, so we need to convert
  // SkTextBlob to slug for each run.
  size_t i = 0;
  flags->DrawToSk(canvas, [op, &params, &i](SkCanvas* c, const SkPaint& p) {
    DCHECK(op->blob);
    c->drawTextBlob(op->blob.get(), op->x, op->y, p);
    if (params.is_analyzing) {
      auto s = GrSlug::ConvertBlob(c, *op->blob, {op->x, op->y}, p);
      if (i == 0) {
        op->slug = std::move(s);
      } else {
        op->extra_slugs.push_back(std::move(s));
      }
    }
    i++;
  });

  if (op->node_id) {
    SkPDF::SetNodeId(canvas, 0);
  }
}

void DrawSlugOp::RasterWithFlags(const DrawSlugOp* op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const PlaybackParams& params) {
  size_t i = 0;
  flags->DrawToSk(canvas, [op, &params, &i](SkCanvas* c, const SkPaint& p) {
    if (i < 1 + op->extra_slugs.size()) {
      DCHECK(!params.is_analyzing);
      const auto& draw_slug = i == 0 ? op->slug : op->extra_slugs[i - 1];
      if (draw_slug) {
        draw_slug->draw(c);
      }
    }
    ++i;
  });
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
    paint->setAlphaf(op->alpha);
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

bool AnnotateOp::EqualsForTesting(const AnnotateOp& other) const {
  return annotation_type == other.annotation_type && rect == other.rect &&
         !data == !other.data && (!data || data->equals(other.data.get()));
}

bool ClipPathOp::EqualsForTesting(const ClipPathOp& other) const {
  return path == other.path && op == other.op && antialias == other.antialias;
}

bool ClipRectOp::EqualsForTesting(const ClipRectOp& other) const {
  return rect == other.rect && op == other.op && antialias == other.antialias;
}

bool ClipRRectOp::EqualsForTesting(const ClipRRectOp& other) const {
  return rrect == other.rrect && op == other.op && antialias == other.antialias;
}

bool ConcatOp::EqualsForTesting(const ConcatOp& other) const {
  return matrix == other.matrix;
}

bool CustomDataOp::EqualsForTesting(const CustomDataOp& other) const {
  return id == other.id;
}

bool DrawColorOp::EqualsForTesting(const DrawColorOp& other) const {
  return color == other.color;
}

bool DrawDRRectOp::EqualsForTesting(const DrawDRRectOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         outer == other.outer && inner == other.inner;
}

bool DrawImageOp::EqualsForTesting(const DrawImageOp& other) const {
  // For now image, sampling and constraint are not compared.
  // scale_adjustment intentionally omitted because it is added during
  // serialization based on raster scale.
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         top == other.top && left == other.left;
}

bool DrawImageRectOp::EqualsForTesting(const DrawImageRectOp& other) const {
  // For now image, sampling and constraint are not compared.
  // scale_adjustment intentionally omitted because it is added during
  // serialization based on raster scale.
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         src == other.src && dst == other.dst;
}

bool DrawIRectOp::EqualsForTesting(const DrawIRectOp& other) const {
  return flags.EqualsForTesting(other.flags) && rect == other.rect;  // IN-TEST
}

bool DrawLineOp::EqualsForTesting(const DrawLineOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1;
}

bool DrawOvalOp::EqualsForTesting(const DrawOvalOp& other) const {
  return flags.EqualsForTesting(other.flags) && oval == other.oval;  // IN-TEST
}

bool DrawPathOp::EqualsForTesting(const DrawPathOp& other) const {
  return flags.EqualsForTesting(other.flags) && path == other.path;  // IN-TEST
}

bool DrawRecordOp::EqualsForTesting(const DrawRecordOp& other) const {
  return record.EqualsForTesting(other.record);  // IN-TEST
}

bool DrawRectOp::EqualsForTesting(const DrawRectOp& other) const {
  return flags.EqualsForTesting(other.flags) && rect == other.rect;  // IN-TEST
}

bool DrawRRectOp::EqualsForTesting(const DrawRRectOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         rrect == other.rrect;
}

bool DrawSkottieOp::EqualsForTesting(const DrawSkottieOp& other) const {
  // TODO(malaykeshav): Verify the skottie objects of each PaintOb are equal
  // bsed on the serialized bytes.
  if (t != other.t || dst != other.dst || color_map != other.color_map ||
      text_map != other.text_map) {
    return false;
  }
  return base::ranges::equal(
      images, other.images, [](const auto& a, const auto& b) {
        return a.first == b.first &&
               // PaintImage::IsSameForTesting() returns false in cases where
               // the
               // image's content may be the same, but it got realloacted to a
               // different spot somewhere in memory via the transfer cache. The
               // next best thing is to just compare the dimensions of the
               // PaintImage.
               a.second.image.width() == b.second.image.width() &&
               a.second.image.height() == b.second.image.height() &&
               a.second.quality == b.second.quality;
      });
}

bool DrawTextBlobOp::EqualsForTesting(const DrawTextBlobOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         x == other.x && y == other.y && node_id == other.node_id;
}

bool DrawSlugOp::EqualsForTesting(const DrawSlugOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         !slug == !other.slug &&
         (!slug || slug->serialize()->equals(other.slug->serialize().get()));
}

bool NoopOp::EqualsForTesting(const NoopOp& other) const {
  return true;
}

bool RestoreOp::EqualsForTesting(const RestoreOp& other) const {
  return true;
}

bool RotateOp::EqualsForTesting(const RotateOp& other) const {
  return degrees == other.degrees;
}

bool SaveOp::EqualsForTesting(const SaveOp& other) const {
  return true;
}

bool SaveLayerOp::EqualsForTesting(const SaveLayerOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         bounds == other.bounds;
}

bool SaveLayerAlphaOp::EqualsForTesting(const SaveLayerAlphaOp& other) const {
  return bounds == other.bounds && alpha == other.alpha;
}

bool ScaleOp::EqualsForTesting(const ScaleOp& other) const {
  return sx == other.sx && sy == other.sy;
}

bool SetMatrixOp::EqualsForTesting(const SetMatrixOp& other) const {
  return matrix == other.matrix;
}

bool SetNodeIdOp::EqualsForTesting(const SetNodeIdOp& other) const {
  return node_id == other.node_id;
}

bool TranslateOp::EqualsForTesting(const TranslateOp& other) const {
  return dx == other.dx && dy == other.dy;
}

bool PaintOp::IsDrawOp() const {
  return g_is_draw_op[type];
}

bool PaintOp::IsPaintOpWithFlags() const {
  return g_has_paint_flags[type];
}

bool PaintOp::EqualsForTesting(const PaintOp& other) const {
  if (GetType() != other.GetType())
    return false;
  return g_equal_for_testing_functions[type](*this, other);
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
  // Need at least enough room for the header.
  if (size < PaintOpWriter::kHeaderBytes) {
    return 0u;
  }

  PaintOpWriter writer(memory, size, options);
  writer.ReserveOpHeader();
  g_serialize_functions[type](*this, writer, flags_to_serialize, current_ctm,
                              original_ctm);

  // Convert DrawTextBlobOp to DrawSlugOp.
  if (GetType() == PaintOpType::DrawTextBlob) {
    return writer.FinishOp(static_cast<uint8_t>(PaintOpType::DrawSlug));
  }
  return writer.FinishOp(type);
}

PaintOp* PaintOp::Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              size_t* read_bytes,
                              const DeserializeOptions& options) {
  DCHECK_GE(output_size, kLargestPaintOpAlignedSize);

  uint8_t type;
  PaintOpReader reader(input, input_size, options);
  if (!reader.ReadAndValidateOpHeader(&type, read_bytes)) {
    return nullptr;
  }
  return g_deserialize_functions[type](reader, output, output_size);
}

PaintOp* PaintOp::DeserializeIntoPaintOpBuffer(
    const volatile void* input,
    size_t input_size,
    PaintOpBuffer* buffer,
    size_t* read_bytes,
    const DeserializeOptions& options) {
  uint8_t type;
  PaintOpReader reader(input, input_size, options);
  if (!reader.ReadAndValidateOpHeader(&type, read_bytes)) {
    return nullptr;
  }

  uint16_t op_aligned_size = g_type_to_aligned_size[type];
  if (auto* op = g_deserialize_functions[type](
          reader, buffer->AllocatePaintOp(op_aligned_size), op_aligned_size)) {
    g_analyze_op_functions[type](buffer, op);
    return op;
  }

  // The last allocated op has already been destroyed if it failed to
  // deserialize. Update the buffer's op tracking to exclude it to avoid
  // access during cleanup at destruction.
  buffer->used_ -= op_aligned_size;
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
    case PaintOpType::DrawSlug: {
      const auto& slug_op = static_cast<const DrawSlugOp&>(op);
      *rect = slug_op.slug->sourceBoundsWithOrigin();
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
  if (!PaintOp::GetBounds(op, &rect) || !rect.isFinite()) {
    return false;
  }

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
  return record.num_slow_paths_up_to_min_for_MSAA();
}

bool DrawRecordOp::HasNonAAPaint() const {
  return record.HasNonAAPaint();
}

bool DrawRecordOp::HasDrawTextOps() const {
  return record.has_draw_text_ops();
}

bool DrawRecordOp::HasSaveLayerOps() const {
  return record.has_save_layer_ops();
}

bool DrawRecordOp::HasSaveLayerAlphaOps() const {
  return record.has_save_layer_alpha_ops();
}

bool DrawRecordOp::HasEffectsPreventingLCDTextForSaveLayerAlpha() const {
  return record.has_effects_preventing_lcd_text_for_save_layer_alpha();
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

DrawRecordOp::DrawRecordOp(PaintRecord record)
    : PaintOp(kType), record(std::move(record)) {}

DrawRecordOp::~DrawRecordOp() = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record.bytes_used();
}

size_t DrawRecordOp::AdditionalOpCount() const {
  return record.total_op_count();
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

bool DrawSkottieOp::HasDiscardableImages() const {
  return !images.empty();
}

bool DrawRecordOp::HasDiscardableImages() const {
  return record.HasDiscardableImages();
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

DrawSlugOp::DrawSlugOp() : PaintOpWithFlags(kType) {}

DrawSlugOp::DrawSlugOp(sk_sp<GrSlug> slug, const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags), slug(std::move(slug)) {}

DrawSlugOp::~DrawSlugOp() = default;

}  // namespace cc
