// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/scoped_raster_flags.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {
DrawImage CreateDrawImage(const PaintImage& image,
                          const PaintFlags* flags,
                          const SkMatrix& matrix) {
  if (!image)
    return DrawImage();
  return DrawImage(image, SkIRect::MakeWH(image.width(), image.height()),
                   flags ? flags->getFilterQuality() : kLow_SkFilterQuality,
                   matrix);
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
                                     const PaintOp::SerializeOptions& options);
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

#define M(T)                                               \
  static_assert(alignof(T) <= PaintOpBuffer::PaintOpAlign, \
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
    case PaintOpType::Translate:
      return "Translate";
  }
  return "UNKNOWN";
}

std::ostream& operator<<(std::ostream& os, PaintOpType type) {
  return os << PaintOpTypeToString(type);
}

template <typename T>
size_t SimpleSerialize(const PaintOp* op, void* memory, size_t size) {
  if (sizeof(T) > size)
    return 0;
  memcpy(memory, op, sizeof(T));
  return sizeof(T);
}

PlaybackParams::PlaybackParams(ImageProvider* image_provider)
    : image_provider(image_provider),
      original_ctm(SkMatrix::I()),
      custom_callback(CustomDataRasterCallback()),
      did_draw_op_callback(DidDrawOpCallback()) {}

PlaybackParams::PlaybackParams(ImageProvider* image_provider,
                               const SkMatrix& original_ctm,
                               CustomDataRasterCallback custom_callback,
                               DidDrawOpCallback did_draw_op_callback)
    : image_provider(image_provider),
      original_ctm(original_ctm),
      custom_callback(custom_callback),
      did_draw_op_callback(did_draw_op_callback) {}

PlaybackParams::~PlaybackParams() {}

PlaybackParams::PlaybackParams(const PlaybackParams& other) = default;
PlaybackParams& PlaybackParams::operator=(const PlaybackParams& other) =
    default;

PaintOp::SerializeOptions::SerializeOptions(
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache,
    ClientPaintCache* paint_cache,
    SkCanvas* canvas,
    SkStrikeServer* strike_server,
    sk_sp<SkColorSpace> color_space,
    bool can_use_lcd_text,
    bool context_supports_distance_field_text,
    int max_texture_size,
    size_t max_texture_bytes,
    const SkMatrix& original_ctm)
    : image_provider(image_provider),
      transfer_cache(transfer_cache),
      paint_cache(paint_cache),
      canvas(canvas),
      strike_server(strike_server),
      color_space(std::move(color_space)),
      can_use_lcd_text(can_use_lcd_text),
      context_supports_distance_field_text(
          context_supports_distance_field_text),
      max_texture_size(max_texture_size),
      max_texture_bytes(max_texture_bytes),
      original_ctm(original_ctm) {}

PaintOp::SerializeOptions::SerializeOptions(const SerializeOptions&) = default;
PaintOp::SerializeOptions& PaintOp::SerializeOptions::operator=(
    const SerializeOptions&) = default;
PaintOp::SerializeOptions::~SerializeOptions() = default;

PaintOp::DeserializeOptions::DeserializeOptions(
    TransferCacheDeserializeHelper* transfer_cache,
    ServicePaintCache* paint_cache,
    SkStrikeClient* strike_client,
    std::vector<uint8_t>* scratch_buffer)
    : transfer_cache(transfer_cache),
      paint_cache(paint_cache),
      strike_client(strike_client),
      scratch_buffer(scratch_buffer) {
  DCHECK(scratch_buffer);
}

size_t AnnotateOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
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
                             const SerializeOptions& options) {
  auto* op = static_cast<const ClipPathOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  helper.Write(op->path);
  helper.Write(op->op);
  helper.Write(op->antialias);
  return helper.size();
}

size_t ClipRectOp::Serialize(const PaintOp* op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
  return SimpleSerialize<ClipRectOp>(op, memory, size);
}

size_t ClipRRectOp::Serialize(const PaintOp* op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  return SimpleSerialize<ClipRRectOp>(op, memory, size);
}

size_t ConcatOp::Serialize(const PaintOp* op,
                           void* memory,
                           size_t size,
                           const SerializeOptions& options) {
  return SimpleSerialize<ConcatOp>(op, memory, size);
}

size_t CustomDataOp::Serialize(const PaintOp* op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options) {
  return SimpleSerialize<CustomDataOp>(op, memory, size);
}

size_t DrawColorOp::Serialize(const PaintOp* op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  return SimpleSerialize<DrawColorOp>(op, memory, size);
}

size_t DrawDRRectOp::Serialize(const PaintOp* base_op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options) {
  auto* op = static_cast<const DrawDRRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->outer);
  helper.Write(op->inner);
  return helper.size();
}

size_t DrawImageOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  auto* op = static_cast<const DrawImageOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);

  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  helper.Write(CreateDrawImage(op->image, serialized_flags,
                               options.canvas->getTotalMatrix()),
               &scale_adjustment);
  helper.AlignMemory(alignof(SkScalar));
  helper.Write(scale_adjustment.width());
  helper.Write(scale_adjustment.height());

  helper.Write(op->left);
  helper.Write(op->top);
  return helper.size();
}

size_t DrawImageRectOp::Serialize(const PaintOp* base_op,
                                  void* memory,
                                  size_t size,
                                  const SerializeOptions& options) {
  auto* op = static_cast<const DrawImageRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);

  // This adjustment mirrors DiscardableImageMap::GatherDiscardableImage logic.
  SkMatrix matrix = options.canvas->getTotalMatrix();
  matrix.postConcat(
      SkMatrix::MakeRectToRect(op->src, op->dst, SkMatrix::kFill_ScaleToFit));
  // Note that we don't request subsets here since the GpuImageCache has no
  // optimizations for using subsets.
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  helper.Write(CreateDrawImage(op->image, serialized_flags, matrix),
               &scale_adjustment);
  helper.AlignMemory(alignof(SkScalar));
  helper.Write(scale_adjustment.width());
  helper.Write(scale_adjustment.height());

  helper.Write(op->src);
  helper.Write(op->dst);
  helper.Write(op->constraint);
  return helper.size();
}

size_t DrawIRectOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  auto* op = static_cast<const DrawIRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->rect);
  return helper.size();
}

size_t DrawLineOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
  auto* op = static_cast<const DrawLineOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.AlignMemory(alignof(SkScalar));
  helper.Write(op->x0);
  helper.Write(op->y0);
  helper.Write(op->x1);
  helper.Write(op->y1);
  return helper.size();
}

size_t DrawOvalOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
  auto* op = static_cast<const DrawOvalOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->oval);
  return helper.size();
}

size_t DrawPathOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
  auto* op = static_cast<const DrawPathOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->path);
  return helper.size();
}

size_t DrawRecordOp::Serialize(const PaintOp* op,
                               void* memory,
                               size_t size,
                               const SerializeOptions& options) {
  // TODO(enne): these must be flattened.  Serializing this will not do
  // anything.
  NOTREACHED();
  return 0u;
}

size_t DrawRectOp::Serialize(const PaintOp* base_op,
                             void* memory,
                             size_t size,
                             const SerializeOptions& options) {
  auto* op = static_cast<const DrawRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->rect);
  return helper.size();
}

size_t DrawRRectOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  auto* op = static_cast<const DrawRRectOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->rrect);
  return helper.size();
}

size_t DrawSkottieOp::Serialize(const PaintOp* op,
                                void* memory,
                                size_t size,
                                const SerializeOptions& options) {
  // TODO(malaykeshav): these must be flattened.  Serializing this will not do
  // anything. See https://crbug.com/894635
  NOTREACHED();
  return 0u;
}

size_t DrawTextBlobOp::Serialize(const PaintOp* base_op,
                                 void* memory,
                                 size_t size,
                                 const SerializeOptions& options) {
  auto* op = static_cast<const DrawTextBlobOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.AlignMemory(alignof(SkScalar));
  helper.Write(op->x);
  helper.Write(op->y);
  helper.Write(op->blob);
  return helper.size();
}

size_t NoopOp::Serialize(const PaintOp* op,
                         void* memory,
                         size_t size,
                         const SerializeOptions& options) {
  return SimpleSerialize<NoopOp>(op, memory, size);
}

size_t RestoreOp::Serialize(const PaintOp* op,
                            void* memory,
                            size_t size,
                            const SerializeOptions& options) {
  return SimpleSerialize<RestoreOp>(op, memory, size);
}

size_t RotateOp::Serialize(const PaintOp* op,
                           void* memory,
                           size_t size,
                           const SerializeOptions& options) {
  return SimpleSerialize<RotateOp>(op, memory, size);
}

size_t SaveOp::Serialize(const PaintOp* op,
                         void* memory,
                         size_t size,
                         const SerializeOptions& options) {
  return SimpleSerialize<SaveOp>(op, memory, size);
}

size_t SaveLayerOp::Serialize(const PaintOp* base_op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  auto* op = static_cast<const SaveLayerOp*>(base_op);
  PaintOpWriter helper(memory, size, options);
  const auto* serialized_flags = options.flags_to_serialize;
  if (!serialized_flags)
    serialized_flags = &op->flags;
  helper.Write(*serialized_flags);
  helper.Write(op->bounds);
  return helper.size();
}

size_t SaveLayerAlphaOp::Serialize(const PaintOp* op,
                                   void* memory,
                                   size_t size,
                                   const SerializeOptions& options) {
  return SimpleSerialize<SaveLayerAlphaOp>(op, memory, size);
}

size_t ScaleOp::Serialize(const PaintOp* op,
                          void* memory,
                          size_t size,
                          const SerializeOptions& options) {
  return SimpleSerialize<ScaleOp>(op, memory, size);
}

size_t SetMatrixOp::Serialize(const PaintOp* op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  if (options.original_ctm.isIdentity())
    return SimpleSerialize<SetMatrixOp>(op, memory, size);

  SetMatrixOp transformed(*static_cast<const SetMatrixOp*>(op));
  transformed.matrix.postConcat(options.original_ctm);
  return SimpleSerialize<SetMatrixOp>(&transformed, memory, size);
}

size_t TranslateOp::Serialize(const PaintOp* op,
                              void* memory,
                              size_t size,
                              const SerializeOptions& options) {
  return SimpleSerialize<TranslateOp>(op, memory, size);
}

template <typename T>
void UpdateTypeAndSkip(T* op) {
  op->type = static_cast<uint8_t>(T::kType);
  op->skip = PaintOpBuffer::ComputeOpSkip(sizeof(T));
}

template <typename T>
T* SimpleDeserialize(const volatile void* input,
                     size_t input_size,
                     void* output,
                     size_t output_size) {
  if (input_size < sizeof(T))
    return nullptr;
  memcpy(output, const_cast<void*>(input), sizeof(T));

  T* op = reinterpret_cast<T*>(output);
  if (!op->IsValid())
    return nullptr;
  // Type and skip were already read once, so could have been changed.
  // Don't trust them and clobber them with something valid.
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* AnnotateOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(AnnotateOp));
  AnnotateOp* op = new (output) AnnotateOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->annotation_type);
  helper.Read(&op->rect);
  helper.Read(&op->data);
  if (!helper.valid() || !op->IsValid()) {
    op->~AnnotateOp();
    return nullptr;
  }

  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* ClipPathOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipPathOp));
  ClipPathOp* op = new (output) ClipPathOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->path);
  helper.Read(&op->op);
  helper.Read(&op->antialias);
  if (!helper.valid() || !op->IsValid()) {
    op->~ClipPathOp();
    return nullptr;
  }

  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* ClipRectOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipRectOp));
  return SimpleDeserialize<ClipRectOp>(input, input_size, output, output_size);
}

PaintOp* ClipRRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ClipRRectOp));
  return SimpleDeserialize<ClipRRectOp>(input, input_size, output, output_size);
}

PaintOp* ConcatOp::Deserialize(const volatile void* input,
                               size_t input_size,
                               void* output,
                               size_t output_size,
                               const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ConcatOp));
  auto* op =
      SimpleDeserialize<ConcatOp>(input, input_size, output, output_size);
  if (op)
    PaintOpReader::FixupMatrixPostSerialization(&op->matrix);
  return op;
}

PaintOp* CustomDataOp::Deserialize(const volatile void* input,
                                   size_t input_size,
                                   void* output,
                                   size_t output_size,
                                   const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(CustomDataOp));
  return SimpleDeserialize<CustomDataOp>(input, input_size, output,
                                         output_size);
}

PaintOp* DrawColorOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawColorOp));
  return SimpleDeserialize<DrawColorOp>(input, input_size, output, output_size);
}

PaintOp* DrawDRRectOp::Deserialize(const volatile void* input,
                                   size_t input_size,
                                   void* output,
                                   size_t output_size,
                                   const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawDRRectOp));
  DrawDRRectOp* op = new (output) DrawDRRectOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->outer);
  helper.Read(&op->inner);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawDRRectOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawImageOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawImageOp));
  DrawImageOp* op = new (output) DrawImageOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);

  helper.Read(&op->image);
  helper.AlignMemory(alignof(SkScalar));
  helper.Read(&op->scale_adjustment.fWidth);
  helper.Read(&op->scale_adjustment.fHeight);

  helper.Read(&op->left);
  helper.Read(&op->top);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawImageOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawImageRectOp::Deserialize(const volatile void* input,
                                      size_t input_size,
                                      void* output,
                                      size_t output_size,
                                      const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawImageRectOp));
  DrawImageRectOp* op = new (output) DrawImageRectOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);

  helper.Read(&op->image);
  helper.AlignMemory(alignof(SkScalar));
  helper.Read(&op->scale_adjustment.fWidth);
  helper.Read(&op->scale_adjustment.fHeight);

  helper.Read(&op->src);
  helper.Read(&op->dst);
  helper.Read(&op->constraint);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawImageRectOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawIRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawIRectOp));
  DrawIRectOp* op = new (output) DrawIRectOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->rect);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawIRectOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawLineOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawLineOp));
  DrawLineOp* op = new (output) DrawLineOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.AlignMemory(alignof(SkScalar));
  helper.Read(&op->x0);
  helper.Read(&op->y0);
  helper.Read(&op->x1);
  helper.Read(&op->y1);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawLineOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawOvalOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawOvalOp));
  DrawOvalOp* op = new (output) DrawOvalOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->oval);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawOvalOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawPathOp::Deserialize(const volatile void* input,
                                 size_t input_size,
                                 void* output,
                                 size_t output_size,
                                 const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawPathOp));
  DrawPathOp* op = new (output) DrawPathOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->path);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawPathOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
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
  DrawRectOp* op = new (output) DrawRectOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->rect);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawRectOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawRRectOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawRRectOp));
  DrawRRectOp* op = new (output) DrawRRectOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->rrect);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawRRectOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* DrawSkottieOp::Deserialize(const volatile void* input,
                                    size_t input_size,
                                    void* output,
                                    size_t output_size,
                                    const DeserializeOptions& options) {
  // TODO(malaykeshav): these must be flattened and not sent directly.
  // See https://crbug.com/894635
  return nullptr;
}

PaintOp* DrawTextBlobOp::Deserialize(const volatile void* input,
                                     size_t input_size,
                                     void* output,
                                     size_t output_size,
                                     const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(DrawTextBlobOp) - sizeof(NodeId));
  DrawTextBlobOp* op = new (output) DrawTextBlobOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.AlignMemory(alignof(SkScalar));
  helper.Read(&op->x);
  helper.Read(&op->y);
  helper.Read(&op->blob);
  if (!helper.valid() || !op->IsValid()) {
    op->~DrawTextBlobOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* NoopOp::Deserialize(const volatile void* input,
                             size_t input_size,
                             void* output,
                             size_t output_size,
                             const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(NoopOp));
  return SimpleDeserialize<NoopOp>(input, input_size, output, output_size);
}

PaintOp* RestoreOp::Deserialize(const volatile void* input,
                                size_t input_size,
                                void* output,
                                size_t output_size,
                                const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(RestoreOp));
  return SimpleDeserialize<RestoreOp>(input, input_size, output, output_size);
}

PaintOp* RotateOp::Deserialize(const volatile void* input,
                               size_t input_size,
                               void* output,
                               size_t output_size,
                               const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(RotateOp));
  return SimpleDeserialize<RotateOp>(input, input_size, output, output_size);
}

PaintOp* SaveOp::Deserialize(const volatile void* input,
                             size_t input_size,
                             void* output,
                             size_t output_size,
                             const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveOp));
  return SimpleDeserialize<SaveOp>(input, input_size, output, output_size);
}

PaintOp* SaveLayerOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveLayerOp));
  SaveLayerOp* op = new (output) SaveLayerOp;

  PaintOpReader helper(input, input_size, options);
  helper.Read(&op->flags);
  helper.Read(&op->bounds);
  if (!helper.valid() || !op->IsValid()) {
    op->~SaveLayerOp();
    return nullptr;
  }
  UpdateTypeAndSkip(op);
  return op;
}

PaintOp* SaveLayerAlphaOp::Deserialize(const volatile void* input,
                                       size_t input_size,
                                       void* output,
                                       size_t output_size,
                                       const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SaveLayerAlphaOp));
  return SimpleDeserialize<SaveLayerAlphaOp>(input, input_size, output,
                                             output_size);
}

PaintOp* ScaleOp::Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(ScaleOp));

  return SimpleDeserialize<ScaleOp>(input, input_size, output, output_size);
}

PaintOp* SetMatrixOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(SetMatrixOp));
  auto* op =
      SimpleDeserialize<SetMatrixOp>(input, input_size, output, output_size);
  if (op)
    PaintOpReader::FixupMatrixPostSerialization(&op->matrix);
  return op;
}

PaintOp* TranslateOp::Deserialize(const volatile void* input,
                                  size_t input_size,
                                  void* output,
                                  size_t output_size,
                                  const DeserializeOptions& options) {
  DCHECK_GE(output_size, sizeof(TranslateOp));
  return SimpleDeserialize<TranslateOp>(input, input_size, output, output_size);
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
    canvas->drawImage(op->image.GetSkImage().get(), op->left, op->top, &paint);
    return;
  }

  DrawImage draw_image(
      op->image, SkIRect::MakeWH(op->image.width(), op->image.height()),
      flags ? flags->getFilterQuality() : kNone_SkFilterQuality,
      canvas->getTotalMatrix());
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
  paint.setFilterQuality(decoded_image.filter_quality());
  canvas->drawImage(decoded_image.image().get(), op->left, op->top, &paint);
}

void DrawImageRectOp::RasterWithFlags(const DrawImageRectOp* op,
                                      const PaintFlags* flags,
                                      SkCanvas* canvas,
                                      const PlaybackParams& params) {
  SkPaint paint = flags ? flags->ToSkPaint() : SkPaint();
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

    DCHECK(IsScaleAdjustmentIdentity(op->scale_adjustment));
    SkAutoCanvasRestore save_restore(canvas, true);
    canvas->concat(
        SkMatrix::MakeRectToRect(op->src, op->dst, SkMatrix::kFill_ScaleToFit));
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

  // TODO(enne): Probably PaintCanvas should just use the skia enum directly.
  SkCanvas::SrcRectConstraint skconstraint =
      static_cast<SkCanvas::SrcRectConstraint>(op->constraint);

  if (!params.image_provider) {
    SkRect adjusted_src = AdjustSrcRectForScale(op->src, op->scale_adjustment);
    canvas->drawImageRect(op->image.GetSkImage().get(), adjusted_src, op->dst,
                          &paint, skconstraint);
    return;
  }

  SkMatrix matrix;
  matrix.setRectToRect(op->src, op->dst, SkMatrix::kFill_ScaleToFit);
  matrix.postConcat(canvas->getTotalMatrix());

  SkIRect int_src_rect;
  op->src.roundOut(&int_src_rect);

  DrawImage draw_image(
      op->image, int_src_rect,
      flags ? flags->getFilterQuality() : kNone_SkFilterQuality, matrix);
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
  paint.setFilterQuality(decoded_image.filter_quality());
  canvas->drawImageRect(decoded_image.image().get(), adjusted_src, op->dst,
                        &paint, skconstraint);
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
  op->skottie->Draw(canvas, op->t, op->dst);
}

void DrawTextBlobOp::RasterWithFlags(const DrawTextBlobOp* op,
                                     const PaintFlags* flags,
                                     SkCanvas* canvas,
                                     const PlaybackParams& params) {
  flags->DrawToSk(canvas, [op](SkCanvas* c, const SkPaint& p) {
    c->drawTextBlob(op->blob.get(), op->x, op->y, p);
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
  canvas->saveLayerAlpha(unset ? nullptr : &op->bounds, op->alpha);
}

void ScaleOp::Raster(const ScaleOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {
  canvas->scale(op->sx, op->sy);
}

void SetMatrixOp::Raster(const SetMatrixOp* op,
                         SkCanvas* canvas,
                         const PlaybackParams& params) {
  canvas->setMatrix(SkMatrix::Concat(params.original_ctm, op->matrix));
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
  return AreSkMatricesEqual(left->matrix, right->matrix);
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

  SkSerialProcs default_procs;
  return left->blob->serialize(default_procs)
      ->equals(right->blob->serialize(default_procs).get());
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
  if (!AreSkMatricesEqual(left->matrix, right->matrix))
    return false;
  return true;
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
                          const SerializeOptions& options) const {
  DCHECK(options.transfer_cache);
  DCHECK(options.canvas);

  // Need at least enough room for a skip/type header.
  if (size < 4)
    return 0u;

  DCHECK_EQ(0u,
            reinterpret_cast<uintptr_t>(memory) % PaintOpBuffer::PaintOpAlign);

  size_t written = g_serialize_functions[type](this, memory, size, options);
  DCHECK_LE(written, size);
  if (written < 4)
    return 0u;

  size_t aligned_written = ((written + PaintOpBuffer::PaintOpAlign - 1) &
                            ~(PaintOpBuffer::PaintOpAlign - 1));
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

// static
bool PaintOp::GetBounds(const PaintOp* op, SkRect* rect) {
  DCHECK(op->IsDrawOp());

  switch (op->GetType()) {
    case PaintOpType::DrawColor:
      return false;
    case PaintOpType::DrawDRRect: {
      auto* rect_op = static_cast<const DrawDRRectOp*>(op);
      *rect = rect_op->outer.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawImage: {
      auto* image_op = static_cast<const DrawImageOp*>(op);
      *rect =
          SkRect::MakeXYWH(image_op->left, image_op->top,
                           image_op->image.width(), image_op->image.height());
      rect->sort();
      return true;
    }
    case PaintOpType::DrawImageRect: {
      auto* image_rect_op = static_cast<const DrawImageRectOp*>(op);
      *rect = image_rect_op->dst;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawIRect: {
      auto* rect_op = static_cast<const DrawIRectOp*>(op);
      *rect = SkRect::Make(rect_op->rect);
      rect->sort();
      return true;
    }
    case PaintOpType::DrawLine: {
      auto* line_op = static_cast<const DrawLineOp*>(op);
      rect->setLTRB(line_op->x0, line_op->y0, line_op->x1, line_op->y1);
      rect->sort();
      return true;
    }
    case PaintOpType::DrawOval: {
      auto* oval_op = static_cast<const DrawOvalOp*>(op);
      *rect = oval_op->oval;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawPath: {
      auto* path_op = static_cast<const DrawPathOp*>(op);
      *rect = path_op->path.getBounds();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRect: {
      auto* rect_op = static_cast<const DrawRectOp*>(op);
      *rect = rect_op->rect;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRRect: {
      auto* rect_op = static_cast<const DrawRRectOp*>(op);
      *rect = rect_op->rrect.rect();
      rect->sort();
      return true;
    }
    case PaintOpType::DrawRecord:
      return false;
    case PaintOpType::DrawSkottie: {
      auto* skottie_op = static_cast<const DrawSkottieOp*>(op);
      *rect = skottie_op->dst;
      rect->sort();
      return true;
    }
    case PaintOpType::DrawTextBlob: {
      auto* text_op = static_cast<const DrawTextBlobOp*>(op);
      *rect = text_op->blob->bounds().makeOffset(text_op->x, text_op->y);
      rect->sort();
      return true;
    }
    default:
      NOTREACHED();
  }
  return false;
}

// static
bool PaintOp::QuickRejectDraw(const PaintOp* op, const SkCanvas* canvas) {
  if (!op->IsDrawOp())
    return false;

  SkRect rect;
  if (!PaintOp::GetBounds(op, &rect))
    return false;

  if (op->IsPaintOpWithFlags()) {
    SkPaint paint = static_cast<const PaintOpWithFlags*>(op)->flags.ToSkPaint();
    if (!paint.canComputeFastBounds())
      return false;
    paint.computeFastBounds(rect, &rect);
  }

  return canvas->quickReject(rect);
}

// static
bool PaintOp::OpHasDiscardableImages(const PaintOp* op) {
  if (op->IsPaintOpWithFlags() && static_cast<const PaintOpWithFlags*>(op)
                                      ->HasDiscardableImagesFromFlags()) {
    return true;
  }

  if (op->GetType() == PaintOpType::DrawImage &&
      static_cast<const DrawImageOp*>(op)->HasDiscardableImages()) {
    return true;
  } else if (op->GetType() == PaintOpType::DrawImageRect &&
             static_cast<const DrawImageRectOp*>(op)->HasDiscardableImages()) {
    return true;
  } else if (op->GetType() == PaintOpType::DrawRecord &&
             static_cast<const DrawRecordOp*>(op)->HasDiscardableImages()) {
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
  return record->numSlowPaths();
}

bool DrawRecordOp::HasNonAAPaint() const {
  return record->HasNonAAPaint();
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

DrawImageOp::DrawImageOp(const PaintImage& image,
                         SkScalar left,
                         SkScalar top,
                         const PaintFlags* flags)
    : PaintOpWithFlags(kType, flags ? *flags : PaintFlags()),
      image(image),
      left(left),
      top(top) {}

bool DrawImageOp::HasDiscardableImages() const {
  return image && !image.IsTextureBacked();
}

DrawImageOp::~DrawImageOp() = default;

DrawImageRectOp::DrawImageRectOp() : PaintOpWithFlags(kType) {}

DrawImageRectOp::DrawImageRectOp(const PaintImage& image,
                                 const SkRect& src,
                                 const SkRect& dst,
                                 const PaintFlags* flags,
                                 PaintCanvas::SrcRectConstraint constraint)
    : PaintOpWithFlags(kType, flags ? *flags : PaintFlags()),
      image(image),
      src(src),
      dst(dst),
      constraint(constraint) {}

bool DrawImageRectOp::HasDiscardableImages() const {
  return image && !image.IsTextureBacked();
}

DrawImageRectOp::~DrawImageRectOp() = default;

DrawRecordOp::DrawRecordOp(sk_sp<const PaintRecord> record)
    : PaintOp(kType), record(std::move(record)) {}

DrawRecordOp::~DrawRecordOp() = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record->bytes_used();
}

size_t DrawRecordOp::AdditionalOpCount() const {
  return record->total_op_count();
}

DrawSkottieOp::DrawSkottieOp(scoped_refptr<SkottieWrapper> skottie,
                             SkRect dst,
                             float t)
    : PaintOp(kType), skottie(std::move(skottie)), dst(dst), t(t) {}

DrawSkottieOp::DrawSkottieOp() : PaintOp(kType) {}

DrawSkottieOp::~DrawSkottieOp() = default;

bool DrawRecordOp::HasDiscardableImages() const {
  return record->HasDiscardableImages();
}

bool DrawRecordOp::HasText() const {
  return record->HasText();
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

PaintOpBuffer::CompositeIterator::CompositeIterator(
    const PaintOpBuffer* buffer,
    const std::vector<size_t>* offsets)
    : using_offsets_(!!offsets) {
  if (using_offsets_)
    offset_iter_.emplace(buffer, offsets);
  else
    iter_.emplace(buffer);
}

PaintOpBuffer::CompositeIterator::CompositeIterator(
    const CompositeIterator& other) = default;
PaintOpBuffer::CompositeIterator::CompositeIterator(CompositeIterator&& other) =
    default;

PaintOpBuffer::PaintOpBuffer()
    : has_non_aa_paint_(false),
      has_discardable_images_(false),
      has_text_(false) {}

PaintOpBuffer::PaintOpBuffer(PaintOpBuffer&& other) {
  *this = std::move(other);
}

PaintOpBuffer::~PaintOpBuffer() {
  Reset();
}

PaintOpBuffer& PaintOpBuffer::operator=(PaintOpBuffer&& other) {
  data_ = std::move(other.data_);
  used_ = other.used_;
  reserved_ = other.reserved_;
  op_count_ = other.op_count_;
  num_slow_paths_ = other.num_slow_paths_;
  subrecord_bytes_used_ = other.subrecord_bytes_used_;
  subrecord_op_count_ = other.subrecord_op_count_;
  has_non_aa_paint_ = other.has_non_aa_paint_;
  has_discardable_images_ = other.has_discardable_images_;
  has_text_ = other.has_text_;

  // Make sure the other pob can destruct safely.
  other.used_ = 0;
  other.op_count_ = 0;
  other.reserved_ = 0;
  return *this;
}

void PaintOpBuffer::Reset() {
  for (auto* op : Iterator(this))
    op->DestroyThis();

  // Leave data_ allocated, reserved_ unchanged. ShrinkToFit will take care of
  // that if called.
  used_ = 0;
  op_count_ = 0;
  num_slow_paths_ = 0;
  has_non_aa_paint_ = false;
  subrecord_bytes_used_ = 0;
  subrecord_op_count_ = 0;
  has_discardable_images_ = false;
  has_text_ = false;
}

// When |op| is a nested PaintOpBuffer, this returns the PaintOp inside
// that buffer if the buffer contains a single drawing op, otherwise it
// returns null. This searches recursively if the PaintOpBuffer contains only
// another PaintOpBuffer.
static const PaintOp* GetNestedSingleDrawingOp(const PaintOp* op) {
  if (!op->IsDrawOp())
    return nullptr;
  while (op->GetType() == PaintOpType::DrawRecord) {
    auto* draw_record_op = static_cast<const DrawRecordOp*>(op);
    if (draw_record_op->record->size() > 1) {
      // If there's more than one op, then we need to keep the
      // SaveLayer.
      return nullptr;
    }

    // Recurse into the single-op DrawRecordOp and make sure it's a
    // drawing op.
    op = draw_record_op->record->GetFirstOp();
    if (!op->IsDrawOp())
      return nullptr;
  }

  return op;
}

void PaintOpBuffer::Playback(SkCanvas* canvas) const {
  Playback(canvas, PlaybackParams(nullptr), nullptr);
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params) const {
  Playback(canvas, params, nullptr);
}

PaintOpBuffer::PlaybackFoldingIterator::PlaybackFoldingIterator(
    const PaintOpBuffer* buffer,
    const std::vector<size_t>* offsets)
    : iter_(buffer, offsets),
      folded_draw_color_(SK_ColorTRANSPARENT, SkBlendMode::kSrcOver) {
  FindNextOp();
}

PaintOpBuffer::PlaybackFoldingIterator::~PlaybackFoldingIterator() = default;

void PaintOpBuffer::PlaybackFoldingIterator::FindNextOp() {
  current_alpha_ = 255u;
  for (current_op_ = NextUnfoldedOp(); current_op_;
       current_op_ = NextUnfoldedOp()) {
    if (current_op_->GetType() != PaintOpType::SaveLayerAlpha)
      break;
    const PaintOp* second = NextUnfoldedOp();
    if (!second)
      break;

    if (second->GetType() == PaintOpType::Restore) {
      // Drop a SaveLayerAlpha/Restore combo.
      continue;
    }

    // Find a nested drawing PaintOp to replace |second| if possible, while
    // holding onto the pointer to |second| in case we can't find a nested
    // drawing op to replace it with.
    const PaintOp* draw_op = GetNestedSingleDrawingOp(second);

    const PaintOp* third = nullptr;
    if (draw_op) {
      third = NextUnfoldedOp();
      if (third && third->GetType() == PaintOpType::Restore) {
        auto* save_op = static_cast<const SaveLayerAlphaOp*>(current_op_);
        if (draw_op->IsPaintOpWithFlags() &&
            // SkPaint::drawTextBlob() applies alpha on each glyph so we don't
            // fold SaveLayerAlpha into DrwaTextBlob to ensure correct alpha
            // even if some glyphs overlap.
            draw_op->GetType() != PaintOpType::DrawTextBlob) {
          auto* flags_op = static_cast<const PaintOpWithFlags*>(draw_op);
          if (flags_op->flags.SupportsFoldingAlpha()) {
            current_alpha_ = save_op->alpha;
            current_op_ = draw_op;
            break;
          }
        } else if (draw_op->GetType() == PaintOpType::DrawColor &&
                   static_cast<const DrawColorOp*>(draw_op)->mode ==
                       SkBlendMode::kSrcOver) {
          auto* draw_color_op = static_cast<const DrawColorOp*>(draw_op);
          SkColor color = draw_color_op->color;
          folded_draw_color_.color = SkColorSetARGB(
              SkMulDiv255Round(save_op->alpha, SkColorGetA(color)),
              SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
          current_op_ = &folded_draw_color_;
          break;
        }
      }
    }

    // If we get here, then we could not find a foldable sequence after
    // this SaveLayerAlpha, so store any peeked at ops.
    stack_->push_back(second);
    if (third)
      stack_->push_back(third);
    break;
  }
}

const PaintOp* PaintOpBuffer::PlaybackFoldingIterator::NextUnfoldedOp() {
  if (stack_->size()) {
    const PaintOp* op = stack_->front();
    // Shift paintops forward.
    stack_->erase(stack_->begin());
    return op;
  }
  if (!iter_)
    return nullptr;
  const PaintOp* op = *iter_;
  ++iter_;
  return op;
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params,
                             const std::vector<size_t>* offsets) const {
  if (!op_count_)
    return;
  if (offsets && offsets->empty())
    return;

  // Prevent PaintOpBuffers from having side effects back into the canvas.
  SkAutoCanvasRestore save_restore(canvas, true);

  // TODO(enne): a PaintRecord that contains a SetMatrix assumes that the
  // SetMatrix is local to that PaintRecord itself.  Said differently, if you
  // translate(x, y), then draw a paint record with a SetMatrix(identity),
  // the translation should be preserved instead of clobbering the top level
  // transform.  This could probably be done more efficiently.
  PlaybackParams new_params(params.image_provider, canvas->getTotalMatrix(),
                            params.custom_callback,
                            params.did_draw_op_callback);
  for (PlaybackFoldingIterator iter(this, offsets); iter; ++iter) {
    const PaintOp* op = *iter;

    // This is an optimization to replicate the behaviour in SkCanvas
    // which rejects ops that draw outside the current clip. In the
    // general case we defer this to the SkCanvas but if we will be
    // using an ImageProvider for pre-decoding images, we can save
    // performing an expensive decode that will never be rasterized.
    const bool skip_op = new_params.image_provider &&
                         PaintOp::OpHasDiscardableImages(op) &&
                         PaintOp::QuickRejectDraw(op, canvas);
    if (skip_op)
      continue;

    if (op->IsPaintOpWithFlags()) {
      const auto* flags_op = static_cast<const PaintOpWithFlags*>(op);
      auto* context = canvas->getGrContext();
      const ScopedRasterFlags scoped_flags(
          &flags_op->flags, new_params.image_provider, canvas->getTotalMatrix(),
          context ? context->maxTextureSize() : 0, iter.alpha());
      if (const auto* raster_flags = scoped_flags.flags())
        flags_op->RasterWithFlags(canvas, raster_flags, new_params);
    } else {
      // TODO(enne): skip SaveLayer followed by restore with nothing in
      // between, however SaveLayer with image filters on it (or maybe
      // other PaintFlags options) are not a noop.  Figure out what these
      // are so we can skip them correctly.
      DCHECK_EQ(iter.alpha(), 255);
      op->Raster(canvas, new_params);
    }

    if (!new_params.did_draw_op_callback.is_null())
      new_params.did_draw_op_callback.Run();
  }
}

bool PaintOpBuffer::Deserialize(const volatile void* input,
                                size_t input_size,
                                const PaintOp::DeserializeOptions& options) {
  Reset();
  size_t total_bytes_read = 0u;
  while (total_bytes_read < input_size) {
    const volatile void* next_op =
        static_cast<const volatile char*>(input) + total_bytes_read;

    uint8_t type;
    uint32_t skip;
    if (!PaintOpReader::ReadAndValidateOpHeader(
            next_op, input_size - total_bytes_read, &type, &skip)) {
      return false;
    }

    size_t op_skip = ComputeOpSkip(g_type_to_size[type]);
    const auto* op = g_deserialize_functions[type](
        next_op, skip, AllocatePaintOp(op_skip), op_skip, options);
    if (!op) {
      // The last allocated op has already been destroyed if it failed to
      // deserialize. Update the buffer's op tracking to exclude it to avoid
      // access during cleanup at destruction.
      used_ -= op_skip;
      op_count_--;
      return false;
    }

    g_analyze_op_functions[type](this, op);
    total_bytes_read += skip;
  }

  DCHECK_GT(size(), 0u);
  return true;
}

// static
sk_sp<PaintOpBuffer> PaintOpBuffer::MakeFromMemory(
    const volatile void* input,
    size_t input_size,
    const PaintOp::DeserializeOptions& options) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  if (input_size == 0)
    return buffer;
  if (!buffer->Deserialize(input, input_size, options))
    return nullptr;
  return buffer;
}

void PaintOpBuffer::ReallocBuffer(size_t new_size) {
  DCHECK_GE(new_size, used_);
  std::unique_ptr<char, base::AlignedFreeDeleter> new_data(
      static_cast<char*>(base::AlignedAlloc(new_size, PaintOpAlign)));
  if (data_)
    memcpy(new_data.get(), data_.get(), used_);
  data_ = std::move(new_data);
  reserved_ = new_size;
}

void* PaintOpBuffer::AllocatePaintOp(size_t skip) {
  DCHECK_LT(skip, PaintOp::kMaxSkip);
  if (used_ + skip > reserved_) {
    // Start reserved_ at kInitialBufferSize and then double.
    // ShrinkToFit can make this smaller afterwards.
    size_t new_size = reserved_ ? reserved_ : kInitialBufferSize;
    while (used_ + skip > new_size)
      new_size *= 2;
    ReallocBuffer(new_size);
  }
  DCHECK_LE(used_ + skip, reserved_);

  void* op = data_.get() + used_;
  used_ += skip;
  op_count_++;
  return op;
}

void PaintOpBuffer::ShrinkToFit() {
  if (used_ == reserved_)
    return;
  if (!used_) {
    reserved_ = 0;
    data_.reset();
  } else {
    ReallocBuffer(used_);
  }
}

bool PaintOpBuffer::operator==(const PaintOpBuffer& other) const {
  if (op_count_ != other.op_count_)
    return false;
  if (num_slow_paths_ != other.num_slow_paths_)
    return false;
  if (subrecord_bytes_used_ != other.subrecord_bytes_used_)
    return false;
  if (subrecord_op_count_ != other.subrecord_op_count_)
    return false;
  if (has_non_aa_paint_ != other.has_non_aa_paint_)
    return false;
  if (has_discardable_images_ != other.has_discardable_images_)
    return false;
  if (has_text_ != other.has_text_)
    return false;

  auto left_iter = Iterator(this);
  auto right_iter = Iterator(&other);

  for (; left_iter != left_iter.end(); ++left_iter, ++right_iter) {
    if (**left_iter != **right_iter)
      return false;
  }

  DCHECK(left_iter == left_iter.end());
  DCHECK(right_iter == right_iter.end());
  return true;
}

}  // namespace cc
