// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/shader_transfer_cache_entry.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/paint_op_matchers.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_options_provider.h"
#include "cc/test/test_paint_worklet_input.h"
#include "cc/test/test_skcanvas.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "skia/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace cc {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FloatEq;
using ::testing::Key;
using ::testing::Le;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::ResultOf;

// An arbitrary size guaranteed to fit the size of any serialized op in this
// unit test.  This can also be used for deserialized op size safely in this
// unit test suite as generally deserialized ops are smaller.
static constexpr size_t kSerializedBytesPerOp = 1000 + sizeof(LargestPaintOp);

static constexpr size_t kDefaultSerializedBufferSize = 4096;
std::unique_ptr<char, base::AlignedFreeDeleter> AllocateSerializedBuffer(
    size_t size = kDefaultSerializedBufferSize) {
  return PaintOpWriter::AllocateAlignedBuffer(size);
}

bool ReadAndValidateOpHeader(const void* memory,
                             size_t size,
                             uint8_t* op_type,
                             size_t* serialized_size) {
  return PaintOpReader(memory, size,
                       TestOptionsProvider().deserialize_options())
      .ReadAndValidateOpHeader(op_type, serialized_size);
}

}  // namespace

class PaintOpSerializationTestUtils {
 public:
  static void FillArbitraryShaderValues(PaintShader* shader, bool use_matrix) {
    shader->shader_type_ = PaintShader::Type::kTwoPointConicalGradient;
    shader->flags_ = 12345;
    shader->end_radius_ = 12.3f;
    shader->start_radius_ = 13.4f;
    shader->tx_ = SkTileMode::kRepeat;
    shader->ty_ = SkTileMode::kMirror;
    shader->fallback_color_ = {0.99f, 0.98f, 0.97f, 0.99f};
    shader->scaling_behavior_ = PaintShader::ScalingBehavior::kRasterAtScale;
    if (use_matrix) {
      shader->local_matrix_.emplace(SkMatrix::I());
      shader->local_matrix_->setSkewX(10);
      shader->local_matrix_->setSkewY(20);
    }
    shader->center_ = SkPoint::Make(50, 40);
    shader->tile_ = SkRect::MakeXYWH(7, 77, 777, 7777);
    shader->start_point_ = SkPoint::Make(-1, -5);
    shader->end_point_ = SkPoint::Make(13, -13);
    shader->start_degrees_ = 123;
    shader->end_degrees_ = 456;
    // TODO(vmpstr): Add PaintImage/PaintRecord.
    shader->colors_ = {{0.1f, 0.2f, 0.3f, 0.4f},
                       {0.05f, 0.15f, 0.25f, 0.35f},
                       {0.0f, 0.5f, 0.9f, 0.1f}};
    shader->positions_ = {0.f, 0.4f, 1.f};
  }
};

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_FALSE(PaintOpBuffer::Iterator(buffer));

  buffer.Reset();
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_FALSE(PaintOpBuffer::Iterator(buffer));

  PaintOpBuffer buffer2(std::move(buffer));
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_FALSE(PaintOpBuffer::Iterator(buffer));
  EXPECT_EQ(buffer2.size(), 0u);
  EXPECT_EQ(buffer2.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_FALSE(PaintOpBuffer::Iterator(buffer2));
}

class PaintOpAppendTest : public ::testing::Test {
 public:
  PaintOpAppendTest() {
    rect_ = SkRect::MakeXYWH(2, 3, 4, 5);
    flags_.setColor(SK_ColorMAGENTA);
    flags_.setAlphaf(0.25f);
  }

  void PushOps(PaintOpBuffer* buffer) {
    buffer->push<SaveLayerOp>(rect_, flags_);
    buffer->push<SaveOp>();
    buffer->push<DrawColorOp>(draw_color_, blend_);
    buffer->push<RestoreOp>();
    EXPECT_EQ(buffer->size(), 4u);
  }

  void VerifyOps(PaintOpBuffer* buffer) {
    EXPECT_THAT(
        *buffer,
        ElementsAre(PaintOpEq<SaveLayerOp>(rect_, flags_), PaintOpEq<SaveOp>(),
                    PaintOpEq<DrawColorOp>(draw_color_, blend_),
                    PaintOpEq<RestoreOp>()));
  }

  void CheckInitialState(PaintOpBuffer* buffer, bool expect_empty_buffer) {
    EXPECT_FALSE(buffer->has_draw_ops());
    EXPECT_FALSE(buffer->has_save_layer_ops());
    EXPECT_EQ(0u, buffer->size());
    EXPECT_EQ(0u, buffer->total_op_count());
    EXPECT_EQ(0u, buffer->next_op_offset());
    EXPECT_FALSE(PaintOpBuffer::Iterator(*buffer));
    if (expect_empty_buffer) {
      EXPECT_EQ(0u, buffer->paint_ops_size());
      EXPECT_EQ(sizeof(PaintOpBuffer), buffer->bytes_used());
    }
  }

 private:
  SkRect rect_;
  PaintFlags flags_;
  SkColor4f draw_color_ = SkColors::kRed;
  SkBlendMode blend_ = SkBlendMode::kSrc;
};

TEST_F(PaintOpAppendTest, SimpleAppend) {
  PaintOpBuffer buffer;
  PushOps(&buffer);
  VerifyOps(&buffer);

  buffer.Reset();
  CheckInitialState(&buffer, false);
  PushOps(&buffer);
  VerifyOps(&buffer);
}

TEST_F(PaintOpAppendTest, MoveThenDestruct) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination(std::move(original));
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  CheckInitialState(&original, true);
}

TEST_F(PaintOpAppendTest, MoveThenDestructOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  CheckInitialState(&original, true);
}

TEST_F(PaintOpAppendTest, MoveThenReappend) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination(std::move(original));

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
  EXPECT_TRUE(original.EqualsForTesting(destination));
}

TEST_F(PaintOpAppendTest, MoveThenReappendOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
  EXPECT_TRUE(original.EqualsForTesting(destination));
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  float paint_flags_alpha = 0.25f;
  PaintFlags draw_flags;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(paint_flags_alpha);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  // Expect the alpha from the draw and the save layer to be folded together.
  // Since alpha is stored in a uint8_t and gets rounded, so use tolerance.
  float expected_alpha = alpha * paint_flags_alpha;
  EXPECT_NEAR(expected_alpha, canvas.paint_.getAlphaf(), 0.01f);
}

// Verify that we don't optimize SaveLayerAlpha / DrawTextBlob / Restore.
TEST(PaintOpBufferTest, SaveDrawTextBlobRestore) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  PaintFlags paint_flags;
  EXPECT_TRUE(paint_flags.SupportsFoldingAlpha());
  buffer.push<DrawTextBlobOp>(SkTextBlob::MakeFromString("abc", SkFont()), 0.0f,
                              0.0f, paint_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when the drawing op's flags are not compatible with being folded into the
// save layer with opacity.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_BadFlags) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(0.25f);
  draw_flags.setBlendMode(SkBlendMode::kSrc);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// Same as above, but the save layer itself appears to be a noop.
// See: http://crbug.com/748485.  If the inner draw op itself
// doesn't support folding, then the external save can't be skipped.
TEST(PaintOpBufferTest, SaveDrawRestore_BadFlags255Alpha) {
  PaintOpBuffer buffer;

  float alpha = 1.0f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(0.25f);
  draw_flags.setBlendMode(SkBlendMode::kColorBurn);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when there are more than one ops between the save and restore.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_TooManyOps) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(0.25f);
  draw_flags.setBlendMode(SkBlendMode::kSrcOver);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// Verify that the save draw restore code works with a single op
// that's not a draw op, and the optimization does not kick in.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpNotADrawOp) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

// Test that the save/draw/restore optimization applies if the single op
// is a DrawRecord that itself has a single draw op.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleOp) {
  PaintOpBuffer sub_buffer;

  float paint_flags_alpha = 0.25f;
  PaintFlags draw_flags;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(paint_flags_alpha);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  sub_buffer.push<DrawRectOp>(rect, draw_flags);
  EXPECT_EQ(sub_buffer.size(), 1u);

  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);
  buffer.push<DrawRecordOp>(sub_buffer.ReleaseAsRecord());
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  float expected_alpha = alpha * paint_flags_alpha;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlphaf()), 0.01f);
}

// The same as the above SingleOpRecord test, but the single op is not
// a draw op.  So, there's no way to fold in the save layer optimization.
// Verify that the optimization doesn't apply and that this doesn't crash.
// See: http://crbug.com/712093.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleNonDrawOp) {
  PaintOpBuffer sub_buffer;
  sub_buffer.push<NoopOp>();
  EXPECT_EQ(sub_buffer.size(), 1u);
  EXPECT_FALSE(sub_buffer.GetFirstOp().IsDrawOp());

  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);
  buffer.push<DrawRecordOp>(sub_buffer.ReleaseAsRecord());
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

TEST(PaintOpBufferTest, SaveLayerRestore_DrawColor) {
  PaintOpBuffer buffer;
  float alpha = 0.4f;
  SkColor original = SkColorSetA(SK_ColorRED, 50);

  buffer.push<SaveLayerAlphaOp>(alpha);
  buffer.push<DrawColorOp>(SkColor4f::FromColor(original),
                           SkBlendMode::kSrcOver);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);
  EXPECT_EQ(canvas.save_count_, 0);
  EXPECT_EQ(canvas.restore_count_, 0);

  // original alpha of 50 * layer alpha of 0.4 == 20
  SkColor expected_color = SkColorSetA(SK_ColorRED, 20);
  EXPECT_EQ(canvas.paint_.getColor(), expected_color);
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_EmptyBuffer) {
  PaintOpBuffer buffer;
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_NoImageOp) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImage) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0));
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_PaintWorkletImage) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(32.0f, 32.0f));
  PaintOpBuffer buffer;
  PaintImage image = CreatePaintWorkletPaintImage(input);
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0));
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_PaintWorkletImageRect) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(32.0f, 32.0f));
  PaintOpBuffer buffer;
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeEmpty();
  SkRect dst = SkRect::MakeEmpty();
  buffer.push<DrawImageRectOp>(image, src, dst,
                               SkCanvas::kStrict_SrcRectConstraint);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImageRect) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  buffer.push<DrawImageRectOp>(image, SkRect::MakeWH(100, 100),
                               SkRect::MakeWH(100, 100),
                               SkCanvas::kFast_SrcRectConstraint);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_OpWithFlags) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  auto image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  flags.setShader(PaintShader::MakeImage(std::move(image), SkTileMode::kClamp,
                                         SkTileMode::kClamp, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, SlowPaths) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 0);

  // Op without slow paths
  buffer.push<SaveLayerOp>(SkRect::MakeXYWH(2, 3, 4, 5), PaintFlags());

  // Line op with a slow path
  PaintFlags line_effect_slow;
  line_effect_slow.setStrokeWidth(1.f);
  line_effect_slow.setStyle(PaintFlags::kStroke_Style);
  line_effect_slow.setStrokeCap(PaintFlags::kRound_Cap);
  SkScalar intervals[] = {1.f, 1.f};
  line_effect_slow.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

  buffer.push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_slow);
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 1);

  // Line effect special case that Skia handles specially.
  PaintFlags line_effect = line_effect_slow;
  line_effect.setStrokeCap(PaintFlags::kButt_Cap);
  buffer.push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 1);

  // Antialiased convex path is not slow.
  SkPath path;
  path.addCircle(2, 2, 5);
  EXPECT_TRUE(path.isConvex());
  buffer.push<ClipPathOp>(path, SkClipOp::kIntersect, /*antialias=*/true,
                          UsePaintCache::kDisabled);
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 1);

  // Concave paths are slow only when antialiased.
  SkPath concave = path;
  concave.addCircle(3, 4, 2);
  EXPECT_FALSE(concave.isConvex());
  buffer.push<ClipPathOp>(concave, SkClipOp::kIntersect, /*antialias=*/true,
                          UsePaintCache::kDisabled);
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 2);
  buffer.push<ClipPathOp>(concave, SkClipOp::kIntersect, /*antialias=*/false,
                          UsePaintCache::kDisabled);
  EXPECT_EQ(buffer.num_slow_paths_up_to_min_for_MSAA(), 2);

  // Drawing a record with slow paths into another adds the same
  // number of slow paths as the record.
  PaintOpBuffer buffer2;
  EXPECT_EQ(0, buffer2.num_slow_paths_up_to_min_for_MSAA());
  PaintRecord record = buffer.ReleaseAsRecord();
  buffer2.push<DrawRecordOp>(record);
  EXPECT_EQ(2, buffer2.num_slow_paths_up_to_min_for_MSAA());
  buffer2.push<DrawRecordOp>(record);
  EXPECT_EQ(4, buffer2.num_slow_paths_up_to_min_for_MSAA());
}

TEST(PaintOpBufferTest, NonAAPaint) {
  // PaintOpWithFlags
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) with AA.
    PaintFlags line_effect;
    line_effect.setAntiAlias(true);
    buffer.push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
    EXPECT_FALSE(buffer.HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) without AA.
    PaintFlags line_effect_no_aa;
    line_effect_no_aa.setAntiAlias(false);
    buffer.push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_no_aa);
    EXPECT_TRUE(buffer.HasNonAAPaint());
  }

  // ClipPathOp
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    SkPath path;
    path.addCircle(2, 2, 5);

    // ClipPathOp with AA
    buffer.push<ClipPathOp>(path, SkClipOp::kIntersect, /*antialias=*/true,
                            UsePaintCache::kDisabled);
    EXPECT_FALSE(buffer.HasNonAAPaint());

    // ClipPathOp without AA
    buffer.push<ClipPathOp>(path, SkClipOp::kIntersect, /*antialias=*/false,
                            UsePaintCache::kDisabled);
    EXPECT_TRUE(buffer.HasNonAAPaint());
  }

  // ClipRRectOp
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    // ClipRRectOp with AA
    buffer.push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                             true /* antialias */);
    EXPECT_FALSE(buffer.HasNonAAPaint());

    // ClipRRectOp without AA
    buffer.push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                             false /* antialias */);
    EXPECT_TRUE(buffer.HasNonAAPaint());
  }

  // Drawing a record with non-aa paths into another propogates the value.
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    PaintOpBuffer sub_buffer;
    SkPath path;
    path.addCircle(2, 2, 5);
    sub_buffer.push<ClipPathOp>(path, SkClipOp::kIntersect,
                                /*antialias=*/false, UsePaintCache::kDisabled);
    EXPECT_TRUE(sub_buffer.HasNonAAPaint());

    buffer.push<DrawRecordOp>(sub_buffer.ReleaseAsRecord());
    EXPECT_TRUE(buffer.HasNonAAPaint());
  }

  // The following PaintOpWithFlags types are overridden to *not* ever have
  // non-AA paint. AA is hard to notice, and these kick us out of MSAA in too
  // many cases.

  // DrawImageOp
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0),
                             SkSamplingOptions(), &non_aa_flags);

    EXPECT_FALSE(buffer.HasNonAAPaint());
  }

  // DrawIRectOp
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer.push<DrawIRectOp>(SkIRect::MakeWH(1, 1), non_aa_flags);

    EXPECT_FALSE(buffer.HasNonAAPaint());
  }

  // SaveLayerOp
  {
    PaintOpBuffer buffer;
    EXPECT_FALSE(buffer.HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer.push<SaveLayerOp>(SkRect::MakeWH(1, 1), non_aa_flags);

    EXPECT_FALSE(buffer.HasNonAAPaint());
  }
}

class PaintOpBufferOffsetsTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {
    offsets_.clear();
    buffer_.Reset();
  }

  template <typename T, typename... Args>
  void push_op(Args&&... args) {
    offsets_.push_back(buffer_.next_op_offset());
    buffer_.push<T>(std::forward<Args>(args)...);
  }

  // Returns a subset of offsets_ by selecting only the specified indices.
  std::vector<size_t> Select(const std::vector<size_t>& indices) {
    std::vector<size_t> result;
    for (size_t i : indices)
      result.push_back(offsets_[i]);
    return result;
  }

  void Playback(SkCanvas* canvas, const std::vector<size_t>& offsets) {
    buffer_.Playback(canvas, PlaybackParams(nullptr), &offsets);
  }

 protected:
  std::vector<size_t> offsets_;
  PaintOpBuffer buffer_;
};

TEST_F(PaintOpBufferOffsetsTest, EmptyClipRectShouldRejectAnOp) {
  SkCanvas device(0, 0);
  SkCanvas* canvas = &device;
  canvas->translate(-254, 0);
  SkIRect bounds = canvas->getDeviceClipBounds();
  EXPECT_TRUE(bounds.isEmpty());
  SkMatrix ctm = canvas->getTotalMatrix();
  EXPECT_EQ(ctm[2], -254);

  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(32.0f, 32.0f));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeLTRB(0, 0, 100, 100);
  SkRect dst = SkRect::MakeLTRB(168, -23, 268, 77);
  push_op<DrawImageRectOp>(image, src, dst,
                           SkCanvas::kStrict_SrcRectConstraint);
  std::vector<size_t> offsets = Select({0});
  for (PaintOpBuffer::PlaybackFoldingIterator iter(buffer_, &offsets); iter;
       ++iter) {
    const PaintOp& op = *iter;
    EXPECT_EQ(op.GetType(), PaintOpType::DrawImageRect);
    EXPECT_TRUE(PaintOp::QuickRejectDraw(op, canvas));
  }
}

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndices) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Plays all items.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, NonContiguousIndices) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Plays 0, 1, 3, 4 indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, FirstTwoIndices) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Plays first two indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  Playback(&canvas, Select({0, 1}));
}

TEST_F(PaintOpBufferOffsetsTest, MiddleIndex) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Plays index 2.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  Playback(&canvas, Select({2}));
}

TEST_F(PaintOpBufferOffsetsTest, LastTwoElements) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Plays last two elements.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndicesWithSaveLayerAlphaRestore) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  float alpha = 0.4f;
  push_op<SaveLayerAlphaOp>(alpha);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Items are {0, 1, save, restore, 2, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped.
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       NonContiguousIndicesWithSaveLayerAlphaRestore) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(SkColor4f::FromColor(0u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(1u), SkBlendMode::kClear);
  float alpha = 0.4f;
  push_op<SaveLayerAlphaOp>(alpha);
  push_op<DrawColorOp>(SkColor4f::FromColor(2u), SkBlendMode::kClear);
  push_op<DrawColorOp>(SkColor4f::FromColor(3u), SkBlendMode::kClear);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(SkColor4f::FromColor(4u), SkBlendMode::kClear);

  // Items are {0, 1, save, 2, 3, restore, 4}.

  // Plays back all indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The SaveLayerAlpha/Restore is not dropped if we draw the middle
    // range, as we need them to represent the two draws inside the layer
    // correctly.
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // Skips the middle indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The now-empty SaveLayerAlpha/Restore is dropped
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       ContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  testing::StrictMock<MockCanvas> canvas;

  auto add_draw_rect = [this](SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    push_op<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(0u);
  add_draw_rect(1u);
  float alpha = 0.4f;
  push_op<SaveLayerAlphaOp>(alpha);
  add_draw_rect(2u);
  push_op<RestoreOp>();
  add_draw_rect(3u);
  add_draw_rect(4u);

  // Items are {0, 1, save, 2, restore, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped, the containing
  // operation can be drawn with alpha.
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST_F(PaintOpBufferOffsetsTest,
       NonContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  testing::StrictMock<MockCanvas> canvas;

  auto add_draw_rect = [this](SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    push_op<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(0u);
  add_draw_rect(1u);
  float alpha = 0.4f;
  push_op<SaveLayerAlphaOp>(alpha);
  add_draw_rect(2u);
  add_draw_rect(3u);
  add_draw_rect(4u);
  push_op<RestoreOp>();

  // Items are are {0, 1, save, 2, 3, 4, restore}.

  // If the middle range is played, then the SaveLayerAlpha/Restore
  // can't be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 4, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 5, 6}));
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    Playback(&canvas, Select({0, 1, 2, 3, 6}));
  }
}

TEST(PaintOpBufferTest, SaveLayerAlphaDrawRestoreWithBadBlendMode) {
  PaintOpBuffer buffer;
  testing::StrictMock<MockCanvas> canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    // This blend mode prevents the optimization.
    flags.setBlendMode(SkBlendMode::kSrc);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(&buffer, 0u);
  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);
  add_draw_rect(&buffer, 1u);
  buffer.push<RestoreOp>();
  add_draw_rect(&buffer, 2u);

  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    buffer.Playback(&canvas);
  }
}

TEST(PaintOpBufferTest, UnmatchedSaveRestoreNoSideEffects) {
  PaintOpBuffer buffer;
  testing::StrictMock<MockCanvas> canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  // Push 2 saves.

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);
  add_draw_rect(&buffer, 0u);
  buffer.push<SaveLayerAlphaOp>(alpha);
  add_draw_rect(&buffer, 1u);
  add_draw_rect(&buffer, 2u);
  // But only 1 restore.
  buffer.push<RestoreOp>();

  testing::Sequence s;
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  // We will restore back to the original save count regardless with 2 restores.
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  buffer.Playback(&canvas);
}

std::vector<float> test_floats = {0.f,
                                  1.f,
                                  -1.f,
                                  2384.981971f,
                                  0.0001f,
                                  std::numeric_limits<float>::min(),
                                  std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::infinity()};

std::vector<uint8_t> test_uint8s = {
    0, 255, 128, 10, 45,
};

static SkRect make_largest_skrect() {
  const float limit = std::numeric_limits<float>::max();
  return {-limit, -limit, limit, limit};
}

static SkIRect make_largest_skirect() {
  // we use half the limit, so that the resulting width/height will not
  // overflow.
  const int32_t limit = std::numeric_limits<int32_t>::max() >> 1;
  return {-limit, -limit, limit, limit};
}

std::vector<SkRect> test_rects = {
    SkRect::MakeXYWH(1, 2.5, 3, 4), SkRect::MakeXYWH(0, 0, 0, 0),
    make_largest_skrect(),          SkRect::MakeXYWH(0.5f, 0.5f, 8.2f, 8.2f),
    SkRect::MakeXYWH(-1, -1, 0, 0), SkRect::MakeXYWH(-100, -101, -102, -103),
    SkRect::MakeXYWH(0, 0, 0, 0),   SkRect::MakeXYWH(0, 0, 0, 0),
    SkRect::MakeXYWH(0, 0, 0, 0),   SkRect::MakeXYWH(0, 0, 0, 0),
    SkRect::MakeXYWH(0, 0, 0, 0),   SkRect::MakeXYWH(0, 0, 0, 0),
};

std::vector<SkRRect> test_rrects = {
    SkRRect::MakeEmpty(), SkRRect::MakeOval(SkRect::MakeXYWH(1, 2, 3, 4)),
    SkRRect::MakeRect(SkRect::MakeXYWH(-10, 100, 5, 4)),
    [] {
      SkRRect rrect = SkRRect::MakeEmpty();
      rrect.setNinePatch(SkRect::MakeXYWH(10, 20, 30, 40), 1, 2, 3, 4);
      return rrect;
    }(),
};

std::vector<SkIRect> test_irects = {
    SkIRect::MakeXYWH(1, 2, 3, 4),   SkIRect::MakeXYWH(0, 0, 0, 0),
    make_largest_skirect(),          SkIRect::MakeXYWH(0, 0, 10, 10),
    SkIRect::MakeXYWH(-1, -1, 0, 0), SkIRect::MakeXYWH(-100, -101, -102, -103)};

std::vector<uint32_t> test_ids = {0, 1, 56, 0xFFFFFFFF, 0xFFFFFFFE, 0x10001};

std::vector<SkM44> test_matrices = {
    SkM44(),
    SkM44::Scale(3.91f, 4.31f, 1.0f),
    SkM44::Translate(-5.2f, 8.7f, 0.0f),
    SkM44(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    SkM44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
};

std::vector<SkPath> test_paths = {
    [] {
      SkPath path;
      path.moveTo(SkIntToScalar(20), SkIntToScalar(20));
      path.lineTo(SkIntToScalar(80), SkIntToScalar(20));
      path.lineTo(SkIntToScalar(30), SkIntToScalar(30));
      path.lineTo(SkIntToScalar(20), SkIntToScalar(80));
      return path;
    }(),
    [] {
      SkPath path;
      path.addCircle(2, 2, 5);
      path.addCircle(3, 4, 2);
      path.addArc(SkRect::MakeXYWH(1, 2, 3, 4), 5, 6);
      return path;
    }(),
    SkPath(),
};

std::vector<PaintFlags> test_flags = {
    PaintFlags(),
    [] {
      PaintFlags flags;
      flags.setColor(SK_ColorMAGENTA);
      flags.setStrokeWidth(4.2f);
      flags.setStrokeMiter(5.91f);
      flags.setBlendMode(SkBlendMode::kDst);
      flags.setStrokeCap(PaintFlags::kSquare_Cap);
      flags.setStrokeJoin(PaintFlags::kBevel_Join);
      flags.setStyle(PaintFlags::kStroke_Style);
      flags.setFilterQuality(PaintFlags::FilterQuality::kMedium);
      flags.setShader(PaintShader::MakeColor({0.1f, 0.2f, 0.3f, 0.4f}));
      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setColor(SK_ColorCYAN);
      flags.setAlphaf(0.25f);
      flags.setStrokeWidth(0.32f);
      flags.setStrokeMiter(7.98f);
      flags.setBlendMode(SkBlendMode::kSrcOut);
      flags.setStrokeCap(PaintFlags::kRound_Cap);
      flags.setStrokeJoin(PaintFlags::kRound_Join);
      flags.setStyle(PaintFlags::kFill_Style);
      flags.setFilterQuality(PaintFlags::FilterQuality::kHigh);

      SkScalar intervals[] = {1.f, 1.f};
      flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
      flags.setMaskFilter(SkMaskFilter::MakeBlur(
          SkBlurStyle::kOuter_SkBlurStyle, 4.3f));
      flags.setColorFilter(SkColorMatrixFilter::MakeLightingFilter(
          SK_ColorYELLOW, SK_ColorGREEN));

      SkLayerDrawLooper::Builder looper_builder;
      looper_builder.addLayer();
      looper_builder.addLayer(2.3f, 4.5f);
      SkLayerDrawLooper::LayerInfo layer_info;
      layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
      layer_info.fColorMode = SkBlendMode::kDst;
      layer_info.fOffset.set(-1.f, 5.2f);
      looper_builder.addLayer(layer_info);
      flags.setLooper(looper_builder.detach());

      sk_sp<PaintShader> shader =
          PaintShader::MakeColor(SkColors::kTransparent);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               true);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setShader(PaintShader::MakeColor({0.1f, 0.2f, 0.3f, 0.4f}));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      sk_sp<PaintShader> shader =
          PaintShader::MakeColor(SkColors::kTransparent);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               false);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      SkPoint points[2] = {SkPoint::Make(1, 2), SkPoint::Make(3, 4)};
      SkColor4f colors[3] = {{0.1f, 0.2f, 0.3f, 0.4f},
                             {0.4f, 0.3f, 0.2f, 0.1f},
                             {0.2f, 0.4f, 0.6f, 0.0f}};
      SkScalar positions[3] = {0.f, 0.3f, 1.f};
      flags.setShader(PaintShader::MakeLinearGradient(points, colors, positions,
                                                      3, SkTileMode::kMirror));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      SkColor4f colors[3] = {{0.1f, 0.2f, 0.3f, 0.4f},
                             {0.4f, 0.3f, 0.2f, 0.1f},
                             {0.2f, 0.4f, 0.6f, 0.0f}};
      flags.setShader(PaintShader::MakeSweepGradient(
          0.2f, -0.8f, colors, nullptr, 3, SkTileMode::kMirror, 10, 20));
      return flags;
    }(),
    PaintFlags(),
    PaintFlags(),
};

std::vector<SkColor4f> test_colors = {
    {0, 0, 0, 0},    {1, 1, 1, 1},    {1, 0.04, 1, 0},
    {0, 0.08, 1, 1}, {1, 0, 1, 0.12}, {0.16, 0, 0, 1},
};

std::vector<std::string> test_strings = {
    "", "foobar",
    "blarbideeblarasdfaiousydfp234poiausdofiuapsodfjknla;sdfkjasd;f",
};

std::vector<std::vector<SkPoint>> test_point_arrays = {
    std::vector<SkPoint>(),
    {SkPoint::Make(1, 2)},
    {SkPoint::Make(1, 2), SkPoint::Make(-5.4f, -3.8f)},
    {SkPoint::Make(0, 0), SkPoint::Make(5, 6), SkPoint::Make(-1, -1),
     SkPoint::Make(9, 9), SkPoint::Make(50, 50), SkPoint::Make(100, 100)},
};

// TODO(enne): In practice, probably all paint images need to be uploaded
// ahead of time and not be bitmaps. These paint images should be fake
// gpu resource paint images.
std::vector<PaintImage> test_images = {
    CreateDiscardablePaintImage(gfx::Size(5, 10)),
    CreateDiscardablePaintImage(gfx::Size(1, 1)),
    CreateDiscardablePaintImage(gfx::Size(50, 50)),
};

#if BUILDFLAG(SKIA_SUPPORT_SKOTTIE)
bool kIsSkottieSupported = true;
#else
bool kIsSkottieSupported = false;
#endif

// Writes as many ops in |buffer| as can fit in |output_size| to |output|.
// Records the numbers of bytes written for each op.
class SimpleSerializer {
 public:
  SimpleSerializer(void* output, size_t output_size)
      : current_(static_cast<char*>(output)),
        output_size_(output_size),
        remaining_(output_size) {}

  void Serialize(const PaintOpBuffer& buffer) {
    bytes_written_.resize(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i)
      bytes_written_[i] = 0;

    size_t op_idx = 0;
    for (const PaintOp& op : buffer) {
      size_t bytes_written = op.Serialize(current_, remaining_,
                                          options_provider_.serialize_options(),
                                          nullptr, SkM44(), SkM44());
      if (!bytes_written)
        return;

      uint8_t type = 0;
      size_t bytes_to_read = 0;
      EXPECT_TRUE(
          ReadAndValidateOpHeader(current_, remaining_, &type, &bytes_to_read));
      if (op.GetType() == PaintOpType::DrawTextBlob) {
        EXPECT_EQ(type, static_cast<int>(PaintOpType::DrawSlug));
      } else {
        EXPECT_EQ(op.type, type);
      }
      EXPECT_EQ(bytes_written, bytes_to_read);

      bytes_written_[op_idx] = bytes_written;
      op_idx++;
      current_ += bytes_written;
      remaining_ -= bytes_written;

      // Number of bytes bytes_written must be a multiple of
      // PaintOpWriter::kMaxAlignment unless the buffer is filled
      // entirely.
      if (remaining_ != 0u) {
        EXPECT_EQ(
            bytes_written,
            base::bits::AlignUp(bytes_written, PaintOpWriter::kMaxAlignment));
      }
    }
  }

  const std::vector<size_t>& bytes_written() const { return bytes_written_; }
  size_t TotalBytesWritten() const { return output_size_ - remaining_; }
  TestOptionsProvider* options_provider() { return &options_provider_; }

 private:
  raw_ptr<char, AllowPtrArithmetic> current_ = nullptr;
  size_t output_size_ = 0u;
  size_t remaining_ = 0u;
  std::vector<size_t> bytes_written_;
  TestOptionsProvider options_provider_;
};

class DeserializerIterator {
 public:
  DeserializerIterator(const void* input,
                       size_t input_size,
                       const PaintOp::DeserializeOptions& options)
      : DeserializerIterator(input,
                             static_cast<const char*>(input),
                             input_size,
                             input_size,
                             options) {}

  DeserializerIterator(DeserializerIterator&&) = default;
  DeserializerIterator& operator=(DeserializerIterator&&) = default;

  ~DeserializerIterator() { DestroyDeserializedOp(); }

  DeserializerIterator begin() {
    return DeserializerIterator(input_, static_cast<const char*>(input_),
                                input_size_, input_size_, options_);
  }
  DeserializerIterator end() {
    return DeserializerIterator(input_,
                                static_cast<const char*>(input_) + input_size_,
                                input_size_, 0, options_);
  }
  bool operator!=(const DeserializerIterator& other) {
    return input_ != other.input_ || current_ != other.current_ ||
           input_size_ != other.input_size_ || remaining_ != other.remaining_;
  }
  DeserializerIterator& operator++() {
    CHECK_GE(remaining_, last_bytes_read_);
    current_ += last_bytes_read_;
    remaining_ -= last_bytes_read_;

    if (remaining_ > 0)
      CHECK_GE(remaining_, 4u);

    DeserializeCurrentOp();

    return *this;
  }

  explicit operator bool() const { return remaining_ > 0u; }
  const PaintOp* operator->() const { return deserialized_op_; }
  const PaintOp& operator*() const { return *deserialized_op_; }

 private:
  DeserializerIterator(const void* input,
                       const char* current,
                       size_t input_size,
                       size_t remaining,
                       const PaintOp::DeserializeOptions& options)
      : input_(input),
        current_(current),
        input_size_(input_size),
        remaining_(remaining),
        options_(options) {
    DeserializeCurrentOp();
  }

  void DestroyDeserializedOp() {
    if (!deserialized_op_)
      return;
    deserialized_op_->DestroyThis();
    deserialized_op_ = nullptr;
  }

  void DeserializeCurrentOp() {
    DestroyDeserializedOp();

    if (!remaining_)
      return;
    deserialized_op_ = PaintOp::Deserialize(
        current_, remaining_, paint_op_buffer_data_,
        std::size(paint_op_buffer_data_), &last_bytes_read_, options_);
  }

  raw_ptr<const void> input_ = nullptr;
  const char* current_ = nullptr;
  size_t input_size_ = 0u;
  size_t remaining_ = 0u;
  size_t last_bytes_read_ = 0u;
  PaintOp::DeserializeOptions options_;
  alignas(PaintOpBuffer::kPaintOpAlign) char paint_op_buffer_data_
      [kLargestPaintOpAlignedSize];
  raw_ptr<PaintOp> deserialized_op_ = nullptr;
};

void PushAnnotateOps(PaintOpBuffer* buffer) {
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::URL, test_rects[0],
                           SkData::MakeWithCString("thingerdoowhatchamagig"));
  // Deliberately test both null and empty SkData.
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::LINK_TO_DESTINATION,
                           test_rects[1], nullptr);
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::NAMED_DESTINATION,
                           test_rects[2], SkData::MakeEmpty());
  EXPECT_THAT(*buffer, Each(PaintOpIs<AnnotateOp>()));
}

void PushClipPathOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_paths.size(); ++i) {
    SkClipOp op = i % 3 ? SkClipOp::kDifference : SkClipOp::kIntersect;
    buffer->push<ClipPathOp>(test_paths[i], op, /*antialias=*/!!(i % 2),
                             UsePaintCache::kDisabled);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<ClipPathOp>()));
}

void PushClipRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRectOp>(test_rects[i], op, antialias);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<ClipRectOp>()));
}

void PushClipRRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rrects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRRectOp>(test_rrects[i], op, antialias);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<ClipRRectOp>()));
}

void PushConcatOps(PaintOpBuffer* buffer) {
  for (auto& test_matrix : test_matrices)
    buffer->push<ConcatOp>(test_matrix);
  EXPECT_THAT(*buffer, Each(PaintOpIs<ConcatOp>()));
}

void PushCustomDataOps(PaintOpBuffer* buffer) {
  for (uint32_t test_id : test_ids)
    buffer->push<CustomDataOp>(test_id);
  EXPECT_THAT(*buffer, Each(PaintOpIs<CustomDataOp>()));
}

void PushDrawColorOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_colors.size(); ++i) {
    buffer->push<DrawColorOp>(test_colors[i], static_cast<SkBlendMode>(i));
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawColorOp>()));
}

void PushDrawDRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size() - 1, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawDRRectOp>(test_rrects[i], test_rrects[i + 1],
                               test_flags[i]);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawDRRectOp>()));
}

void PushDrawImageOps(PaintOpBuffer* buffer) {
  size_t len =
      std::min({test_images.size(), test_flags.size(), test_floats.size() - 1});
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawImageOp>(test_images[i], test_floats[i],
                              test_floats[i + 1],
                              PaintFlags::FilterQualityToSkSamplingOptions(
                                  test_flags[i].getFilterQuality()),
                              &test_flags[i]);
  }

  // Test optional flags
  // TODO(enne): maybe all these optional ops should not be optional.
  buffer->push<DrawImageOp>(test_images[0], test_floats[0], test_floats[1]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawImageOp>()));
}

void PushDrawImageRectOps(PaintOpBuffer* buffer) {
  size_t len =
      std::min({test_images.size(), test_flags.size(), test_rects.size() - 1});
  for (size_t i = 0; i < len; ++i) {
    SkCanvas::SrcRectConstraint constraint =
        i % 2 ? SkCanvas::kStrict_SrcRectConstraint
              : SkCanvas::kFast_SrcRectConstraint;
    buffer->push<DrawImageRectOp>(test_images[i], test_rects[i],
                                  test_rects[i + 1],
                                  PaintFlags::FilterQualityToSkSamplingOptions(
                                      test_flags[i].getFilterQuality()),
                                  &test_flags[i], constraint);
  }

  // Test optional flags.
  buffer->push<DrawImageRectOp>(test_images[0], test_rects[0], test_rects[1],
                                SkCanvas::kStrict_SrcRectConstraint);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawImageRectOp>()));
}

void PushDrawIRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_irects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawIRectOp>(test_irects[i], test_flags[i]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawIRectOp>()));
}

void PushDrawLineOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_floats.size() - 3, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawLineOp>(test_floats[i], test_floats[i + 1],
                             test_floats[i + 2], test_floats[i + 3],
                             test_flags[i]);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawLineOp>()));
}

void PushDrawOvalOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawOvalOp>(test_rects[i], test_flags[i]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawOvalOp>()));
}

void PushDrawPathOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawPathOp>(test_paths[i], test_flags[i],
                             UsePaintCache::kDisabled);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawPathOp>()));
}

void PushDrawRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRectOp>(test_rects[i], test_flags[i]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawRectOp>()));
}

void PushDrawRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRRectOp>(test_rrects[i], test_flags[i]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawRRectOp>()));
}

SkottieFrameDataMap GetTestImagesForSkottie(SkottieWrapper& skottie,
                                            const SkRect& skottie_rect,
                                            PaintFlags::FilterQuality quality,
                                            float t) {
  SkottieFrameDataMap images;
  skottie.Seek(
      t,
      base::BindLambdaForTesting([&](SkottieResourceIdHash asset_id,
                                     float t_frame, sk_sp<SkImage>& image_out,
                                     SkSamplingOptions& sampling_out) {
        SkottieFrameData frame_data;
        frame_data.image = CreateBitmapImage(
            gfx::Size(skottie_rect.width() / 2, skottie_rect.height() / 2));
        frame_data.quality = quality;
        images[asset_id] = std::move(frame_data);
        return SkottieWrapper::FrameDataFetchResult::NO_UPDATE;
      }));
  return images;
}

SkottieFrameDataMap GetNullImagesForSkottie(SkottieWrapper& skottie, float t) {
  SkottieFrameDataMap images;
  skottie.Seek(
      t, base::BindLambdaForTesting(
             [&](SkottieResourceIdHash asset_id, float t_frame,
                 sk_sp<SkImage>& image_out, SkSamplingOptions& sampling_out) {
               images[asset_id] = SkottieFrameData();
               return SkottieWrapper::FrameDataFetchResult::NO_UPDATE;
             }));
  return images;
}

void PushDrawSkottieOps(PaintOpBuffer* buffer) {
  std::vector<scoped_refptr<SkottieWrapper>> test_skotties;
  std::vector<float> test_skottie_floats;
  std::vector<SkRect> test_skottie_rects;
  std::vector<SkottieFrameDataMap> test_skottie_images;
  std::vector<SkottieColorMap> test_skottie_color_maps;
  std::vector<SkottieTextPropertyValueMap> test_skottie_text_maps;
  if (kIsSkottieSupported) {
    test_skotties = {
        CreateSkottie(gfx::Size(10, 20), 4),
        CreateSkottie(gfx::Size(100, 40), 5),
        CreateSkottie(gfx::Size(80, 70), 6),
        CreateSkottieFromString(kLottieDataWith2Assets),
        CreateSkottieFromString(kLottieDataWith2Assets),
        CreateSkottieFromTestDataDir(kLottieDataWith2TextFileName)};
    test_skottie_floats = {0, 0.1f, 1.f, 0.2f, 0.2f, 0.3f};
    test_skottie_rects = {
        SkRect::MakeXYWH(10, 20, 30, 40),   SkRect::MakeXYWH(0, 5, 10, 20),
        SkRect::MakeXYWH(6, 0, 3, 50),      SkRect::MakeXYWH(10, 10, 100, 100),
        SkRect::MakeXYWH(10, 10, 100, 100), SkRect::MakeXYWH(5, 5, 50, 50)};
    test_skottie_images = {
        SkottieFrameDataMap(),
        SkottieFrameDataMap(),
        SkottieFrameDataMap(),
        GetTestImagesForSkottie(*test_skotties[3], test_skottie_rects[3],
                                PaintFlags::FilterQuality::kHigh,
                                test_skottie_floats[3]),
        GetNullImagesForSkottie(*test_skotties[4], test_skottie_floats[4]),
        SkottieFrameDataMap()};
    test_skottie_color_maps = {
        {SkottieMapColor("green", SK_ColorGREEN),
         SkottieMapColor("yellow", SK_ColorYELLOW),
         SkottieMapColor("red", SK_ColorRED),
         SkottieMapColor("blue", SK_ColorBLUE)},
        {},
        {SkottieMapColor("green", SK_ColorGREEN)},
        {SkottieMapColor("transparent", SK_ColorTRANSPARENT)},
        {},
        {}};
    test_skottie_text_maps = {
        {},
        {},
        {},
        {},
        {},
        {{HashSkottieResourceId(kLottieDataWith2TextNode1),
          SkottieTextPropertyValue(
              std::string(kLottieDataWith2TextNode1Text.data()),
              kLottieDataWith2TextNode1Box)},
         {HashSkottieResourceId(kLottieDataWith2TextNode2),
          SkottieTextPropertyValue(
              std::string(kLottieDataWith2TextNode2Text.data()),
              kLottieDataWith2TextNode2Box)}}};
  }

  size_t len = std::min(test_skotties.size(), test_flags.size());
  for (size_t i = 0; i < len; i++) {
    buffer->push<DrawSkottieOp>(test_skotties[i], test_skottie_rects[i],
                                test_skottie_floats[i], test_skottie_images[i],
                                test_skottie_color_maps[i],
                                test_skottie_text_maps[i]);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawSkottieOp>()));
}

void PushDrawTextBlobOps(PaintOpBuffer* buffer) {
  static std::vector<std::vector<sk_sp<SkTypeface>>> test_typefaces = {
      [] {
        return std::vector<sk_sp<SkTypeface>>{SkTypeface::MakeDefault()};
      }(),
      [] {
        return std::vector<sk_sp<SkTypeface>>{SkTypeface::MakeDefault(),
                                              SkTypeface::MakeDefault()};
      }(),
  };
  static std::vector<sk_sp<SkTextBlob>> test_paint_blobs = {
      [] {
        SkFont font;
        font.setTypeface(test_typefaces[0][0]);

        SkTextBlobBuilder builder;
        int glyph_count = 5;
        const auto& run = builder.allocRun(font, glyph_count, 1.2f, 2.3f);
        // allocRun() allocates only the glyph buffer.
        std::fill(run.glyphs, run.glyphs + glyph_count, 0);
        return builder.make();
      }(),
      [] {
        SkFont font;
        font.setTypeface(test_typefaces[1][0]);

        SkTextBlobBuilder builder;
        int glyph_count = 5;
        const auto& run1 = builder.allocRun(font, glyph_count, 1.2f, 2.3f);
        // allocRun() allocates only the glyph buffer.
        std::fill(run1.glyphs, run1.glyphs + glyph_count, 0);

        glyph_count = 16;
        const auto& run2 = builder.allocRunPos(font, glyph_count);
        // allocRun() allocates the glyph buffer, and 2 scalars per glyph for
        // the pos buffer.
        std::fill(run2.glyphs, run2.glyphs + glyph_count, 0);
        std::fill(run2.pos, run2.pos + glyph_count * 2, 0);

        font.setTypeface(test_typefaces[1][1]);
        glyph_count = 8;
        const auto& run3 = builder.allocRunPosH(font, glyph_count, 0);
        // allocRun() allocates the glyph buffer, and 1 scalar per glyph for the
        // pos buffer.
        std::fill(run3.glyphs, run3.glyphs + glyph_count, 0);
        std::fill(run3.pos, run3.pos + glyph_count, 0);
        return builder.make();
      }(),
  };
  size_t len = std::min(
      {test_paint_blobs.size(), test_flags.size(), test_floats.size() - 1});
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawTextBlobOp>(test_paint_blobs[i], test_floats[i],
                                 test_floats[i + 1], test_flags[i]);
  }
  EXPECT_THAT(*buffer, Each(PaintOpIs<DrawTextBlobOp>()));
}

void PushNoopOps(PaintOpBuffer* buffer) {
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  EXPECT_THAT(*buffer, Each(PaintOpIs<NoopOp>()));
}

void PushRestoreOps(PaintOpBuffer* buffer) {
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  EXPECT_THAT(*buffer, Each(PaintOpIs<RestoreOp>()));
}

void PushRotateOps(PaintOpBuffer* buffer) {
  for (float test_float : test_floats)
    buffer->push<RotateOp>(test_float);
  EXPECT_THAT(*buffer, Each(PaintOpIs<RotateOp>()));
}

void PushSaveOps(PaintOpBuffer* buffer) {
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  EXPECT_THAT(*buffer, Each(PaintOpIs<SaveOp>()));
}

void PushSaveLayerOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_flags.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerOp>(test_rects[i], test_flags[i]);

  // Test combinations of optional args.
  buffer->push<SaveLayerOp>(test_flags[0]);
  buffer->push<SaveLayerOp>(test_rects[0], PaintFlags());
  buffer->push<SaveLayerOp>(PaintFlags());
  EXPECT_THAT(*buffer, Each(PaintOpIs<SaveLayerOp>()));
}

void PushSaveLayerAlphaOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_floats.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerAlphaOp>(test_rects[i], test_floats[i]);

  // Test optional args.
  buffer->push<SaveLayerAlphaOp>(test_floats[0]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<SaveLayerAlphaOp>()));
}

void PushScaleOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size() - 1; i += 2)
    buffer->push<ScaleOp>(test_floats[i], test_floats[i + 1]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<ScaleOp>()));
}

void PushSetMatrixOps(PaintOpBuffer* buffer) {
  for (auto& test_matrix : test_matrices)
    buffer->push<SetMatrixOp>(test_matrix);
  EXPECT_THAT(*buffer, Each(PaintOpIs<SetMatrixOp>()));
}

void PushTranslateOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size() - 1; i += 2)
    buffer->push<TranslateOp>(test_floats[i], test_floats[i + 1]);
  EXPECT_THAT(*buffer, Each(PaintOpIs<TranslateOp>()));
}

void PushSetNodeIdOps(PaintOpBuffer* buffer) {
  for (uint32_t test_id : test_ids)
    buffer->push<SetNodeIdOp>(static_cast<int>(test_id));
  EXPECT_THAT(*buffer, Each(PaintOpIs<SetNodeIdOp>()));
}

class PaintOpSerializationTest : public ::testing::TestWithParam<uint8_t> {
 public:
  PaintOpType GetParamType() const {
    return static_cast<PaintOpType>(GetParam());
  }

  void PushTestOps(PaintOpType type) {
    switch (type) {
      case PaintOpType::Annotate:
        PushAnnotateOps(&buffer_);
        break;
      case PaintOpType::ClipPath:
        PushClipPathOps(&buffer_);
        break;
      case PaintOpType::ClipRect:
        PushClipRectOps(&buffer_);
        break;
      case PaintOpType::ClipRRect:
        PushClipRRectOps(&buffer_);
        break;
      case PaintOpType::Concat:
        PushConcatOps(&buffer_);
        break;
      case PaintOpType::CustomData:
        PushCustomDataOps(&buffer_);
        break;
      case PaintOpType::DrawColor:
        PushDrawColorOps(&buffer_);
        break;
      case PaintOpType::DrawDRRect:
        PushDrawDRRectOps(&buffer_);
        break;
      case PaintOpType::DrawImage:
        PushDrawImageOps(&buffer_);
        break;
      case PaintOpType::DrawImageRect:
        PushDrawImageRectOps(&buffer_);
        break;
      case PaintOpType::DrawIRect:
        PushDrawIRectOps(&buffer_);
        break;
      case PaintOpType::DrawLine:
        PushDrawLineOps(&buffer_);
        break;
      case PaintOpType::DrawOval:
        PushDrawOvalOps(&buffer_);
        break;
      case PaintOpType::DrawPath:
        PushDrawPathOps(&buffer_);
        break;
      case PaintOpType::DrawRecord:
        // Not supported.
        break;
      case PaintOpType::DrawRect:
        PushDrawRectOps(&buffer_);
        break;
      case PaintOpType::DrawRRect:
        PushDrawRRectOps(&buffer_);
        break;
      case PaintOpType::DrawSkottie:
        PushDrawSkottieOps(&buffer_);
        break;
      case PaintOpType::DrawSlug:
        // TODO(crbug.com/1321150): fix the test for DrawSlug.
        break;
      case PaintOpType::DrawTextBlob:
        // TODO(crbug.com/1321150): fix the test for DrawTextBlobs
        // PushDrawTextBlobOps(&buffer_);
        break;
      case PaintOpType::Noop:
        PushNoopOps(&buffer_);
        break;
      case PaintOpType::Restore:
        PushRestoreOps(&buffer_);
        break;
      case PaintOpType::Rotate:
        PushRotateOps(&buffer_);
        break;
      case PaintOpType::Save:
        PushSaveOps(&buffer_);
        break;
      case PaintOpType::SaveLayer:
        PushSaveLayerOps(&buffer_);
        break;
      case PaintOpType::SaveLayerAlpha:
        PushSaveLayerAlphaOps(&buffer_);
        break;
      case PaintOpType::Scale:
        PushScaleOps(&buffer_);
        break;
      case PaintOpType::SetMatrix:
        PushSetMatrixOps(&buffer_);
        break;
      case PaintOpType::Translate:
        PushTranslateOps(&buffer_);
        break;
      case PaintOpType::SetNodeId:
        PushSetNodeIdOps(&buffer_);
        break;
    }
  }

  void ResizeOutputBuffer() {
    // An serialization buffer size that should fit all the ops in the buffer_.
    output_size_ = kSerializedBytesPerOp * buffer_.size();
    output_ = AllocateSerializedBuffer(output_size_);
  }

  bool IsTypeSupported() {
    // TODO(crbug.com/1321150): fix the test for DrawTextBlobs
    if (GetParamType() == PaintOpType::DrawTextBlob ||
        GetParamType() == PaintOpType::DrawSlug) {
      return false;
    }

    // DrawRecordOps must be flattened and are not currently serialized. All
    // other types must push non-zero amounts of ops in PushTestOps.
    return GetParamType() != PaintOpType::DrawRecord &&
           (GetParamType() != PaintOpType::DrawSkottie || kIsSkottieSupported);
  }

 protected:
  std::unique_ptr<char, base::AlignedFreeDeleter> output_;
  size_t output_size_ = 0u;
  PaintOpBuffer buffer_;
};

INSTANTIATE_TEST_SUITE_P(
    P,
    PaintOpSerializationTest,
    ::testing::Range(static_cast<uint8_t>(0),
                     static_cast<uint8_t>(PaintOpType::LastPaintOpType)));

// Test serializing and then deserializing all test ops.  They should all
// write successfully and be identical to the original ops in the buffer.
TEST_P(PaintOpSerializationTest, SmokeTest) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);

  auto canvas =
      serializer.options_provider()->strike_server()->makeAnalysisCanvas(
          1024, 768, {}, nullptr, true);
  PlaybackParams params(nullptr, canvas->getLocalToDevice());
  params.is_analyzing = true;
  buffer_.Playback(canvas.get(), params);

  std::vector<uint8_t> strike_data;
  serializer.options_provider()->strike_server()->writeStrikeData(&strike_data);

  if (!strike_data.empty()) {
    serializer.options_provider()->strike_client()->readStrikeData(
        strike_data.data(), strike_data.size());
  }
  serializer.Serialize(buffer_);

  // Expect all ops to write more than 0 bytes.
  for (size_t i = 0; i < buffer_.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zd", PaintOpTypeToString(GetParamType()).c_str(), i));
    EXPECT_GT(serializer.bytes_written()[i], 0u);
  }

  PaintOpBuffer::Iterator iter(buffer_);
  size_t i = 0;
  for (const PaintOp& base_written : DeserializerIterator(
           output_.get(), serializer.TotalBytesWritten(),
           serializer.options_provider()->deserialize_options())) {
    ASSERT_TRUE(iter);
    EXPECT_TRUE(base_written.EqualsForTesting(*iter))
        << "i: " << i
        << "\n    Written:  " << PaintOpHelper::ToString(base_written)
        << "\n    Original: " << PaintOpHelper::ToString(*iter);
    ++iter;
    ++i;
  }

  EXPECT_EQ(buffer_.size(), i);
}

// Verify for all test ops that serializing into a smaller size aborts
// correctly and doesn't write anything.
TEST_P(PaintOpSerializationTest, SerializationFailures) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);
  serializer.Serialize(buffer_);
  std::vector<size_t> bytes_written = serializer.bytes_written();

  TestOptionsProvider options_provider;

  size_t op_idx = 0;
  for (const PaintOp& op : buffer_) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zu", PaintOpTypeToString(GetParamType()).c_str(), op_idx));
    size_t expected_bytes = bytes_written[op_idx];
    EXPECT_GT(expected_bytes, 0u);
    EXPECT_EQ(
        expected_bytes,
        base::bits::AlignUp(expected_bytes, PaintOpWriter::kMaxAlignment));

    // Attempt to write op into a buffer of size |i|, and only expect
    // it to succeed if the buffer is large enough.
    for (size_t i = 0; i < bytes_written[op_idx] + 2; ++i) {
      options_provider.ClearPaintCache();
      options_provider.ForcePurgeSkottieSerializationHistory();
      size_t written_bytes =
          op.Serialize(output_.get(), i, options_provider.serialize_options(),
                       nullptr, SkM44(), SkM44());
      if (i >= expected_bytes) {
        EXPECT_EQ(expected_bytes, written_bytes) << "i: " << i;
      } else {
        EXPECT_EQ(0u, written_bytes) << "i: " << i;
      }
    }
    ++op_idx;
  }
}

// Verify that deserializing test ops from too small buffers aborts
// correctly, in case the deserialized data is lying about how big it is.
TEST_P(PaintOpSerializationTest, DeserializationFailures) {
  if (!IsTypeSupported())
    return;

  PushTestOps(GetParamType());

  ResizeOutputBuffer();

  SimpleSerializer serializer(output_.get(), output_size_);
  serializer.Serialize(buffer_);
  TestOptionsProvider* options_provider = serializer.options_provider();

  char* first = static_cast<char*>(output_.get());
  char* current = first;

  static constexpr size_t kAlign = PaintOpWriter::kMaxAlignment;
  static constexpr size_t kOutputOpSize = kSerializedBytesPerOp;
  auto deserialize_buffer = AllocateSerializedBuffer(kOutputOpSize);

  size_t total_read = 0;
  for (size_t op_idx = 0; op_idx < buffer_.size(); ++op_idx) {
    uint8_t serialized_type = 0;
    size_t serialized_size = 0;
    ReadAndValidateOpHeader(current, PaintOpWriter::kHeaderBytes,
                            &serialized_type, &serialized_size);
    EXPECT_EQ(static_cast<uint8_t>(GetParamType()), serialized_type);

    // Read from buffers of various sizes to make sure that having a serialized
    // op size that is larger than the input buffer provided causes a
    // deserialization failure to return nullptr.  Also test a few valid sizes
    // larger than read size.
    for (size_t read_size = 0; read_size < serialized_size + kAlign * 2 + 2;
         ++read_size) {
      SCOPED_TRACE(base::StringPrintf(
          "%s #%zd, read_size: %zu, align: %zu, serialized_size: %zu",
          PaintOpTypeToString(GetParamType()).c_str(), op_idx, read_size,
          kAlign, serialized_size));
      // Because PaintOp::Deserialize() early outs when the input size is <
      // serialized_size, here deliberately lie about the serialized_size.
      // This will verify that individual op deserializing code behaves properly
      // when presented with invalid offsets.
      PaintOpWriter::WriteHeaderForTesting(current, serialized_type, read_size);
      size_t bytes_read = 0;
      PaintOp* written = PaintOp::Deserialize(
          current, read_size, deserialize_buffer.get(), kOutputOpSize,
          &bytes_read, options_provider->deserialize_options());

      // Deserialize buffers with valid ops until the last op. This verifies
      // that the complete buffer is invalidated on encountering the first
      // corrupted op.
      auto deserialized_buffer = PaintOpBuffer::MakeFromMemory(
          first, total_read + read_size,
          options_provider->deserialize_options());

      // Serizlized sizes are only valid if they are aligned.
      if (read_size >= serialized_size && read_size % kAlign == 0) {
        ASSERT_NE(nullptr, written);
        ASSERT_LE(written->aligned_size, kOutputOpSize);
        EXPECT_EQ(GetParamType(), written->GetType());
        EXPECT_EQ(read_size, bytes_read);

        ASSERT_TRUE(deserialized_buffer);
        EXPECT_EQ(deserialized_buffer->size(), op_idx + 1);
      } else if (read_size == 0 && op_idx != 0) {
        // If no data was read for a subsequent op while some ops were
        // deserialized, we still have a valid buffer with the deserialized ops.
        ASSERT_TRUE(deserialized_buffer);
        EXPECT_EQ(deserialized_buffer->size(), op_idx);
      } else {
        // If a subsequent op was corrupted or no ops could be serialized, we
        // have an invalid buffer.
        EXPECT_EQ(nullptr, written);
        // If the buffer is exactly 0 bytes, then MakeFromMemory treats it as a
        // valid empty buffer.
        if (deserialized_buffer) {
          EXPECT_EQ(0u, read_size);
          EXPECT_EQ(0u, deserialized_buffer->size());
          // Verify that we can create an iterator from this buffer, but it's
          // empty.
          PaintOpBuffer::Iterator it(*deserialized_buffer);
          EXPECT_FALSE(it);
        } else {
          EXPECT_NE(0u, read_size);
        }
      }

      if (written)
        written->DestroyThis();
    }

    // Restore the correct serialized_size.
    PaintOpWriter::WriteHeaderForTesting(current, serialized_type,
                                         serialized_size);
    current += serialized_size;
    total_read += serialized_size;
  }
}

TEST_P(PaintOpSerializationTest, UsesOverridenFlags) {
  if (!PaintOp::TypeHasFlags(GetParamType())) {
    return;
  }

  // See https://crbug.com/1321150#c3.
  if (GetParamType() == PaintOpType::DrawTextBlob ||
      GetParamType() == PaintOpType::DrawSlug) {
    return;
  }

  PushTestOps(GetParamType());
  ResizeOutputBuffer();

  TestOptionsProvider options_provider;
  alignas(PaintOpBuffer::kPaintOpAlign) char
      deserialized[kLargestPaintOpAlignedSize];
  for (const PaintOp& op : buffer_) {
    size_t bytes_written = op.Serialize(output_.get(), output_size_,
                                        options_provider.serialize_options(),
                                        nullptr, SkM44(), SkM44());
    size_t bytes_read = 0u;
    PaintOp* written = PaintOp::Deserialize(
        output_.get(), bytes_written, deserialized, std::size(deserialized),
        &bytes_read, options_provider.deserialize_options());
    ASSERT_TRUE(written) << PaintOpTypeToString(GetParamType());
    EXPECT_TRUE(op.EqualsForTesting(*written))
        << "\n    Written:  " << PaintOpHelper::ToString(*written)
        << "\n    Original: " << PaintOpHelper::ToString(op);

    written->DestroyThis();
    written = nullptr;

    PaintFlags override_flags = static_cast<const PaintOpWithFlags&>(op).flags;
    override_flags.setAlphaf(override_flags.getAlphaf() * 0.5f);
    bytes_written = op.Serialize(output_.get(), output_size_,
                                 options_provider.serialize_options(),
                                 &override_flags, SkM44(), SkM44());
    written = PaintOp::Deserialize(output_.get(), bytes_written, deserialized,
                                   std::size(deserialized), &bytes_read,
                                   options_provider.deserialize_options());
    ASSERT_TRUE(written);
    ASSERT_TRUE(written->IsPaintOpWithFlags());
    EXPECT_EQ(static_cast<const PaintOpWithFlags*>(written)->flags.getAlpha(),
              override_flags.getAlpha());
    written->DestroyThis();
    written = nullptr;
  }
}

TEST(PaintOpSerializationTest, CompleteBufferSerialization) {
  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc);

  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(1000, 1000);
  preamble.playback_rect = gfx::Rect(preamble.content_size);
  preamble.full_raster_rect = preamble.playback_rect;
  preamble.requires_clear = true;

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  // The deserialized buffer has an extra pair of save/restores and a clear, for
  // the preamble and root buffer.
  EXPECT_THAT(*deserialized_buffer,
              ElementsAre(
                  // Preamble:
                  PaintOpEq<SaveOp>(), PaintOpIs<DrawColorOp>(),
                  PaintOpIs<ClipRectOp>(),
                  // Serialized buffer:
                  PaintOpEq<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc),
                  // End restore:
                  PaintOpEq<RestoreOp>()));
}

TEST(PaintOpSerializationTest, Preamble) {
  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(30, 40);
  preamble.full_raster_rect = gfx::Rect(10, 20, 8, 7);
  preamble.playback_rect = gfx::Rect(12, 25, 1, 2);
  preamble.post_translation = gfx::Vector2dF(4.3f, 7.f);
  preamble.post_scale = gfx::Vector2dF(0.5f, 0.5f);
  preamble.requires_clear = true;

  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  EXPECT_THAT(
      *deserialized_buffer,
      ElementsAre(
          // Preamble:
          PaintOpEq<SaveOp>(),
          PaintOpEq<TranslateOp>(-preamble.full_raster_rect.x(),
                                 -preamble.full_raster_rect.y()),
          PaintOpEq<ClipRectOp>(RectToSkRect(preamble.playback_rect),
                                SkClipOp::kIntersect, /*antialias=*/false),
          PaintOpEq<TranslateOp>(preamble.post_translation.x(),
                                 preamble.post_translation.y()),
          PaintOpEq<ScaleOp>(preamble.post_scale.x(), preamble.post_scale.y()),
          PaintOpEq<DrawColorOp>(SkColors::kTransparent, SkBlendMode::kSrc),
          // From the serialized buffer:
          PaintOpEq<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc),
          // End restore:
          PaintOpEq<RestoreOp>()));
}

TEST(PaintOpSerializationTest,
     ConvertToDrawSlugWhenSerializationAndRasterization) {
  PaintOpBuffer buffer;
  PushDrawTextBlobOps(&buffer);
  EXPECT_TRUE(buffer.has_draw_text_ops());

  PaintOpBuffer::Iterator iter(buffer);
  const PaintOp* op = iter.get();
  ASSERT_TRUE(op);
  EXPECT_EQ(op->GetType(), PaintOpType::DrawTextBlob);

  size_t output_size = kSerializedBytesPerOp * buffer.size();
  std::unique_ptr<char, base::AlignedFreeDeleter> output =
      AllocateSerializedBuffer(output_size);
  SimpleSerializer serializer(output.get(), output_size);

  auto canvas =
      serializer.options_provider()->strike_server()->makeAnalysisCanvas(
          1024, 768, {}, nullptr, true);
  PlaybackParams params(nullptr, canvas->getLocalToDevice());
  params.is_analyzing = true;
  buffer.Playback(canvas.get(), params);

  std::vector<uint8_t> strike_data;
  serializer.options_provider()->strike_server()->writeStrikeData(&strike_data);

  if (!strike_data.empty()) {
    serializer.options_provider()->strike_client()->readStrikeData(
        strike_data.data(), strike_data.size());
  }

  serializer.Serialize(buffer);

  size_t i = 0;
  for (const PaintOp& base_written : DeserializerIterator(
           output.get(), serializer.TotalBytesWritten(),
           serializer.options_provider()->deserialize_options())) {
    ASSERT_TRUE(iter);
    EXPECT_EQ(PaintOpType::DrawSlug, base_written.GetType());
    ++iter;
    ++i;
  }

  EXPECT_EQ(buffer.size(), i);
}

TEST(PaintOpSerializationTest, SerializesNestedRecords) {
  PaintOpBuffer sub_buffer;
  sub_buffer.push<ScaleOp>(0.5f, 0.75f);
  sub_buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 20.f), PaintFlags());
  auto record = sub_buffer.ReleaseAsRecord();
  PaintOpBuffer buffer;
  buffer.push<DrawRecordOp>(record);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  PaintOpBufferSerializer::Preamble preamble;
  serializer.Serialize(buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  EXPECT_THAT(
      *deserialized_buffer,
      ElementsAre(
          // Preamble:
          PaintOpEq<SaveOp>(),
          PaintOpEq<DrawColorOp>(SkColors::kTransparent, SkBlendMode::kSrc),
          PaintOpEq<SaveOp>(),
          // From the serialized buffer:
          PaintOpEq<ScaleOp>(0.5f, 0.75f),
          PaintOpEq<DrawRectOp>(SkRect::MakeWH(10.f, 20.f), PaintFlags()),
          // End restore:
          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(PaintOpBufferTest, ClipsImagesDuringSerialization) {
  struct {
    gfx::Rect clip_rect;
    gfx::Rect image_rect;
    bool should_draw;
  } test_cases[] = {
      {gfx::Rect(0, 0, 100, 100), gfx::Rect(50, 50, 100, 100), true},
      {gfx::Rect(0, 0, 100, 100), gfx::Rect(105, 105, 100, 100), false},
      {gfx::Rect(0, 0, 500, 500), gfx::Rect(450, 450, 100, 100), true},
      {gfx::Rect(0, 0, 500, 500), gfx::Rect(750, 750, 100, 100), false},
      {gfx::Rect(250, 250, 250, 250), gfx::Rect(450, 450, 100, 100), true},
      {gfx::Rect(250, 250, 250, 250), gfx::Rect(50, 50, 100, 100), false},
      {gfx::Rect(0, 0, 100, 500), gfx::Rect(250, 250, 100, 100), false},
      {gfx::Rect(0, 0, 200, 500), gfx::Rect(100, 250, 100, 100), true}};

  for (const auto& test_case : test_cases) {
    PaintOpBuffer buffer;
    buffer.push<DrawImageOp>(
        CreateDiscardablePaintImage(test_case.image_rect.size()),
        static_cast<SkScalar>(test_case.image_rect.x()),
        static_cast<SkScalar>(test_case.image_rect.y()));

    auto memory = AllocateSerializedBuffer();
    TestOptionsProvider options_provider;
    SimpleBufferSerializer serializer(memory.get(),
                                      kDefaultSerializedBufferSize,
                                      options_provider.serialize_options());
    PaintOpBufferSerializer::Preamble preamble;
    preamble.playback_rect = test_case.clip_rect;
    preamble.full_raster_rect = gfx::Rect(0, 0, test_case.clip_rect.right(),
                                          test_case.clip_rect.bottom());
    // Avoid clearing.
    preamble.content_size = gfx::Size(1000, 1000);
    preamble.requires_clear = false;
    serializer.Serialize(buffer, nullptr, preamble);
    ASSERT_NE(serializer.written(), 0u);

    sk_sp<PaintOpBuffer> deserialized_buffer =
        PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                      options_provider.deserialize_options());

    std::vector<Matcher<PaintOp>> matchers;
    matchers.push_back(PaintOpIs<SaveOp>());
    matchers.push_back(PaintOpIs<ClipRectOp>());
    if (test_case.should_draw) {
      matchers.push_back(PaintOpIs<DrawImageOp>());
    }
    matchers.push_back(PaintOpIs<RestoreOp>());
    EXPECT_THAT(*deserialized_buffer, ElementsAreArray(matchers));
  }
}

TEST(PaintOpBufferSerializationTest, AlphaFoldingDuringSerialization) {
  PaintOpBuffer buffer;

  float alpha = 0.4f;
  buffer.push<SaveLayerAlphaOp>(alpha);

  PaintFlags draw_flags;
  float draw_rect_alpha = 0.25f;
  draw_flags.setColor(SkColors::kMagenta);
  draw_flags.setAlphaf(draw_rect_alpha);
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(1000, 1000);
  preamble.playback_rect = gfx::Rect(gfx::Size(100, 100));
  preamble.full_raster_rect = preamble.playback_rect;
  preamble.requires_clear = false;

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());

  EXPECT_THAT(*deserialized_buffer,
              ElementsAre(PaintOpIs<SaveOp>(), PaintOpIs<ClipRectOp>(),
                          AllOf(PaintOpIs<DrawRectOp>(),
                                // Expect the alpha from the draw and the save
                                // layer to be folded together.
                                ResultOf(
                                    [](const PaintOp& op) {
                                      return static_cast<const DrawRectOp&>(op)
                                          .flags.getAlphaf();
                                    },
                                    FloatEq(alpha * draw_rect_alpha))),
                          PaintOpIs<RestoreOp>()));
}

// Test generic PaintOp deserializing failure cases.
TEST(PaintOpBufferTest, PaintOpDeserialize) {
  auto input = AllocateSerializedBuffer(kSerializedBytesPerOp);
  alignas(PaintOpBuffer::kPaintOpAlign) char output[kLargestPaintOpAlignedSize];

  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SkColors::kMagenta, SkBlendMode::kSrc);

  PaintOpBuffer::Iterator iter(buffer);
  const PaintOp* op = iter.get();
  ASSERT_TRUE(op);

  TestOptionsProvider options_provider;
  size_t bytes_written = op->Serialize(input.get(), kSerializedBytesPerOp,
                                       options_provider.serialize_options(),
                                       nullptr, SkM44(), SkM44());
  ASSERT_GT(bytes_written, 0u);

  // Can deserialize from exactly the right size.
  size_t bytes_read = 0;
  PaintOp* success = PaintOp::Deserialize(
      input.get(), bytes_written, output, std::size(output), &bytes_read,
      options_provider.deserialize_options());
  ASSERT_TRUE(success);
  EXPECT_EQ(bytes_written, bytes_read);
  success->DestroyThis();

  // Fail to deserialize if serialized_size goes past input size (the
  // DeserializationFailures test above tests if the serialized_size is lying).
  for (size_t i = 0; i < bytes_written - 1; ++i) {
    EXPECT_FALSE(PaintOp::Deserialize(input.get(), i, output, std::size(output),
                                      &bytes_read,
                                      options_provider.deserialize_options()));
  }

  // Unaligned serialized_size should fail to deserialize.
  uint8_t serialized_type = 0;
  size_t serialized_size = 0;
  ReadAndValidateOpHeader(input.get(), bytes_written, &serialized_type,
                          &serialized_size);
  EXPECT_EQ(serialized_size,
            base::bits::AlignUp(serialized_size, PaintOpWriter::kMaxAlignment));
  PaintOpWriter::WriteHeaderForTesting(input.get(), serialized_type,
                                       serialized_size - 1);
  EXPECT_FALSE(PaintOp::Deserialize(input.get(), bytes_written, output,
                                    std::size(output), &bytes_read,
                                    options_provider.deserialize_options()));

  // Bogus types fail to deserialize.
  PaintOpWriter::WriteHeaderForTesting(
      input.get(), static_cast<uint8_t>(PaintOpType::LastPaintOpType) + 1,
      serialized_size);
  EXPECT_FALSE(PaintOp::Deserialize(input.get(), bytes_written, output,
                                    std::size(output), &bytes_read,
                                    options_provider.deserialize_options()));
}

// Test that deserializing invalid paint ops fails silently. Skia release
// asserts on invalid values in several places so these are not safe to pass
// them to the SkCanvas API.
TEST(PaintOpBufferTest, ValidateRects) {
  auto serialized = AllocateSerializedBuffer(kSerializedBytesPerOp);
  alignas(PaintOpBuffer::kPaintOpAlign) char
      deserialized[kLargestPaintOpAlignedSize];
  // We may read uninitialized gaps in this test. Initialize the buffer with a
  // special value to avoid MSAN errors.
  memset(serialized.get(), 0xA5, kSerializedBytesPerOp);
  memset(deserialized, 0x5A, std::size(deserialized));

  float rect_size = 0x8.765432p1;
  SkRect rect = SkRect::MakeWH(rect_size, rect_size);
  // Push all op variations that take rects.
  PaintOpBuffer buffer;
  buffer.push<AnnotateOp>(PaintCanvas::AnnotationType::URL, rect,
                          SkData::MakeWithCString("test1"));
  buffer.push<ClipRectOp>(rect, SkClipOp::kDifference, true);

  buffer.push<DrawImageRectOp>(test_images[0], rect, test_rects[1],
                               SkCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawImageRectOp>(test_images[0], test_rects[0], rect,
                               SkCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawOvalOp>(rect, test_flags[0]);
  buffer.push<DrawRectOp>(rect, test_flags[0]);
  buffer.push<SaveLayerOp>(rect, PaintFlags());
  buffer.push<SaveLayerOp>(rect, test_flags[0]);
  buffer.push<SaveLayerAlphaOp>(rect, test_floats[0]);

  TestOptionsProvider options_provider;

  // Every op should serialize but fail to deserialize due to the bad rect.
  int op_idx = 0;
  for (const PaintOp& op : buffer) {
    size_t bytes_written = op.Serialize(serialized.get(), kSerializedBytesPerOp,
                                        options_provider.serialize_options(),
                                        nullptr, SkM44(), SkM44());
    ASSERT_GT(bytes_written, sizeof(float));

    size_t bytes_read = 0;
    PaintOp* deserialized_op = PaintOp::Deserialize(
        serialized.get(), bytes_written, deserialized, std::size(deserialized),
        &bytes_read, options_provider.deserialize_options());
    EXPECT_TRUE(deserialized_op) << op_idx;
    deserialized_op->DestroyThis();

    // Replace the first occurrence of rect_size with NaN to make the ClipRectOp
    // invalid.
    for (size_t i = 0; i < bytes_written; i += sizeof(float)) {
      float* f = reinterpret_cast<float*>(serialized.get() + i);
      if (*f == rect_size) {
        *f = std::numeric_limits<float>::quiet_NaN();
        break;
      }
    }
    deserialized_op = PaintOp::Deserialize(
        serialized.get(), bytes_written, deserialized, std::size(deserialized),
        &bytes_read, options_provider.deserialize_options());
    EXPECT_FALSE(deserialized_op) << op_idx;

    ++op_idx;
  }
}

TEST(PaintOpBufferTest, ValidateSkClip) {
  SkPath path;
  ClipPathOp good(path, SkClipOp::kMax_EnumValue, /*antialias=*/true,
                  UsePaintCache::kDisabled);
  EXPECT_TRUE(good.IsValid());

  SkClipOp bad_clip = static_cast<SkClipOp>(
      static_cast<uint32_t>(SkClipOp::kMax_EnumValue) + 1);
  ClipPathOp bad1(path, bad_clip, /*antialias=*/true, UsePaintCache::kDisabled);
  EXPECT_FALSE(bad1.IsValid());
  ClipRectOp bad2(test_rects[0], bad_clip, true);
  EXPECT_FALSE(bad2.IsValid());
  ClipRRectOp bad3(test_rrects[0], bad_clip, false);
  EXPECT_FALSE(bad3.IsValid());
}

TEST(PaintOpBufferTest, ValidateSkBlendMode) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(SkBlendMode::kLastMode) + 1;
       i++) {
    SkBlendMode blend_mode = static_cast<SkBlendMode>(i);

    DrawColorOp draw_color(SkColors::kMagenta, blend_mode);
    EXPECT_EQ(blend_mode <= SkBlendMode::kLastCoeffMode, draw_color.IsValid());

    PaintFlags flags = test_flags[i % test_flags.size()];
    flags.setBlendMode(blend_mode);
    DrawRectOp draw_rect(test_rects[0], flags);
    EXPECT_EQ(blend_mode <= SkBlendMode::kLastMode, draw_rect.IsValid());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageOp) {
  PaintOpBuffer buffer;
  PushDrawImageOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawImageOp&>(base_op);

    SkRect image_rect =
        SkRect::MakeXYWH(op.left, op.top, op.image.width(), op.image.height());
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, image_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageRectOp) {
  PaintOpBuffer buffer;
  PushDrawImageRectOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawImageRectOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.dst.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawIRectOp) {
  PaintOpBuffer buffer;
  PushDrawIRectOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawIRectOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, SkRect::Make(op.rect).makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawOvalOp) {
  PaintOpBuffer buffer;
  PushDrawOvalOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawOvalOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.oval.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawPathOp) {
  PaintOpBuffer buffer;
  PushDrawPathOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawPathOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.path.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRectOp) {
  PaintOpBuffer buffer;
  PushDrawRectOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawRectOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRRectOp) {
  PaintOpBuffer buffer;
  PushDrawRRectOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawRRectOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.rrect.rect().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawLineOp) {
  PaintOpBuffer buffer;
  PushDrawLineOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawLineOp&>(base_op);

    SkRect line_rect;
    line_rect.fLeft = op.x0;
    line_rect.fTop = op.y0;
    line_rect.fRight = op.x1;
    line_rect.fBottom = op.y1;
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, line_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawDRRectOp) {
  PaintOpBuffer buffer;
  PushDrawDRRectOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawDRRectOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.outer.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawTextBlobOp) {
  PaintOpBuffer buffer;
  PushDrawTextBlobOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawTextBlobOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.blob->bounds().makeOffset(op.x, op.y).makeSorted());
  }
}

class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  explicit MockImageProvider(bool fail_all_decodes)
      : fail_all_decodes_(fail_all_decodes) {}
  MockImageProvider(std::vector<SkSize> src_rect_offset,
                    std::vector<SkSize> scale,
                    std::vector<PaintFlags::FilterQuality> quality)
      : src_rect_offset_(src_rect_offset), scale_(scale), quality_(quality) {}

  ~MockImageProvider() override = default;

  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override {
    decoded_images_.push_back(draw_image);
    if (draw_image.paint_image().IsPaintWorklet())
      return ScopedResult(record_);

    if (fail_all_decodes_)
      return ImageProvider::ScopedResult();

    SkBitmap bitmap;
    bitmap.allocPixelsFlags(SkImageInfo::MakeN32Premul(10, 10),
                            SkBitmap::kZeroPixels_AllocFlag);
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
    size_t i = index_++;
    return ScopedResult(DecodedDrawImage(image, nullptr, src_rect_offset_[i],
                                         scale_[i], quality_[i], true));
  }

  void SetRecord(PaintRecord record) { record_ = std::move(record); }

  const std::vector<DrawImage>& decoded_images() const {
    return decoded_images_;
  }

 private:
  std::vector<SkSize> src_rect_offset_;
  std::vector<SkSize> scale_;
  std::vector<PaintFlags::FilterQuality> quality_;
  size_t index_ = 0;
  bool fail_all_decodes_ = false;
  PaintRecord record_;
  std::vector<DrawImage> decoded_images_;
};

TEST(PaintOpBufferTest, SkipsOpsOutsideClip) {
  // All ops with images draw outside the clip and should be skipped. If any
  // call is made to the ImageProvider, it should crash.
  MockImageProvider image_provider;
  PaintOpBuffer buffer;

  // Apply a clip outside the region for images.
  buffer.push<ClipRectOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                          SkClipOp::kIntersect, false);

  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f);
  PaintFlags image_flags;
  image_flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                               SkTileMode::kRepeat, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(110, 110, 100, 100), image_flags);

  SkRect rect = SkRect::MakeXYWH(0, 0, 100, 100);
  buffer.push<DrawRectOp>(rect, PaintFlags());

  // The single save/restore call is from the PaintOpBuffer's use of
  // SkAutoRestoreCanvas.
  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(_)).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

TEST(PaintOpBufferTest, SkipsOpsWithFailedDecodes) {
  MockImageProvider image_provider(true);
  PaintOpBuffer buffer;

  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f);
  PaintFlags image_flags;
  image_flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                               SkTileMode::kRepeat, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(110, 110, 100, 100), image_flags);
  buffer.push<DrawColorOp>(SkColors::kRed, SkBlendMode::kSrcOver);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(_)).InSequence(s);
  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

MATCHER(NonLazyImage, "") {
  return !arg->isLazyGenerated();
}

MATCHER_P(MatchesPaintImage, paint_image, "") {
  return arg.paint_image().IsSameForTesting(paint_image);
}

MATCHER_P2(MatchesRect, rect, scale, "") {
  EXPECT_EQ(arg.x(), rect.x() * scale.width());
  EXPECT_EQ(arg.y(), rect.y() * scale.height());
  EXPECT_EQ(arg.width(), rect.width() * scale.width());
  EXPECT_EQ(arg.height(), rect.height() * scale.height());
  return true;
}

MATCHER_P(MatchesQuality, quality, "") {
  return quality == arg->getFilterQuality();
}

MATCHER_P2(MatchesShader, flags, scale, "") {
  SkMatrix matrix;
  SkTileMode xy[2];
  SkImage* image = arg.getShader()->isAImage(&matrix, xy);

  EXPECT_FALSE(image->isLazyGenerated());

  SkSize local_scale;
  matrix.decomposeScale(&local_scale, nullptr);
  EXPECT_EQ(local_scale.width(), 1.0f / scale.width());
  EXPECT_EQ(local_scale.height(), 1.0f / scale.height());

  EXPECT_EQ(flags.getShader()->tx(), xy[0]);
  EXPECT_EQ(flags.getShader()->ty(), xy[1]);

  return true;
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectBasicCase) {
  PaintOpBuffer paint_worklet_buffer;
  paint_worklet_buffer.push<TranslateOp>(8.0f, 8.0f);
  paint_worklet_buffer.push<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                         PaintFlags());
  PaintFlags draw_flags;
  draw_flags.setColor(0u);
  SkRect rect = SkRect::MakeXYWH(0, 0, 100, 100);
  paint_worklet_buffer.push<DrawRectOp>(rect, draw_flags);

  MockImageProvider provider;
  provider.SetRecord(paint_worklet_buffer.ReleaseAsRecord());

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(0, 0, 100, 100);
  blink_buffer.push<DrawImageRectOp>(image, src, dst,
                                     SkCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didTranslate(8.0f, 8.0f));
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectTranslated) {
  PaintOpBuffer paint_worklet_buffer;
  paint_worklet_buffer.push<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                         PaintFlags());
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  paint_worklet_buffer.push<DrawImageOp>(
      paint_image, 0.0f, 0.0f, SkSamplingOptions(SkFilterMode::kLinear),
      nullptr);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer.ReleaseAsRecord());

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(5, 7, 100, 100);
  blink_buffer.push<DrawImageRectOp>(image, src, dst,
                                     SkCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  SkSamplingOptions sampling({0, 1.0f / 2});

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, didConcat44(SkM44::Translate(5.0f, 7.0f)));
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didScale(1.0f / scale_adjustment[0].width(),
                               1.0f / scale_adjustment[0].height()));
  EXPECT_CALL(canvas, onDrawImage2(NonLazyImage(), 0.0f, 0.0f, sampling, _));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectScaled) {
  PaintOpBuffer paint_worklet_buffer;
  paint_worklet_buffer.push<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                         PaintFlags());
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  paint_worklet_buffer.push<DrawImageOp>(
      paint_image, 0.0f, 0.0f, SkSamplingOptions(SkFilterMode::kLinear),
      nullptr);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer.ReleaseAsRecord());

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(0, 0, 200, 150);
  blink_buffer.push<DrawImageRectOp>(image, src, dst,
                                     SkCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  SkSamplingOptions sampling({0, 1.0f / 2});

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, didConcat44(SkM44::Scale(2.f, 1.5f)));
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didScale(1.0f / scale_adjustment[0].width(),
                               1.0f / scale_adjustment[0].height()));
  EXPECT_CALL(canvas, onDrawImage2(NonLazyImage(), 0.0f, 0.0f, sampling, _));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectClipped) {
  PaintOpBuffer paint_worklet_buffer;
  paint_worklet_buffer.push<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 60, 60),
                                         PaintFlags());
  SkSamplingOptions linear(SkFilterMode::kLinear);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  // One rect inside the src-rect, one outside.
  paint_worklet_buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, linear,
                                         nullptr);
  paint_worklet_buffer.push<DrawImageOp>(paint_image, 50.0f, 50.0f, linear,
                                         nullptr);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer.ReleaseAsRecord());

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 20, 20);
  SkRect dst = SkRect::MakeXYWH(0, 0, 20, 20);
  blink_buffer.push<DrawImageRectOp>(image, src, dst,
                                     SkCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  SkSamplingOptions sampling({0, 1.0f / 2});

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didScale(1.0f / scale_adjustment[0].width(),
                               1.0f / scale_adjustment[0].height()));
  EXPECT_CALL(canvas, onDrawImage2(NonLazyImage(), 0.0f, 0.0f, sampling, _));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, ReplacesImagesFromProvider) {
  std::vector<SkSize> src_rect_offset = {
      SkSize::MakeEmpty(), SkSize::Make(2.0f, 2.0f), SkSize::Make(3.0f, 3.0f)};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f),
                                          SkSize::Make(0.3f, 0.3f),
                                          SkSize::Make(0.4f, 0.4f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh, PaintFlags::FilterQuality::kMedium,
      PaintFlags::FilterQuality::kHigh};

  MockImageProvider image_provider(src_rect_offset, scale_adjustment, quality);
  PaintOpBuffer buffer;

  SkRect rect = SkRect::MakeWH(10, 10);
  SkSamplingOptions sampling(SkFilterMode::kLinear);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, sampling, nullptr);
  buffer.push<DrawImageRectOp>(paint_image, rect, rect, sampling, nullptr,
                               SkCanvas::kFast_SrcRectConstraint);
  PaintFlags flags;
  flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                         SkTileMode::kRepeat, nullptr));
  buffer.push<DrawOvalOp>(SkRect::MakeWH(10, 10), flags);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  SkSamplingOptions sampling0({0, 1.0f / 2});
  SkSamplingOptions sampling1(SkFilterMode::kLinear, SkMipmapMode::kNearest);

  // Save/scale/image/restore from DrawImageop.
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didScale(1.0f / scale_adjustment[0].width(),
                               1.0f / scale_adjustment[0].height()));
  EXPECT_CALL(canvas, onDrawImage2(NonLazyImage(), 0.0f, 0.0f, sampling0, _));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  // DrawImageRectop.
  SkRect src_rect =
      rect.makeOffset(src_rect_offset[1].width(), src_rect_offset[1].height());
  EXPECT_CALL(canvas,
              onDrawImageRect2(NonLazyImage(),
                               MatchesRect(src_rect, scale_adjustment[1]),
                               SkRect::MakeWH(10, 10), sampling1, _,
                               SkCanvas::kFast_SrcRectConstraint));

  // DrawOvalop.
  EXPECT_CALL(canvas, onDrawOval(SkRect::MakeWH(10, 10),
                                 MatchesShader(flags, scale_adjustment[2])));

  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

TEST(PaintOpBufferTest, DrawImageRectOpWithLooperNoImageProvider) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  SkLayerDrawLooper::Builder sk_draw_looper_builder;
  sk_draw_looper_builder.addLayer(20.0, 20.0);
  SkLayerDrawLooper::LayerInfo info_unmodified;
  sk_draw_looper_builder.addLayerOnTop(info_unmodified);

  PaintFlags paint_flags;
  paint_flags.setLooper(sk_draw_looper_builder.detach());
  buffer.push<DrawImageRectOp>(image, SkRect::MakeWH(100, 100),
                               SkRect::MakeWH(100, 100), SkSamplingOptions(),
                               &paint_flags, SkCanvas::kFast_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  EXPECT_CALL(canvas, willSave);
  EXPECT_CALL(canvas, didTranslate);
  EXPECT_CALL(canvas, willRestore);
  EXPECT_CALL(canvas, onDrawImageRect2).Times(2);

  buffer.Playback(&canvas, PlaybackParams(nullptr));
}

TEST(PaintOpBufferTest, DrawImageRectOpWithLooperWithImageProvider) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  SkLayerDrawLooper::Builder sk_draw_looper_builder;
  sk_draw_looper_builder.addLayer(20.0, 20.0);
  SkLayerDrawLooper::LayerInfo info_unmodified;
  sk_draw_looper_builder.addLayerOnTop(info_unmodified);

  PaintFlags paint_flags;
  paint_flags.setLooper(sk_draw_looper_builder.detach());
  buffer.push<DrawImageRectOp>(image, SkRect::MakeWH(100, 100),
                               SkRect::MakeWH(100, 100), SkSamplingOptions(),
                               &paint_flags, SkCanvas::kFast_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  EXPECT_CALL(canvas, willSave);
  EXPECT_CALL(canvas, didTranslate);
  EXPECT_CALL(canvas, willRestore);
  EXPECT_CALL(canvas, onDrawImageRect2).Times(2);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(1.0f, 1.0f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh};
  MockImageProvider image_provider(src_rect_offset, scale_adjustment, quality);
  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

TEST(PaintOpBufferTest, ReplacesImagesFromProviderOOP) {
  PaintOpBuffer buffer;
  SkSize expected_scale = SkSize::Make(0.2f, 0.5f);

  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  SkSamplingOptions sampling(SkFilterMode::kLinear);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<ScaleOp>(expected_scale.width(), expected_scale.height());
  buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, sampling, nullptr);
  buffer.push<DrawImageRectOp>(paint_image, rect, rect, sampling, nullptr,
                               SkCanvas::kFast_SrcRectConstraint);
  flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                         SkTileMode::kRepeat, nullptr));
  buffer.push<DrawOvalOp>(SkRect::MakeWH(10, 10), flags);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);
  ASSERT_NE(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);

  for (const PaintOp& op : *deserialized_buffer) {
    testing::NiceMock<MockCanvas> canvas;
    PlaybackParams params(nullptr);
    testing::Sequence s;

    if (op.GetType() == PaintOpType::DrawImage) {
      // Save/scale/image/restore from DrawImageop.
      EXPECT_CALL(canvas, willSave()).InSequence(s);
      EXPECT_CALL(canvas, didScale(1.0f / expected_scale.width(),
                                   1.0f / expected_scale.height()));
      EXPECT_CALL(canvas, onDrawImage2(NonLazyImage(), 0.0f, 0.0f, _, _));
      EXPECT_CALL(canvas, willRestore()).InSequence(s);
      op.Raster(&canvas, params);
    } else if (op.GetType() == PaintOpType::DrawImageRect) {
      EXPECT_CALL(canvas, onDrawImageRect2(NonLazyImage(),
                                           MatchesRect(rect, expected_scale),
                                           SkRect::MakeWH(10, 10), _, _,
                                           SkCanvas::kFast_SrcRectConstraint));
      op.Raster(&canvas, params);
    } else if (op.GetType() == PaintOpType::DrawOval) {
      EXPECT_CALL(canvas, onDrawOval(SkRect::MakeWH(10, 10),
                                     MatchesShader(flags, expected_scale)));
      op.Raster(&canvas, params);
    }
  }
}

class PaintFilterSerializationTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(PaintFilterSerializationTests,
                         PaintFilterSerializationTest,
                         ::testing::Values(true, false));

TEST_P(PaintFilterSerializationTest, Basic) {
  SkScalar scalars[9] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f};
  std::vector<sk_sp<PaintFilter>> filters = {
      sk_sp<PaintFilter>{new ColorFilterPaintFilter(
          SkColorFilters::LinearToSRGBGamma(), nullptr)},
      sk_sp<PaintFilter>{
          new BlurPaintFilter(0.5f, 0.3f, SkTileMode::kRepeat, nullptr)},
      sk_sp<PaintFilter>{new DropShadowPaintFilter(
          5.f, 10.f, 0.1f, 0.3f, SkColors::kBlue,
          DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr)},
      sk_sp<PaintFilter>{new MagnifierPaintFilter(SkRect::MakeXYWH(5, 6, 7, 8),
                                                  10.5f, nullptr)},
      sk_sp<PaintFilter>{new AlphaThresholdPaintFilter(
          SkRegion(SkIRect::MakeXYWH(0, 0, 100, 200)), 10.f, 20.f, nullptr)},
      sk_sp<PaintFilter>{new MatrixConvolutionPaintFilter(
          SkISize::Make(3, 3), scalars, 30.f, 123.f, SkIPoint::Make(0, 0),
          SkTileMode::kDecal, true, nullptr)},
      sk_sp<PaintFilter>{new MorphologyPaintFilter(
          MorphologyPaintFilter::MorphType::kErode, 15.5f, 30.2f, nullptr)},
      sk_sp<PaintFilter>{new OffsetPaintFilter(-1.f, -2.f, nullptr)},
      sk_sp<PaintFilter>{new TilePaintFilter(
          SkRect::MakeXYWH(1, 2, 3, 4), SkRect::MakeXYWH(4, 3, 2, 1), nullptr)},
      sk_sp<PaintFilter>{new TurbulencePaintFilter(
          TurbulencePaintFilter::TurbulenceType::kFractalNoise, 3.3f, 4.4f, 2,
          123, nullptr)},
      sk_sp<PaintFilter>{new MatrixPaintFilter(
          SkMatrix::I(), PaintFlags::FilterQuality::kHigh, nullptr)},
      sk_sp<PaintFilter>{new LightingDistantPaintFilter(
          PaintFilter::LightingType::kSpecular, SkPoint3::Make(1, 2, 3),
          SkColors::kCyan, 1.1f, 2.2f, 3.3f, nullptr)},
      sk_sp<PaintFilter>{new LightingPointPaintFilter(
          PaintFilter::LightingType::kDiffuse, SkPoint3::Make(2, 3, 4),
          SkColors::kRed, 1.2f, 3.4f, 5.6f, nullptr)},
      sk_sp<PaintFilter>{new LightingSpotPaintFilter(
          PaintFilter::LightingType::kSpecular, SkPoint3::Make(100, 200, 300),
          SkPoint3::Make(400, 500, 600), 1, 2, SkColors::kMagenta, 3, 4, 5,
          nullptr)},
      sk_sp<PaintFilter>{
          new ImagePaintFilter(CreateDiscardablePaintImage(gfx::Size(100, 100)),
                               SkRect::MakeWH(50, 50), SkRect::MakeWH(70, 70),
                               PaintFlags::FilterQuality::kMedium)}};

  filters.emplace_back(new ComposePaintFilter(filters[0], filters[1]));
  filters.emplace_back(
      new XfermodePaintFilter(SkBlendMode::kDst, filters[2], filters[3]));
  filters.emplace_back(new ArithmeticPaintFilter(1.1f, 2.2f, 3.3f, 4.4f, false,
                                                 filters[4], filters[5]));
  filters.emplace_back(new DisplacementMapEffectPaintFilter(
      SkColorChannel::kR, SkColorChannel::kG, 10, filters[6], filters[7]));
  filters.emplace_back(new MergePaintFilter(filters.data(), filters.size()));
  filters.emplace_back(
      new RecordPaintFilter(PaintRecord(), SkRect::MakeXYWH(10, 15, 20, 25)));

  // Use a non-identity ctm to confirm that RecordPaintFilters are converted
  // from raster-at-scale to fixed scale properly.
  float scale_x = 2.f;
  float scale_y = 3.f;
  SkM44 ctm = SkM44::Scale(scale_x, scale_y);

  TestOptionsProvider options_provider;
  for (size_t i = 0; i < filters.size(); ++i) {
    SCOPED_TRACE(i);

    auto& filter = filters[i];
    size_t buffer_size = filter->type() == PaintFilter::Type::kPaintRecord
                             ? kDefaultSerializedBufferSize
                             : PaintOpWriter::SerializedSize(filter.get());
    auto memory = AllocateSerializedBuffer(buffer_size);

    PaintOpWriter writer(memory.get(), buffer_size,
                         options_provider.serialize_options(), GetParam());
    writer.Write(filter.get(), ctm);
    ASSERT_GT(writer.size(), 0u) << PaintFilter::TypeToString(filter->type());

    sk_sp<PaintFilter> deserialized_filter;
    PaintOpReader reader(memory.get(), writer.size(),
                         options_provider.deserialize_options(), GetParam());
    reader.Read(&deserialized_filter);
    ASSERT_TRUE(deserialized_filter);

    if (filter->type() == PaintFilter::Type::kPaintRecord) {
      // The filter's scaling behavior should be converted to kFixedScale so
      // they are no longer equal.
      ASSERT_EQ(deserialized_filter->type(), PaintFilter::Type::kPaintRecord);

      const RecordPaintFilter& expected =
          static_cast<const RecordPaintFilter&>(*filter);
      const RecordPaintFilter& actual =
          static_cast<const RecordPaintFilter&>(*deserialized_filter);

      EXPECT_EQ(actual.scaling_behavior(),
                RecordPaintFilter::ScalingBehavior::kFixedScale);

      SkRect expected_bounds =
          SkRect::MakeXYWH(scale_x * expected.record_bounds().x(),
                           scale_y * expected.record_bounds().y(),
                           scale_x * expected.record_bounds().width(),
                           scale_y * expected.record_bounds().height());
      EXPECT_EQ(actual.record_bounds(), expected_bounds);
      EXPECT_EQ(actual.raster_scale().width(), scale_x);
      EXPECT_EQ(actual.raster_scale().height(), scale_y);

      // And the first op in the deserialized filter's record should be a
      // ScaleOp containing the extracted scale factors (if there's no
      // security constraints that disable record serialization)
      if (!GetParam()) {
        EXPECT_THAT(actual.record(),
                    ElementsAre(PaintOpEq<ScaleOp>(scale_x, scale_y)));
      }
    } else {
      EXPECT_TRUE(filter->EqualsForTesting(*deserialized_filter));
    }
  }
}

TEST(PaintOpBufferTest, RecordPaintFilterDeserializationInvalidPaintOp) {
  float rect_size = 0x8.765432p1;
  PaintOpBuffer buffer;
  buffer.push<ClipRectOp>(SkRect::MakeWH(rect_size, rect_size),
                          SkClipOp::kDifference, true);
  auto filter = sk_make_sp<RecordPaintFilter>(buffer.ReleaseAsRecord(),
                                              SkRect::MakeWH(100, 100));

  TestOptionsProvider options_provider;
  std::vector<uint8_t> memory(kDefaultSerializedBufferSize);
  PaintOpWriter writer(memory.data(), memory.size(),
                       options_provider.serialize_options(), false);
  writer.Write(filter.get(), SkM44());
  ASSERT_GT(writer.size(), sizeof(float));

  // Replace the first occurrence of rect_size with NaN to make the ClipRectOp
  // invalid.
  for (size_t i = 0; i < writer.size(); i += sizeof(float)) {
    float* f = reinterpret_cast<float*>(memory.data() + i);
    if (*f == rect_size) {
      *f = std::numeric_limits<float>::quiet_NaN();
      break;
    }
  }
  sk_sp<PaintFilter> deserialized_filter;
  PaintOpReader reader(memory.data(), writer.size(),
                       options_provider.deserialize_options(), false);
  reader.Read(&deserialized_filter);
  EXPECT_FALSE(deserialized_filter);
}

TEST(PaintOpBufferTest, PaintRecordShaderSerialization) {
  auto memory = AllocateSerializedBuffer();
  PaintOpBuffer shader_buffer;
  shader_buffer.push<DrawRectOp>(SkRect::MakeXYWH(0, 0, 1, 1), PaintFlags());

  TestOptionsProvider options_provider;
  PaintFlags flags;
  flags.setShader(PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), SkRect::MakeWH(10, 10),
      SkTileMode::kClamp, SkTileMode::kRepeat, nullptr));
  PaintOpBuffer buffer;
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(1, 2, 3, 4), flags);

  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);
  ASSERT_TRUE(serializer.valid());
  ASSERT_GT(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  EXPECT_THAT(*deserialized_buffer, ElementsAre(PaintOpEq<DrawRectOp>(
                                        SkRect::MakeXYWH(1, 2, 3, 4), flags)));
}

#if BUILDFLAG(SKIA_SUPPORT_SKOTTIE)
TEST(PaintOpBufferTest, BoundingRect_DrawSkottieOp) {
  PaintOpBuffer buffer;
  PushDrawSkottieOps(&buffer);

  SkRect rect;
  for (const PaintOp& base_op : buffer) {
    const auto& op = static_cast<const DrawSkottieOp&>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op.dst.makeSorted());
  }
}

// Skottie-specific deserialization failure case.
TEST(PaintOpBufferTest,
     DrawSkottieOpSerializationFailureFromUnPrivilegedProcess) {
  auto memory = AllocateSerializedBuffer();

  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottie(gfx::Size(100, 100), /*duration_secs=*/1);
  ASSERT_TRUE(skottie->is_valid());
  const SkRect input_rect = SkRect::MakeIWH(400, 300);
  const float input_t = 0.4f;

  PaintOpBuffer buffer;
  buffer.push<DrawSkottieOp>(
      skottie, input_rect, input_t,
      GetTestImagesForSkottie(*skottie, input_rect,
                              PaintFlags::FilterQuality::kHigh, input_t),
      SkottieColorMap(), SkottieTextPropertyValueMap());

  // Serialize
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);
  ASSERT_TRUE(serializer.valid());
  ASSERT_GT(serializer.written(), 0u);

  // De-Serialize
  PaintOp::DeserializeOptions d_options(options_provider.deserialize_options());

  // Deserialization should fail on a non privileged process.
  d_options.is_privileged = false;

  auto deserialized_buffer = PaintOpBuffer::MakeFromMemory(
      memory.get(), serializer.written(), d_options);
  ASSERT_FALSE(deserialized_buffer);
}

TEST(PaintOpBufferTest, DrawSkottieOpRasterWithoutImageAssets) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottie(gfx::Size(100, 100), /*duration_secs=*/5);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);
  DrawSkottieOp skottie_op(skottie, skottie_rect, /*t=*/0.1,
                           /*images=*/SkottieFrameDataMap(), SkottieColorMap(),
                           SkottieTextPropertyValueMap());
  PlaybackParams playback_params(/*image_provider=*/nullptr);
  {
    NiceMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onDrawImage2(_, _, _, _, _)).Times(0);
    DrawSkottieOp::Raster(&skottie_op, &canvas, playback_params);
  }
}

TEST(PaintOpBufferTest, DrawSkottieOpRasterWithNullImages) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);

  SkottieFrameDataMap images_in = GetNullImagesForSkottie(*skottie, /*t=*/0.1f);
  ASSERT_FALSE(images_in.empty());
  DrawSkottieOp skottie_op(skottie, skottie_rect, /*t=*/0.1, images_in,
                           SkottieColorMap(), SkottieTextPropertyValueMap());
  PlaybackParams playback_params(/*image_provider=*/nullptr);
  {
    NiceMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onDrawImage2(_, _, _, _, _)).Times(0);
    DrawSkottieOp::Raster(&skottie_op, &canvas, playback_params);
  }
}

TEST(PaintOpBufferTest, DrawSkottieOpRasterWithoutImageProvider) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);
  SkottieFrameDataMap images_in = GetTestImagesForSkottie(
      *skottie, skottie_rect, PaintFlags::FilterQuality::kHigh, /*t=*/0.1f);
  ASSERT_FALSE(images_in.empty());
  DrawSkottieOp skottie_op(skottie, skottie_rect, /*t=*/0.1, images_in,
                           SkottieColorMap(), SkottieTextPropertyValueMap());
  PlaybackParams playback_params(/*image_provider=*/nullptr);
  {
    NiceMock<MockCanvas> canvas;
    // Do not over-assert. Ultimately it is up to Skottie's implementation how
    // many "draw image" calls are made, and what the arguments are. But it's
    // fair to say that it has to make at least one "draw image" call for a
    // frame in the animation that renders one of the assets.
    EXPECT_CALL(canvas, onDrawImage2(NotNull(), _, _, _, _)).Times(AtLeast(1));
    DrawSkottieOp::Raster(&skottie_op, &canvas, playback_params);
  }
}

TEST(PaintOpBufferTest, DrawSkottieOpRasterWithImageProvider) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);
  std::vector<SkSize> src_rect_offset = {SkSize::Make(2.0f, 2.0f),
                                         SkSize::Make(3.0f, 3.0f)};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f),
                                          SkSize::Make(0.3f, 0.3f)};
  std::vector<PaintFlags::FilterQuality> quality = {
      PaintFlags::FilterQuality::kHigh, PaintFlags::FilterQuality::kMedium};

  MockImageProvider image_provider(src_rect_offset, scale_adjustment, quality);
  PlaybackParams playback_params(&image_provider);
  ASSERT_TRUE(image_provider.decoded_images().empty());
  {
    SkottieFrameDataMap images_in = GetTestImagesForSkottie(
        *skottie, skottie_rect, PaintFlags::FilterQuality::kHigh, /*t=*/0.25f);
    ASSERT_THAT(images_in, Contains(Key(HashSkottieResourceId("image_0"))));
    DrawSkottieOp skottie_op(skottie, skottie_rect, /*t=*/0.25, images_in,
                             SkottieColorMap(), SkottieTextPropertyValueMap());
    NiceMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onDrawImage2(NotNull(), _, _, _, _)).Times(AtLeast(1));
    DrawSkottieOp::Raster(&skottie_op, &canvas, playback_params);
    ASSERT_EQ(image_provider.decoded_images().size(), 1u);
    EXPECT_THAT(image_provider.decoded_images(),
                Contains(MatchesPaintImage(
                    images_in.at(HashSkottieResourceId("image_0")).image)));
  }
  {
    SkottieFrameDataMap images_in = GetTestImagesForSkottie(
        *skottie, skottie_rect, PaintFlags::FilterQuality::kHigh, /*t=*/0.75f);
    ASSERT_THAT(images_in, Contains(Key(HashSkottieResourceId("image_1"))));
    DrawSkottieOp skottie_op(skottie, skottie_rect, /*t=*/0.75, images_in,
                             SkottieColorMap(), SkottieTextPropertyValueMap());
    NiceMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onDrawImage2(NotNull(), _, _, _, _)).Times(AtLeast(1));
    DrawSkottieOp::Raster(&skottie_op, &canvas, playback_params);
    ASSERT_EQ(image_provider.decoded_images().size(), 2u);
    EXPECT_THAT(image_provider.decoded_images(),
                Contains(MatchesPaintImage(
                    images_in.at(HashSkottieResourceId("image_1")).image)));
  }
}

TEST(PaintOpBufferTest, DiscardableImagesTrackingSkottieOpNoImages) {
  PaintOpBuffer buffer;
  buffer.push<DrawSkottieOp>(
      CreateSkottie(gfx::Size(100, 100), /*duration_secs=*/1),
      /*dst=*/SkRect::MakeWH(100, 100), /*t=*/0.1f, SkottieFrameDataMap(),
      SkottieColorMap(), SkottieTextPropertyValueMap());
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTrackingSkottieOpWithImages) {
  PaintOpBuffer buffer;
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);
  SkottieFrameDataMap images_in = GetTestImagesForSkottie(
      *skottie, skottie_rect, PaintFlags::FilterQuality::kHigh, /*t=*/0.1f);
  ASSERT_FALSE(images_in.empty());
  buffer.push<DrawSkottieOp>(skottie, skottie_rect, /*t=*/0.1f, images_in,
                             SkottieColorMap(), SkottieTextPropertyValueMap());
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, OpHasDiscardableImagesSkottieOpNoImages) {
  DrawSkottieOp op(CreateSkottie(gfx::Size(100, 100), /*duration_secs=*/1),
                   /*dst=*/SkRect::MakeWH(100, 100), /*t=*/0.1f,
                   SkottieFrameDataMap(), SkottieColorMap(),
                   SkottieTextPropertyValueMap());
  EXPECT_FALSE(PaintOp::OpHasDiscardableImages(op));
}

TEST(PaintOpBufferTest, OpHasDiscardableImagesSkottieOpWithImages) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  SkRect skottie_rect = SkRect::MakeWH(100, 100);
  SkottieFrameDataMap images_in = GetTestImagesForSkottie(
      *skottie, skottie_rect, PaintFlags::FilterQuality::kHigh, /*t=*/0.1f);
  ASSERT_FALSE(images_in.empty());
  DrawSkottieOp op(skottie, skottie_rect, /*t=*/0.1f, images_in,
                   SkottieColorMap(), SkottieTextPropertyValueMap());
  EXPECT_TRUE(PaintOp::OpHasDiscardableImages(op));
}
#endif  // BUILDFLAG(SKIA_SUPPORT_SKOTTIE)

TEST(PaintOpBufferTest, CustomData) {
  // Basic tests: size, move, comparison.
  {
    PaintOpBuffer buffer;
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
    buffer.push<CustomDataOp>(1234u);
    EXPECT_EQ(buffer.size(), 1u);
    EXPECT_GT(buffer.bytes_used(),
              sizeof(PaintOpBuffer) + sizeof(CustomDataOp));

    PaintOpBuffer new_buffer = std::move(buffer);
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(new_buffer.size(), 1u);
    EXPECT_EQ(new_buffer.GetFirstOp().GetType(), PaintOpType::CustomData);

    PaintOpBuffer buffer2;
    buffer2.push<CustomDataOp>(1234u);
    EXPECT_THAT(new_buffer, ElementsAre(PaintOpEq<CustomDataOp>(1234u)));
  }

  // Push and verify.
  {
    PaintOpBuffer buffer;
    buffer.push<SaveOp>();
    buffer.push<CustomDataOp>(0xFFFFFFFF);
    buffer.push<RestoreOp>();

    EXPECT_THAT(buffer, ElementsAre(PaintOpEq<SaveOp>(),
                                    PaintOpEq<CustomDataOp>(0xFFFFFFFF),
                                    PaintOpEq<RestoreOp>()));
  }

  // Playback.
  {
    PaintOpBuffer buffer;
    buffer.push<CustomDataOp>(9999u);
    testing::StrictMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onCustomCallback(&canvas, 9999)).Times(1);
    buffer.Playback(&canvas, PlaybackParams(nullptr, SkM44(),
                                            base::BindRepeating(
                                                &MockCanvas::onCustomCallback,
                                                base::Unretained(&canvas))));
  }
}

TEST(PaintOpBufferTest, SecurityConstrainedImageSerialization) {
  auto image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  sk_sp<PaintFilter> filter = sk_make_sp<ImagePaintFilter>(
      image, SkRect::MakeWH(10, 10), SkRect::MakeWH(10, 10),
      PaintFlags::FilterQuality::kLow);
  const bool enable_security_constraints = true;

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  PaintOpWriter writer(memory.get(), kDefaultSerializedBufferSize,
                       options_provider.serialize_options(),
                       enable_security_constraints);
  writer.Write(filter.get(), SkM44());

  sk_sp<PaintFilter> out_filter;
  PaintOpReader reader(memory.get(), writer.size(),
                       options_provider.deserialize_options(),
                       enable_security_constraints);
  reader.Read(&out_filter);
  EXPECT_TRUE(filter->EqualsForTesting(*out_filter));
}

TEST(PaintOpBufferTest, DrawImageRectSerializeScaledImages) {
  PaintOpBuffer buffer;

  // scales: x dimension = x0.25, y dimension = x5
  // translations here are arbitrary
  SkRect src = SkRect::MakeXYWH(3, 4, 20, 6);
  SkRect dst = SkRect::MakeXYWH(20, 38, 5, 30);

  // Adjust transform matrix so that order of operations for src->dst is
  // confirmed to be applied before the canvas's transform.
  buffer.push<TranslateOp>(.5f * dst.centerX(), 2.f * dst.centerY());
  buffer.push<RotateOp>(90.f);
  buffer.push<TranslateOp>(-.5f * dst.centerX(), -2.f * dst.centerY());
  buffer.push<ScaleOp>(0.5f, 2.0f);

  buffer.push<DrawImageRectOp>(CreateDiscardablePaintImage(gfx::Size(32, 16)),
                               src, dst, SkCanvas::kStrict_SrcRectConstraint);
  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);

  ASSERT_EQ(options_provider.decoded_images().size(), 1u);
  auto scale = options_provider.decoded_images().at(0).scale();
  EXPECT_EQ(scale.width(), 0.5f * 0.25f);
  EXPECT_EQ(scale.height(), 2.0f * 5.0f);
}

TEST(PaintOpBufferTest, RecordShadersSerializeScaledImages) {
  PaintOpBuffer shader_buffer;
  shader_buffer.push<DrawImageOp>(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f, 0.f);

  auto shader = PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), SkRect::MakeWH(10.f, 10.f),
      SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(true);
  PaintOpBuffer buffer;
  buffer.push<ScaleOp>(0.5f, 0.8f);
  PaintFlags flags;
  flags.setShader(shader);
  buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);

  ASSERT_EQ(options_provider.decoded_images().size(), 1u);
  auto scale = options_provider.decoded_images().at(0).scale();
  EXPECT_EQ(scale.width(), 0.5f);
  EXPECT_EQ(scale.height(), 0.8f);
}

TEST(PaintOpBufferTest, RecordShadersCached) {
  PaintOpBuffer shader_buffer;
  shader_buffer.push<DrawImageOp>(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f, 0.f);
  auto shader = PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), SkRect::MakeWH(10.f, 10.f),
      SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(false);
  auto shader_id = shader->paint_record_shader_id();
  TestOptionsProvider options_provider;
  auto* transfer_cache = options_provider.transfer_cache_helper();

  // Generate serialized |memory|.
  auto memory = AllocateSerializedBuffer();
  size_t memory_written = 0;
  {
    PaintOpBuffer buffer;
    PaintFlags flags;
    flags.setShader(shader);
    buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

    SimpleBufferSerializer serializer(memory.get(),
                                      kDefaultSerializedBufferSize,
                                      options_provider.serialize_options());
    serializer.Serialize(buffer);
    memory_written = serializer.written();
  }

  // Generate serialized |memory_scaled|, which is the same pob, but with
  // a scale factor.
  auto memory_scaled = AllocateSerializedBuffer();
  size_t memory_scaled_written = 0;
  {
    PaintOpBuffer buffer;
    PaintFlags flags;
    flags.setShader(shader);
    // This buffer has an additional scale op.
    buffer.push<ScaleOp>(2.0f, 3.7f);
    buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

    SimpleBufferSerializer serializer(memory_scaled.get(),
                                      kDefaultSerializedBufferSize,
                                      options_provider.serialize_options());
    serializer.Serialize(buffer);
    memory_scaled_written = serializer.written();
  }

  // Hold onto records so PaintShader pointer comparisons are valid.
  sk_sp<PaintOpBuffer> buffers[5];
  SkPicture* last_shader = nullptr;
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions deserialize_options(
      transfer_cache, options_provider.service_paint_cache(),
      options_provider.strike_client(), &scratch_buffer, true, nullptr);

  // Several deserialization test cases:
  // (0) deserialize once, verify cached is the same as deserialized version
  // (1) deserialize again, verify shader gets reused
  // (2) change scale, verify shader is new
  // (3) sanity check, same new scale + same new colorspace, shader is reused.
  for (size_t i = 0; i < 4; ++i) {
    if (i < 2) {
      buffers[i] = PaintOpBuffer::MakeFromMemory(memory.get(), memory_written,
                                                 deserialize_options);
    } else {
      buffers[i] = PaintOpBuffer::MakeFromMemory(
          memory_scaled.get(), memory_scaled_written, deserialize_options);
    }

    auto* entry =
        transfer_cache->GetEntryAs<ServiceShaderTransferCacheEntry>(shader_id);
    ASSERT_TRUE(entry);
    if (i < 2)
      EXPECT_EQ(buffers[i]->size(), 1u);
    else
      EXPECT_EQ(buffers[i]->size(), 2u);

    for (const PaintOp& base_op : *buffers[i]) {
      if (base_op.GetType() != PaintOpType::DrawRect)
        continue;
      const auto& op = static_cast<const DrawRectOp&>(base_op);

      // In every case, the shader in the op should get cached for future
      // use.
      auto* op_skshader = op.flags.getShader()->sk_cached_picture_.get();
      EXPECT_EQ(op_skshader, entry->shader()->sk_cached_picture_.get());
      switch (i) {
        case 0:
          // Nothing to check.
          break;
        case 1:
          EXPECT_EQ(op_skshader, last_shader);
          break;
        case 2:
          EXPECT_NE(op_skshader, last_shader);
          break;
        case 3:
          EXPECT_EQ(op_skshader, last_shader);
          break;
      }
      last_shader = op_skshader;
    }
  }
}

TEST(PaintOpBufferTest, RecordShadersCachedSize) {
  PaintOpBuffer shader_buffer;
  size_t estimated_image_size = 30 * 30 * 4;
  auto image = CreateBitmapImage(gfx::Size(30, 30));
  shader_buffer.push<DrawImageOp>(image, 0.f, 0.f);
  auto shader = PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), SkRect::MakeWH(10.f, 10.f),
      SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(false);
  auto shader_id = shader->paint_record_shader_id();
  TestOptionsProvider options_provider;
  auto* transfer_cache = options_provider.transfer_cache_helper();

  // Generate serialized |memory|.
  auto memory = AllocateSerializedBuffer();
  PaintOpBuffer buffer;
  PaintFlags flags;
  flags.setShader(shader);
  buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  options_provider.context_supports_distance_field_text();
  serializer.Serialize(buffer);

  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions deserialize_options(
      transfer_cache, options_provider.service_paint_cache(),
      options_provider.strike_client(), &scratch_buffer, true, nullptr);
  auto deserialized = PaintOpBuffer::MakeFromMemory(
      memory.get(), serializer.written(), deserialize_options);
  auto* shader_entry =
      transfer_cache->GetEntryAs<ServiceShaderTransferCacheEntry>(shader_id);
  ASSERT_TRUE(shader_entry);

  // The size of the shader in the cache should be bigger than both the record
  // and the image.  Exact numbers not used here to not overfit this test.
  size_t shader_size = shader_entry->CachedSize();
  EXPECT_GT(estimated_image_size, serializer.written());
  EXPECT_GT(shader_size, estimated_image_size);
}

TEST(PaintOpBufferTest, RecordFilterSerializeScaledImages) {
  PaintOpBuffer filter_buffer;
  filter_buffer.push<DrawImageOp>(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f, 0.f);

  auto filter = sk_make_sp<RecordPaintFilter>(filter_buffer.ReleaseAsRecord(),
                                              SkRect::MakeWH(10.f, 10.f));
  PaintOpBuffer buffer;
  buffer.push<ScaleOp>(0.5f, 0.8f);
  PaintFlags flags;
  flags.setImageFilter(filter);
  buffer.push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);

  ASSERT_EQ(options_provider.decoded_images().size(), 1u);
  auto scale = options_provider.decoded_images().at(0).scale();
  EXPECT_EQ(scale.width(), 0.5f);
  EXPECT_EQ(scale.height(), 0.8f);
}

TEST(PaintOpBufferTest, TotalOpCount) {
  PaintOpBuffer record_buffer;
  PaintOpBuffer sub_record_buffer;
  PaintOpBuffer sub_sub_record_buffer;
  PushDrawRectOps(&sub_sub_record_buffer);
  PushDrawRectOps(&sub_record_buffer);
  PushDrawRectOps(&record_buffer);
  size_t sub_sub_total_op_count = sub_sub_record_buffer.total_op_count();
  sub_record_buffer.push<DrawRecordOp>(sub_sub_record_buffer.ReleaseAsRecord());
  size_t sub_total_op_count = sub_record_buffer.total_op_count();
  record_buffer.push<DrawRecordOp>(sub_record_buffer.ReleaseAsRecord());

  size_t len = std::min(test_rects.size(), test_flags.size());
  EXPECT_EQ(len, sub_sub_total_op_count);
  EXPECT_EQ(2 * len + 1, sub_total_op_count);
  EXPECT_EQ(3 * len + 2, record_buffer.total_op_count());
}

TEST(PaintOpBufferTest, NullImages) {
  PaintOpBuffer buffer;
  buffer.push<DrawImageOp>(PaintImage(), 0.f, 0.f);

  auto memory = AllocateSerializedBuffer();
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);
  ASSERT_TRUE(serializer.valid());
  ASSERT_GT(serializer.written(), 0u);

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  EXPECT_THAT(*deserialized_buffer,
              ElementsAre(PaintOpEq<DrawImageOp>(PaintImage(), 0.f, 0.f)));
}

TEST(PaintOpBufferTest, HasDrawOpsAndHasDrawTextOps) {
  PaintOpBuffer buffer1;
  EXPECT_FALSE(buffer1.has_draw_ops());
  EXPECT_FALSE(buffer1.has_draw_text_ops());
  buffer1.push<DrawRectOp>(SkRect::MakeWH(3, 4), PaintFlags());
  PushDrawRectOps(&buffer1);
  EXPECT_TRUE(buffer1.has_draw_ops());
  EXPECT_FALSE(buffer1.has_draw_text_ops());

  PaintOpBuffer buffer2;
  EXPECT_FALSE(buffer2.has_draw_ops());
  EXPECT_FALSE(buffer2.has_draw_text_ops());
  buffer2.push<DrawRecordOp>(buffer1.ReleaseAsRecord());
  EXPECT_TRUE(buffer2.has_draw_ops());
  EXPECT_FALSE(buffer2.has_draw_text_ops());
  buffer2.push<DrawTextBlobOp>(SkTextBlob::MakeFromString("abc", SkFont()),
                               0.0f, 0.0f, PaintFlags());
  EXPECT_TRUE(buffer2.has_draw_ops());
  EXPECT_TRUE(buffer2.has_draw_text_ops());
  buffer2.push<DrawRectOp>(SkRect::MakeWH(4, 5), PaintFlags());
  EXPECT_TRUE(buffer2.has_draw_ops());
  EXPECT_TRUE(buffer2.has_draw_text_ops());

  PaintOpBuffer buffer3;
  EXPECT_FALSE(buffer3.has_draw_text_ops());
  EXPECT_FALSE(buffer3.has_draw_ops());
  buffer3.push<DrawRecordOp>(buffer2.ReleaseAsRecord());
  EXPECT_TRUE(buffer3.has_draw_ops());
  EXPECT_TRUE(buffer3.has_draw_text_ops());
}

TEST(PaintOpBufferTest, HasEffectsPreventingLCDTextForSaveLayerAlpha) {
  PaintOpBuffer buffer1;
  EXPECT_FALSE(buffer1.has_effects_preventing_lcd_text_for_save_layer_alpha());
  buffer1.push<DrawRectOp>(SkRect::MakeWH(3, 4), PaintFlags());
  EXPECT_FALSE(buffer1.has_effects_preventing_lcd_text_for_save_layer_alpha());

  PaintOpBuffer buffer2;
  EXPECT_FALSE(buffer2.has_effects_preventing_lcd_text_for_save_layer_alpha());
  buffer2.push<DrawRecordOp>(buffer1.ReleaseAsRecord());
  EXPECT_FALSE(buffer2.has_effects_preventing_lcd_text_for_save_layer_alpha());
  buffer2.push<SaveLayerOp>(PaintFlags());
  EXPECT_TRUE(buffer2.has_effects_preventing_lcd_text_for_save_layer_alpha());
  buffer2.push<DrawRectOp>(SkRect::MakeWH(4, 5), PaintFlags());
  EXPECT_TRUE(buffer2.has_effects_preventing_lcd_text_for_save_layer_alpha());

  PaintOpBuffer buffer3;
  EXPECT_FALSE(buffer3.has_effects_preventing_lcd_text_for_save_layer_alpha());
  buffer3.push<DrawRecordOp>(buffer2.ReleaseAsRecord());
  EXPECT_TRUE(buffer3.has_effects_preventing_lcd_text_for_save_layer_alpha());
}

TEST(PaintOpBufferTest, NeedsAdditionalInvalidationForLCDText) {
  PaintOpBuffer buffer1;
  buffer1.push<SaveLayerAlphaOp>(0.4f);
  EXPECT_FALSE(buffer1.has_draw_text_ops());
  EXPECT_TRUE(buffer1.has_save_layer_alpha_ops());
  EXPECT_FALSE(buffer1.has_effects_preventing_lcd_text_for_save_layer_alpha());

  PaintOpBuffer buffer2;
  buffer2.push<DrawTextBlobOp>(SkTextBlob::MakeFromString("abc", SkFont()),
                               0.0f, 0.0f, PaintFlags());
  buffer2.push<SaveLayerOp>(PaintFlags());
  EXPECT_TRUE(buffer2.has_draw_ops());
  EXPECT_FALSE(buffer2.has_save_layer_alpha_ops());
  EXPECT_TRUE(buffer2.has_effects_preventing_lcd_text_for_save_layer_alpha());

  // Neither buffer has effects preventing lcd text for SaveLayerAlpha.
  EXPECT_FALSE(buffer1.NeedsAdditionalInvalidationForLCDText(buffer2));
  EXPECT_FALSE(buffer2.NeedsAdditionalInvalidationForLCDText(buffer1));

  PaintRecord record2 = buffer2.ReleaseAsRecord();
  {
    PaintOpBuffer buffer3;
    buffer3.push<DrawRecordOp>(record2);
    EXPECT_TRUE(buffer3.has_effects_preventing_lcd_text_for_save_layer_alpha());
    // Neither buffer has both DrawText and SaveLayerAlpha.
    EXPECT_FALSE(buffer1.NeedsAdditionalInvalidationForLCDText(buffer3));
    EXPECT_FALSE(buffer3.NeedsAdditionalInvalidationForLCDText(buffer1));
    EXPECT_FALSE(
        record2.buffer().NeedsAdditionalInvalidationForLCDText(buffer3));
    EXPECT_FALSE(
        buffer3.NeedsAdditionalInvalidationForLCDText(record2.buffer()));
  }
  {
    buffer1.push<DrawTextBlobOp>(SkTextBlob::MakeFromString("abc", SkFont()),
                                 0.0f, 0.0f, PaintFlags());
    EXPECT_TRUE(buffer1.has_draw_text_ops());
    EXPECT_TRUE(buffer1.has_save_layer_alpha_ops());
    EXPECT_FALSE(
        buffer1.has_effects_preventing_lcd_text_for_save_layer_alpha());
    PaintRecord record1 = buffer1.ReleaseAsRecord();
    PaintOpBuffer buffer3;
    buffer3.push<DrawRecordOp>(record1);
    buffer3.push<DrawRecordOp>(record2);
    EXPECT_TRUE(buffer3.has_draw_text_ops());
    EXPECT_TRUE(buffer3.has_save_layer_alpha_ops());
    EXPECT_TRUE(buffer3.has_effects_preventing_lcd_text_for_save_layer_alpha());
    // Both have DrawText and SaveLayerAlpha, and have different
    // has_effects_preventing_lcd_text_for_save_layer_alpha().
    EXPECT_TRUE(
        record1.buffer().NeedsAdditionalInvalidationForLCDText(buffer3));
    EXPECT_TRUE(
        buffer3.NeedsAdditionalInvalidationForLCDText(record1.buffer()));
    EXPECT_FALSE(buffer3.NeedsAdditionalInvalidationForLCDText(buffer3));
  }
}

// A regression test for crbug.com/1195276. Ensure that PlaybackParams works
// with SetMatrix operations.
TEST(PaintOpBufferTest, SetMatrixOpWithNonIdentityPlaybackParams) {
  for (const auto& original_ctm : test_matrices) {
    for (const auto& matrix : test_matrices) {
      SkCanvas device(0, 0);
      SkCanvas* canvas = &device;

      PlaybackParams params(nullptr, original_ctm);
      SetMatrixOp op(matrix);
      SetMatrixOp::Raster(&op, canvas, params);
      EXPECT_TRUE(canvas->getLocalToDevice() == SkM44(original_ctm, matrix));
    }
  }
}

TEST(PaintOpBufferTest, PathCaching) {
  SkPath path;
  PaintFlags flags;

  // Grow path large enough to trigger caching
  path.moveTo(0, 0);
  for (int x = 1; x < 100; ++x)
    path.lineTo(x, x % 1);

  TestOptionsProvider options_provider;

  auto memory = AllocateSerializedBuffer();
  PaintOpBuffer buffer;
  buffer.push<DrawPathOp>(path, flags, UsePaintCache::kEnabled);
  SimpleBufferSerializer serializer(memory.get(), kDefaultSerializedBufferSize,
                                    options_provider.serialize_options());
  serializer.Serialize(buffer);

  EXPECT_TRUE(options_provider.client_paint_cache()->Get(
      PaintCacheDataType::kPath, path.getGenerationID()));

  sk_sp<PaintOpBuffer> deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  EXPECT_THAT(*deserialized_buffer, ElementsAre(PaintOpIs<DrawPathOp>()));

  SkPath cached_path;
  EXPECT_TRUE(options_provider.service_paint_cache()->GetPath(
      path.getGenerationID(), &cached_path));
}

TEST(PaintOpBufferTest, ShrinkToFit) {
  PaintOpBuffer buffer;
  EXPECT_EQ(sizeof(PaintOpBuffer), buffer.bytes_used());
  EXPECT_FALSE(buffer.DataBufferForTesting());
  buffer.ShrinkToFit();
  EXPECT_EQ(sizeof(PaintOpBuffer), buffer.bytes_used());
  EXPECT_FALSE(buffer.DataBufferForTesting());

  buffer.push<DrawColorOp>(SkColors::kRed, SkBlendMode::kSrc);
  EXPECT_GT(buffer.bytes_used(), sizeof(PaintOpBuffer) + sizeof(DrawColorOp));
  const char* data_buffer = buffer.DataBufferForTesting();
  ASSERT_TRUE(data_buffer);
  buffer.ShrinkToFit();
  EXPECT_EQ(sizeof(PaintOpBuffer) + sizeof(DrawColorOp), buffer.bytes_used());
  EXPECT_NE(data_buffer, buffer.DataBufferForTesting());

  data_buffer = buffer.DataBufferForTesting();
  buffer.ShrinkToFit();
  EXPECT_EQ(sizeof(PaintOpBuffer) + sizeof(DrawColorOp), buffer.bytes_used());
  EXPECT_EQ(data_buffer, buffer.DataBufferForTesting());
}

TEST(PaintOpBufferTest, ReleaseAsRecord) {
  PaintOpBuffer buffer;
  PaintRecord record = buffer.ReleaseAsRecord();
  EXPECT_EQ(sizeof(PaintOpBuffer), record.bytes_used());
  EXPECT_FALSE(record.buffer().DataBufferForTesting());
  EXPECT_EQ(sizeof(PaintOpBuffer), buffer.bytes_used());
  EXPECT_FALSE(buffer.DataBufferForTesting());

  // `buffer` has more reserved than used.
  buffer.push<DrawColorOp>(SkColors::kRed, SkBlendMode::kSrc);
  size_t old_bytes_used = buffer.bytes_used();
  EXPECT_GT(old_bytes_used, sizeof(PaintOpBuffer) + sizeof(DrawColorOp));
  const char* data_buffer = buffer.DataBufferForTesting();
  ASSERT_TRUE(data_buffer);
  EXPECT_EQ(1u, buffer.size());

  // `record` should allocate a new data buffer that fits, and `buffer` should
  // retain the old data buffer.
  record = buffer.ReleaseAsRecord();
  EXPECT_EQ(1u, record.size());
  EXPECT_EQ(sizeof(PaintOpBuffer) + sizeof(DrawColorOp), record.bytes_used());
  EXPECT_NE(data_buffer, record.buffer().DataBufferForTesting());
  EXPECT_EQ(data_buffer, buffer.DataBufferForTesting());
  EXPECT_EQ(old_bytes_used, buffer.bytes_used());

  // `buffer` now fits.
  buffer.push<DrawColorOp>(SkColors::kRed, SkBlendMode::kSrc);
  buffer.ShrinkToFit();
  old_bytes_used = buffer.bytes_used();
  EXPECT_EQ(old_bytes_used, sizeof(PaintOpBuffer) + sizeof(DrawColorOp));
  data_buffer = buffer.DataBufferForTesting();
  ASSERT_TRUE(data_buffer);

  // `record` takes the data buffer of `buffer`.
  record = buffer.ReleaseAsRecord();
  EXPECT_EQ(1u, record.size());
  EXPECT_EQ(sizeof(PaintOpBuffer) + sizeof(DrawColorOp), record.bytes_used());
  EXPECT_EQ(data_buffer, record.buffer().DataBufferForTesting());
  EXPECT_FALSE(buffer.DataBufferForTesting());
  EXPECT_EQ(sizeof(PaintOpBuffer), buffer.bytes_used());
}

TEST(IteratorTest, StlContainerLikeIterationTest) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));
  EXPECT_THAT(buffer, ElementsAre(PaintOpEq<SaveOp>(),
                                  PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));
}

TEST(IteratorTest, IterationTest) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));
  EXPECT_THAT(PaintOpBuffer::Iterator(buffer),
              ElementsAre(PaintOpEq<SaveOp>(),
                          PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));
}

TEST(IteratorTest, OffsetIterationTest) {
  PaintOpBuffer buffer;
  const PaintOp& op1 = buffer.push<SaveOp>();
  const PaintOp& op2 = buffer.push<RestoreOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));

  std::vector<size_t> offsets = {
      0, static_cast<size_t>(op1.aligned_size + op2.aligned_size)};
  EXPECT_THAT(PaintOpBuffer::OffsetIterator(buffer, offsets),
              ElementsAre(PaintOpEq<SaveOp>(),
                          PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));
}

TEST(IteratorTest, CompositeIterationTest) {
  PaintOpBuffer buffer;
  const PaintOp& op1 = buffer.push<SaveOp>();
  const PaintOp& op2 = buffer.push<RestoreOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));
  std::vector<size_t> offsets = {
      0, static_cast<size_t>(op1.aligned_size + op2.aligned_size)};

  EXPECT_THAT(PaintOpBuffer::CompositeIterator(buffer, /*offsets=*/nullptr),
              ElementsAre(PaintOpEq<SaveOp>(), PaintOpEq<RestoreOp>(),
                          PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));

  EXPECT_THAT(PaintOpBuffer::CompositeIterator(buffer, &offsets),
              ElementsAre(PaintOpEq<SaveOp>(),
                          PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));
}

TEST(IteratorTest, EqualityTest) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));
  PaintOpBuffer::Iterator iter1(buffer);
  PaintOpBuffer::Iterator iter2(buffer);
  EXPECT_TRUE(iter1 == iter2);
  EXPECT_FALSE(iter1 == ++iter2);
}

TEST(IteratorTest, OffsetEqualityTest) {
  PaintOpBuffer buffer;
  size_t offset = 0;
  offset += buffer.push<SaveOp>().aligned_size;
  offset += buffer.push<SetMatrixOp>(SkM44::Scale(1, 2)).aligned_size;
  buffer.push<NoopOp>();

  std::vector<size_t> offsets = {0, offset};
  PaintOpBuffer::OffsetIterator iter1(buffer, offsets);
  PaintOpBuffer::OffsetIterator iter2(buffer, offsets);

  EXPECT_TRUE(iter1 == iter2);
  EXPECT_FALSE(iter1 == ++iter2);
}

TEST(IteratorTest, CompositeEqualityTest) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  buffer.push<SetMatrixOp>(SkM44::Scale(1, 2));

  PaintOpBuffer::CompositeIterator iter1(buffer, /*offsets=*/nullptr);
  PaintOpBuffer::CompositeIterator iter2(buffer, /*offsets=*/nullptr);
  EXPECT_TRUE(iter1 == iter2);
  EXPECT_FALSE(iter1 == ++iter2);
}

TEST(IteratorTest, CompositeOffsetEqualityTest) {
  PaintOpBuffer buffer;
  size_t offset = 0;
  offset += buffer.push<SaveOp>().aligned_size;
  offset += buffer.push<SetMatrixOp>(SkM44::Scale(1, 2)).aligned_size;
  buffer.push<NoopOp>();

  std::vector<size_t> offsets = {0, offset};
  PaintOpBuffer::CompositeIterator iter1(buffer, &offsets);
  PaintOpBuffer::CompositeIterator iter2(buffer, &offsets);

  EXPECT_TRUE(iter1 == iter2);
  EXPECT_FALSE(iter1 == ++iter2);
}

TEST(IteratorTest, CompositeOffsetMixedTypeEqualityTest) {
  PaintOpBuffer buffer;
  std::vector<size_t> offsets = {0, 4};
  PaintOpBuffer::CompositeIterator iter(buffer, /*offsets=*/nullptr);
  PaintOpBuffer::CompositeIterator offset_iter(buffer, &offsets);

  EXPECT_TRUE(iter == iter);
  EXPECT_TRUE(offset_iter == offset_iter);
  EXPECT_FALSE(iter == offset_iter);
}

TEST(IteratorTest, CompositeOffsetBoolCheck) {
  PaintOpBuffer buffer;
  size_t offset = 0;
  offset += buffer.push<SaveOp>().aligned_size;
  offset += buffer.push<SetMatrixOp>(SkM44::Scale(1, 2)).aligned_size;
  buffer.push<NoopOp>();

  PaintOpBuffer::CompositeIterator iter(buffer, /*offsets=*/nullptr);
  EXPECT_TRUE(iter);
  EXPECT_TRUE(++iter);
  EXPECT_TRUE(++iter);
  EXPECT_FALSE(++iter);

  std::vector<size_t> offsets = {0, offset};
  PaintOpBuffer::CompositeIterator offset_iter(buffer, &offsets);
  EXPECT_TRUE(offset_iter);
  EXPECT_TRUE(++offset_iter);
  EXPECT_FALSE(++offset_iter);
}

}  // namespace cc
