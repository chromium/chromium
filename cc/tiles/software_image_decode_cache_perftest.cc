// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/timer/lap_timer.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

sk_sp<SkImage> CreateImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  return SkImage::MakeFromBitmap(bitmap);
}

SkMatrix CreateMatrix(const SkSize& scale) {
  SkMatrix matrix;
  matrix.setScale(scale.width(), scale.height());
  return matrix;
}

class SoftwareImageDecodeCachePerfTest : public testing::Test {
 public:
  SoftwareImageDecodeCachePerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void RunFromImage() {
    SkFilterQuality qualities[] = {kNone_SkFilterQuality, kLow_SkFilterQuality,
                                   kMedium_SkFilterQuality,
                                   kHigh_SkFilterQuality};
    std::pair<SkIRect, SkIRect> image_rect_subrect[] = {
        std::make_pair(SkIRect::MakeWH(100, 100), SkIRect::MakeWH(100, 100)),
        std::make_pair(SkIRect::MakeWH(100, 100), SkIRect::MakeWH(50, 50)),
        std::make_pair(SkIRect::MakeWH(100, 100), SkIRect::MakeWH(1000, 1000))};
    std::pair<float, float> scales[] = {
        std::make_pair(1.f, 1.f), std::make_pair(0.5f, 0.5f),
        std::make_pair(2.f, 2.f), std::make_pair(0.5f, 1.5f)};

    std::vector<DrawImage> images;
    for (auto& quality : qualities) {
      for (auto& rect_subrect : image_rect_subrect) {
        auto& rect = rect_subrect.first;
        auto& subrect = rect_subrect.second;
        for (auto& scale : scales) {
          images.emplace_back(
              PaintImageBuilder::WithDefault()
                  .set_id(PaintImage::GetNextId())
                  .set_image(CreateImage(rect.width(), rect.height()),
                             PaintImage::GetNextContentId())
                  .TakePaintImage(),
              subrect, quality,
              CreateMatrix(SkSize::Make(scale.first, scale.second)), 0u,
              gfx::ColorSpace());
        }
      }
    }

    timer_.Reset();
    do {
      for (auto& image : images) {
        SoftwareImageDecodeCache::CacheKey::FromDrawImage(image,
                                                          kN32_SkColorType);
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter("software_image_decode_cache",
                                           "fromdrawimage");
    reporter.RegisterImportantMetric("", "runs/s");
    reporter.AddResult("", timer_.LapsPerSecond());
  }

 private:
  base::LapTimer timer_;
};

TEST_F(SoftwareImageDecodeCachePerfTest, FromDrawImage) {
  RunFromImage();
}

}  // namespace
}  // namespace cc
