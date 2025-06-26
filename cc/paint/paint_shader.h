// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_SHADER_H_
#define CC_PAINT_PAINT_SHADER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "cc/paint/image_analysis_state.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "ui/gfx/geometry/size_f.h"

class SkShader;

namespace gpu {
struct Mailbox;
}

namespace cc {
class ImageProvider;

class CC_PAINT_EXPORT PaintShader : public SkRefCnt {
 public:
  enum class Type : uint8_t {
    kEmpty,
    kColor,
    kLinearGradient,
    kRadialGradient,
    kTwoPointConicalGradient,
    kSweepGradient,
    kImage,
    kPaintRecord,
    kSkSLCommand,
    kShaderCount
  };

  using RecordShaderId = uint32_t;
  static const RecordShaderId kInvalidRecordShaderId;

  // Scaling behavior dictates how a PaintRecord shader will behave. Use
  // RasterAtScale to create a picture shader. Use FixedScale to create an image
  // shader that is backed by the paint record.
  enum class ScalingBehavior : uint8_t { kRasterAtScale, kFixedScale };

  static sk_sp<PaintShader> MakeEmpty();

  static sk_sp<PaintShader> MakeColor(SkColor4f color);

  // TODO(crbug.com/40735471) SkMatrix is deprecated in favor of SkM44.
  static sk_sp<PaintShader> MakeLinearGradient(
      const SkPoint* points,
      const SkColor4f colors[],
      const SkScalar* pos,
      int count,
      SkTileMode mode,
      SkGradientShader::Interpolation interpolation = DefaultInterpolation(),
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor4f fallback_color = SkColors::kTransparent);

  static sk_sp<PaintShader> MakeRadialGradient(
      const SkPoint& center,
      SkScalar radius,
      const SkColor4f colors[],
      const SkScalar pos[],
      int color_count,
      SkTileMode mode,
      SkGradientShader::Interpolation interpolation = DefaultInterpolation(),
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor4f fallback_color = SkColors::kTransparent);

  static sk_sp<PaintShader> MakeTwoPointConicalGradient(
      const SkPoint& start,
      SkScalar start_radius,
      const SkPoint& end,
      SkScalar end_radius,
      const SkColor4f colors[],
      const SkScalar pos[],
      int color_count,
      SkTileMode mode,
      SkGradientShader::Interpolation interpolation = DefaultInterpolation(),
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor4f fallback_color = SkColors::kTransparent);

  static sk_sp<PaintShader> MakeSweepGradient(
      SkScalar cx,
      SkScalar cy,
      const SkColor4f colors[],
      const SkScalar pos[],
      int color_count,
      SkTileMode mode,
      SkScalar start_degrees,
      SkScalar end_degrees,
      SkGradientShader::Interpolation interpolation = DefaultInterpolation(),
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor4f fallback_color = SkColors::kTransparent);

  // |tile_rect| is not null only if the |image| is paint worklet backed.
  static sk_sp<PaintShader> MakeImage(const PaintImage& image,
                                      SkTileMode tx,
                                      SkTileMode ty,
                                      const SkMatrix* local_matrix,
                                      const SkRect* tile_rect = nullptr);

  static sk_sp<PaintShader> MakePaintRecord(
      PaintRecord record,
      const SkRect& tile,
      SkTileMode tx,
      SkTileMode ty,
      const SkMatrix* local_matrix,
      ScalingBehavior scaling_behavior = ScalingBehavior::kRasterAtScale);

  // Returns null if the `sksl` command is invalid.
  //
  // NOTE:
  // - This is only intended for trusted shader (e.g., shaders that are part of
  //   the Chromium binary). GPU service has security constraints to prevent the
  //   PaintShader being deserialized if it is not serialized from the browser
  //   thread.
  // - Not using flat_map because SkString does not have built-in comparator.
  template <typename ValueType>
  struct Uniform {
    SkString name;
    ValueType value;

    bool operator==(const Uniform& other) const {
      return name == other.name && value == other.value;
    }
  };
  using FloatUniform = Uniform<SkScalar>;
  using Float2Uniform = Uniform<SkV2>;
  using Float4Uniform = Uniform<SkV4>;
  using IntUniform = Uniform<int>;
  constexpr static size_t kMaxNumUniformsPerType = 16u;
  static sk_sp<PaintShader> MakeSkSLCommand(
      std::string_view sksl,
      std::vector<FloatUniform> float_uniforms,
      std::vector<Float2Uniform> float2_uniforms,
      std::vector<Float4Uniform> float4_uniforms,
      std::vector<IntUniform> int_uniforms,
      sk_sp<PaintShader> cached_paint_shader);

  static size_t GetSerializedSize(const PaintShader* shader);

  PaintShader(const PaintShader&) = delete;
  ~PaintShader() override;

  PaintShader& operator=(const PaintShader&) = delete;

  void set_has_animated_images(bool has_animated_images) {
    image_analysis_state_ = has_animated_images
                                ? ImageAnalysisState::kAnimatedImages
                                : ImageAnalysisState::kNoAnimatedImages;
  }
  ImageAnalysisState image_analysis_state() const {
    return image_analysis_state_;
  }

  // If `content_color_usage` is not null, the function should update
  // `*content_color_usage` to be
  // max(*content_color_usage, max_content_color_usage_of_the_flags).
  bool HasDiscardableImages(
      gfx::ContentColorUsage* content_color_usage = nullptr) const;

  SkMatrix GetLocalMatrix() const {
    return local_matrix_ ? *local_matrix_ : SkMatrix::I();
  }
  Type shader_type() const { return shader_type_; }
  const PaintImage& paint_image() const {
    DCHECK_EQ(Type::kImage, shader_type_);
    return image_;
  }

  const PaintRecord* paint_record() const;

  bool GetRasterizationTileRect(const SkMatrix& ctm, SkRect* tile_rect) const {
    return GetClampedRasterizationTileRect(ctm, /*max_texture_size=*/0,
                                           tile_rect);
  }

  SkTileMode tx() const { return tx_; }
  SkTileMode ty() const { return ty_; }
  SkRect tile() const { return tile_; }

  bool IsOpaque() const;

  // Returns true if the shader looks like it is valid (ie the members required
  // for this shader type all look reasonable. Returns false otherwise. Note
  // that this is a best effort function since truly validating whether the
  // shader is correct is hard.
  bool IsValid() const;

  bool EqualsForTesting(const PaintShader& other) const;
  bool MatchingCachedRuntimeEffectForTesting(const PaintShader& other) const;

  RecordShaderId paint_record_shader_id() const {
    DCHECK(id_ == kInvalidRecordShaderId || shader_type_ == Type::kPaintRecord);
    return id_;
  }

  uint32_t sk_runtime_effect_id() const { return sk_runtime_effect_id_; }

 private:
  friend class PaintFlags;
  friend class PaintOpHelper;
  friend class PaintOpReader;
  friend class PaintOpSerializationTestUtils;
  friend class PaintOpWriter;
  friend class ScopedRasterFlags;
  friend class ShaderPaintFilter;
  FRIEND_TEST_ALL_PREFIXES(PaintShaderTest, DecodePaintRecord);
  FRIEND_TEST_ALL_PREFIXES(PaintShaderTest, InfinityStopShouldBeValid);
  FRIEND_TEST_ALL_PREFIXES(PaintOpBufferTest, PaintRecordShaderSerialization);
  FRIEND_TEST_ALL_PREFIXES(PaintOpBufferTest, RecordShadersCached);

  static SkGradientShader::Interpolation DefaultInterpolation() {
    SkGradientShader::Interpolation default_interpolation;
    return default_interpolation;
  }

  explicit PaintShader(Type type);

  bool GetClampedRasterizationTileRect(const SkMatrix& ctm,
                                       int max_texture_size,
                                       SkRect* tile_rect) const;

  sk_sp<SkShader> GetSkShader(PaintFlags::FilterQuality quality) const;

  // If the type needs a resolve skia object (e.g. SkImage or SkPicture), this
  // will create and cache it internally. Most types do not need this, but it
  // is safe to call on any type.
  void ResolveSkObjects(const gfx::SizeF* raster_scale = nullptr,
                        ImageProvider* image_provider = nullptr);

  // Creates a PaintShader to be rasterized at the given ctm. |raster_scale| is
  // set to the scale at which the record should be rasterized when the shader
  // is used.
  // Note that this does not create a skia backing for the shader.
  // Valid only for PaintRecord backed shaders.
  sk_sp<PaintShader> CreateScaledPaintRecord(const SkMatrix& ctm,
                                             int max_texture_size,
                                             gfx::SizeF* raster_scale) const;

  // Creates a PaintShader with images from |image_provider| to be rasterized
  // at the given ctm.
  // |transfer_cache_entry_id| is set to the transfer cache id for the image, if
  // the decode is backed by the transfer cache.
  // |raster_quality| is set to the filter quality the shader should be
  // rasterized with.
  // Valid only for PaintImage backed shaders.
  sk_sp<PaintShader> CreateDecodedImage(
      const SkMatrix& ctm,
      PaintFlags::FilterQuality requested_quality,
      ImageProvider* image_provider,
      uint32_t* transfer_cache_entry_id,
      PaintFlags::FilterQuality* raster_quality,
      bool* needs_mips,
      gpu::Mailbox* mailbox) const;

  // Creates a paint record shader for worklet-backed images.
  sk_sp<PaintShader> CreatePaintWorkletRecord(
      ImageProvider* image_provider) const;

  void SetColorsAndPositions(const SkColor4f* colors,
                             const SkScalar* positions,
                             int count);
  void SetMatrixAndTiling(const SkMatrix* matrix, SkTileMode tx, SkTileMode ty);
  void SetFlagsAndFallback(uint32_t flags, SkColor4f fallback_color);
  void SetGradientInterpolation(SkGradientShader::Interpolation interpolation) {
    gradient_interpolation_ = interpolation;
  }

  Type shader_type_ = Type::kShaderCount;

  uint32_t flags_ = 0;
  SkScalar end_radius_ = 0;
  SkScalar start_radius_ = 0;
  SkTileMode tx_ = SkTileMode::kClamp;
  SkTileMode ty_ = SkTileMode::kClamp;
  SkColor4f fallback_color_ = SkColors::kTransparent;
  ScalingBehavior scaling_behavior_ = ScalingBehavior::kRasterAtScale;

  std::optional<SkMatrix> local_matrix_;
  SkPoint center_ = SkPoint::Make(0, 0);
  SkRect tile_ = SkRect::MakeEmpty();

  SkPoint start_point_ = SkPoint::Make(0, 0);
  SkPoint end_point_ = SkPoint::Make(0, 0);

  SkScalar start_degrees_ = 0;
  SkScalar end_degrees_ = 0;

  PaintImage image_;
  std::optional<PaintRecord> record_;
  RecordShaderId id_ = kInvalidRecordShaderId;

  // For decoded PaintRecord shaders, specifies the scale at which the record
  // will be rasterized.
  std::optional<gfx::SizeF> tile_scale_;

  std::vector<SkColor4f> colors_;
  std::vector<SkScalar> positions_;

  SkGradientShader::Interpolation gradient_interpolation_;

  // Cached intermediates, for Paint objects that may not be thread-safe
  sk_sp<SkPicture> sk_cached_picture_;
  sk_sp<SkImage> sk_cached_image_;

  ImageAnalysisState image_analysis_state_ = ImageAnalysisState::kNoAnalysis;

  // The command to be (de)serialized for `Type::kSkSLCommand`. Remains empty
  // for other shader types.
  SkString sksl_command_;

  // Uniforms for `sksl_command_`. The keys of the map are the variable name of
  // the uniform.
  std::vector<FloatUniform> scalar_uniforms_;
  std::vector<Float2Uniform> float2_uniforms_;
  std::vector<Float4Uniform> float4_uniforms_;
  std::vector<IntUniform> int_uniforms_;

  // Unique ID for `Type::kSkSLCommand`. Remains 0u for other types.
  uint32_t sk_runtime_effect_id_ = 0u;

  // Does not participate in de/serialization. In software rasterization it is
  // set when the PaintShader object is created; in hardware rasterization, it
  // is set when the PaintShader is deserialized.
  sk_sp<SkRuntimeEffect> cached_sk_runtime_effect_ = nullptr;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_SHADER_H_
