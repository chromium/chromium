// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/test/sk_color_eq.h"

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

// A wrapper to call the QuitClosure when the callback runs.
void CallbackWrapper(base::RepeatingClosure closure,
                     const WallpaperCalculatedColors&) {
  closure.Run();
}

WallpaperColorCalculator::WallpaperColorCallback Wrap(
    base::RepeatingClosure closure) {
  return base::BindOnce(&CallbackWrapper, closure);
}

class WallpaperColorCalculatorTest : public testing::Test {
 public:
  WallpaperColorCalculatorTest();

  WallpaperColorCalculatorTest(const WallpaperColorCalculatorTest&) = delete;
  WallpaperColorCalculatorTest& operator=(const WallpaperColorCalculatorTest&) =
      delete;

  ~WallpaperColorCalculatorTest() override = default;

 protected:
  // Installs the given |task_runner| globally and on the |calculator_| instance
  // if it exists.
  void InstallTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Creates a new |calculator_| for the given |image| and installs an
  // appropriate TaskRunner.
  void CreateCalculator(const gfx::ImageSkia& image);

  std::unique_ptr<WallpaperColorCalculator> calculator_;

  base::HistogramTester histograms_;

 private:
  // Needed for RunLoop and ThreadPool usage.
  base::test::TaskEnvironment task_environment_;
};

WallpaperColorCalculatorTest::WallpaperColorCalculatorTest() {
  CreateCalculator(CreateColorProducingImage(kAsyncImageSize));
  InstallTaskRunner(task_environment_.GetMainThreadTaskRunner());
}

void WallpaperColorCalculatorTest::InstallTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (calculator_)
    calculator_->SetTaskRunnerForTest(task_runner);
}

void WallpaperColorCalculatorTest::CreateCalculator(
    const gfx::ImageSkia& image) {
  std::vector<color_utils::ColorProfile> color_profiles;
  color_profiles.emplace_back(color_utils::LumaRange::NORMAL,
                              color_utils::SaturationRange::VIBRANT);
  calculator_ =
      std::make_unique<WallpaperColorCalculator>(image, color_profiles);
}

// Used to group the asynchronous calculation tests.
using WallPaperColorCalculatorAsyncTest = WallpaperColorCalculatorTest;

void CalculationComplete(bool* notify,
                         base::OnceClosure quit_closure,
                         const WallpaperCalculatedColors& /*colors*/) {
  *notify = true;
  std::move(quit_closure).Run();
}

TEST_F(WallPaperColorCalculatorAsyncTest,
       ObserverNotifiedOnSuccessfulCalculation) {
  base::RunLoop run_loop;
  bool notified = false;
  EXPECT_TRUE(calculator_->StartCalculation(
      base::BindOnce(&CalculationComplete, &notified, run_loop.QuitClosure())));
  EXPECT_FALSE(notified);

  run_loop.Run();
  EXPECT_TRUE(notified);
}

TEST_F(WallPaperColorCalculatorAsyncTest, ColorUpdatedOnSuccessfulCalculation) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kJelly);

  std::vector<SkColor> colors = {kDefaultColor};
  SkColor k_mean_color = kDefaultColor;
  calculator_->set_calculated_colors_for_test(
      WallpaperCalculatedColors(colors, k_mean_color, kDefaultColor));

  base::RunLoop run_loop;
  EXPECT_TRUE(calculator_->StartCalculation(Wrap(run_loop.QuitClosure())));

  run_loop.Run();
  ASSERT_TRUE(calculator_->get_calculated_colors());
  EXPECT_NE(kDefaultColor,
            calculator_->get_calculated_colors()->prominent_colors[0]);
  EXPECT_EQ(kGray, calculator_->get_calculated_colors()->k_mean_color);
}

TEST_F(WallPaperColorCalculatorAsyncTest, CelebiCalculatedWhenJellyEnabled) {
  base::test::ScopedFeatureList features(features::kJelly);

  base::RunLoop run_loop;
  EXPECT_TRUE(calculator_->StartCalculation(Wrap(run_loop.QuitClosure())));

  run_loop.Run();
  ASSERT_TRUE(calculator_->get_calculated_colors());
  EXPECT_EQ(kVibrantGreen, calculator_->get_calculated_colors()->celebi_color);
}

TEST_F(WallPaperColorCalculatorAsyncTest,
       NoCrashWhenCalculatorDestroyedBeforeTaskProcessing) {
  base::RunLoop run_loop;
  bool notified = false;

  EXPECT_TRUE(calculator_->StartCalculation(
      base::BindOnce(&CalculationComplete, &notified, run_loop.QuitClosure())));
  calculator_.reset();

  // Since the calculator was deleted, Quit will never get called so we just
  // clear the pending tasks.
  run_loop.RunUntilIdle();
  EXPECT_FALSE(notified);
}

// Used to group the synchronous calculation tests.
using WallpaperColorCalculatorSyncTest = WallpaperColorCalculatorTest;

TEST_F(WallpaperColorCalculatorSyncTest, SetsCalculatedColorsSync) {
  CreateCalculator(CreateColorProducingImage(kSyncImageSize));
  calculator_->SetTaskRunnerForTest(nullptr);

  EXPECT_FALSE(calculator_->get_calculated_colors().has_value());
  EXPECT_TRUE(calculator_->StartCalculation(base::DoNothing()));
  EXPECT_TRUE(calculator_->get_calculated_colors().has_value());
  EXPECT_THAT(calculator_->get_calculated_colors()->prominent_colors,
              ElementsAre(kVibrantGreen));
  EXPECT_SKCOLOR_EQ(calculator_->get_calculated_colors()->k_mean_color, kGray);
}

TEST_F(WallpaperColorCalculatorSyncTest, SyncFailedExtraction) {
  CreateCalculator(CreateNonColorProducingImage(kSyncImageSize));

  EXPECT_FALSE(calculator_->get_calculated_colors().has_value());
  EXPECT_TRUE(calculator_->StartCalculation(base::DoNothing()));
  auto calculated_colors = calculator_->get_calculated_colors();
  EXPECT_TRUE(calculated_colors.has_value());
  // 1 color profile in test that failed to extract color.
  EXPECT_THAT(calculated_colors->prominent_colors,
              ElementsAre(kInvalidWallpaperColor));
  // `CreateNonColorProducingImage` returns solid gray.
  EXPECT_SKCOLOR_EQ(kGray, calculated_colors->k_mean_color);
}

}  // namespace
}  // namespace ash
