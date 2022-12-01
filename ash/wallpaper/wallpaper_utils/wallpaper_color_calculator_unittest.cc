// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"

#include <memory>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_extraction_result.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace ash {
namespace {

const SkColor kDefaultColor = SK_ColorTRANSPARENT;

const SkColor kGray = SkColorSetRGB(10, 10, 10);

const SkColor kVibrantGreen = SkColorSetRGB(25, 200, 25);

// Image size that causes the WallpaperColorCalculator to synchronously extract
// the prominent color.
constexpr gfx::Size kSyncImageSize = gfx::Size(5, 5);

// Image size that causes the WallpaperColorCalculator to asynchronously extract
// the prominent color.
constexpr gfx::Size kAsyncImageSize = gfx::Size(50, 50);

class TestWallpaperColorCalculatorObserver
    : public WallpaperColorCalculatorObserver {
 public:
  TestWallpaperColorCalculatorObserver() {}

  TestWallpaperColorCalculatorObserver(
      const TestWallpaperColorCalculatorObserver&) = delete;
  TestWallpaperColorCalculatorObserver& operator=(
      const TestWallpaperColorCalculatorObserver&) = delete;

  ~TestWallpaperColorCalculatorObserver() override {}

  bool WasNotified() const { return notified_; }

  // WallpaperColorCalculatorObserver:
  void OnColorCalculationComplete() override { notified_ = true; }

 private:
  bool notified_ = false;
};

// Returns an image that will yield a color using the LumaRange::NORMAL and
// SaturationRange::VIBRANT values.
gfx::ImageSkia CreateColorProducingImage(const gfx::Size& size) {
  gfx::Canvas canvas(size, 1.0f, true);
  canvas.DrawColor(kGray);
  canvas.FillRect(gfx::Rect(0, 1, size.height(), 1), kVibrantGreen);
  return gfx::ImageSkia::CreateFrom1xBitmap(canvas.GetBitmap());
}

// Returns an image that will not yield a color using the LumaRange::NORMAL and
// SaturationRange::VIBRANT values.
gfx::ImageSkia CreateNonColorProducingImage(const gfx::Size& size) {
  gfx::Canvas canvas(size, 1.0f, true);
  canvas.DrawColor(kGray);
  return gfx::ImageSkia::CreateFrom1xBitmap(canvas.GetBitmap());
}

class WallpaperColorCalculatorTest : public testing::Test {
 public:
  WallpaperColorCalculatorTest();

  WallpaperColorCalculatorTest(const WallpaperColorCalculatorTest&) = delete;
  WallpaperColorCalculatorTest& operator=(const WallpaperColorCalculatorTest&) =
      delete;

  ~WallpaperColorCalculatorTest() override;

 protected:
  // Installs the given |task_runner| globally and on the |calculator_| instance
  // if it exists.
  void InstallTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Creates a new |calculator_| for the given |image| and installs the current
  // |task_runner_|.
  void CreateCalculator(const gfx::ImageSkia& image);

  std::unique_ptr<WallpaperColorCalculator> calculator_;

  // Required for asynchronous calculations.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  TestWallpaperColorCalculatorObserver observer_;

  base::HistogramTester histograms_;

 private:
  // Required for asynchronous calculations, e.g. by PostTaskAndReplyImpl.
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      task_runner_handle_;
};

WallpaperColorCalculatorTest::WallpaperColorCalculatorTest()
    : task_runner_(new base::TestMockTimeTaskRunner()) {
  CreateCalculator(CreateColorProducingImage(kAsyncImageSize));
  InstallTaskRunner(task_runner_);
}

WallpaperColorCalculatorTest::~WallpaperColorCalculatorTest() {}

void WallpaperColorCalculatorTest::InstallTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_handle_.reset();
  task_runner_handle_ =
      std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
          task_runner);
  if (calculator_)
    calculator_->SetTaskRunnerForTest(task_runner);
}

void WallpaperColorCalculatorTest::CreateCalculator(
    const gfx::ImageSkia& image) {
  std::vector<color_utils::ColorProfile> color_profiles;
  color_profiles.emplace_back(color_utils::LumaRange::NORMAL,
                              color_utils::SaturationRange::VIBRANT);
  calculator_ = std::make_unique<WallpaperColorCalculator>(
      image, color_profiles, task_runner_);
  calculator_->AddObserver(&observer_);
}

// Used to group the asynchronous calculation tests.
using WallPaperColorCalculatorAsyncTest = WallpaperColorCalculatorTest;

TEST_F(WallPaperColorCalculatorAsyncTest, MetricsForSuccessfulExtraction) {
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 0);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              IsEmpty());

  EXPECT_TRUE(calculator_->StartCalculation());
  task_runner_->RunUntilIdle();

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 1);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 1);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              ElementsAre(base::Bucket(RESULT_NORMAL_VIBRANT_OPAQUE, 1)));
}

TEST_F(WallPaperColorCalculatorAsyncTest, MetricsWhenPostingTaskFails) {
  scoped_refptr<base::NullTaskRunner> task_runner = new base::NullTaskRunner();
  InstallTaskRunner(task_runner);

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 0);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              IsEmpty());

  EXPECT_FALSE(calculator_->StartCalculation());
  task_runner_->RunUntilIdle();

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 0);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              IsEmpty());

  EXPECT_EQ(kDefaultColor,
            calculator_->get_calculated_colors().prominent_colors[0]);
  EXPECT_EQ(kDefaultColor, calculator_->get_calculated_colors().k_mean_color);
}

TEST_F(WallPaperColorCalculatorAsyncTest,
       ObserverNotifiedOnSuccessfulCalculation) {
  EXPECT_FALSE(observer_.WasNotified());

  EXPECT_TRUE(calculator_->StartCalculation());
  EXPECT_FALSE(observer_.WasNotified());

  task_runner_->RunUntilIdle();
  EXPECT_TRUE(observer_.WasNotified());
}

TEST_F(WallPaperColorCalculatorAsyncTest, ColorUpdatedOnSuccessfulCalculation) {
  std::vector<SkColor> colors = {kDefaultColor};
  SkColor k_mean_color = kDefaultColor;
  calculator_->set_calculated_colors_for_test(
      WallpaperCalculatedColors(colors, k_mean_color));

  EXPECT_TRUE(calculator_->StartCalculation());
  EXPECT_EQ(kDefaultColor,
            calculator_->get_calculated_colors().prominent_colors[0]);
  EXPECT_EQ(kDefaultColor, calculator_->get_calculated_colors().k_mean_color);

  task_runner_->RunUntilIdle();
  EXPECT_NE(kDefaultColor,
            calculator_->get_calculated_colors().prominent_colors[0]);
  EXPECT_EQ(kGray, calculator_->get_calculated_colors().k_mean_color);
}

TEST_F(WallPaperColorCalculatorAsyncTest,
       NoCrashWhenCalculatorDestroyedBeforeTaskProcessing) {
  EXPECT_TRUE(calculator_->StartCalculation());
  calculator_.reset();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunUntilIdle();
  EXPECT_FALSE(observer_.WasNotified());
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

// Used to group the synchronous calculation tests.
using WallpaperColorCalculatorSyncTest = WallpaperColorCalculatorTest;

TEST_F(WallpaperColorCalculatorSyncTest, MetricsForSuccessfulExtraction) {
  CreateCalculator(CreateColorProducingImage(kSyncImageSize));
  calculator_->SetTaskRunnerForTest(nullptr);

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 0);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              IsEmpty());

  EXPECT_TRUE(calculator_->StartCalculation());

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 1);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              ElementsAre(base::Bucket(RESULT_NORMAL_VIBRANT_OPAQUE, 1)));
}

TEST_F(WallpaperColorCalculatorSyncTest, MetricsForFailedExctraction) {
  CreateCalculator(CreateNonColorProducingImage(kSyncImageSize));

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 0);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              IsEmpty());

  EXPECT_TRUE(calculator_->StartCalculation());

  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.Durations", 1);
  histograms_.ExpectTotalCount("Ash.Wallpaper.ColorExtraction.UserDelay", 0);
  EXPECT_THAT(histograms_.GetAllSamples("Ash.Wallpaper.ColorExtractionResult2"),
              ElementsAre(base::Bucket(RESULT_NORMAL_VIBRANT_TRANSPARENT, 1)));
}

}  // namespace
}  // namespace ash
