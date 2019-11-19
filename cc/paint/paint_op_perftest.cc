// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/timer/lap_timer.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/test/test_options_provider.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kNumWarmupRuns = 20;
static const int kTimeCheckInterval = 1;

static const size_t kMaxSerializedBufferBytes = 100000;

class PaintOpPerfTest : public testing::Test {
 public:
  PaintOpPerfTest()
      : timer_(kNumWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        serialized_data_(static_cast<char*>(
            base::AlignedAlloc(kMaxSerializedBufferBytes,
                               PaintOpBuffer::PaintOpAlign))),
        deserialized_data_(static_cast<char*>(
            base::AlignedAlloc(sizeof(LargestPaintOp),
                               PaintOpBuffer::PaintOpAlign))) {}

  void RunTest(const std::string& name, const PaintOpBuffer& buffer) {
    TestOptionsProvider test_options_provider;

    size_t bytes_written = 0u;
    PaintOpBufferSerializer::Preamble preamble;

    timer_.Reset();
    do {
      SimpleBufferSerializer serializer(
          serialized_data_.get(), kMaxSerializedBufferBytes,
          test_options_provider.image_provider(),
          test_options_provider.transfer_cache_helper(),
          test_options_provider.client_paint_cache(),
          test_options_provider.strike_server(),
          test_options_provider.color_space(),
          test_options_provider.can_use_lcd_text(),
          test_options_provider.context_supports_distance_field_text(),
          test_options_provider.max_texture_size(),
          test_options_provider.max_texture_bytes());
      serializer.Serialize(&buffer, nullptr, preamble);
      bytes_written = serializer.written();

      // Force client paint cache entries to be written every time.
      test_options_provider.client_paint_cache()->PurgeAll();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
    CHECK_GT(bytes_written, 0u);

    perf_test::PerfResultReporter reporter(name, "  serialize");
    reporter.RegisterImportantMetric("", "runs/s");
    reporter.AddResult("", timer_.LapsPerSecond());

    size_t bytes_read = 0;
    timer_.Reset();
    test_options_provider.PushFonts();

    do {
      size_t remaining_read_bytes = bytes_written;
      char* to_read = serialized_data_.get();

      while (true) {
        PaintOp* deserialized_op = PaintOp::Deserialize(
            to_read, remaining_read_bytes, deserialized_data_.get(),
            sizeof(LargestPaintOp), &bytes_read,
            test_options_provider.deserialize_options());
        CHECK(deserialized_op);
        deserialized_op->DestroyThis();

        DCHECK_GE(remaining_read_bytes, bytes_read);
        if (remaining_read_bytes == bytes_read)
          break;

        remaining_read_bytes -= bytes_read;
        to_read += bytes_read;
      }

      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    reporter = perf_test::PerfResultReporter(name, "deserialize");
    reporter.RegisterImportantMetric("", "runs/s");
    reporter.AddResult("", timer_.LapsPerSecond());
  }

 protected:
  base::LapTimer timer_;
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized_data_;
  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized_data_;
};

// Ops that can be memcopied both when serializing and deserializing.
TEST_F(PaintOpPerfTest, SimpleOps) {
  PaintOpBuffer buffer;
  for (size_t i = 0; i < 100; ++i)
    buffer.push<ConcatOp>(SkMatrix::I());
  RunTest("simple", buffer);
}

// Drawing ops with flags that don't have nested objects.
TEST_F(PaintOpPerfTest, DrawOps) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  for (size_t i = 0; i < 100; ++i)
    buffer.push<DrawRectOp>(SkRect::MakeXYWH(1, 1, 1, 1), flags);
  RunTest("draw", buffer);
}

// Ops with worst case flags.
TEST_F(PaintOpPerfTest, ManyFlagsOps) {
  PaintOpBuffer buffer;

  PaintFlags flags;
  SkScalar intervals[] = {1.f, 1.f};
  flags.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
  flags.setMaskFilter(SkMaskFilter::MakeBlur(
      SkBlurStyle::kOuter_SkBlurStyle, 4.3));
  flags.setColorFilter(
      SkColorMatrixFilter::MakeLightingFilter(SK_ColorYELLOW, SK_ColorGREEN));

  SkLayerDrawLooper::Builder looper_builder;
  looper_builder.addLayer();
  looper_builder.addLayer(2.3f, 4.5f);
  SkLayerDrawLooper::LayerInfo layer_info;
  looper_builder.addLayer(layer_info);
  flags.setLooper(looper_builder.detach());

  sk_sp<PaintShader> shader = PaintShader::MakeColor(SK_ColorTRANSPARENT);
  flags.setShader(std::move(shader));

  SkPath path;
  path.addCircle(2, 2, 5);
  path.addCircle(3, 4, 2);
  path.addArc(SkRect::MakeXYWH(1, 2, 3, 4), 5, 6);

  for (size_t i = 0; i < 100; ++i)
    buffer.push<DrawPathOp>(path, flags);
  RunTest("flags", buffer);
}

// DrawTextBlobOps,
TEST_F(PaintOpPerfTest, TextOps) {
  PaintOpBuffer buffer;

  auto typeface = SkTypeface::MakeDefault();

  SkFont font;
  font.setTypeface(typeface);

  SkTextBlobBuilder builder;
  int glyph_count = 5;
  const auto& run = builder.allocRun(font, glyph_count, 1.2f, 2.3f);
  std::fill(run.glyphs, run.glyphs + glyph_count, 0);
  auto blob = builder.make();

  PaintFlags flags;
  for (size_t i = 0; i < 100; ++i)
    buffer.push<DrawTextBlobOp>(blob, 0.f, 0.f, flags);

  RunTest("text", buffer);
}

}  // namespace
}  // namespace cc
