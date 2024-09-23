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
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"

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

enum class TestMode { kGpu, kSw };

class GpuImageDecodeCachePerfTest
    : public testing::Test,
      public testing::WithParamInterface<TestMode> {
 public:
  GpuImageDecodeCachePerfTest()
      : timer_(kWarmupRuns,
               base::Milliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        context_provider_(
            base::MakeRefCounted<viz::TestInProcessContextProvider>(
                ParamToTestContextType(GetParam()),
                /*support_locking=*/false)) {}

  void SetUp() override {
    gpu::ContextResult result = context_provider_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    cache_ = std::make_unique<GpuImageDecodeCache>(
        context_provider_.get(), UseTransferCache(), kRGBA_8888_SkColorType,
        kCacheSize, MaxTextureSize(), nullptr);
  }

 protected:
  size_t MaxTextureSize() const {
    switch (GetParam()) {
      case TestMode::kGpu:
        return 4096;
      case TestMode::kSw:
        return 0;
    }
  }

  bool UseTransferCache() const { return GetParam() == TestMode::kGpu; }

  const char* ParamName() const {
    switch (GetParam()) {
      case TestMode::kGpu:
        return "GPU";
      case TestMode::kSw:
        return "SW";
    }
  }

  viz::TestContextType ParamToTestContextType(TestMode mode) {
    switch (mode) {
      case TestMode::kGpu:
        return viz::TestContextType::kGpuRaster;
      case TestMode::kSw:
        return viz::TestContextType::kSoftwareRaster;
    }
  }

  perf_test::PerfResultReporter SetUpReporter(
      const std::string& metric_suffix) {
    perf_test::PerfResultReporter reporter("gpu_image_decode_cache",
                                           ParamName());
    reporter.RegisterImportantMetric(metric_suffix, "runs/s");
    return reporter;
  }

  base::LapTimer timer_;
  scoped_refptr<viz::TestInProcessContextProvider> context_provider_;
  std::unique_ptr<GpuImageDecodeCache> cache_;
};

INSTANTIATE_TEST_SUITE_P(P,
                         GpuImageDecodeCachePerfTest,
                         testing::Values(TestMode::kGpu, TestMode::kSw));

TEST_P(GpuImageDecodeCachePerfTest, DecodeWithColorConversion) {
  timer_.Reset();
  do {
    DrawImage image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(CreateImage(1024, 2048), PaintImage::GetNextContentId())
            .TakePaintImage(),
        false, SkIRect::MakeWH(1024, 2048), PaintFlags::FilterQuality::kMedium,
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

using GpuImageDecodeCachePerfTestNoSw = GpuImageDecodeCachePerfTest;
INSTANTIATE_TEST_SUITE_P(P,
                         GpuImageDecodeCachePerfTestNoSw,
                         testing::Values(TestMode::kGpu));

TEST_P(GpuImageDecodeCachePerfTestNoSw, DecodeWithMips) {
  // Surface to render into.
  auto surface = SkSurfaces::RenderTarget(
      context_provider_->GrContext(), skgpu::Budgeted::kNo,
      SkImageInfo::MakeN32Premul(2048, 2048));

  timer_.Reset();
  do {
    DrawImage image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(CreateImage(1024, 2048), PaintImage::GetNextContentId())
            .TakePaintImage(),
        false, SkIRect::MakeWH(1024, 2048), PaintFlags::FilterQuality::kMedium,
        CreateMatrix(SkSize::Make(0.6f, 0.6f)), 0u, TargetColorParams());

    DecodedDrawImage decoded_image = cache_->GetDecodedImageForDraw(image);

    cache_->DrawWithImageFinished(image, decoded_image);
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter = SetUpReporter("_with_mips");
  reporter.AddResult("_with_mips", timer_.LapsPerSecond());
}

TEST_P(GpuImageDecodeCachePerfTest, AcquireExistingImages) {
  timer_.Reset();
  DrawImage image(
      PaintImageBuilder::WithDefault()
          .set_id(PaintImage::GetNextId())
          .set_image(CreateImage(1024, 2048), PaintImage::GetNextContentId())
          .TakePaintImage(),
      false, SkIRect::MakeWH(1024, 2048), PaintFlags::FilterQuality::kMedium,
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
