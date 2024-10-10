// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_op.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/values_equivalent.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/optional_util.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skottie_serialization_history.h"
#include "cc/paint/tone_map_util.h"
#include "skia/ext/draw_gainmap_image.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTiledImageUtils.h"
#include "third_party/skia/include/core/SkVertices.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "third_party/skia/include/private/chromium/Slug.h"
#include "third_party/skia/src/core/SkCanvasPriv.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {

BASE_FEATURE(kUseLitePaintOps,
             "UseLitePaintOps",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
    SkTiledImageUtils::DrawImage(canvas, image, 0, 0, options, paint);
    canvas->restore();
    return;
  }
  SkTiledImageUtils::DrawImageRect(canvas, image, src, dst, options, paint,
                                   constraint);
}

PaintFlags::ScalingOperation MatrixToScalingOperation(SkMatrix m) {
  SkSize scale;
  if (m.decomposeScale(&scale)) {
    return (scale.width() > 1 && scale.height() > 1)
               ? PaintFlags::ScalingOperation::kUpscale
               : PaintFlags::ScalingOperation::kUnknown;
  }
  return PaintFlags::ScalingOperation::kUnknown;
}

#define TYPES(M)             \
  M(AnnotateOp)              \
  M(ClipPathOp)              \
  M(ClipRectOp)              \
  M(ClipRRectOp)             \
  M(ConcatOp)                \
  M(CustomDataOp)            \
  M(DrawArcOp)               \
  M(DrawArcLiteOp)           \
  M(DrawColorOp)             \
  M(DrawDRRectOp)            \
  M(DrawImageOp)             \
  M(DrawImageRectOp)         \
  M(DrawIRectOp)             \
  M(DrawLineOp)              \
  M(DrawLineLiteOp)          \
  M(DrawOvalOp)              \
  M(DrawPathOp)              \
  M(DrawRecordOp)            \
  M(DrawRectOp)              \
  M(DrawRRectOp)             \
  M(DrawScrollingContentsOp) \
  M(DrawSkottieOp)           \
  M(DrawSlugOp)              \
  M(DrawTextBlobOp)          \
  M(DrawVerticesOp)          \
  M(NoopOp)                  \
  M(RestoreOp)               \
  M(RotateOp)                \
  M(SaveOp)                  \
  M(SaveLayerOp)             \
  M(SaveLayerAlphaOp)        \
  M(SaveLayerFiltersOp)      \
  M(ScaleOp)                 \
  M(SetMatrixOp)             \
  M(SetNodeIdOp)             \
  M(TranslateOp)

static constexpr size_t kNumOpTypes = PaintOp::kNumOpTypes;

// Verify that every op is in the TYPES macro.
#define M(T) +1
static_assert(kNumOpTypes == TYPES(M), "Missing op in list");
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
ALWAYS_INLINE void Serialize(const PaintOp& op,
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

}  // namespace

#define M(T) PaintOpBuffer::ComputeOpAlignedSize<T>(),
uint16_t PaintOp::g_type_to_aligned_size[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T) T::kIsDrawOp,
bool PaintOp::g_is_draw_op[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T) T::kHasPaintFlags,
bool PaintOp::g_has_paint_flags[kNumOpTypes] = {TYPES(M)};
#undef M

const SkRect PaintOp::kUnsetRect = {SK_ScalarInfinity, 0, 0, 0};

std::string PaintOpTypeToString(PaintOpType type) {
  switch (type) {
#define M(T)     \
  case T::kType: \
    return #T;

  TYPES(M)
#undef M
  }
  NOTREACHED();
}

bool IsDiscardableImage(const PaintImage& image,
                        gfx::ContentColorUsage* content_color_usage) {
  if (!image || image.IsTextureBacked()) {
    return false;
  }
  if (content_color_usage) {
    *content_color_usage =
        std::max(*content_color_usage, image.GetContentColorUsage());
  }
  return true;
}

bool OpHasDiscardableImagesImpl(const PaintOp& op) {
  gfx::ContentColorUsage* const unused_content_color_usage = nullptr;
  if (op.IsPaintOpWithFlags() &&
      static_cast<const PaintOpWithFlags&>(op).HasDiscardableImagesFromFlags(
          unused_content_color_usage)) {
    return true;
  }
  switch (op.GetType()) {
#define M(T)                                               \
  case T::kType:                                           \
    return static_cast<const T&>(op).HasDiscardableImages( \
        unused_content_color_usage);

    TYPES(M)
#undef M
  }
}

#undef TYPES

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
  writer.WriteSimpleMultiple(x0, y0, x1, y1, draw_as_path);
}

void DrawLineLiteOp::Serialize(PaintOpWriter& writer,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) const {
  writer.WriteSimpleMultiple(x0, y0, x1, y1, core_paint_flags);
}

void DrawArcOp::Serialize(PaintOpWriter& writer,
                          const PaintFlags* flags_to_serialize,
                          const SkM44& current_ctm,
                          const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(oval);
  writer.Write(start_angle_degrees);
  writer.Write(sweep_angle_degrees);
}

void DrawArcLiteOp::Serialize(PaintOpWriter& writer,
                              const PaintFlags* flags_to_serialize,
                              const SkM44& current_ctm,
                              const SkM44& original_ctm) const {
  writer.WriteSimpleMultiple(oval, start_angle_degrees, sweep_angle_degrees,
                             core_paint_flags);
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

void DrawScrollingContentsOp::Serialize(PaintOpWriter& writer,
                                        const PaintFlags* flags_to_serialize,
                                        const SkM44& current_ctm,
                                        const SkM44& original_ctm) const {
  // These are flattened in PaintOpBufferSerializer.
  NOTREACHED();
}

void DrawVerticesOp::Serialize(PaintOpWriter& writer,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);

  writer.Write(vertices->data());
  writer.Write(uvs->data());
  writer.Write(indices->data());
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

void DrawSlugOp::SerializeSlugs(
    const sk_sp<sktext::gpu::Slug>& slug,
    const std::vector<sk_sp<sktext::gpu::Slug>>& extra_slugs,
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

void SaveLayerFiltersOp::Serialize(PaintOpWriter& writer,
                                   const PaintFlags* flags_to_serialize,
                                   const SkM44& current_ctm,
                                   const SkM44& original_ctm) const {
  writer.Write(*flags_to_serialize, current_ctm);
  writer.Write(filters, current_ctm);
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

  reader.Read(&op->image, op->flags.getDynamicRangeLimit());
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

  reader.Read(&op->image, op->flags.getDynamicRangeLimit());
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

PaintOp* DrawLineLiteOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawLineLiteOp* op = new (output) DrawLineLiteOp;
  reader.Read(&op->x0);
  reader.Read(&op->y0);
  reader.Read(&op->x1);
  reader.Read(&op->y1);
  reader.Read(&op->core_paint_flags);
  return op;
}

PaintOp* DrawArcOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawArcOp* op = new (output) DrawArcOp;
  reader.Read(&op->flags);
  reader.Read(&op->oval);
  reader.Read(&op->start_angle_degrees);
  reader.Read(&op->sweep_angle_degrees);
  return op;
}

PaintOp* DrawArcLiteOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawArcLiteOp* op = new (output) DrawArcLiteOp;
  reader.Read(&op->oval);
  reader.Read(&op->start_angle_degrees);
  reader.Read(&op->sweep_angle_degrees);
  reader.Read(&op->core_paint_flags);
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

PaintOp* DrawScrollingContentsOp::Deserialize(PaintOpReader& reader,
                                              void* output) {
  // These are flattened during serialization.
  return nullptr;
}

PaintOp* DrawVerticesOp::Deserialize(PaintOpReader& reader, void* output) {
  DrawVerticesOp* op = new (output) DrawVerticesOp;

  reader.Read(&op->flags);

  std::vector<SkPoint> vertices;
  reader.Read(&vertices);
  op->vertices =
      base::MakeRefCounted<RefCountedBuffer<SkPoint>>(std::move(vertices));

  std::vector<SkPoint> uvs;
  reader.Read(&uvs);
  op->uvs = base::MakeRefCounted<RefCountedBuffer<SkPoint>>(std::move(uvs));

  std::vector<uint16_t> indices;
  reader.Read(&indices);
  op->indices =
      base::MakeRefCounted<RefCountedBuffer<uint16_t>>(std::move(indices));

  return op;
}

namespace {

// |max_map_size| is purely a safety mechanism to prevent disastrous behavior
// (trying to allocate an enormous map, looping for long periods of time, etc)
// in case the serialization buffer is corrupted somehow.
template <typename T>
bool DeserializeSkottieMap(
    base::flat_map<SkottieResourceIdHash, T>& map,
    std::optional<size_t> max_map_size,
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
  reader.Read(&frame_data.image, PaintFlags::DynamicRangeLimitMixture(
                                     PaintFlags::DynamicRangeLimit::kHigh));
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
  reader.ReadData(base::as_writable_byte_span(text));
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
                                     /*max_map_size=*/std::nullopt, reader,
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
  if (count > 0) {
    reader.Read(&op->slug);
    const size_t remaining_slug_count =
        std::min<size_t>(op->extra_slugs.max_size(), count - 1);
    if (!reader.CanReadVector(remaining_slug_count, op->extra_slugs)) {
      return op;
    }
    op->extra_slugs.resize(remaining_slug_count);
    for (auto& extra_slug : op->extra_slugs) {
      reader.Read(&extra_slug);
    }
  }
  return op;
}

PaintOp* DrawTextBlobOp::Deserialize(PaintOpReader& reader, void* output) {
  NOTREACHED();
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

PaintOp* SaveLayerFiltersOp::Deserialize(PaintOpReader& reader, void* output) {
  SaveLayerFiltersOp* op = new (output) SaveLayerFiltersOp;
  reader.Read(&op->flags);
  reader.Read(&op->filters);
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
    case PaintCanvas::AnnotationType::kUrl:
      SkAnnotateRectWithURL(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::kLinkToDestination:
      SkAnnotateLinkToDestination(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::kNameDestination: {
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
  if (params.callbacks.custom_callback) {
    params.callbacks.custom_callback.Run(canvas, op->id);
  }
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
    if (!sk_image) {
      return;
    }

    // If this uses a gainmap shader, then replace DrawImage with a shader.
    if (ToneMapUtil::UseGainmapShader(op->image)) {
      skia::DrawGainmapImage(
          canvas, op->image.cached_sk_image_, op->image.gainmap_sk_image_,
          op->image.gainmap_info_.value(), op->image.target_hdr_headroom_,
          op->left, op->top, op->sampling, paint);
      return;
    }

    // Add a tone mapping filter to `paint` if needed.
    if (ToneMapUtil::UseGlobalToneMapFilter(op->image)) {
      auto dst_color_space = canvas->imageInfo().refColorSpace();
      ToneMapUtil::AddGlobalToneMapFilterToPaint(paint, op->image,
                                                 dst_color_space);
      sk_image = sk_image->reinterpretColorSpace(dst_color_space);
    }

    SkTiledImageUtils::DrawImage(canvas, sk_image.get(), op->left, op->top,
                                 op->sampling, &paint);
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
  PaintFlags::ScalingOperation scale =
      MatrixToScalingOperation(canvas->getLocalToDeviceAs3x3());
  SkTiledImageUtils::DrawImage(canvas, decoded_image.image().get(), op->left,
                               op->top,
                               PaintFlags::FilterQualityToSkSamplingOptions(
                                   decoded_image.filter_quality(), scale),
                               &paint);
}

void DrawImageRectOp::RasterWithFlags(const DrawImageRectOp* op,
                                      const PaintFlags* flags,
                                      SkCanvas* canvas,
                                      const PlaybackParams& params) {
  // TODO(crbug.com/40613771): make sure to support the case where paint worklet
  // generated images are used in other raster work such as canvas2d.
  if (op->image.IsDeferredPaintRecord()) {
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
    if (op->image.NeedsLayer()) {
      // TODO(crbug.com/343439032): See if we can be less aggressive about use
      // of a save layer operation for CSS paint worklets since expensive.
      canvas->saveLayer(&op->src, &paint);
    }

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
      if (!sk_image) {
        return;
      }

      // If the PaintImage uses a gainmap shader, then replace DrawImage with a
      // shader.
      if (ToneMapUtil::UseGainmapShader(op->image)) {
        skia::DrawGainmapImageRect(
            c, op->image.cached_sk_image_, op->image.gainmap_sk_image_,
            op->image.gainmap_info_.value(), op->image.target_hdr_headroom_,
            adjusted_src, op->dst, op->sampling, p);
        return;
      }

      // If this uses a global tone map filter, then incorporate that filter
      // into the paint.
      if (ToneMapUtil::UseGlobalToneMapFilter(op->image)) {
        SkPaint tonemap_paint = p;
        ToneMapUtil::AddGlobalToneMapFilterToPaint(
            tonemap_paint, op->image, c->imageInfo().refColorSpace());
        sk_image =
            sk_image->reinterpretColorSpace(c->imageInfo().refColorSpace());
        DrawImageRect(c, sk_image.get(), adjusted_src, op->dst, op->sampling,
                      &tonemap_paint, op->constraint);
        return;
      }

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
  PaintFlags::ScalingOperation scale = MatrixToScalingOperation(matrix.asM33());
  flags->DrawToSk(canvas, [op, &decoded_image, adjusted_src, scale](
                              SkCanvas* c, const SkPaint& p) {
    SkSamplingOptions options = PaintFlags::FilterQualityToSkSamplingOptions(
        decoded_image.filter_quality(), scale);
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

void DrawLineLiteOp::Raster(const DrawLineLiteOp* op,
                            SkCanvas* canvas,
                            const PlaybackParams& params) {
  PaintFlags flags(op->core_paint_flags);
  flags.DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawLine(op->x0, op->y0, op->x1, op->y1, p);
  });
}

void DrawArcImpl(SkCanvas* canvas,
                 const SkRect& oval,
                 float start_angle_degrees,
                 float sweep_angle_degrees,
                 const SkPaint& paint,
                 const PaintFlags& flags) {
  if (!flags.isArcClosed()) {
    // drawArc can only handle open arcs.
    canvas->drawArc(oval, start_angle_degrees, sweep_angle_degrees, false,
                    paint);
    return;
  }

  if (SkScalarNearlyEqual(std::abs(sweep_angle_degrees), 360)) {
    // Closed ellipses can be rendered using drawOval.
    canvas->drawOval(oval, paint);
  } else {
    // Closed partial arcs -> general SkPath.
    SkPath path;
    path.arcTo(oval, start_angle_degrees, sweep_angle_degrees, false);
    path.close();
    canvas->drawPath(path, paint);
  }
}

void DrawArcOp::RasterWithFlags(const DrawArcOp* op,
                                const PaintFlags* flags,
                                SkCanvas* canvas,
                                const PlaybackParams& params) {
  op->RasterWithFlagsImpl(flags, canvas);
}

void DrawArcOp::RasterWithFlagsImpl(const PaintFlags* flags,
                                    SkCanvas* canvas) const {
  flags->DrawToSk(canvas, [this, flags](SkCanvas* c, const SkPaint& p) {
    DrawArcImpl(c, oval, start_angle_degrees, sweep_angle_degrees, p, *flags);
  });
}

void DrawArcLiteOp::Raster(const DrawArcLiteOp* op,
                           SkCanvas* canvas,
                           const PlaybackParams& params) {
  PaintFlags flags(op->core_paint_flags);
  flags.DrawToSk(canvas, [op, &flags](SkCanvas* c, const SkPaint& p) {
    DrawArcImpl(c, op->oval, op->start_angle_degrees, op->sweep_angle_degrees,
                p, flags);
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
  op->record.Playback(canvas, params, op->local_ctm);
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

void DrawScrollingContentsOp::Raster(const DrawScrollingContentsOp* op,
                                     SkCanvas* canvas,
                                     const PlaybackParams& params) {
  canvas->save();
  CHECK(params.raster_inducing_scroll_offsets);
  gfx::PointF scroll_offset =
      params.raster_inducing_scroll_offsets->at(op->scroll_element_id);
  canvas->translate(-scroll_offset.x(), -scroll_offset.y());
  op->display_item_list->Raster(canvas, params);
  canvas->restore();
}

void DrawVerticesOp::RasterWithFlags(const DrawVerticesOp* op,
                                     const PaintFlags* flags,
                                     SkCanvas* canvas,
                                     const PlaybackParams& params) {
  CHECK_EQ(op->vertices->data().size(), op->uvs->data().size());
  SkVertices::Builder vertices_builder(
      SkVertices::kTriangles_VertexMode,
      base::checked_cast<int>(op->vertices->data().size()),
      base::checked_cast<int>(op->indices->data().size()),
      SkVertices::kHasTexCoords_BuilderFlag);

  std::copy(op->vertices->data().data(),
            op->vertices->data().data() + op->vertices->data().size(),
            vertices_builder.positions());
  std::copy(op->uvs->data().data(),
            op->uvs->data().data() + op->uvs->data().size(),
            vertices_builder.texCoords());
  std::copy(op->indices->data().data(),
            op->indices->data().data() + op->indices->data().size(),
            vertices_builder.indices());

  const sk_sp<SkVertices> skverts = vertices_builder.detach();

  flags->DrawToSk(canvas, [&skverts](SkCanvas* c, const SkPaint& p) {
    c->drawVertices(skverts, SkBlendMode::kSrcOver, p);
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
    return SkottieWrapper::FrameDataFetchResult::kNoUpdate;

  const SkottieFrameData& frame_data = images_iter->second;
  SkM44 matrix = canvas->getLocalToDevice();
  if (!frame_data.image) {
    sk_image = nullptr;
  } else if (params.image_provider) {
    // There is no use case for applying dark mode filters to skottie images
    // currently.
    DrawImage draw_image(
        frame_data.image, /*use_dark_mode=*/false,
        SkIRect::MakeWH(frame_data.image.width(), frame_data.image.height()),
        frame_data.quality, matrix);
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
  PaintFlags::ScalingOperation scale = MatrixToScalingOperation(matrix.asM33());
  sampling_out =
      PaintFlags::FilterQualityToSkSamplingOptions(frame_data.quality, scale);
  return SkottieWrapper::FrameDataFetchResult::kNewDataAvailable;
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

  // flags may contain DrawLooper for shadow effect, so we need to convert
  // SkTextBlob to slug for each run.
  size_t i = 0;
  flags->DrawToSk(canvas, [op, &params, &i](SkCanvas* c, const SkPaint& p) {
    DCHECK(op->blob);
    c->drawTextBlob(op->blob.get(), op->x, op->y, p);
    if (params.is_analyzing) {
      auto s = sktext::gpu::Slug::ConvertBlob(c, *op->blob, {op->x, op->y}, p);
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
        draw_slug->draw(c, p);
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
  std::optional<SkPaint> paint;
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

void SaveLayerFiltersOp::RasterWithFlags(const SaveLayerFiltersOp* op,
                                         const PaintFlags* flags,
                                         SkCanvas* canvas,
                                         const PlaybackParams& params) {
  SkPaint paint = flags->ToSkPaint();
  canvas->saveLayer(SkCanvasPriv::ScaledBackdropLayer(
      /*bounds=*/nullptr, &paint, /*backdrop=*/nullptr, /*backdropScale=*/1.0f,
      /*saveLayerFlags=*/0, PaintFilter::ToSkImageFilters(op->filters)));
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

bool DrawLineLiteOp::EqualsForTesting(const DrawLineLiteOp& other) const {
  return x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1 &&
         core_paint_flags == other.core_paint_flags;
}

bool DrawArcOp::EqualsForTesting(const DrawArcOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         oval == other.oval &&
         start_angle_degrees == other.start_angle_degrees &&
         sweep_angle_degrees == other.sweep_angle_degrees;
}

bool DrawArcLiteOp::EqualsForTesting(const DrawArcLiteOp& other) const {
  return oval == other.oval &&
         start_angle_degrees == other.start_angle_degrees &&
         sweep_angle_degrees == other.sweep_angle_degrees &&
         core_paint_flags == other.core_paint_flags;
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

bool DrawScrollingContentsOp::EqualsForTesting(
    const DrawScrollingContentsOp& other) const {
  return scroll_element_id == other.scroll_element_id &&
         display_item_list == other.display_item_list;
}

bool DrawVerticesOp::EqualsForTesting(const DrawVerticesOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         *vertices == *other.vertices && *uvs == *other.uvs &&
         *indices == *other.indices;
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

bool SaveLayerFiltersOp::EqualsForTesting(
    const SaveLayerFiltersOp& other) const {
  return flags.EqualsForTesting(other.flags) &&  // IN-TEST
         base::ranges::equal(
             filters, other.filters,
             [](const sk_sp<PaintFilter>& lhs, const sk_sp<PaintFilter>& rhs) {
               return base::ValuesEquivalent(
                   lhs, rhs, [](const PaintFilter& x, const PaintFilter& y) {
                     return x.EqualsForTesting(y);  // IN-TEST
                   });
             });
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
  if (GetType() == PaintOpType::kDrawTextBlob) {
    return writer.FinishOp(static_cast<uint8_t>(PaintOpType::kDrawSlug));
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
  switch (op.GetType()) {
    case PaintOpType::kAnnotate:
      return false;
    case PaintOpType::kClipPath:
      return false;
    case PaintOpType::kClipRect:
      return false;
    case PaintOpType::kClipRRect:
      return false;
    case PaintOpType::kConcat:
      return false;
    case PaintOpType::kCustomData:
      return false;
    case PaintOpType::kDrawColor:
      return false;
    case PaintOpType::kDrawDRRect: {
      const auto& rect_op = static_cast<const DrawDRRectOp&>(op);
      *rect = rect_op.outer.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawImage: {
      const auto& image_op = static_cast<const DrawImageOp&>(op);
      *rect = SkRect::MakeXYWH(image_op.left, image_op.top,
                               image_op.image.width(), image_op.image.height());
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawImageRect: {
      const auto& image_rect_op = static_cast<const DrawImageRectOp&>(op);
      *rect = image_rect_op.dst;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawIRect: {
      const auto& rect_op = static_cast<const DrawIRectOp&>(op);
      *rect = SkRect::Make(rect_op.rect);
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawLine: {
      const auto& line_op = static_cast<const DrawLineOp&>(op);
      rect->setLTRB(line_op.x0, line_op.y0, line_op.x1, line_op.y1);
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawLineLite: {
      const auto& line_op = static_cast<const DrawLineLiteOp&>(op);
      rect->setLTRB(line_op.x0, line_op.y0, line_op.x1, line_op.y1);
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawArc: {
      const auto& arc_op = static_cast<const DrawArcOp&>(op);
      *rect = arc_op.oval;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawArcLite: {
      const auto& arc_op = static_cast<const DrawArcLiteOp&>(op);
      *rect = arc_op.oval;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawOval: {
      const auto& oval_op = static_cast<const DrawOvalOp&>(op);
      *rect = oval_op.oval;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawPath: {
      const auto& path_op = static_cast<const DrawPathOp&>(op);
      *rect = path_op.path.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawRecord:
      return false;
    case PaintOpType::kDrawRect: {
      const auto& rect_op = static_cast<const DrawRectOp&>(op);
      *rect = rect_op.rect;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawRRect: {
      const auto& rect_op = static_cast<const DrawRRectOp&>(op);
      *rect = rect_op.rrect.rect();
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawScrollingContents:
      return false;
    case PaintOpType::kDrawSkottie: {
      const auto& skottie_op = static_cast<const DrawSkottieOp&>(op);
      *rect = skottie_op.dst;
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawTextBlob: {
      const auto& text_op = static_cast<const DrawTextBlobOp&>(op);
      *rect = text_op.blob->bounds().makeOffset(text_op.x, text_op.y);
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawSlug: {
      const auto& slug_op = static_cast<const DrawSlugOp&>(op);
      *rect = slug_op.slug->sourceBoundsWithOrigin();
      rect->sort();
      return true;
    }
    case PaintOpType::kDrawVertices: {
      const auto& vertices_op = static_cast<const DrawVerticesOp&>(op);
      rect->setBounds(
          vertices_op.vertices->data().data(),
          base::checked_cast<int>(vertices_op.vertices->data().size()));
      return true;
    }
    case PaintOpType::kNoop:
      return false;
    case PaintOpType::kRestore:
      return false;
    case PaintOpType::kRotate:
      return false;
    case PaintOpType::kSave:
      return false;
    case PaintOpType::kSaveLayer:
      return false;
    case PaintOpType::kSaveLayerAlpha:
      return false;
    case PaintOpType::kSaveLayerFilters:
      return false;
    case PaintOpType::kScale:
      return false;
    case PaintOpType::kSetMatrix:
      return false;
    case PaintOpType::kSetNodeId:
      return false;
    case PaintOpType::kTranslate:
      return false;
  }
  return false;
}

// static
gfx::Rect PaintOp::ComputePaintRect(const PaintOp& op,
                                    const SkRect& clip_rect,
                                    const SkMatrix& ctm) {
  gfx::Rect transformed_rect;
  SkRect op_rect;
  if (!PaintOp::GetBounds(op, &op_rect)) {
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
  transformed_rect.Outset(1);
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
  return OpHasDiscardableImagesImpl(op);
}

void PaintOp::DestroyThis() {
  auto func = g_destructor_functions[type];
  if (func)
    func(this);
}

bool PaintOpWithFlags::HasDiscardableImagesFromFlags(
    gfx::ContentColorUsage* content_color_usage) const {
  return flags.HasDiscardableImages(content_color_usage);
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
  if (const PathEffect* effect = flags.getPathEffect().get()) {
    if (flags.getStrokeCap() != PaintFlags::kRound_Cap &&
        effect->dash_interval_count() == 2) {
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
  return record.has_non_aa_paint();
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

bool DrawRecordOp::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  bool has_discardable_images = record.has_discardable_images();
  if (has_discardable_images && content_color_usage) {
    *content_color_usage =
        std::max(*content_color_usage, record.content_color_usage());
  }
  return has_discardable_images;
}

int DrawScrollingContentsOp::CountSlowPaths() const {
  return display_item_list->num_slow_paths_up_to_min_for_MSAA();
}

bool DrawScrollingContentsOp::HasNonAAPaint() const {
  return display_item_list->has_non_aa_paint();
}

bool DrawScrollingContentsOp::HasDrawTextOps() const {
  return display_item_list->has_draw_text_ops();
}

bool DrawScrollingContentsOp::HasSaveLayerOps() const {
  return display_item_list->has_save_layer_ops();
}

bool DrawScrollingContentsOp::HasSaveLayerAlphaOps() const {
  return display_item_list->has_save_layer_alpha_ops();
}

bool DrawScrollingContentsOp::HasEffectsPreventingLCDTextForSaveLayerAlpha()
    const {
  return display_item_list
      ->has_effects_preventing_lcd_text_for_save_layer_alpha();
}

bool DrawScrollingContentsOp::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  bool has_discardable_images = display_item_list->has_discardable_images();
  if (has_discardable_images && content_color_usage) {
    *content_color_usage = std::max(*content_color_usage,
                                    display_item_list->content_color_usage());
  }
  return has_discardable_images;
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

bool DrawImageOp::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  return IsDiscardableImage(image, content_color_usage);
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

bool DrawImageRectOp::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  return IsDiscardableImage(image, content_color_usage);
}

DrawImageRectOp::~DrawImageRectOp() = default;

DrawRecordOp::DrawRecordOp(PaintRecord record, bool local_ctm)
    : PaintOp(kType), record(std::move(record)), local_ctm(local_ctm) {}

DrawRecordOp::~DrawRecordOp() = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record.bytes_used();
}

size_t DrawRecordOp::AdditionalOpCount() const {
  return record.total_op_count();
}

DrawScrollingContentsOp::DrawScrollingContentsOp(
    ElementId scroll_element_id,
    scoped_refptr<DisplayItemList> display_item_list)
    : PaintOp(kType),
      scroll_element_id(scroll_element_id),
      display_item_list(std::move(display_item_list)) {}

DrawScrollingContentsOp::~DrawScrollingContentsOp() = default;

size_t DrawScrollingContentsOp::AdditionalBytesUsed() const {
  return display_item_list->BytesUsed();
}

size_t DrawScrollingContentsOp::AdditionalOpCount() const {
  return display_item_list->TotalOpCount();
}

DrawVerticesOp::DrawVerticesOp() : PaintOpWithFlags(kType) {}

DrawVerticesOp::DrawVerticesOp(
    scoped_refptr<RefCountedBuffer<SkPoint>> vertices,
    scoped_refptr<RefCountedBuffer<SkPoint>> uvs,
    scoped_refptr<RefCountedBuffer<uint16_t>> indices,
    const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags),
      vertices(std::move(vertices)),
      uvs(std::move(uvs)),
      indices(std::move(indices)) {}

DrawVerticesOp::~DrawVerticesOp() = default;

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

bool DrawSkottieOp::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  if (images.empty()) {
    return false;
  }
  if (content_color_usage) {
    for (auto& [_, frame_data] : images) {
      *content_color_usage = std::max(*content_color_usage,
                                      frame_data.image.GetContentColorUsage());
    }
  }
  return true;
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

DrawSlugOp::DrawSlugOp(sk_sp<sktext::gpu::Slug> slug, const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags), slug(std::move(slug)) {}

DrawSlugOp::~DrawSlugOp() = default;

SaveLayerFiltersOp::SaveLayerFiltersOp(base::span<sk_sp<PaintFilter>> filters,
                                       const PaintFlags& flags)
    : PaintOpWithFlags(kType, flags),
      filters(std::make_move_iterator(filters.begin()),
              std::make_move_iterator(filters.end())) {}

SaveLayerFiltersOp::SaveLayerFiltersOp() : PaintOpWithFlags(kType) {}

SaveLayerFiltersOp::~SaveLayerFiltersOp() = default;

bool AreLiteOpsEnabled() {
  static const bool enabled = base::FeatureList::IsEnabled(kUseLitePaintOps);
  return enabled;
}

}  // namespace cc
