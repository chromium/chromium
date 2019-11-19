// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/shader_transfer_cache_entry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/paint_op_helper.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_options_provider.h"
#include "cc/test/test_paint_worklet_input.h"
#include "cc/test/test_skcanvas.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"
#include "third_party/skia/src/core/SkRemoteGlyphCache.h"

using testing::_;
using testing::Property;
using testing::Mock;

namespace cc {
namespace {
// An arbitrary size guaranteed to fit the size of any serialized op in this
// unit test.  This can also be used for deserialized op size safely in this
// unit test suite as generally deserialized ops are smaller.
static constexpr size_t kBufferBytesPerOp = 1000 + sizeof(LargestPaintOp);

template <typename T>
void ValidateOps(PaintOpBuffer* buffer) {
  // Make sure all test data is valid before serializing it.
  for (auto* op : PaintOpBuffer::Iterator(buffer))
    EXPECT_TRUE(static_cast<T*>(op)->IsValid());
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
    shader->fallback_color_ = SkColorSetARGB(254, 252, 250, 248);
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
    shader->colors_ = {SkColorSetARGB(1, 2, 3, 4), SkColorSetARGB(5, 6, 7, 8),
                       SkColorSetARGB(9, 0, 1, 2)};
    shader->positions_ = {0.f, 0.4f, 1.f};
  }
};

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  buffer.Reset();
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  PaintOpBuffer buffer2(std::move(buffer));
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);
  EXPECT_EQ(buffer2.size(), 0u);
  EXPECT_EQ(buffer2.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer2), false);
}

class PaintOpAppendTest : public ::testing::Test {
 public:
  PaintOpAppendTest() {
    rect_ = SkRect::MakeXYWH(2, 3, 4, 5);
    flags_.setColor(SK_ColorMAGENTA);
    flags_.setAlpha(100);
  }

  void PushOps(PaintOpBuffer* buffer) {
    buffer->push<SaveLayerOp>(&rect_, &flags_);
    buffer->push<SaveOp>();
    buffer->push<DrawColorOp>(draw_color_, blend_);
    buffer->push<RestoreOp>();
    EXPECT_EQ(buffer->size(), 4u);
  }

  void VerifyOps(PaintOpBuffer* buffer) {
    EXPECT_EQ(buffer->size(), 4u);

    PaintOpBuffer::Iterator iter(buffer);
    ASSERT_EQ(iter->GetType(), PaintOpType::SaveLayer);
    SaveLayerOp* save_op = static_cast<SaveLayerOp*>(*iter);
    EXPECT_EQ(save_op->bounds, rect_);
    EXPECT_EQ(save_op->flags, flags_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Save);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::DrawColor);
    DrawColorOp* op = static_cast<DrawColorOp*>(*iter);
    EXPECT_EQ(op->color, draw_color_);
    EXPECT_EQ(op->mode, blend_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
    ++iter;

    EXPECT_FALSE(iter);
  }

 private:
  SkRect rect_;
  PaintFlags flags_;
  SkColor draw_color_ = SK_ColorRED;
  SkBlendMode blend_ = SkBlendMode::kSrc;
};

TEST_F(PaintOpAppendTest, SimpleAppend) {
  PaintOpBuffer buffer;
  PushOps(&buffer);
  VerifyOps(&buffer);

  buffer.Reset();
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
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
}

TEST_F(PaintOpAppendTest, MoveThenDestructOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&original), false);
}

TEST_F(PaintOpAppendTest, MoveThenReappend) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination(std::move(original));

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
  EXPECT_EQ(original, destination);
}

TEST_F(PaintOpAppendTest, MoveThenReappendOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
  EXPECT_EQ(original, destination);
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
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
  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// Verify that we don't optimize SaveLayerAlpha / DrawTextBlob / Restore.
TEST(PaintOpBufferTest, SaveDrawTextBlobRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags paint_flags;
  EXPECT_TRUE(paint_flags.SupportsFoldingAlpha());
  buffer.push<DrawTextBlobOp>(SkTextBlob::MakeFromString("abc", SkFont()), 0, 0,
                              paint_flags);
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

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
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

  uint8_t alpha = 255;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
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

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
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

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

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
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  record->push<DrawRectOp>(rect, draw_flags);
  EXPECT_EQ(record->size(), 1u);

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// The same as the above SingleOpRecord test, but the single op is not
// a draw op.  So, there's no way to fold in the save layer optimization.
// Verify that the optimization doesn't apply and that this doesn't crash.
// See: http://crbug.com/712093.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleNonDrawOp) {
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();
  record->push<NoopOp>();
  EXPECT_EQ(record->size(), 1u);
  EXPECT_FALSE(record->GetFirstOp()->IsDrawOp());

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

TEST(PaintOpBufferTest, SaveLayerRestore_DrawColor) {
  PaintOpBuffer buffer;
  uint8_t alpha = 100;
  SkColor original = SkColorSetA(50, SK_ColorRED);

  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  buffer.push<DrawColorOp>(original, SkBlendMode::kSrcOver);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);
  EXPECT_EQ(canvas.save_count_, 0);
  EXPECT_EQ(canvas.restore_count_, 0);

  uint8_t expected_alpha = SkMulDiv255Round(alpha, SkColorGetA(original));
  EXPECT_EQ(canvas.paint_.getColor(), SkColorSetA(original, expected_alpha));
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
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0), nullptr);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_PaintWorkletImage) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(32.0f, 32.0f));
  PaintOpBuffer buffer;
  PaintImage image = CreatePaintWorkletPaintImage(input);
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0), nullptr);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_PaintWorkletImageRect) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(32.0f, 32.0f));
  PaintOpBuffer buffer;
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeEmpty();
  SkRect dst = SkRect::MakeEmpty();
  buffer.push<DrawImageRectOp>(image, src, dst, nullptr,
                               PaintCanvas::kStrict_SrcRectConstraint);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImageRect) {
  PaintOpBuffer buffer;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  buffer.push<DrawImageRectOp>(
      image, SkRect::MakeWH(100, 100), SkRect::MakeWH(100, 100), nullptr,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
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
  auto buffer = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(buffer->numSlowPaths(), 0);

  // Op without slow paths
  PaintFlags noop_flags;
  SkRect rect = SkRect::MakeXYWH(2, 3, 4, 5);
  buffer->push<SaveLayerOp>(&rect, &noop_flags);

  // Line op with a slow path
  PaintFlags line_effect_slow;
  line_effect_slow.setStrokeWidth(1.f);
  line_effect_slow.setStyle(PaintFlags::kStroke_Style);
  line_effect_slow.setStrokeCap(PaintFlags::kRound_Cap);
  SkScalar intervals[] = {1.f, 1.f};
  line_effect_slow.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_slow);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Line effect special case that Skia handles specially.
  PaintFlags line_effect = line_effect_slow;
  line_effect.setStrokeCap(PaintFlags::kButt_Cap);
  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Antialiased convex path is not slow.
  SkPath path;
  path.addCircle(2, 2, 5);
  EXPECT_TRUE(path.isConvex());
  buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Concave paths are slow only when antialiased.
  SkPath concave = path;
  concave.addCircle(3, 4, 2);
  EXPECT_FALSE(concave.isConvex());
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 2);
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, false);
  EXPECT_EQ(buffer->numSlowPaths(), 2);

  // Drawing a record with slow paths into another adds the same
  // number of slow paths as the record.
  auto buffer2 = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(0, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(2, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(4, buffer2->numSlowPaths());
}

TEST(PaintOpBufferTest, NonAAPaint) {
  // PaintOpWithFlags
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) with AA.
    PaintFlags line_effect;
    line_effect.setAntiAlias(true);
    buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // Add a PaintOpWithFlags (in this case a line) without AA.
    PaintFlags line_effect_no_aa;
    line_effect_no_aa.setAntiAlias(false);
    buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_no_aa);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // ClipPathOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    SkPath path;
    path.addCircle(2, 2, 5);

    // ClipPathOp with AA
    buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, true /* antialias */);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipPathOp without AA
    buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, false /* antialias */);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // ClipRRectOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipRRectOp with AA
    buffer->push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                              true /* antialias */);
    EXPECT_FALSE(buffer->HasNonAAPaint());

    // ClipRRectOp without AA
    buffer->push<ClipRRectOp>(SkRRect::MakeEmpty(), SkClipOp::kIntersect,
                              false /* antialias */);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // Drawing a record with non-aa paths into another propogates the value.
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    auto sub_buffer = sk_make_sp<PaintOpBuffer>();
    SkPath path;
    path.addCircle(2, 2, 5);
    sub_buffer->push<ClipPathOp>(path, SkClipOp::kIntersect,
                                 false /* antialias */);
    EXPECT_TRUE(sub_buffer->HasNonAAPaint());

    buffer->push<DrawRecordOp>(sub_buffer);
    EXPECT_TRUE(buffer->HasNonAAPaint());
  }

  // The following PaintOpWithFlags types are overridden to *not* ever have
  // non-AA paint. AA is hard to notice, and these kick us out of MSAA in too
  // many cases.

  // DrawImageOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer->push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0),
                              &non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
  }

  // DrawIRectOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    buffer->push<DrawIRectOp>(SkIRect::MakeWH(1, 1), non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
  }

  // SaveLayerOp
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    EXPECT_FALSE(buffer->HasNonAAPaint());

    PaintFlags non_aa_flags;
    non_aa_flags.setAntiAlias(true);
    auto bounds = SkRect::MakeWH(1, 1);
    buffer->push<SaveLayerOp>(&bounds, &non_aa_flags);

    EXPECT_FALSE(buffer->HasNonAAPaint());
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

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndices) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

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

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

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

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays first two indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  Playback(&canvas, Select({0, 1}));
}

TEST_F(PaintOpBufferOffsetsTest, MiddleIndex) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays index 2.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  Playback(&canvas, Select({2}));
}

TEST_F(PaintOpBufferOffsetsTest, LastTwoElements) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays last two elements.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&canvas, Select({3, 4}));
}

TEST_F(PaintOpBufferOffsetsTest, ContiguousIndicesWithSaveLayerAlphaRestore) {
  testing::StrictMock<MockCanvas> canvas;

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

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

  push_op<DrawColorOp>(0u, SkBlendMode::kClear);
  push_op<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha);
  push_op<DrawColorOp>(2u, SkBlendMode::kClear);
  push_op<DrawColorOp>(3u, SkBlendMode::kClear);
  push_op<RestoreOp>();
  push_op<DrawColorOp>(4u, SkBlendMode::kClear);

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
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha);
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
  uint8_t alpha = 100;
  push_op<SaveLayerAlphaOp>(nullptr, alpha);
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
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
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

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  add_draw_rect(&buffer, 0u);
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
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

std::vector<SkMatrix> test_matrices = {
    SkMatrix::I(),
    SkMatrix::MakeScale(3.91f, 4.31f),
    SkMatrix::MakeTrans(-5.2f, 8.7f),
    [] {
      SkMatrix matrix;
      SkScalar buffer[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
      matrix.set9(buffer);
      return matrix;
    }(),
    [] {
      SkMatrix matrix;
      SkScalar buffer[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      matrix.set9(buffer);
      return matrix;
    }(),
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
      flags.setStyle(PaintFlags::kStrokeAndFill_Style);
      flags.setFilterQuality(SkFilterQuality::kMedium_SkFilterQuality);
      flags.setShader(PaintShader::MakeColor(SkColorSetARGB(1, 2, 3, 4)));
      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setColor(SK_ColorCYAN);
      flags.setAlpha(103);
      flags.setStrokeWidth(0.32f);
      flags.setStrokeMiter(7.98f);
      flags.setBlendMode(SkBlendMode::kSrcOut);
      flags.setStrokeCap(PaintFlags::kRound_Cap);
      flags.setStrokeJoin(PaintFlags::kRound_Join);
      flags.setStyle(PaintFlags::kFill_Style);
      flags.setFilterQuality(SkFilterQuality::kHigh_SkFilterQuality);

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

      sk_sp<PaintShader> shader = PaintShader::MakeColor(SK_ColorTRANSPARENT);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               true);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      flags.setShader(PaintShader::MakeColor(SkColorSetARGB(12, 34, 56, 78)));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      sk_sp<PaintShader> shader = PaintShader::MakeColor(SK_ColorTRANSPARENT);
      PaintOpSerializationTestUtils::FillArbitraryShaderValues(shader.get(),
                                                               false);
      flags.setShader(std::move(shader));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      SkPoint points[2] = {SkPoint::Make(1, 2), SkPoint::Make(3, 4)};
      SkColor colors[3] = {SkColorSetARGB(1, 2, 3, 4),
                           SkColorSetARGB(4, 3, 2, 1),
                           SkColorSetARGB(0, 10, 20, 30)};
      SkScalar positions[3] = {0.f, 0.3f, 1.f};
      flags.setShader(PaintShader::MakeLinearGradient(points, colors, positions,
                                                      3, SkTileMode::kMirror));

      return flags;
    }(),
    [] {
      PaintFlags flags;
      SkColor colors[3] = {SkColorSetARGB(1, 2, 3, 4),
                           SkColorSetARGB(4, 3, 2, 1),
                           SkColorSetARGB(0, 10, 20, 30)};
      flags.setShader(PaintShader::MakeSweepGradient(
          0.2f, -0.8f, colors, nullptr, 3, SkTileMode::kMirror, 10, 20));
      return flags;
    }(),
    PaintFlags(),
    PaintFlags(),
};

std::vector<SkColor> test_colors = {
    SkColorSetARGB(0, 0, 0, 0),      SkColorSetARGB(255, 255, 255, 255),
    SkColorSetARGB(0, 255, 10, 255), SkColorSetARGB(255, 0, 20, 255),
    SkColorSetARGB(30, 255, 0, 255), SkColorSetARGB(255, 40, 0, 0),
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

std::vector<scoped_refptr<SkottieWrapper>> test_skotties = {
    CreateSkottie(gfx::Size(10, 20), 4), CreateSkottie(gfx::Size(100, 40), 5),
    CreateSkottie(gfx::Size(80, 70), 6)};

std::vector<float> test_skottie_floats = {0, 0.1f, 1.f};

std::vector<SkRect> test_skottie_rects = {SkRect::MakeXYWH(10, 20, 30, 40),
                                          SkRect::MakeXYWH(0, 5, 10, 20),
                                          SkRect::MakeXYWH(6, 0, 3, 50)};

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
    for (const auto* op : PaintOpBuffer::Iterator(&buffer)) {
      size_t bytes_written = op->Serialize(
          current_, remaining_, options_provider_.serialize_options());
      if (!bytes_written)
        return;

      PaintOp* written = reinterpret_cast<PaintOp*>(current_);
      EXPECT_EQ(op->GetType(), written->GetType());
      EXPECT_EQ(bytes_written, written->skip);

      bytes_written_[op_idx] = bytes_written;
      op_idx++;
      current_ += bytes_written;
      remaining_ -= bytes_written;

      // Number of bytes bytes_written must be a multiple of PaintOpAlign
      // unless the buffer is filled entirely.
      if (remaining_ != 0u)
        DCHECK_EQ(0u, bytes_written % PaintOpBuffer::PaintOpAlign);
    }
  }

  const std::vector<size_t>& bytes_written() const { return bytes_written_; }
  size_t TotalBytesWritten() const { return output_size_ - remaining_; }
  TestOptionsProvider* options_provider() { return &options_provider_; }

 private:
  char* current_ = nullptr;
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

  operator bool() const { return remaining_ == 0u; }
  const PaintOp* operator->() const { return deserialized_op_; }
  const PaintOp* operator*() const { return deserialized_op_; }

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
    data_.reset(static_cast<char*>(base::AlignedAlloc(
        sizeof(LargestPaintOp), PaintOpBuffer::PaintOpAlign)));
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
    deserialized_op_ = PaintOp::Deserialize(current_, remaining_, data_.get(),
                                            sizeof(LargestPaintOp),
                                            &last_bytes_read_, options_);
  }

  const void* input_ = nullptr;
  const char* current_ = nullptr;
  size_t input_size_ = 0u;
  size_t remaining_ = 0u;
  size_t last_bytes_read_ = 0u;
  PaintOp::DeserializeOptions options_;
  std::unique_ptr<char, base::AlignedFreeDeleter> data_;
  PaintOp* deserialized_op_ = nullptr;
};

void PushAnnotateOps(PaintOpBuffer* buffer) {
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::URL, test_rects[0],
                           SkData::MakeWithCString("thingerdoowhatchamagig"));
  // Deliberately test both null and empty SkData.
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::LINK_TO_DESTINATION,
                           test_rects[1], nullptr);
  buffer->push<AnnotateOp>(PaintCanvas::AnnotationType::NAMED_DESTINATION,
                           test_rects[2], SkData::MakeEmpty());
  ValidateOps<AnnotateOp>(buffer);
}

void PushClipPathOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_paths.size(); ++i) {
    SkClipOp op = i % 3 ? SkClipOp::kDifference : SkClipOp::kIntersect;
    buffer->push<ClipPathOp>(test_paths[i], op, !!(i % 2));
  }
  ValidateOps<ClipPathOp>(buffer);
}

void PushClipRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRectOp>(test_rects[i], op, antialias);
  }
  ValidateOps<ClipRectOp>(buffer);
}

void PushClipRRectOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_rrects.size(); ++i) {
    SkClipOp op = i % 2 ? SkClipOp::kIntersect : SkClipOp::kDifference;
    bool antialias = !!(i % 3);
    buffer->push<ClipRRectOp>(test_rrects[i], op, antialias);
  }
  ValidateOps<ClipRRectOp>(buffer);
}

void PushConcatOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_matrices.size(); ++i)
    buffer->push<ConcatOp>(test_matrices[i]);
  ValidateOps<ConcatOp>(buffer);
}

void PushCustomDataOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_ids.size(); ++i)
    buffer->push<CustomDataOp>(test_ids[i]);
  ValidateOps<CustomDataOp>(buffer);
}

void PushDrawColorOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_colors.size(); ++i) {
    buffer->push<DrawColorOp>(test_colors[i], static_cast<SkBlendMode>(i));
  }
  ValidateOps<DrawColorOp>(buffer);
}

void PushDrawDRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size() - 1, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawDRRectOp>(test_rrects[i], test_rrects[i + 1],
                               test_flags[i]);
  }
  ValidateOps<DrawDRRectOp>(buffer);
}

void PushDrawImageOps(PaintOpBuffer* buffer) {
  size_t len =
      std::min({test_images.size(), test_flags.size(), test_floats.size() - 1});
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawImageOp>(test_images[i], test_floats[i],
                              test_floats[i + 1], &test_flags[i]);
  }

  // Test optional flags
  // TODO(enne): maybe all these optional ops should not be optional.
  buffer->push<DrawImageOp>(test_images[0], test_floats[0], test_floats[1],
                            nullptr);
  ValidateOps<DrawImageOp>(buffer);
}

void PushDrawImageRectOps(PaintOpBuffer* buffer) {
  size_t len =
      std::min({test_images.size(), test_flags.size(), test_rects.size() - 1});
  for (size_t i = 0; i < len; ++i) {
    PaintCanvas::SrcRectConstraint constraint =
        i % 2 ? PaintCanvas::kStrict_SrcRectConstraint
              : PaintCanvas::kFast_SrcRectConstraint;
    buffer->push<DrawImageRectOp>(test_images[i], test_rects[i],
                                  test_rects[i + 1], &test_flags[i],
                                  constraint);
  }

  // Test optional flags.
  buffer->push<DrawImageRectOp>(test_images[0], test_rects[0], test_rects[1],
                                nullptr,
                                PaintCanvas::kStrict_SrcRectConstraint);
  ValidateOps<DrawImageRectOp>(buffer);
}

void PushDrawIRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_irects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawIRectOp>(test_irects[i], test_flags[i]);
  ValidateOps<DrawIRectOp>(buffer);
}

void PushDrawLineOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_floats.size() - 3, test_flags.size());
  for (size_t i = 0; i < len; ++i) {
    buffer->push<DrawLineOp>(test_floats[i], test_floats[i + 1],
                             test_floats[i + 2], test_floats[i + 3],
                             test_flags[i]);
  }
  ValidateOps<DrawLineOp>(buffer);
}

void PushDrawOvalOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawOvalOp>(test_rects[i], test_flags[i]);
  ValidateOps<DrawOvalOp>(buffer);
}

void PushDrawPathOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_paths.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawPathOp>(test_paths[i], test_flags[i]);
  ValidateOps<DrawPathOp>(buffer);
}

void PushDrawRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRectOp>(test_rects[i], test_flags[i]);
  ValidateOps<DrawRectOp>(buffer);
}

void PushDrawRRectOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_rrects.size(), test_flags.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<DrawRRectOp>(test_rrects[i], test_flags[i]);
  ValidateOps<DrawRRectOp>(buffer);
}

void PushDrawSkottieOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_skotties.size(), test_flags.size());
  for (size_t i = 0; i < len; i++) {
    buffer->push<DrawSkottieOp>(test_skotties[i], test_skottie_rects[i],
                                test_skottie_floats[i]);
  }
  ValidateOps<DrawSkottieOp>(buffer);
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
  ValidateOps<DrawTextBlobOp>(buffer);
}

void PushNoopOps(PaintOpBuffer* buffer) {
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  buffer->push<NoopOp>();
  ValidateOps<NoopOp>(buffer);
}

void PushRestoreOps(PaintOpBuffer* buffer) {
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  buffer->push<RestoreOp>();
  ValidateOps<RestoreOp>(buffer);
}

void PushRotateOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size(); ++i)
    buffer->push<RotateOp>(test_floats[i]);
  ValidateOps<RotateOp>(buffer);
}

void PushSaveOps(PaintOpBuffer* buffer) {
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  buffer->push<SaveOp>();
  ValidateOps<SaveOp>(buffer);
}

void PushSaveLayerOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_flags.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerOp>(&test_rects[i], &test_flags[i]);

  // Test combinations of optional args.
  buffer->push<SaveLayerOp>(nullptr, &test_flags[0]);
  buffer->push<SaveLayerOp>(&test_rects[0], nullptr);
  buffer->push<SaveLayerOp>(nullptr, nullptr);
  ValidateOps<SaveLayerOp>(buffer);
}

void PushSaveLayerAlphaOps(PaintOpBuffer* buffer) {
  size_t len = std::min(test_uint8s.size(), test_rects.size());
  for (size_t i = 0; i < len; ++i)
    buffer->push<SaveLayerAlphaOp>(&test_rects[i], test_uint8s[i]);

  // Test optional args.
  buffer->push<SaveLayerAlphaOp>(nullptr, test_uint8s[0]);
  ValidateOps<SaveLayerAlphaOp>(buffer);
}

void PushScaleOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size() - 1; i += 2)
    buffer->push<ScaleOp>(test_floats[i], test_floats[i + 1]);
  ValidateOps<ScaleOp>(buffer);
}

void PushSetMatrixOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_matrices.size(); ++i)
    buffer->push<SetMatrixOp>(test_matrices[i]);
  ValidateOps<SetMatrixOp>(buffer);
}

void PushTranslateOps(PaintOpBuffer* buffer) {
  for (size_t i = 0; i < test_floats.size() - 1; i += 2)
    buffer->push<TranslateOp>(test_floats[i], test_floats[i + 1]);
  ValidateOps<TranslateOp>(buffer);
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
        // Not supported
        // TODO(malaykeshav): Add test when Drawable supports serialization.
        break;
      case PaintOpType::DrawTextBlob:
        PushDrawTextBlobOps(&buffer_);
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
    }
  }

  void ResizeOutputBuffer() {
    // An arbitrary deserialization buffer size that should fit all the ops
    // in the buffer_.
    output_size_ = kBufferBytesPerOp * buffer_.size();
    output_.reset(static_cast<char*>(
        base::AlignedAlloc(output_size_, PaintOpBuffer::PaintOpAlign)));
  }

  bool IsTypeSupported() {
    // DrawRecordOps and DrawSkottieOps must be flattened and are not currently
    // serialized. All other types must push non-zero amounts of ops in
    // PushTestOps.
    return GetParamType() != PaintOpType::DrawRecord &&
           GetParamType() != PaintOpType::DrawSkottie;
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
  serializer.Serialize(buffer_);

  // Expect all ops to write more than 0 bytes.
  for (size_t i = 0; i < buffer_.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zd", PaintOpTypeToString(GetParamType()).c_str(), i));
    EXPECT_GT(serializer.bytes_written()[i], 0u);
  }

  PaintOpBuffer::Iterator iter(&buffer_);
  size_t i = 0;
  for (auto* base_written : DeserializerIterator(
           output_.get(), serializer.TotalBytesWritten(),
           serializer.options_provider()->deserialize_options())) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zu", PaintOpTypeToString(GetParamType()).c_str(), i));
    ASSERT_EQ(!*iter, !base_written);
    EXPECT_EQ(**iter, *base_written);
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
  for (PaintOpBuffer::Iterator iter(&buffer_); iter; ++iter, ++op_idx) {
    SCOPED_TRACE(base::StringPrintf(
        "%s #%zu", PaintOpTypeToString(GetParamType()).c_str(), op_idx));
    size_t expected_bytes = bytes_written[op_idx];
    EXPECT_GT(expected_bytes, 0u);

    // Attempt to write op into a buffer of size |i|, and only expect
    // it to succeed if the buffer is large enough.
    for (size_t i = 0; i < bytes_written[op_idx] + 2; ++i) {
      options_provider.ClearPaintCache();
      size_t written_bytes = iter->Serialize(
          output_.get(), i, options_provider.serialize_options());
      if (i >= expected_bytes) {
        EXPECT_EQ(expected_bytes, written_bytes) << "i: " << i;
      } else {
        EXPECT_EQ(0u, written_bytes) << "i: " << i;
      }
    }
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

  static constexpr size_t kAlign = PaintOpBuffer::PaintOpAlign;
  static constexpr size_t kOutputOpSize = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialize_buffer_(
      static_cast<char*>(base::AlignedAlloc(kOutputOpSize, kAlign)));

  size_t op_idx = 0;
  size_t total_read = 0;
  for (PaintOpBuffer::Iterator iter(&buffer_); iter; ++iter, ++op_idx) {
    PaintOp* serialized = reinterpret_cast<PaintOp*>(current);
    uint32_t skip = serialized->skip;

    // Read from buffers of various sizes to make sure that having a serialized
    // op size that is larger than the input buffer provided causes a
    // deserialization failure to return nullptr.  Also test a few valid sizes
    // larger than read size.
    for (size_t read_size = 0; read_size < skip + kAlign * 2 + 2; ++read_size) {
      SCOPED_TRACE(
          base::StringPrintf("%s #%zd, read_size: %zu, align: %zu, skip: %u",
                             PaintOpTypeToString(GetParamType()).c_str(),
                             op_idx, read_size, kAlign, skip));
      // Because PaintOp::Deserialize early outs when the input size is < skip
      // deliberately lie about the skip.  This op tooooootally fits.
      // This will verify that individual op deserializing code behaves
      // properly when presented with invalid offsets.
      serialized->skip = read_size;
      size_t bytes_read = 0;
      PaintOp* written = PaintOp::Deserialize(
          current, read_size, deserialize_buffer_.get(), kOutputOpSize,
          &bytes_read, options_provider->deserialize_options());

      // Deserialize buffers with valid ops until the last op. This verifies
      // that the complete buffer is invalidated on encountering the first
      // corrupted op.
      auto deserialized_buffer = PaintOpBuffer::MakeFromMemory(
          first, total_read + read_size,
          options_provider->deserialize_options());

      // Skips are only valid if they are aligned.
      if (read_size >= skip && read_size % kAlign == 0) {
        ASSERT_NE(nullptr, written);
        ASSERT_LE(written->skip, kOutputOpSize);
        EXPECT_EQ(GetParamType(), written->GetType());
        EXPECT_EQ(serialized->skip, bytes_read);

        ASSERT_NE(nullptr, deserialized_buffer);
        EXPECT_EQ(deserialized_buffer->size(), op_idx + 1);
      } else if (read_size == 0 && op_idx != 0) {
        // If no data was read for a subsequent op while some ops were
        // deserialized, we still have a valid buffer with the deserialized ops.
        ASSERT_NE(nullptr, deserialized_buffer);
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
          PaintOpBuffer::Iterator it(deserialized_buffer.get());
          EXPECT_FALSE(it);
        } else {
          EXPECT_NE(0u, read_size);
          EXPECT_EQ(nullptr, deserialized_buffer.get());
        }
      }

      if (written)
        written->DestroyThis();
    }

    serialized->skip = skip;
    current += skip;
    total_read += skip;
  }
}

TEST_P(PaintOpSerializationTest, UsesOverridenFlags) {
  if (!PaintOp::TypeHasFlags(GetParamType()))
    return;

  PushTestOps(GetParamType());
  ResizeOutputBuffer();

  TestOptionsProvider options_provider;
  size_t deserialized_size = sizeof(LargestPaintOp) + PaintOp::kMaxSkip;
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(deserialized_size, PaintOpBuffer::PaintOpAlign)));
  for (const auto* op : PaintOpBuffer::Iterator(&buffer_)) {
    options_provider.mutable_serialize_options().flags_to_serialize =
        &static_cast<const PaintOpWithFlags*>(op)->flags;

    size_t bytes_written = op->Serialize(output_.get(), output_size_,
                                         options_provider.serialize_options());
    size_t bytes_read = 0u;
    PaintOp* written = PaintOp::Deserialize(
        output_.get(), bytes_written, deserialized.get(), deserialized_size,
        &bytes_read, options_provider.deserialize_options());
    ASSERT_TRUE(written) << PaintOpTypeToString(GetParamType());
    EXPECT_EQ(*op, *written);
    written->DestroyThis();
    written = nullptr;

    PaintFlags override_flags = static_cast<const PaintOpWithFlags*>(op)->flags;
    override_flags.setAlpha(override_flags.getAlpha() * 0.5);
    options_provider.mutable_serialize_options().flags_to_serialize =
        &override_flags;
    bytes_written = op->Serialize(output_.get(), output_size_,
                                  options_provider.serialize_options());
    written = PaintOp::Deserialize(
        output_.get(), bytes_written, deserialized.get(), deserialized_size,
        &bytes_read, options_provider.deserialize_options());
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
  PushDrawIRectOps(&buffer);

  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(1000, 1000);
  preamble.playback_rect = gfx::Rect(preamble.content_size);
  preamble.full_raster_rect = preamble.playback_rect;
  preamble.requires_clear = true;

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);

  // The deserialized buffer has an extra pair of save/restores and a clear, for
  // the preamble and root buffer.
  ASSERT_EQ(deserialized_buffer->size(), buffer.size() + 4u);

  size_t i = 0;
  auto serialized_iter = PaintOpBuffer::Iterator(&buffer);
  for (const auto* op : PaintOpBuffer::Iterator(deserialized_buffer.get())) {
    SCOPED_TRACE(i);
    i++;

    if (i == 1) {
      // Save.
      ASSERT_EQ(op->GetType(), PaintOpType::Save)
          << PaintOpTypeToString(op->GetType());
      continue;
    }

    if (i == 2) {
      // Preamble partial raster clear.
      ASSERT_EQ(op->GetType(), PaintOpType::DrawColor)
          << PaintOpTypeToString(op->GetType());
      continue;
    }
    if (i == 3) {
      // Preamble playback rect clip.
      ASSERT_EQ(op->GetType(), PaintOpType::ClipRect)
          << PaintOpTypeToString(op->GetType());
      EXPECT_EQ(static_cast<const ClipRectOp*>(op)->rect,
                gfx::RectToSkRect(preamble.playback_rect));
      continue;
    }

    if (serialized_iter) {
      // Root buffer.
      ASSERT_EQ(op->GetType(), (*serialized_iter)->GetType())
          << PaintOpTypeToString(op->GetType());
      EXPECT_EQ(*op, **serialized_iter);
      ++serialized_iter;
      continue;
    }

    // End restore.
    ASSERT_EQ(op->GetType(), PaintOpType::Restore)
        << PaintOpTypeToString(op->GetType());
  }
}

TEST(PaintOpSerializationTest, Preamble) {
  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(30, 40);
  preamble.full_raster_rect = gfx::Rect(10, 20, 8, 7);
  preamble.playback_rect = gfx::Rect(12, 25, 1, 2);
  preamble.post_translation = gfx::Vector2dF(4.3f, 7.f);
  preamble.post_scale = gfx::SizeF(0.5f, 0.5f);
  preamble.requires_clear = true;

  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);
  // 5 ops for the preamble and 2 for save/restore.
  ASSERT_EQ(deserialized_buffer->size(), buffer.size() + 7u);

  size_t i = 0;
  for (const auto* op : PaintOpBuffer::Iterator(deserialized_buffer.get())) {
    i++;

    if (i == 1) {
      // Save.
      ASSERT_EQ(op->GetType(), PaintOpType::Save)
          << PaintOpTypeToString(op->GetType());
      continue;
    }

    if (i == 2) {
      // Translate.
      ASSERT_EQ(op->GetType(), PaintOpType::Translate)
          << PaintOpTypeToString(op->GetType());
      const auto* translate_op = static_cast<const TranslateOp*>(op);
      EXPECT_EQ(translate_op->dx, -preamble.full_raster_rect.x());
      EXPECT_EQ(translate_op->dy, -preamble.full_raster_rect.y());
      continue;
    }

    if (i == 3) {
      // Clip.
      ASSERT_EQ(op->GetType(), PaintOpType::ClipRect)
          << PaintOpTypeToString(op->GetType());
      const auto* clip_op = static_cast<const ClipRectOp*>(op);
      EXPECT_FLOAT_RECT_EQ(gfx::SkRectToRectF(clip_op->rect),
                           preamble.playback_rect);
      continue;
    }

    if (i == 4) {
      // Post translate.
      ASSERT_EQ(op->GetType(), PaintOpType::Translate)
          << PaintOpTypeToString(op->GetType());
      const auto* translate_op = static_cast<const TranslateOp*>(op);
      EXPECT_EQ(translate_op->dx, preamble.post_translation.x());
      EXPECT_EQ(translate_op->dy, preamble.post_translation.y());
      continue;
    }

    if (i == 5) {
      // Scale.
      ASSERT_EQ(op->GetType(), PaintOpType::Scale)
          << PaintOpTypeToString(op->GetType());
      const auto* scale_op = static_cast<const ScaleOp*>(op);
      EXPECT_EQ(scale_op->sx, preamble.post_scale.width());
      EXPECT_EQ(scale_op->sy, preamble.post_scale.height());
      continue;
    }

    if (i == 6) {
      // Partial raster clear goes last.
      ASSERT_EQ(op->GetType(), PaintOpType::DrawColor)
          << PaintOpTypeToString(op->GetType());
      const auto* draw_color_op = static_cast<const DrawColorOp*>(op);
      EXPECT_EQ(draw_color_op->color, SK_ColorTRANSPARENT);
      EXPECT_EQ(draw_color_op->mode, SkBlendMode::kSrc);
      continue;
    }

    if (i == 7) {
      // Buffer.
      EXPECT_EQ(*op, *buffer.GetFirstOp());
      continue;
    }

    // End restore.
    ASSERT_EQ(op->GetType(), PaintOpType::Restore)
        << PaintOpTypeToString(op->GetType());
  }
}

TEST(PaintOpSerializationTest, SerializesNestedRecords) {
  auto record = sk_make_sp<PaintOpBuffer>();
  record->push<ScaleOp>(0.5f, 0.75f);
  record->push<DrawRectOp>(SkRect::MakeWH(10.f, 20.f), PaintFlags());
  PaintOpBuffer buffer;
  buffer.push<DrawRecordOp>(record);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  PaintOpBufferSerializer::Preamble preamble;
  serializer.Serialize(&buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);
  ASSERT_EQ(deserialized_buffer->size(), record->size() + 5u);

  size_t i = 0;
  auto serialized_iter = PaintOpBuffer::Iterator(record.get());
  for (const auto* op : PaintOpBuffer::Iterator(deserialized_buffer.get())) {
    i++;
    if (i == 1 || i == 3) {
      // First 2 saves.
      ASSERT_EQ(op->GetType(), PaintOpType::Save)
          << PaintOpTypeToString(op->GetType());
      continue;
    }
    // Clear.
    if (i == 2) {
      ASSERT_EQ(op->GetType(), PaintOpType::DrawColor)
          << PaintOpTypeToString(op->GetType());
      continue;
    }

    if (serialized_iter) {
      // Nested buffer.
      ASSERT_EQ(op->GetType(), (*serialized_iter)->GetType())
          << PaintOpTypeToString(op->GetType());
      EXPECT_EQ(*op, **serialized_iter);
      ++serialized_iter;
      continue;
    }

    // End restores.
    ASSERT_EQ(op->GetType(), PaintOpType::Restore)
        << PaintOpTypeToString(op->GetType());
  }
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
        static_cast<SkScalar>(test_case.image_rect.y()), nullptr);

    std::unique_ptr<char, base::AlignedFreeDeleter> memory(
        static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                              PaintOpBuffer::PaintOpAlign)));
    TestOptionsProvider options_provider;
    SimpleBufferSerializer serializer(
        memory.get(), PaintOpBuffer::kInitialBufferSize,
        options_provider.image_provider(),
        options_provider.transfer_cache_helper(),
        options_provider.client_paint_cache(), options_provider.strike_server(),
        options_provider.color_space(), options_provider.can_use_lcd_text(),
        options_provider.context_supports_distance_field_text(),
        options_provider.max_texture_size(),
        options_provider.max_texture_bytes());
    PaintOpBufferSerializer::Preamble preamble;
    preamble.playback_rect = test_case.clip_rect;
    preamble.full_raster_rect = gfx::Rect(0, 0, test_case.clip_rect.right(),
                                          test_case.clip_rect.bottom());
    // Avoid clearing.
    preamble.content_size = gfx::Size(1000, 1000);
    preamble.requires_clear = false;
    serializer.Serialize(&buffer, nullptr, preamble);
    ASSERT_NE(serializer.written(), 0u);

    auto deserialized_buffer =
        PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                      options_provider.deserialize_options());
    ASSERT_TRUE(deserialized_buffer);

    auto deserialized_iter = PaintOpBuffer::Iterator(deserialized_buffer.get());
    ASSERT_EQ((*deserialized_iter)->GetType(), PaintOpType::Save)
        << PaintOpTypeToString((*deserialized_iter)->GetType());
    ++deserialized_iter;
    ASSERT_EQ((*deserialized_iter)->GetType(), PaintOpType::ClipRect)
        << PaintOpTypeToString((*deserialized_iter)->GetType());
    ++deserialized_iter;
    if (test_case.should_draw) {
      ASSERT_EQ((*deserialized_iter)->GetType(), PaintOpType::DrawImage)
          << PaintOpTypeToString((*deserialized_iter)->GetType());
      ++deserialized_iter;
    }
    ASSERT_EQ((*deserialized_iter)->GetType(), PaintOpType::Restore)
        << PaintOpTypeToString((*deserialized_iter)->GetType());
    ++deserialized_iter;
    ASSERT_EQ(deserialized_iter.end(), deserialized_iter);
  }
}

TEST(PaintOpBufferSerializationTest, AlphaFoldingDuringSerialization) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = gfx::Size(1000, 1000);
  preamble.playback_rect = gfx::Rect(gfx::Size(100, 100));
  preamble.full_raster_rect = preamble.playback_rect;
  preamble.requires_clear = false;

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer, nullptr, preamble);
  ASSERT_NE(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);

  // 4 additional ops for save, clip, clear, and restore.
  ASSERT_EQ(deserialized_buffer->size(), 4u);
  size_t i = 0;
  for (const auto* op : PaintOpBuffer::Iterator(deserialized_buffer.get())) {
    ++i;
    if (i == 1) {
      EXPECT_EQ(op->GetType(), PaintOpType::Save);
      continue;
    }

    if (i == 2) {
      EXPECT_EQ(op->GetType(), PaintOpType::ClipRect);
      continue;
    }

    if (i == 4) {
      EXPECT_EQ(op->GetType(), PaintOpType::Restore);
      continue;
    }

    ASSERT_EQ(op->GetType(), PaintOpType::DrawRect);
    // Expect the alpha from the draw and the save layer to be folded together.
    // Since alpha is stored in a uint8_t and gets rounded, so use tolerance.
    float expected_alpha = alpha * 50 / 255.f;
    EXPECT_LE(std::abs(expected_alpha -
                       static_cast<const DrawRectOp*>(op)->flags.getAlpha()),
              1.f);
  }
}

// Test generic PaintOp deserializing failure cases.
TEST(PaintOpBufferTest, PaintOpDeserialize) {
  static constexpr size_t kSize = sizeof(LargestPaintOp) + 100;
  static constexpr size_t kAlign = PaintOpBuffer::PaintOpAlign;
  std::unique_ptr<char, base::AlignedFreeDeleter> input_(
      static_cast<char*>(base::AlignedAlloc(kSize, kAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> output_(
      static_cast<char*>(base::AlignedAlloc(kSize, kAlign)));

  PaintOpBuffer buffer;
  buffer.push<DrawColorOp>(SK_ColorMAGENTA, SkBlendMode::kSrc);

  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* op = *iter;
  ASSERT_TRUE(op);

  TestOptionsProvider options_provider;
  size_t bytes_written =
      op->Serialize(input_.get(), kSize, options_provider.serialize_options());
  ASSERT_GT(bytes_written, 0u);

  // can deserialize from exactly the right size
  size_t bytes_read = 0;
  PaintOp* success =
      PaintOp::Deserialize(input_.get(), bytes_written, output_.get(), kSize,
                           &bytes_read, options_provider.deserialize_options());
  ASSERT_TRUE(success);
  EXPECT_EQ(bytes_written, bytes_read);
  success->DestroyThis();

  // fail to deserialize if skip goes past input size
  // (the DeserializationFailures test above tests if the skip is lying)
  for (size_t i = 0; i < bytes_written - 1; ++i)
    EXPECT_FALSE(PaintOp::Deserialize(input_.get(), i, output_.get(), kSize,
                                      &bytes_read,
                                      options_provider.deserialize_options()));

  // unaligned skips fail to deserialize
  PaintOp* serialized = reinterpret_cast<PaintOp*>(input_.get());
  EXPECT_EQ(0u, serialized->skip % kAlign);
  serialized->skip -= 1;
  EXPECT_FALSE(PaintOp::Deserialize(input_.get(), bytes_written, output_.get(),
                                    kSize, &bytes_read,
                                    options_provider.deserialize_options()));
  serialized->skip += 1;

  // bogus types fail to deserialize
  serialized->type = static_cast<uint8_t>(PaintOpType::LastPaintOpType) + 1;
  EXPECT_FALSE(PaintOp::Deserialize(input_.get(), bytes_written, output_.get(),
                                    kSize, &bytes_read,
                                    options_provider.deserialize_options()));
}

// Test that deserializing invalid SkClipOp enums fails silently.
// Skia release asserts on this in several places so these are not safe
// to pass through to the SkCanvas API.
TEST(PaintOpBufferTest, ValidateSkClip) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  PaintOpBuffer buffer;

  // Successful first op.
  SkPath path;
  buffer.push<ClipPathOp>(path, SkClipOp::kMax_EnumValue, true);

  // Bad other ops.
  SkClipOp bad_clip = static_cast<SkClipOp>(
      static_cast<uint32_t>(SkClipOp::kMax_EnumValue) + 1);

  buffer.push<ClipPathOp>(path, bad_clip, true);
  buffer.push<ClipRectOp>(test_rects[0], bad_clip, true);
  buffer.push<ClipRRectOp>(test_rrects[0], bad_clip, false);

  SkClipOp bad_clip_max = static_cast<SkClipOp>(~static_cast<uint32_t>(0));
  buffer.push<ClipRectOp>(test_rects[1], bad_clip_max, false);

  TestOptionsProvider options_provider;

  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written = op->Serialize(serialized.get(), buffer_size,
                                         options_provider.serialize_options());
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written = PaintOp::Deserialize(
        serialized.get(), bytes_written, deserialized.get(), buffer_size,
        &bytes_read, options_provider.deserialize_options());
    // First op should succeed.  Other ops with bad enums should
    // serialize correctly but fail to deserialize due to the bad
    // SkClipOp enum.
    if (!op_idx) {
      EXPECT_TRUE(written) << "op: " << op_idx;
      EXPECT_EQ(bytes_written, bytes_read);
      written->DestroyThis();
    } else {
      EXPECT_FALSE(written) << "op: " << op_idx;
    }

    ++op_idx;
  }
}

TEST(PaintOpBufferTest, ValidateSkBlendMode) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  PaintOpBuffer buffer;

  // Successful first two ops.
  buffer.push<DrawColorOp>(SK_ColorMAGENTA, SkBlendMode::kDstIn);
  PaintFlags good_flags = test_flags[0];
  good_flags.setBlendMode(SkBlendMode::kColorBurn);
  buffer.push<DrawRectOp>(test_rects[0], good_flags);

  // Modes that are not supported by drawColor or SkPaint.
  SkBlendMode bad_modes_for_draw_color[] = {
      SkBlendMode::kOverlay,
      SkBlendMode::kDarken,
      SkBlendMode::kLighten,
      SkBlendMode::kColorDodge,
      SkBlendMode::kColorBurn,
      SkBlendMode::kHardLight,
      SkBlendMode::kSoftLight,
      SkBlendMode::kDifference,
      SkBlendMode::kExclusion,
      SkBlendMode::kMultiply,
      SkBlendMode::kHue,
      SkBlendMode::kSaturation,
      SkBlendMode::kColor,
      SkBlendMode::kLuminosity,
      static_cast<SkBlendMode>(static_cast<uint32_t>(SkBlendMode::kLastMode) +
                               1),
      static_cast<SkBlendMode>(static_cast<uint32_t>(~0)),
  };

  SkBlendMode bad_modes_for_flags[] = {
      static_cast<SkBlendMode>(static_cast<uint32_t>(SkBlendMode::kLastMode) +
                               1),
      static_cast<SkBlendMode>(static_cast<uint32_t>(~0)),
  };

  for (size_t i = 0; i < base::size(bad_modes_for_draw_color); ++i) {
    buffer.push<DrawColorOp>(SK_ColorMAGENTA, bad_modes_for_draw_color[i]);
  }

  for (size_t i = 0; i < base::size(bad_modes_for_flags); ++i) {
    PaintFlags flags = test_flags[i % test_flags.size()];
    flags.setBlendMode(bad_modes_for_flags[i]);
    buffer.push<DrawRectOp>(test_rects[i % test_rects.size()], flags);
  }

  TestOptionsProvider options_provider;

  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written = op->Serialize(serialized.get(), buffer_size,
                                         options_provider.serialize_options());
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written = PaintOp::Deserialize(
        serialized.get(), bytes_written, deserialized.get(), buffer_size,
        &bytes_read, options_provider.deserialize_options());
    // First two ops should succeed.  Other ops with bad enums should
    // serialize correctly but fail to deserialize due to the bad
    // SkBlendMode enum.
    if (op_idx < 2) {
      EXPECT_TRUE(written) << "op: " << op_idx;
      EXPECT_EQ(bytes_written, bytes_read);
      written->DestroyThis();
    } else {
      EXPECT_FALSE(written) << "op: " << op_idx;
    }

    ++op_idx;
  }
}

TEST(PaintOpBufferTest, ValidateRects) {
  size_t buffer_size = kBufferBytesPerOp;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized(static_cast<char*>(
      base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
      static_cast<char*>(
          base::AlignedAlloc(buffer_size, PaintOpBuffer::PaintOpAlign)));

  SkRect bad_rect = SkRect::MakeEmpty();
  bad_rect.fBottom = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(bad_rect.isFinite());

  // Push all op variations that take rects.
  PaintOpBuffer buffer;
  buffer.push<AnnotateOp>(PaintCanvas::AnnotationType::URL, bad_rect,
                          SkData::MakeWithCString("test1"));
  buffer.push<ClipRectOp>(bad_rect, SkClipOp::kDifference, true);

  buffer.push<DrawImageRectOp>(test_images[0], bad_rect, test_rects[1], nullptr,
                               PaintCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawImageRectOp>(test_images[0], test_rects[0], bad_rect, nullptr,
                               PaintCanvas::kStrict_SrcRectConstraint);
  buffer.push<DrawOvalOp>(bad_rect, test_flags[0]);
  buffer.push<DrawRectOp>(bad_rect, test_flags[0]);
  buffer.push<SaveLayerOp>(&bad_rect, nullptr);
  buffer.push<SaveLayerOp>(&bad_rect, &test_flags[0]);
  buffer.push<SaveLayerAlphaOp>(&bad_rect, test_uint8s[0]);

  TestOptionsProvider options_provider;

  // Every op should serialize but fail to deserialize due to the bad rect.
  int op_idx = 0;
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    const PaintOp* op = *iter;
    size_t bytes_written = op->Serialize(serialized.get(), buffer_size,
                                         options_provider.serialize_options());
    ASSERT_GT(bytes_written, 0u);
    size_t bytes_read = 0;
    PaintOp* written = PaintOp::Deserialize(
        serialized.get(), bytes_written, deserialized.get(), buffer_size,
        &bytes_read, options_provider.deserialize_options());
    EXPECT_FALSE(written) << "op: " << op_idx;
    ++op_idx;
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageOp) {
  PaintOpBuffer buffer;
  PushDrawImageOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawImageOp*>(base_op);

    SkRect image_rect =
        SkRect::MakeXYWH(op->left, op->top, op->image.GetSkImage()->width(),
                         op->image.GetSkImage()->height());
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, image_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawImageRectOp) {
  PaintOpBuffer buffer;
  PushDrawImageRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawImageRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->dst.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawIRectOp) {
  PaintOpBuffer buffer;
  PushDrawIRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawIRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, SkRect::Make(op->rect).makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawOvalOp) {
  PaintOpBuffer buffer;
  PushDrawOvalOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawOvalOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->oval.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawPathOp) {
  PaintOpBuffer buffer;
  PushDrawPathOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawPathOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->path.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRectOp) {
  PaintOpBuffer buffer;
  PushDrawRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawRRectOp) {
  PaintOpBuffer buffer;
  PushDrawRRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawRRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->rrect.rect().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawLineOp) {
  PaintOpBuffer buffer;
  PushDrawLineOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawLineOp*>(base_op);

    SkRect line_rect;
    line_rect.fLeft = op->x0;
    line_rect.fTop = op->y0;
    line_rect.fRight = op->x1;
    line_rect.fBottom = op->y1;
    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, line_rect.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawDRRectOp) {
  PaintOpBuffer buffer;
  PushDrawDRRectOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawDRRectOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->outer.getBounds().makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawSkottieOp) {
  PaintOpBuffer buffer;
  PushDrawSkottieOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawSkottieOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->dst.makeSorted());
  }
}

TEST(PaintOpBufferTest, BoundingRect_DrawTextBlobOp) {
  PaintOpBuffer buffer;
  PushDrawTextBlobOps(&buffer);

  SkRect rect;
  for (auto* base_op : PaintOpBuffer::Iterator(&buffer)) {
    auto* op = static_cast<DrawTextBlobOp*>(base_op);

    ASSERT_TRUE(PaintOp::GetBounds(op, &rect));
    EXPECT_EQ(rect, op->blob->bounds().makeOffset(op->x, op->y).makeSorted());
  }
}

class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  explicit MockImageProvider(bool fail_all_decodes)
      : fail_all_decodes_(fail_all_decodes) {}
  MockImageProvider(std::vector<SkSize> src_rect_offset,
                    std::vector<SkSize> scale,
                    std::vector<SkFilterQuality> quality)
      : src_rect_offset_(src_rect_offset), scale_(scale), quality_(quality) {}

  ~MockImageProvider() override = default;

  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override {
    if (draw_image.paint_image().IsPaintWorklet())
      return ScopedResult(record_);

    if (fail_all_decodes_)
      return ImageProvider::ScopedResult();

    SkBitmap bitmap;
    bitmap.allocPixelsFlags(SkImageInfo::MakeN32Premul(10, 10),
                            SkBitmap::kZeroPixels_AllocFlag);
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
    size_t i = index_++;
    return ScopedResult(DecodedDrawImage(image, src_rect_offset_[i], scale_[i],
                                         quality_[i], true));
  }

  void SetRecord(sk_sp<PaintRecord> record) { record_ = std::move(record); }

 private:
  std::vector<SkSize> src_rect_offset_;
  std::vector<SkSize> scale_;
  std::vector<SkFilterQuality> quality_;
  size_t index_ = 0;
  bool fail_all_decodes_ = false;
  sk_sp<PaintRecord> record_;
};

TEST(PaintOpBufferTest, SkipsOpsOutsideClip) {
  // All ops with images draw outside the clip and should be skipped. If any
  // call is made to the ImageProvider, it should crash.
  MockImageProvider image_provider;
  PaintOpBuffer buffer;

  // Apply a clip outside the region for images.
  buffer.push<ClipRectOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                          SkClipOp::kIntersect, false);

  PaintFlags flags;
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f, &flags);
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

  PaintFlags flags;
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 105.0f, 105.0f, &flags);
  PaintFlags image_flags;
  image_flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                               SkTileMode::kRepeat, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(110, 110, 100, 100), image_flags);
  buffer.push<DrawColorOp>(SK_ColorRED, SkBlendMode::kSrcOver);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(_)).InSequence(s);
  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

MATCHER(NonLazyImage, "") {
  return !arg->isLazyGenerated();
}

MATCHER_P(MatchesInvScale, expected, "") {
  SkSize scale;
  arg.decomposeScale(&scale, nullptr);
  SkSize inv = SkSize::Make(1.0f / scale.width(), 1.0f / scale.height());
  return inv == expected;
}

MATCHER_P2(MatchesRect, rect, scale, "") {
  EXPECT_EQ(arg->x(), rect.x() * scale.width());
  EXPECT_EQ(arg->y(), rect.y() * scale.height());
  EXPECT_EQ(arg->width(), rect.width() * scale.width());
  EXPECT_EQ(arg->height(), rect.height() * scale.height());
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
  sk_sp<PaintOpBuffer> paint_worklet_buffer = sk_make_sp<PaintOpBuffer>();
  PaintFlags noop_flags;
  SkRect savelayer_rect = SkRect::MakeXYWH(0, 0, 100, 100);
  paint_worklet_buffer->push<TranslateOp>(8.0f, 8.0f);
  paint_worklet_buffer->push<SaveLayerOp>(&savelayer_rect, &noop_flags);
  PaintFlags draw_flags;
  draw_flags.setColor(0u);
  SkRect rect = SkRect::MakeXYWH(0, 0, 100, 100);
  paint_worklet_buffer->push<DrawRectOp>(rect, draw_flags);

  MockImageProvider provider;
  provider.SetRecord(paint_worklet_buffer);

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(0, 0, 100, 100);
  blink_buffer.push<DrawImageRectOp>(image, src, dst, nullptr,
                                     PaintCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(SkMatrix::MakeTrans(8.0f, 8.0f)));
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectTranslated) {
  sk_sp<PaintOpBuffer> paint_worklet_buffer = sk_make_sp<PaintOpBuffer>();
  PaintFlags noop_flags;
  SkRect savelayer_rect = SkRect::MakeXYWH(0, 0, 10, 10);
  paint_worklet_buffer->push<SaveLayerOp>(&savelayer_rect, &noop_flags);
  PaintFlags draw_flags;
  draw_flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  paint_worklet_buffer->push<DrawImageOp>(paint_image, 0.0f, 0.0f, &draw_flags);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<SkFilterQuality> quality = {kHigh_SkFilterQuality};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer);

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(5, 7, 100, 100);
  blink_buffer.push<DrawImageRectOp>(image, src, dst, nullptr,
                                     PaintCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(SkMatrix::MakeTrans(5.0f, 7.0f)));
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(MatchesInvScale(scale_adjustment[0])));
  EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f,
                                  MatchesQuality(quality[0])));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectScaled) {
  sk_sp<PaintOpBuffer> paint_worklet_buffer = sk_make_sp<PaintOpBuffer>();
  PaintFlags noop_flags;
  SkRect savelayer_rect = SkRect::MakeXYWH(0, 0, 10, 10);
  paint_worklet_buffer->push<SaveLayerOp>(&savelayer_rect, &noop_flags);
  PaintFlags draw_flags;
  draw_flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  paint_worklet_buffer->push<DrawImageOp>(paint_image, 0.0f, 0.0f, &draw_flags);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<SkFilterQuality> quality = {kHigh_SkFilterQuality};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer);

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 100, 100);
  SkRect dst = SkRect::MakeXYWH(0, 0, 200, 150);
  blink_buffer.push<DrawImageRectOp>(image, src, dst, nullptr,
                                     PaintCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(SkMatrix::MakeScale(2.f, 1.5f)));
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(MatchesInvScale(scale_adjustment[0])));
  EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f,
                                  MatchesQuality(quality[0])));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  blink_buffer.Playback(&canvas, PlaybackParams(&provider));
}

TEST(PaintOpBufferTest, RasterPaintWorkletImageRectClipped) {
  sk_sp<PaintOpBuffer> paint_worklet_buffer = sk_make_sp<PaintOpBuffer>();
  PaintFlags noop_flags;
  SkRect savelayer_rect = SkRect::MakeXYWH(0, 0, 60, 60);
  paint_worklet_buffer->push<SaveLayerOp>(&savelayer_rect, &noop_flags);
  PaintFlags draw_flags;
  draw_flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  // One rect inside the src-rect, one outside.
  paint_worklet_buffer->push<DrawImageOp>(paint_image, 0.0f, 0.0f, &draw_flags);
  paint_worklet_buffer->push<DrawImageOp>(paint_image, 50.0f, 50.0f,
                                          &draw_flags);

  std::vector<SkSize> src_rect_offset = {SkSize::MakeEmpty()};
  std::vector<SkSize> scale_adjustment = {SkSize::Make(0.2f, 0.2f)};
  std::vector<SkFilterQuality> quality = {kHigh_SkFilterQuality};
  MockImageProvider provider(src_rect_offset, scale_adjustment, quality);
  provider.SetRecord(paint_worklet_buffer);

  PaintOpBuffer blink_buffer;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  SkRect src = SkRect::MakeXYWH(0, 0, 20, 20);
  SkRect dst = SkRect::MakeXYWH(0, 0, 20, 20);
  blink_buffer.push<DrawImageRectOp>(image, src, dst, nullptr,
                                     PaintCanvas::kStrict_SrcRectConstraint);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(MatchesInvScale(scale_adjustment[0])));
  EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f,
                                  MatchesQuality(quality[0])));
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
  std::vector<SkFilterQuality> quality = {
      kHigh_SkFilterQuality, kMedium_SkFilterQuality, kHigh_SkFilterQuality};

  MockImageProvider image_provider(src_rect_offset, scale_adjustment, quality);
  PaintOpBuffer buffer;

  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, &flags);
  buffer.push<DrawImageRectOp>(
      paint_image, rect, rect, &flags,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
  flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                         SkTileMode::kRepeat, nullptr));
  buffer.push<DrawOvalOp>(SkRect::MakeWH(10, 10), flags);

  testing::StrictMock<MockCanvas> canvas;
  testing::Sequence s;

  // Save/scale/image/restore from DrawImageop.
  EXPECT_CALL(canvas, willSave()).InSequence(s);
  EXPECT_CALL(canvas, didConcat(MatchesInvScale(scale_adjustment[0])));
  EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f,
                                  MatchesQuality(quality[0])));
  EXPECT_CALL(canvas, willRestore()).InSequence(s);

  // DrawImageRectop.
  SkRect src_rect =
      rect.makeOffset(src_rect_offset[1].width(), src_rect_offset[1].height());
  EXPECT_CALL(canvas,
              onDrawImageRect(
                  NonLazyImage(), MatchesRect(src_rect, scale_adjustment[1]),
                  SkRect::MakeWH(10, 10), MatchesQuality(quality[1]),
                  SkCanvas::kFast_SrcRectConstraint));

  // DrawOvalop.
  EXPECT_CALL(canvas, onDrawOval(SkRect::MakeWH(10, 10),
                                 MatchesShader(flags, scale_adjustment[2])));

  buffer.Playback(&canvas, PlaybackParams(&image_provider));
}

TEST(PaintOpBufferTest, ReplacesImagesFromProviderOOP) {
  PaintOpBuffer buffer;
  SkSize expected_scale = SkSize::Make(0.2f, 0.5f);

  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  PaintImage paint_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  buffer.push<ScaleOp>(expected_scale.width(), expected_scale.height());
  buffer.push<DrawImageOp>(paint_image, 0.0f, 0.0f, &flags);
  buffer.push<DrawImageRectOp>(
      paint_image, rect, rect, &flags,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
  flags.setShader(PaintShader::MakeImage(paint_image, SkTileMode::kRepeat,
                                         SkTileMode::kRepeat, nullptr));
  buffer.push<DrawOvalOp>(SkRect::MakeWH(10, 10), flags);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer);
  ASSERT_NE(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);

  for (auto* op : PaintOpBuffer::Iterator(deserialized_buffer.get())) {
    testing::NiceMock<MockCanvas> canvas;
    PlaybackParams params(nullptr);
    testing::Sequence s;

    if (op->GetType() == PaintOpType::DrawImage) {
      // Save/scale/image/restore from DrawImageop.
      EXPECT_CALL(canvas, willSave()).InSequence(s);
      EXPECT_CALL(canvas, didConcat(MatchesInvScale(expected_scale)));
      EXPECT_CALL(canvas, onDrawImage(NonLazyImage(), 0.0f, 0.0f, _));
      EXPECT_CALL(canvas, willRestore()).InSequence(s);
      op->Raster(&canvas, params);
    } else if (op->GetType() == PaintOpType::DrawImageRect) {
      EXPECT_CALL(canvas, onDrawImageRect(NonLazyImage(),
                                          MatchesRect(rect, expected_scale),
                                          SkRect::MakeWH(10, 10), _,
                                          SkCanvas::kFast_SrcRectConstraint));
      op->Raster(&canvas, params);
    } else if (op->GetType() == PaintOpType::DrawOval) {
      EXPECT_CALL(canvas, onDrawOval(SkRect::MakeWH(10, 10),
                                     MatchesShader(flags, expected_scale)));
      op->Raster(&canvas, params);
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
      sk_sp<PaintFilter>{new BlurPaintFilter(
          0.5f, 0.3f, SkBlurImageFilter::kRepeat_TileMode, nullptr)},
      sk_sp<PaintFilter>{new DropShadowPaintFilter(
          5.f, 10.f, 0.1f, 0.3f, SK_ColorBLUE,
          SkDropShadowImageFilter::kDrawShadowOnly_ShadowMode, nullptr)},
      sk_sp<PaintFilter>{new MagnifierPaintFilter(SkRect::MakeXYWH(5, 6, 7, 8),
                                                  10.5f, nullptr)},
      sk_sp<PaintFilter>{new AlphaThresholdPaintFilter(
          SkRegion(SkIRect::MakeXYWH(0, 0, 100, 200)), 10.f, 20.f, nullptr)},
      sk_sp<PaintFilter>{new MatrixConvolutionPaintFilter(
          SkISize::Make(3, 3), scalars, 30.f, 123.f, SkIPoint::Make(0, 0),
          SkMatrixConvolutionImageFilter::kClampToBlack_TileMode, true,
          nullptr)},
      sk_sp<PaintFilter>{new MorphologyPaintFilter(
          MorphologyPaintFilter::MorphType::kErode, 15, 30, nullptr)},
      sk_sp<PaintFilter>{new OffsetPaintFilter(-1.f, -2.f, nullptr)},
      sk_sp<PaintFilter>{new TilePaintFilter(
          SkRect::MakeXYWH(1, 2, 3, 4), SkRect::MakeXYWH(4, 3, 2, 1), nullptr)},
      sk_sp<PaintFilter>{new TurbulencePaintFilter(
          TurbulencePaintFilter::TurbulenceType::kFractalNoise, 3.3f, 4.4f, 2,
          123, nullptr)},
      sk_sp<PaintFilter>{
          new MatrixPaintFilter(SkMatrix::I(), kHigh_SkFilterQuality, nullptr)},
      sk_sp<PaintFilter>{new LightingDistantPaintFilter(
          PaintFilter::LightingType::kSpecular, SkPoint3::Make(1, 2, 3),
          SK_ColorCYAN, 1.1f, 2.2f, 3.3f, nullptr)},
      sk_sp<PaintFilter>{new LightingPointPaintFilter(
          PaintFilter::LightingType::kDiffuse, SkPoint3::Make(2, 3, 4),
          SK_ColorRED, 1.2f, 3.4f, 5.6f, nullptr)},
      sk_sp<PaintFilter>{new LightingSpotPaintFilter(
          PaintFilter::LightingType::kSpecular, SkPoint3::Make(100, 200, 300),
          SkPoint3::Make(400, 500, 600), 1, 2, SK_ColorMAGENTA, 3, 4, 5,
          nullptr)},
      sk_sp<PaintFilter>{
          new ImagePaintFilter(CreateDiscardablePaintImage(gfx::Size(100, 100)),
                               SkRect::MakeWH(50, 50), SkRect::MakeWH(70, 70),
                               kMedium_SkFilterQuality)}};

  filters.emplace_back(new ComposePaintFilter(filters[0], filters[1]));
  filters.emplace_back(
      new XfermodePaintFilter(SkBlendMode::kDst, filters[2], filters[3]));
  filters.emplace_back(new ArithmeticPaintFilter(
      1.1f, 2.2f, 3.3f, 4.4f, false, filters[4], filters[5], nullptr));
  filters.emplace_back(new DisplacementMapEffectPaintFilter(
      SkDisplacementMapEffect::kR_ChannelSelectorType,
      SkDisplacementMapEffect::kG_ChannelSelectorType, 10, filters[6],
      filters[7]));
  filters.emplace_back(new MergePaintFilter(filters.data(), filters.size()));
  filters.emplace_back(new RecordPaintFilter(
      sk_sp<PaintRecord>{new PaintRecord}, SkRect::MakeXYWH(10, 15, 20, 25)));

  TestOptionsProvider options_provider;
  for (size_t i = 0; i < filters.size(); ++i) {
    SCOPED_TRACE(i);

    auto& filter = filters[i];
    std::vector<uint8_t> memory;
    size_t buffer_size = filter->type() == PaintFilter::Type::kPaintRecord
                             ? PaintOpBuffer::kInitialBufferSize
                             : PaintFilter::GetFilterSize(filter.get());
    buffer_size += PaintOpWriter::HeaderBytes();
    memory.resize(buffer_size);

    PaintOpWriter writer(memory.data(), memory.size(),
                         options_provider.serialize_options(), GetParam());
    writer.Write(filter.get());
    ASSERT_GT(writer.size(), 0u) << PaintFilter::TypeToString(filter->type());

    sk_sp<PaintFilter> deserialized_filter;
    PaintOpReader reader(memory.data(), writer.size(),
                         options_provider.deserialize_options(), GetParam());
    reader.Read(&deserialized_filter);
    ASSERT_TRUE(deserialized_filter);
    EXPECT_TRUE(*filter == *deserialized_filter);
  }
}

TEST(PaintOpBufferTest, PaintRecordShaderSerialization) {
  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  sk_sp<PaintOpBuffer> record_buffer(new PaintOpBuffer);
  record_buffer->push<DrawRectOp>(SkRect::MakeXYWH(0, 0, 1, 1), PaintFlags());

  TestOptionsProvider options_provider;
  PaintFlags flags;
  flags.setShader(PaintShader::MakePaintRecord(
      record_buffer, SkRect::MakeWH(10, 10), SkTileMode::kClamp,
      SkTileMode::kRepeat, nullptr));
  PaintOpBuffer buffer;
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(1, 2, 3, 4), flags);

  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer);
  ASSERT_TRUE(serializer.valid());
  ASSERT_GT(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);
  PaintOpBuffer::Iterator it(deserialized_buffer.get());
  ASSERT_TRUE(it);
  auto* op = *it;
  ASSERT_TRUE(op->GetType() == PaintOpType::DrawRect);
  auto* rect_op = static_cast<DrawRectOp*>(op);
  EXPECT_FLOAT_RECT_EQ(rect_op->rect, SkRect::MakeXYWH(1, 2, 3, 4));
  EXPECT_TRUE(rect_op->flags == flags);
  EXPECT_TRUE(*rect_op->flags.getShader() == *flags.getShader());
  EXPECT_TRUE(!!rect_op->flags.getShader()->GetSkShader());
}

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
    EXPECT_EQ(new_buffer.GetFirstOp()->GetType(), PaintOpType::CustomData);

    PaintOpBuffer buffer2;
    buffer2.push<CustomDataOp>(1234u);
    EXPECT_TRUE(*new_buffer.GetFirstOp() == *buffer2.GetFirstOp());
  }

  // Push and verify.
  {
    PaintOpBuffer buffer;
    buffer.push<SaveOp>();
    buffer.push<CustomDataOp>(0xFFFFFFFF);
    buffer.push<RestoreOp>();
    EXPECT_EQ(buffer.size(), 3u);

    PaintOpBuffer::Iterator iter(&buffer);
    ASSERT_EQ(iter->GetType(), PaintOpType::Save);
    ++iter;
    ASSERT_EQ(iter->GetType(), PaintOpType::CustomData);
    ++iter;
    ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
    ++iter;
  }

  // Playback.
  {
    PaintOpBuffer buffer;
    buffer.push<CustomDataOp>(9999u);
    testing::StrictMock<MockCanvas> canvas;
    EXPECT_CALL(canvas, onCustomCallback(&canvas, 9999)).Times(1);
    buffer.Playback(&canvas, PlaybackParams(nullptr, SkMatrix::I(),
                                            base::BindRepeating(
                                                &MockCanvas::onCustomCallback,
                                                base::Unretained(&canvas))));
  }
}

TEST(PaintOpBufferTest, SecurityConstrainedImageSerialization) {
  auto image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  sk_sp<PaintFilter> filter = sk_make_sp<ImagePaintFilter>(
      image, SkRect::MakeWH(10, 10), SkRect::MakeWH(10, 10),
      kLow_SkFilterQuality);
  const bool enable_security_constraints = true;

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  PaintOpWriter writer(memory.get(), PaintOpBuffer::kInitialBufferSize,
                       options_provider.serialize_options(),
                       enable_security_constraints);
  writer.Write(filter.get());

  sk_sp<PaintFilter> out_filter;
  PaintOpReader reader(memory.get(), writer.size(),
                       options_provider.deserialize_options(),
                       enable_security_constraints);
  reader.Read(&out_filter);
  EXPECT_TRUE(*filter == *out_filter);
}

TEST(PaintOpBufferTest, DrawImageRectSerializeScaledImages) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  buffer->push<ScaleOp>(0.5f, 2.0f);

  // scales: x dimension = x0.25, y dimension = x5
  // translations here are arbitrary
  SkRect src = SkRect::MakeXYWH(3, 4, 20, 6);
  SkRect dst = SkRect::MakeXYWH(20, 38, 5, 30);
  buffer->push<DrawImageRectOp>(
      CreateDiscardablePaintImage(gfx::Size(32, 16)), src, dst, nullptr,
      PaintCanvas::SrcRectConstraint::kStrict_SrcRectConstraint);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(buffer.get());

  ASSERT_EQ(options_provider.decoded_images().size(), 1u);
  auto scale = options_provider.decoded_images().at(0).scale();
  EXPECT_EQ(scale.width(), 0.5f * 0.25f);
  EXPECT_EQ(scale.height(), 2.0f * 5.0f);
}

TEST(PaintOpBufferTest, RecordShadersSerializeScaledImages) {
  auto record_buffer = sk_make_sp<PaintOpBuffer>();
  record_buffer->push<DrawImageOp>(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f, 0.f, nullptr);

  auto shader = PaintShader::MakePaintRecord(
      record_buffer, SkRect::MakeWH(10.f, 10.f), SkTileMode::kRepeat,
      SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(true);
  auto buffer = sk_make_sp<PaintOpBuffer>();
  buffer->push<ScaleOp>(0.5f, 0.8f);
  PaintFlags flags;
  flags.setShader(shader);
  buffer->push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(buffer.get());

  ASSERT_EQ(options_provider.decoded_images().size(), 1u);
  auto scale = options_provider.decoded_images().at(0).scale();
  EXPECT_EQ(scale.width(), 0.5f);
  EXPECT_EQ(scale.height(), 0.8f);
}

TEST(PaintOpBufferTest, RecordShadersCached) {
  auto record_buffer = sk_make_sp<PaintOpBuffer>();
  record_buffer->push<DrawImageOp>(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f, 0.f, nullptr);
  auto shader = PaintShader::MakePaintRecord(
      record_buffer, SkRect::MakeWH(10.f, 10.f), SkTileMode::kRepeat,
      SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(false);
  auto shader_id = shader->paint_record_shader_id();
  TestOptionsProvider options_provider;
  auto* transfer_cache = options_provider.transfer_cache_helper();

  // Generate serialized |memory|.
  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  size_t memory_written = 0;
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    PaintFlags flags;
    flags.setShader(shader);
    buffer->push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

    SimpleBufferSerializer serializer(
        memory.get(), PaintOpBuffer::kInitialBufferSize,
        options_provider.image_provider(), transfer_cache,
        options_provider.client_paint_cache(), options_provider.strike_server(),
        options_provider.color_space(), options_provider.can_use_lcd_text(),
        options_provider.context_supports_distance_field_text(),
        options_provider.max_texture_size(),
        options_provider.max_texture_bytes());
    serializer.Serialize(buffer.get());
    memory_written = serializer.written();
  }

  // Generate serialized |memory_scaled|, which is the same pob, but with
  // a scale factor.
  std::unique_ptr<char, base::AlignedFreeDeleter> memory_scaled(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  size_t memory_scaled_written = 0;
  {
    auto buffer = sk_make_sp<PaintOpBuffer>();
    PaintFlags flags;
    flags.setShader(shader);
    // This buffer has an additional scale op.
    buffer->push<ScaleOp>(2.0f, 3.7f);
    buffer->push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

    SimpleBufferSerializer serializer(
        memory_scaled.get(), PaintOpBuffer::kInitialBufferSize,
        options_provider.image_provider(), transfer_cache,
        options_provider.client_paint_cache(), options_provider.strike_server(),
        options_provider.color_space(), options_provider.can_use_lcd_text(),
        options_provider.context_supports_distance_field_text(),
        options_provider.max_texture_size(),
        options_provider.max_texture_bytes());
    serializer.Serialize(buffer.get());
    memory_scaled_written = serializer.written();
  }

  // Hold onto records so PaintShader pointer comparisons are valid.
  sk_sp<PaintRecord> records[5];
  const SkShader* last_shader = nullptr;
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions deserialize_options(
      transfer_cache, options_provider.service_paint_cache(),
      options_provider.strike_client(), &scratch_buffer);

  // Several deserialization test cases:
  // (0) deserialize once, verify cached is the same as deserialized version
  // (1) deserialize again, verify shader gets reused
  // (2) change scale, verify shader is new
  // (3) sanity check, same new scale + same new colorspace, shader is reused.
  for (size_t i = 0; i < 4; ++i) {
    if (i < 2) {
      records[i] = PaintOpBuffer::MakeFromMemory(memory.get(), memory_written,
                                                 deserialize_options);
    } else {
      records[i] = PaintOpBuffer::MakeFromMemory(
          memory_scaled.get(), memory_scaled_written, deserialize_options);
    }

    auto* entry =
        transfer_cache->GetEntryAs<ServiceShaderTransferCacheEntry>(shader_id);
    ASSERT_TRUE(entry);
    if (i < 2)
      EXPECT_EQ(records[i]->size(), 1u);
    else
      EXPECT_EQ(records[i]->size(), 2u);

    for (auto* base_op : PaintOpBuffer::Iterator(records[i].get())) {
      if (base_op->GetType() != PaintOpType::DrawRect)
        continue;
      auto* op = static_cast<const DrawRectOp*>(base_op);

      // In every case, the shader in the op should get cached for future
      // use.
      auto* op_skshader = op->flags.getShader()->GetSkShader().get();
      EXPECT_EQ(op_skshader, entry->shader()->GetSkShader().get());
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
  auto record_buffer = sk_make_sp<PaintOpBuffer>();
  size_t estimated_image_size = 30 * 30 * 4;
  auto image = CreateBitmapImage(gfx::Size(30, 30));
  record_buffer->push<DrawImageOp>(image, 0.f, 0.f, nullptr);
  auto shader = PaintShader::MakePaintRecord(
      record_buffer, SkRect::MakeWH(10.f, 10.f), SkTileMode::kRepeat,
      SkTileMode::kRepeat, nullptr);
  shader->set_has_animated_images(false);
  auto shader_id = shader->paint_record_shader_id();
  TestOptionsProvider options_provider;
  auto* transfer_cache = options_provider.transfer_cache_helper();

  // Generate serialized |memory|.
  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  auto buffer = sk_make_sp<PaintOpBuffer>();
  PaintFlags flags;
  flags.setShader(shader);
  buffer->push<DrawRectOp>(SkRect::MakeWH(10.f, 10.f), flags);

  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  options_provider.context_supports_distance_field_text();
  serializer.Serialize(buffer.get());

  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions deserialize_options(
      transfer_cache, options_provider.service_paint_cache(),
      options_provider.strike_client(), &scratch_buffer);
  auto record = PaintOpBuffer::MakeFromMemory(
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

TEST(PaintOpBufferTest, TotalOpCount) {
  auto record_buffer = sk_make_sp<PaintOpBuffer>();
  auto sub_record_buffer = sk_make_sp<PaintOpBuffer>();
  auto sub_sub_record_buffer = sk_make_sp<PaintOpBuffer>();
  PushDrawRectOps(sub_sub_record_buffer.get());
  PushDrawRectOps(sub_record_buffer.get());
  PushDrawRectOps(record_buffer.get());
  sub_record_buffer->push<DrawRecordOp>(sub_sub_record_buffer);
  record_buffer->push<DrawRecordOp>(sub_record_buffer);

  size_t len = std::min(test_rects.size(), test_flags.size());
  EXPECT_EQ(len, sub_sub_record_buffer->total_op_count());
  EXPECT_EQ(2 * len + 1, sub_record_buffer->total_op_count());
  EXPECT_EQ(3 * len + 2, record_buffer->total_op_count());
}

TEST(PaintOpBufferTest, NullImages) {
  PaintOpBuffer buffer;
  buffer.push<DrawImageOp>(PaintImage(), 0.f, 0.f, nullptr);

  std::unique_ptr<char, base::AlignedFreeDeleter> memory(
      static_cast<char*>(base::AlignedAlloc(PaintOpBuffer::kInitialBufferSize,
                                            PaintOpBuffer::PaintOpAlign)));
  TestOptionsProvider options_provider;
  SimpleBufferSerializer serializer(
      memory.get(), PaintOpBuffer::kInitialBufferSize,
      options_provider.image_provider(),
      options_provider.transfer_cache_helper(),
      options_provider.client_paint_cache(), options_provider.strike_server(),
      options_provider.color_space(), options_provider.can_use_lcd_text(),
      options_provider.context_supports_distance_field_text(),
      options_provider.max_texture_size(),
      options_provider.max_texture_bytes());
  serializer.Serialize(&buffer);
  ASSERT_TRUE(serializer.valid());
  ASSERT_GT(serializer.written(), 0u);

  auto deserialized_buffer =
      PaintOpBuffer::MakeFromMemory(memory.get(), serializer.written(),
                                    options_provider.deserialize_options());
  ASSERT_TRUE(deserialized_buffer);
  ASSERT_EQ(deserialized_buffer->size(), 1u);
  ASSERT_EQ(deserialized_buffer->GetFirstOp()->GetType(),
            PaintOpType::DrawImage);
}

}  // namespace cc
