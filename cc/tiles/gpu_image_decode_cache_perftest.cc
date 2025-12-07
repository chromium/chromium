// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <memory>
#include <vector>

#include "base/timer/lap_timer.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/raster/tile_task.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/gpu/GpuTypes.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;
static const int kCacheSize = 128 * 1024 * 1024;

sk_sp<SkImage> CreateImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeS32(width, height, kPremul_SkAlphaType));
  return SkImages::RasterFromBitmap(bitmap);
}

SkM44 CreateMatrix(const SkSize& scale) {
  return SkM44::Scale(scale.width(), scale.height());
}

class GpuImageDecodeCachePerfTest : public testing::Test {
 public:
  GpuImageDecodeCachePerfTest()
      : timer_(kWarmupRuns,
               base::Milliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        context_provider_(
            base::MakeRefCounted<viz::TestInProcessContextProvider>(
                viz::TestContextType::kGpuRaster,
                /*support_locking=*/false)) {}

  void SetUp() override {
    gpu::ContextResult result = context_provider_->BindToCurrentSequence();
    max_texture_size_ =
        context_provider_->ContextCapabilities().max_texture_size;
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    cache_ = std::make_unique<GpuImageDecodeCache>(
        context_provider_.get(), kRGBA_8888_SkColorType, kCacheSize,
        MaxTextureSize(), nullptr);
  }

 protected:
  size_t MaxTextureSize() const { return 4096; }

  // Returns dimensions for an image that will fit in GPU memory.
  gfx::Size GetNormalImageSize() const {
    int dimension = std::min(100, max_texture_size_ - 1);
    return gfx::Size(dimension, dimension);
  }

  perf_test::PerfResultReporter SetUpReporter(
      const std::string& metric_suffix) {
    perf_test::PerfResultReporter reporter("gpu_image_decode_cache", "GPU");
    reporter.RegisterImportantMetric(metric_suffix, "runs/s");
    return reporter;
  }

  base::LapTimer timer_;
  scoped_refptr<viz::TestInProcessContextProvider> context_provider_;
  std::unique_ptr<GpuImageDecodeCache> cache_;
  int max_texture_size_ = 0;
};

TEST_F(GpuImageDecodeCachePerfTest, DecodeWithColorConversion) {
  timer_.Reset();
  auto gfx_size = GetNormalImageSize();
  do {
    DrawImage image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(CreateImage(gfx_size.width(), gfx_size.height()),
                       PaintImage::GetNextContentId())
            .TakePaintImage(),
        false, SkIRect::MakeWH(gfx_size.width(), gfx_size.height()),
        PaintFlags::FilterQuality::kMedium,
        CreateMatrix(SkSize::Make(1.0f, 1.0f)), 0u,
        TargetColorParams(gfx::ColorSpace::CreateXYZD50()));

    DecodedDrawImage decoded_image = cache_->GetDecodedImageForDraw(image);
    cache_->DrawWithImageFinished(image, decoded_image);
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter =
      SetUpReporter("_with_color_conversion");
  reporter.AddResult("_with_color_conversion", timer_.LapsPerSecond());
}

TEST_F(GpuImageDecodeCachePerfTest, DecodeWithMips) {
  auto gfx_size = GetNormalImageSize();
  timer_.Reset();
  do {
    DrawImage image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(CreateImage(gfx_size.width(), gfx_size.height()),
                       PaintImage::GetNextContentId())
            .TakePaintImage(),
        false, SkIRect::MakeWH(gfx_size.width(), gfx_size.height()),
        PaintFlags::FilterQuality::kMedium,
        CreateMatrix(SkSize::Make(0.6f, 0.6f)), 0u, TargetColorParams());

    DecodedDrawImage decoded_image = cache_->GetDecodedImageForDraw(image);

    cache_->DrawWithImageFinished(image, decoded_image);
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter = SetUpReporter("_with_mips");
  reporter.AddResult("_with_mips", timer_.LapsPerSecond());
}

TEST_F(GpuImageDecodeCachePerfTest, AcquireExistingImages) {
  timer_.Reset();
  auto gfx_size = GetNormalImageSize();
  DrawImage image(
      PaintImageBuilder::WithDefault()
          .set_id(PaintImage::GetNextId())
          .set_image(CreateImage(gfx_size.width(), gfx_size.height()),
                     PaintImage::GetNextContentId())
          .TakePaintImage(),
      false, SkIRect::MakeWH(gfx_size.width(), gfx_size.height()),
      PaintFlags::FilterQuality::kMedium,
      CreateMatrix(SkSize::Make(1.0f, 1.0f)), 0u,
      TargetColorParams(gfx::ColorSpace::CreateXYZD50()));

  DecodedDrawImage decoded_image = cache_->GetDecodedImageForDraw(image);
  cache_->DrawWithImageFinished(image, decoded_image);

  do {
    decoded_image = cache_->GetDecodedImageForDraw(image);
    cache_->DrawWithImageFinished(image, decoded_image);
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter =
      SetUpReporter("_acquire_existing_images");
  reporter.AddResult("_acquire_existing_images", timer_.LapsPerSecond());
}

}  // namespace
}  // namespace cc
